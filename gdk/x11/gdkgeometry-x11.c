/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* gdkgeometry-x11.c: emulation of 32 bit coordinates within the
 * limits of X. 
 *
 * By Owen Taylor <otaylor@redhat.com>
 * Copyright Red Hat, Inc. 2000
 *
 * The algorithms implemented in this file are an extension of the
 * idea of guffaw scrolling, a technique (and name) taken from the classic
 * Netscape source code. The basic idea of guffaw scrolling is a trick
 * to get around a limitation of X: there is no way of scrolling the
 * contents of a window. Guffaw scrolling exploits the X concepts of
 * window gravity and bit gravity:
 *
 *  window gravity: the window gravity of a window affects what happens
 *   to a windows position when _its parent_ is resized, or
 *   moved and resized simultaneously.
 *
 *  bit gravity: the bit gravity of a window affects what happens to
 *  the pixels of a window when _it_ is is resized, or moved and
 *  resized simultaneously.
 *
 * These were basically intended to do things like have right
 * justified widgets in a window automatically stay right justified
 * when the window was resized, but there is also the special
 * "StaticGravity" which means "do nothing." We can exploit
 * StaticGravity to scroll a window:
 *
 *     |  VISIBLE  |
 * 
 *     |abcdefghijk|
 *     |abcdefghijk    |   (1) Resize bigger
 * |    efghijk    |       (2) Move  
 *     |efghijk    |       (3) Move-resize back to the original size
 *
 * Or, going the other way:

 *     |abcdefghijk|
 * |    abcdefghijk|       (1) Move-resize bigger
 *     |    abcdefghijk|   (2) Move  
 *     |    abcdefg|       (4) Resize back to the original size
 *
 * By using this technique, we can simulate scrolling around in a
 * large virtual space without having to actually have windows that
 * big; for the pixels of the window, this is all we have to do.  For
 * subwindows, we have to take care of one other detail - since
 * coordinates in X are limited to 16 bits, subwindows scrolled off
 * will wrap around and come back eventually. So, we have to take care
 * to unmap windows that go outside the 16-bit range and remap them as
 * they come back in.
 *
 * Since we are temporarily making the window bigger, this only looks
 * good if the edges of the window are obscured. Typically, we do
 * this by making the window we are scrolling the immediate child
 * of a "clip window".
 *
 * But, this isn't a perfect API for applications for several reasons:
 *
 *  - We have to use this inefficient technique even for small windows
 *    if the window _could_ be big.
 *  - Applications have to use a special scrolling API.
 *
 * What we'd like is to simply have windows with 32 bit coordinates
 * so applications could scroll in the classic way - just move a big
 * window around.
 *
 * It turns out that StaticGravity can also be used to achieve emulation
 * of 32 bit coordinates with only 16 bit coordinates if we expand
 * our horizons just a bit; what guffaw scrolling really is is a way
 * to move the contents of a window a different amount than we move
 * the borders of of the window. In the above example pictures we
 * ended up with the borders of the window not moving at all, but
 * that isn't necessary.
 *
 * So, what we do is set up a mapping from virtual 32 bit window position/size
 * to:
 *
 *  - Real window position/size
 *  - Offset between virtual coordinates and real coordinates for the window
 *  - Map state (mapped or unmapped)
 *
 * By the following rules:
 *
 *  - If the window is less than 32767 pixels in width (resp. height), we use it's
 *    virtual width and position.
 *  - Otherwise, we use a width of 32767 and determine the position of the window
 *    so that the portion of the real window [16384, 16383] in _toplevel window
 *    coordinates_ is the same as the portion of the real window 
 *
 * This is implemented in gdk_window_compute_position(). Then the algorithm
 * for a moving a window (_window_move_resize_child ()) is:
 * 
 *  - Compute the new window mappings for the window and all subwindows
 *  - Expand out the boundary of the window and all subwindows by the amount
 *    that the real/virtual offset changes for each window. 
 *    (compute_intermediate_position() computes expanded boundary)
 *  - Move the toplevel by the amount that it's contents need to translate.
 *  - Move/resize the window and all subwindows to the newly computed
 *    positions.
 *
 * If we just are scrolling (gdk_window_guffaw_scroll()), then things
 * are similar, except that the final mappings for the toplevel are
 * the same as the initial mappings, but we act as if it moved by the
 * amount we are scrolling by.
 *
 * Note that we don't have to worry about a clip window in
 * _gdk_window_move_resize() since we have set up our translation so
 * that things in the range [16384,16383] in toplevel window
 * coordinates look exactly as they would if we were simply moving the
 * windows, and nothing outside this range is going to be visible
 * unless the user has a _really_ huge screen.
 */

#include <config.h>
#include "gdk.h"		/* For gdk_rectangle_intersect */
#include "gdkprivate-x11.h"
#include "gdkx.h"
#include "gdkregion.h"
#include "gdkinternals.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkwindow-x11.h"
#include "gdkalias.h"

typedef struct _GdkWindowQueueItem GdkWindowQueueItem;
typedef struct _GdkWindowParentPos GdkWindowParentPos;

typedef enum {
  GDK_WINDOW_QUEUE_TRANSLATE,
  GDK_WINDOW_QUEUE_ANTIEXPOSE
} GdkWindowQueueType;

struct _GdkWindowQueueItem
{
  GdkWindow *window;
  gulong serial;
  GdkWindowQueueType type;
  union {
    struct {
      GdkRegion *area;
      gint dx;
      gint dy;
    } translate;
    struct {
      GdkRegion *area;
    } antiexpose;
  } u;
};

struct _GdkWindowParentPos
{
  gint x;
  gint y;
  gint x11_x;
  gint x11_y;
  GdkRectangle clip_rect;
};

static void gdk_window_compute_position   (GdkWindowImplX11      *window,
					   GdkWindowParentPos *parent_pos,
					   GdkXPositionInfo   *info);
static void gdk_window_compute_parent_pos (GdkWindowImplX11      *window,
					   GdkWindowParentPos *parent_pos);
static void gdk_window_premove            (GdkWindow          *window,
					   GdkWindowParentPos *parent_pos);
static void gdk_window_postmove           (GdkWindow          *window,
					   GdkWindowParentPos *parent_pos);
static void gdk_window_queue_translation  (GdkWindow          *window,
					   GdkRegion	      *area,
					   gint                dx,
					   gint                dy);
static void gdk_window_clip_changed       (GdkWindow          *window,
					   GdkRectangle       *old_clip,
					   GdkRectangle       *new_clip);

void
_gdk_windowing_window_get_offsets (GdkWindow *window,
				   gint      *x_offset,
				   gint      *y_offset)
{
  GdkWindowImplX11 *impl =
    GDK_WINDOW_IMPL_X11 (GDK_WINDOW_OBJECT (window)->impl);

  *x_offset = impl->position_info.x_offset;
  *y_offset = impl->position_info.y_offset;
}

void
_gdk_window_init_position (GdkWindow *window)
{
  GdkWindowParentPos parent_pos;
  GdkWindowImplX11 *impl;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  impl =
    GDK_WINDOW_IMPL_X11 (GDK_WINDOW_OBJECT (window)->impl);
  
  gdk_window_compute_parent_pos (impl, &parent_pos);
  gdk_window_compute_position (impl, &parent_pos, &impl->position_info);
}

static void
gdk_window_copy_area_scroll (GdkWindow    *window,
			     GdkRectangle *dest_rect,
			     gint          dx,
			     gint          dy)
{
  GdkWindowObject *obj = GDK_WINDOW_OBJECT (window);
  GList *tmp_list;

  if (dest_rect->width > 0 && dest_rect->height > 0)
    {
      GdkGC *gc;

      gc = _gdk_drawable_get_scratch_gc (window, TRUE);
      
      gdk_window_queue_translation (window, NULL, dx, dy);
   
      XCopyArea (GDK_WINDOW_XDISPLAY (window),
		 GDK_WINDOW_XID (window),
		 GDK_WINDOW_XID (window),
		 gdk_x11_gc_get_xgc (gc),
		 dest_rect->x - dx, dest_rect->y - dy,
		 dest_rect->width, dest_rect->height,
		 dest_rect->x, dest_rect->y);
    }

  tmp_list = obj->children;
  while (tmp_list)
    {
      GdkWindow *child = GDK_WINDOW (tmp_list->data);
      GdkWindowObject *child_obj = GDK_WINDOW_OBJECT (child);
	  
      gdk_window_move (child, child_obj->x + dx, child_obj->y + dy);
      
      tmp_list = tmp_list->next;
    }
}

static void
compute_intermediate_position (GdkXPositionInfo *position_info,
			       GdkXPositionInfo *new_info,
			       gint              d_xoffset,
			       gint              d_yoffset,
			       GdkRectangle     *new_position)
{
  gint new_x0, new_x1, new_y0, new_y1;
  
  /* Wrap d_xoffset, d_yoffset into [-32768,32767] range. For the
   * purposes of subwindow movement, it doesn't matter if we are
   * off by a factor of 65536, and if we don't do this range
   * reduction, we'll end up with invalid widths.
   */
  d_xoffset = (gint16)d_xoffset;
  d_yoffset = (gint16)d_yoffset;
  
  if (d_xoffset < 0)
    {
      new_x0 = position_info->x + d_xoffset;
      new_x1 = position_info->x + position_info->width;
    }
  else
    {
      new_x0 = position_info->x;
      new_x1 = position_info->x + new_info->width + d_xoffset;
    }

  new_position->x = new_x0;
  new_position->width = new_x1 - new_x0;
  
  if (d_yoffset < 0)
    {
      new_y0 = position_info->y + d_yoffset;
      new_y1 = position_info->y + position_info->height;
    }
  else
    {
      new_y0 = position_info->y;
      new_y1 = position_info->y + new_info->height + d_yoffset;
    }
  
  new_position->y = new_y0;
  new_position->height = new_y1 - new_y0;
}

static void
gdk_window_guffaw_scroll (GdkWindow    *window,
			  gint          dx,
			  gint          dy)
{
  GdkWindowObject *obj = GDK_WINDOW_OBJECT (window);
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (obj->impl);

  gint d_xoffset = -dx;
  gint d_yoffset = -dy;
  GdkRectangle new_position;
  GdkXPositionInfo new_info;
  GdkWindowParentPos parent_pos;
  GList *tmp_list;
  
  gdk_window_compute_parent_pos (impl, &parent_pos);
  gdk_window_compute_position (impl, &parent_pos, &new_info);

  parent_pos.x += obj->x;
  parent_pos.y += obj->y;
  parent_pos.x11_x += new_info.x;
  parent_pos.x11_y += new_info.y;
  parent_pos.clip_rect = new_info.clip_rect;

  _gdk_x11_window_tmp_unset_bg (window, FALSE);;

  if (dx > 0 || dy > 0)
    gdk_window_queue_translation (window, NULL, MAX (dx, 0), MAX (dy, 0));
	
  gdk_window_set_static_gravities (window, TRUE);

  compute_intermediate_position (&impl->position_info, &new_info, d_xoffset, d_yoffset,
				 &new_position);
  
  XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     new_position.x, new_position.y, new_position.width, new_position.height);
  
  tmp_list = obj->children;
  while (tmp_list)
    {
      GDK_WINDOW_OBJECT(tmp_list->data)->x -= d_xoffset;
      GDK_WINDOW_OBJECT(tmp_list->data)->y -= d_yoffset;

      gdk_window_premove (tmp_list->data, &parent_pos);
      tmp_list = tmp_list->next;
    }
  
  XMoveWindow (GDK_WINDOW_XDISPLAY (window),
	       GDK_WINDOW_XID (window),
	       new_position.x - d_xoffset, new_position.y - d_yoffset);
  
  if (dx < 0 || dy < 0)
    gdk_window_queue_translation (window, NULL, MIN (dx, 0), MIN (dy, 0));
  
  XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     impl->position_info.x, impl->position_info.y,
		     impl->position_info.width, impl->position_info.height);
  
  if (impl->position_info.no_bg)
    _gdk_x11_window_tmp_reset_bg (window, FALSE);
  
  impl->position_info = new_info;
  
  tmp_list = obj->children;
  while (tmp_list)
    {
      gdk_window_postmove (tmp_list->data, &parent_pos);
      tmp_list = tmp_list->next;
    }
}

/**
 * gdk_window_scroll:
 * @window: a #GdkWindow
 * @dx: Amount to scroll in the X direction
 * @dy: Amount to scroll in the Y direction
 * 
 * Scroll the contents of @window, both pixels and children, by the given
 * amount. @window itself does not move.  Portions of the window that the scroll
 * operation brings in from offscreen areas are invalidated. The invalidated
 * region may be bigger than what would strictly be necessary.  (For X11, a
 * minimum area will be invalidated if the window has no subwindows, or if the
 * edges of the window's parent do not extend beyond the edges of the window. In
 * other cases, a multi-step process is used to scroll the window which may
 * produce temporary visual artifacts and unnecessary invalidations.)
 **/
void
gdk_window_scroll (GdkWindow *window,
		   gint       dx,
		   gint       dy)
{
  gboolean can_guffaw_scroll = FALSE;
  GdkRegion *invalidate_region;
  GdkWindowImplX11 *impl;
  GdkWindowObject *obj;
  GdkRectangle src_rect, dest_rect;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_X11 (obj->impl);  

  if (dx == 0 && dy == 0)
    return;
  
  /* Move the current invalid region */
  if (obj->update_area)
    gdk_region_offset (obj->update_area, dx, dy);
  
  /* impl->position_info.clip_rect isn't meaningful for toplevels */
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
    src_rect = impl->position_info.clip_rect;
  else
    {
      src_rect.x = 0;
      src_rect.y = 0;
      src_rect.width = impl->width;
      src_rect.height = impl->height;
    }
  
  invalidate_region = gdk_region_rectangle (&src_rect);

  dest_rect = src_rect;
  dest_rect.x += dx;
  dest_rect.y += dy;
  gdk_rectangle_intersect (&dest_rect, &src_rect, &dest_rect);

  if (dest_rect.width > 0 && dest_rect.height > 0)
    {
      GdkRegion *tmp_region;

      tmp_region = gdk_region_rectangle (&dest_rect);
      gdk_region_subtract (invalidate_region, tmp_region);
      gdk_region_destroy (tmp_region);
    }
  
  gdk_window_invalidate_region (window, invalidate_region, TRUE);
  gdk_region_destroy (invalidate_region);

  /* We can guffaw scroll if we are a child window, and the parent
   * does not extend beyond our edges. Otherwise, we use XCopyArea, then
   * move any children later
   */
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
    {
      GdkWindowImplX11 *parent_impl = GDK_WINDOW_IMPL_X11 (obj->parent->impl);  
      can_guffaw_scroll = ((dx == 0 || (obj->x <= 0 && obj->x + impl->width >= parent_impl->width)) &&
			   (dy == 0 || (obj->y <= 0 && obj->y + impl->height >= parent_impl->height)));
    }

  if (!obj->children || !can_guffaw_scroll)
    gdk_window_copy_area_scroll (window, &dest_rect, dx, dy);
  else
    gdk_window_guffaw_scroll (window, dx, dy);
}

/**
 * gdk_window_move_region:
 * @window: a #GdkWindow
 * @region: The #GdkRegion to move
 * @dx: Amount to move in the X direction
 * @dy: Amount to move in the Y direction
 * 
 * Move the part of @window indicated by @region by @dy pixels in the Y 
 * direction and @dx pixels in the X direction. The portions of @region 
 * that not covered by the new position of @region are invalidated.
 * 
 * Child windows are not moved.
 * 
 * Since: 2.8
 **/
void
gdk_window_move_region (GdkWindow *window,
			GdkRegion *region,
			gint       dx,
			gint       dy)
{
  GdkWindowImplX11 *impl;
  GdkWindowObject *private;
  GdkRegion *window_clip;
  GdkRegion *src_region;
  GdkRegion *brought_in;
  GdkRegion *dest_region;
  GdkRegion *moving_invalid_region;
  GdkRectangle dest_extents;
  GdkGC *gc;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (region != NULL);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_X11 (private->impl);  

  if (dx == 0 && dy == 0)
    return;

  window_clip = gdk_region_rectangle (&impl->position_info.clip_rect);

  /* compute source regions */
  src_region = gdk_region_copy (region);
  brought_in = gdk_region_copy (region);
  gdk_region_intersect (src_region, window_clip);

  gdk_region_subtract (brought_in, src_region);
  gdk_region_offset (brought_in, dx, dy);

  /* compute destination regions */
  dest_region = gdk_region_copy (src_region);
  gdk_region_offset (dest_region, dx, dy);
  gdk_region_intersect (dest_region, window_clip);
  gdk_region_get_clipbox (dest_region, &dest_extents);

  gdk_region_destroy (window_clip);

  /* calculating moving part of current invalid area */
  moving_invalid_region = NULL;
  if (private->update_area)
    {
      moving_invalid_region = gdk_region_copy (private->update_area);
      gdk_region_intersect (moving_invalid_region, src_region);
      gdk_region_offset (moving_invalid_region, dx, dy);
    }
  
  /* invalidate all of the src region */
  gdk_window_invalidate_region (window, src_region, FALSE);

  /* un-invalidate destination region */
  if (private->update_area)
    gdk_region_subtract (private->update_area, dest_region);
  
  /* invalidate moving parts of existing update area */
  if (moving_invalid_region)
    {
      gdk_window_invalidate_region (window, moving_invalid_region, FALSE);
      gdk_region_destroy (moving_invalid_region);
    }

  /* invalidate area brought in from off-screen */
  gdk_window_invalidate_region (window, brought_in, FALSE);
  gdk_region_destroy (brought_in);

  /* Actually do the moving */
  gdk_window_queue_translation (window, src_region, dx, dy);

  gc = _gdk_drawable_get_scratch_gc (window, TRUE);
  gdk_gc_set_clip_region (gc, dest_region);
  
  XCopyArea (GDK_WINDOW_XDISPLAY (window),
	     GDK_WINDOW_XID (window),
	     GDK_WINDOW_XID (window),
	     GDK_GC_XGC (gc),
	     dest_extents.x - dx, dest_extents.y - dy,
	     dest_extents.width, dest_extents.height,
	     dest_extents.x, dest_extents.y);

  /* Unset clip region of cached GC */
  gdk_gc_set_clip_region (gc, NULL);
  
  gdk_region_destroy (src_region);
  gdk_region_destroy (dest_region);
}

static void
reset_backgrounds (GdkWindow *window)
{
  GdkWindowObject *obj = (GdkWindowObject *)window;

  _gdk_x11_window_tmp_reset_bg (window, FALSE);
  
  if (obj->parent)
    _gdk_x11_window_tmp_reset_bg ((GdkWindow *)obj->parent, FALSE);
}

void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkWindowImplX11 *impl;
  GdkWindowObject *obj;
  GdkXPositionInfo new_info;
  GdkWindowParentPos parent_pos;
  GList *tmp_list;
  
  gint d_xoffset, d_yoffset;
  gint dx, dy;
  gboolean is_move;
  gboolean is_resize;

  GdkRectangle old_pos;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window)); 

  impl = GDK_WINDOW_IMPL_X11 (GDK_WINDOW_OBJECT (window)->impl);
  obj = GDK_WINDOW_OBJECT (window);

  dx = x - obj->x;
  dy = y - obj->y;
  
  is_move = dx != 0 || dy != 0;
  is_resize = impl->width != width || impl->height != height;

  if (!is_move && !is_resize)
    return;
  
  old_pos.x = obj->x;
  old_pos.y = obj->y;
  old_pos.width = impl->width;
  old_pos.height = impl->height;

  obj->x = x;
  obj->y = y;
  impl->width = width;
  impl->height = height;

  gdk_window_compute_parent_pos (impl, &parent_pos);
  gdk_window_compute_position (impl, &parent_pos, &new_info);

  gdk_window_clip_changed (window, &impl->position_info.clip_rect, &new_info.clip_rect);

  parent_pos.x += obj->x;
  parent_pos.y += obj->y;
  parent_pos.x11_x += new_info.x;
  parent_pos.x11_y += new_info.y;
  parent_pos.clip_rect = new_info.clip_rect;

  d_xoffset = new_info.x_offset - impl->position_info.x_offset;
  d_yoffset = new_info.y_offset - impl->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      GdkRectangle new_position;

      gdk_window_set_static_gravities (window, TRUE);

      if (d_xoffset < 0 || d_yoffset < 0)
	gdk_window_queue_translation (window, NULL, MIN (d_xoffset, 0), MIN (d_yoffset, 0));

      compute_intermediate_position (&impl->position_info, &new_info, d_xoffset, d_yoffset,
				     &new_position);
      
      XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XID (window),
			 new_position.x, new_position.y, new_position.width, new_position.height);
      
      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_premove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      XMoveWindow (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
		   new_position.x + dx, new_position.y + dy);
      
      if (d_xoffset > 0 || d_yoffset > 0)
	gdk_window_queue_translation (window, NULL, MAX (d_xoffset, 0), MAX (d_yoffset, 0));
      
      XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XID (window),
			 new_info.x, new_info.y, new_info.width, new_info.height);

      reset_backgrounds (window);

      if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
	XMapWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
      
      impl->position_info = new_info;
      
      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_postmove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}
    }
  else
    {
      if (is_move && is_resize)
	gdk_window_set_static_gravities (window, FALSE);

      if (impl->position_info.mapped && !new_info.mapped)
	XUnmapWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
      
      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_premove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      if (is_resize)
	XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
			   GDK_WINDOW_XID (window),
			   new_info.x, new_info.y, new_info.width, new_info.height);
      else
	XMoveWindow (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     new_info.x, new_info.y);

      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_postmove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      reset_backgrounds (window);
      
      if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
	XMapWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));

      impl->position_info = new_info;
    }

  if (GDK_WINDOW_IS_MAPPED (obj) && obj->parent)
    gdk_window_invalidate_rect ((GdkWindow *)obj->parent, &old_pos, FALSE);
}

static void
gdk_window_compute_position (GdkWindowImplX11   *window,
			     GdkWindowParentPos *parent_pos,
			     GdkXPositionInfo   *info)
{
  GdkWindowObject *wrapper;
  int parent_x_offset;
  int parent_y_offset;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_X11 (window));

  wrapper = GDK_WINDOW_OBJECT (GDK_DRAWABLE_IMPL_X11 (window)->wrapper);
  
  info->big = FALSE;
  
  if (window->width <= 32767)
    {
      info->width = window->width;
      info->x = parent_pos->x + wrapper->x - parent_pos->x11_x;
    }
  else
    {
      info->big = TRUE;
      info->width = 32767;
      if (parent_pos->x + wrapper->x < -16384)
	{
	  if (parent_pos->x + wrapper->x + window->width < 16384)
	    info->x = parent_pos->x + wrapper->x + window->width - info->width - parent_pos->x11_x;
	  else
	    info->x = -16384 - parent_pos->x11_x;
	}
      else
	info->x = parent_pos->x + wrapper->x - parent_pos->x11_x;
    }

  if (window->height <= 32767)
    {
      info->height = window->height;
      info->y = parent_pos->y + wrapper->y - parent_pos->x11_y;
    }
  else
    {
      info->big = TRUE;
      info->height = 32767;
      if (parent_pos->y + wrapper->y < -16384)
	{
	  if (parent_pos->y + wrapper->y + window->height < 16384)
	    info->y = parent_pos->y + wrapper->y + window->height - info->height - parent_pos->x11_y;
	  else
	    info->y = -16384 - parent_pos->x11_y;
	}
      else
	info->y = parent_pos->y + wrapper->y - parent_pos->x11_y;
    }

  parent_x_offset = parent_pos->x11_x - parent_pos->x;
  parent_y_offset = parent_pos->x11_y - parent_pos->y;
  
  info->x_offset = parent_x_offset + info->x - wrapper->x;
  info->y_offset = parent_y_offset + info->y - wrapper->y;

  /* We don't considering the clipping of toplevel windows and their immediate children
   * by their parents, and simply always map those windows.
   */
  if (parent_pos->clip_rect.width == G_MAXINT)
    info->mapped = TRUE;
  /* Check if the window would wrap around into the visible space in either direction */
  else if (info->x + parent_x_offset < parent_pos->clip_rect.x + parent_pos->clip_rect.width - 65536 ||
      info->x + info->width + parent_x_offset > parent_pos->clip_rect.x + 65536 ||
      info->y + parent_y_offset < parent_pos->clip_rect.y + parent_pos->clip_rect.height - 65536 ||
      info->y + info->height + parent_y_offset  > parent_pos->clip_rect.y + 65536)
    info->mapped = FALSE;
  else
    info->mapped = TRUE;

  info->no_bg = FALSE;

  if (GDK_WINDOW_TYPE (wrapper) == GDK_WINDOW_CHILD)
    {
      info->clip_rect.x = wrapper->x;
      info->clip_rect.y = wrapper->y;
      info->clip_rect.width = window->width;
      info->clip_rect.height = window->height;
      
      gdk_rectangle_intersect (&info->clip_rect, &parent_pos->clip_rect, &info->clip_rect);

      info->clip_rect.x -= wrapper->x;
      info->clip_rect.y -= wrapper->y;
    }
  else
    {
      info->clip_rect.x = 0;
      info->clip_rect.y = 0;
      info->clip_rect.width = G_MAXINT;
      info->clip_rect.height = G_MAXINT;
    }
}

static void
gdk_window_compute_parent_pos (GdkWindowImplX11      *window,
			       GdkWindowParentPos *parent_pos)
{
  GdkWindowObject *wrapper;
  GdkWindowObject *parent;
  GdkRectangle tmp_clip;
  
  int clip_xoffset = 0;
  int clip_yoffset = 0;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_X11 (window));

  wrapper =
    GDK_WINDOW_OBJECT (GDK_DRAWABLE_IMPL_X11 (window)->wrapper);
  
  parent_pos->x = 0;
  parent_pos->y = 0;
  parent_pos->x11_x = 0;
  parent_pos->x11_y = 0;

  /* We take a simple approach here and simply consider toplevel
   * windows not to clip their children on the right/bottom, since the
   * size of toplevel windows is not directly under our
   * control. Clipping only really matters when scrolling and
   * generally we aren't going to be moving the immediate child of a
   * toplevel beyond the bounds of that toplevel.
   *
   * We could go ahead and recompute the clips of toplevel windows and
   * their descendents when we receive size notification, but it would
   * probably not be an improvement in most cases.
   */
  parent_pos->clip_rect.x = 0;
  parent_pos->clip_rect.y = 0;
  parent_pos->clip_rect.width = G_MAXINT;
  parent_pos->clip_rect.height = G_MAXINT;

  parent = (GdkWindowObject *)wrapper->parent;
  while (parent && parent->window_type == GDK_WINDOW_CHILD)
    {
      GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (parent->impl);
      
      tmp_clip.x = - clip_xoffset;
      tmp_clip.y = - clip_yoffset;
      tmp_clip.width = impl->width;
      tmp_clip.height = impl->height;

      gdk_rectangle_intersect (&parent_pos->clip_rect, &tmp_clip, &parent_pos->clip_rect);

      parent_pos->x += parent->x;
      parent_pos->y += parent->y;
      parent_pos->x11_x += impl->position_info.x;
      parent_pos->x11_y += impl->position_info.y;

      clip_xoffset += parent->x;
      clip_yoffset += parent->y;

      parent = (GdkWindowObject *)parent->parent;
    }
}

static void
gdk_window_premove (GdkWindow          *window,
		    GdkWindowParentPos *parent_pos)
{
  GdkWindowImplX11 *impl;
  GdkWindowObject *obj;
  GdkXPositionInfo new_info;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  GdkWindowParentPos this_pos;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_X11 (obj->impl);
  
  gdk_window_compute_position (impl, parent_pos, &new_info);

  gdk_window_clip_changed (window, &impl->position_info.clip_rect, &new_info.clip_rect);

  this_pos.x = parent_pos->x + obj->x;
  this_pos.y = parent_pos->y + obj->y;
  this_pos.x11_x = parent_pos->x11_x + new_info.x;
  this_pos.x11_y = parent_pos->x11_y + new_info.y;
  this_pos.clip_rect = new_info.clip_rect;

  if (impl->position_info.mapped && !new_info.mapped)
    XUnmapWindow (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window));

  d_xoffset = new_info.x_offset - impl->position_info.x_offset;
  d_yoffset = new_info.y_offset - impl->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      GdkRectangle new_position;

      if (d_xoffset < 0 || d_yoffset < 0)
	gdk_window_queue_translation (window, NULL, MIN (d_xoffset, 0), MIN (d_yoffset, 0));

      compute_intermediate_position (&impl->position_info, &new_info, d_xoffset, d_yoffset,
				     &new_position);

      XMoveResizeWindow (GDK_DRAWABLE_XDISPLAY (window),
			 GDK_DRAWABLE_XID (window),
			 new_position.x, new_position.y, new_position.width, new_position.height);
    }

  tmp_list = obj->children;
  while (tmp_list)
    {
      gdk_window_premove (tmp_list->data, &this_pos);
      tmp_list = tmp_list->next;
    }
}

static void
gdk_window_postmove (GdkWindow          *window,
		     GdkWindowParentPos *parent_pos)
{
  GdkWindowImplX11 *impl;
  GdkWindowObject *obj;
  GdkXPositionInfo new_info;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  GdkWindowParentPos this_pos;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_X11 (obj->impl);
  
  gdk_window_compute_position (impl, parent_pos, &new_info);

  this_pos.x = parent_pos->x + obj->x;
  this_pos.y = parent_pos->y + obj->y;
  this_pos.x11_x = parent_pos->x11_x + new_info.x;
  this_pos.x11_y = parent_pos->x11_y + new_info.y;
  this_pos.clip_rect = new_info.clip_rect;

  d_xoffset = new_info.x_offset - impl->position_info.x_offset;
  d_yoffset = new_info.y_offset - impl->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      if (d_xoffset > 0 || d_yoffset > 0)
	gdk_window_queue_translation (window, NULL, MAX (d_xoffset, 0), MAX (d_yoffset, 0));
	
      XMoveResizeWindow (GDK_DRAWABLE_XDISPLAY (window),
			 GDK_DRAWABLE_XID (window),
			 new_info.x, new_info.y, new_info.width, new_info.height);
    }

  if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
    XMapWindow (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window));

  reset_backgrounds (window);

  impl->position_info = new_info;

  tmp_list = obj->children;
  while (tmp_list)
    {
      gdk_window_postmove (tmp_list->data, &this_pos);
      tmp_list = tmp_list->next;
    }
}

static Bool
expose_serial_predicate (Display *xdisplay,
			 XEvent  *xev,
			 XPointer arg)
{
  gulong *serial = (gulong *)arg;

  if (xev->xany.type == Expose)
    *serial = MIN (*serial, xev->xany.serial);

  return False;
}

/* Find oldest possible serial for an outstanding expose event
 */
static gulong
find_current_serial (Display *xdisplay)
{
  XEvent xev;
  gulong serial = NextRequest (xdisplay);
  
  XSync (xdisplay, False);

  XCheckIfEvent (xdisplay, &xev, expose_serial_predicate, (XPointer)&serial);

  return serial;
}

static void
queue_delete_link (GQueue *queue,
		   GList  *link)
{
  if (queue->tail == link)
    queue->tail = link->prev;
  
  queue->head = g_list_remove_link (queue->head, link);
  g_list_free_1 (link);
  queue->length--;
}

static void
queue_item_free (GdkWindowQueueItem *item)
{
  if (item->window)
    {
      g_object_remove_weak_pointer (G_OBJECT (item->window),
				    (gpointer *)&(item->window));
    }
  
  if (item->type == GDK_WINDOW_QUEUE_ANTIEXPOSE)
    gdk_region_destroy (item->u.antiexpose.area);
  else
    {
      if (item->u.translate.area)
	gdk_region_destroy (item->u.translate.area);
    }
  
  g_free (item);
}

static void
gdk_window_queue (GdkWindow          *window,
		  GdkWindowQueueItem *item)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));
  
  if (!display_x11->translate_queue)
    display_x11->translate_queue = g_queue_new ();

  /* Keep length of queue finite by, if it grows too long,
   * figuring out the latest relevant serial and discarding
   * irrelevant queue items.
   */
  if (display_x11->translate_queue->length >= 64)
    {
      gulong serial = find_current_serial (GDK_WINDOW_XDISPLAY (window));
      GList *tmp_list = display_x11->translate_queue->head;
      
      while (tmp_list)
	{
	  GdkWindowQueueItem *item = tmp_list->data;
	  GList *next = tmp_list->next;
	  
	  if (serial > item->serial)
	    {
	      queue_delete_link (display_x11->translate_queue, tmp_list);
	      queue_item_free (item);
	    }

	  tmp_list = next;
	}
    }

  /* Catch the case where someone isn't processing events and there
   * is an event stuck in the event queue with an old serial:
   * If we can't reduce the queue length by the above method,
   * discard anti-expose items. (We can't discard translate
   * items 
   */
  if (display_x11->translate_queue->length >= 64)
    {
      GList *tmp_list = display_x11->translate_queue->head;
      
      while (tmp_list)
	{
	  GdkWindowQueueItem *item = tmp_list->data;
	  GList *next = tmp_list->next;
	  
	  if (item->type == GDK_WINDOW_QUEUE_ANTIEXPOSE)
	    {
	      queue_delete_link (display_x11->translate_queue, tmp_list);
	      queue_item_free (item);
	    }

	  tmp_list = next;
	}
    }

  item->window = window;
  item->serial = NextRequest (GDK_WINDOW_XDISPLAY (window));
  
  g_object_add_weak_pointer (G_OBJECT (window),
			     (gpointer *)&(item->window));

  g_queue_push_tail (display_x11->translate_queue, item);
}

static void
gdk_window_queue_translation (GdkWindow *window,
			      GdkRegion *area,
			      gint       dx,
			      gint       dy)
{
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);
  item->type = GDK_WINDOW_QUEUE_TRANSLATE;
  item->u.translate.area = area ? gdk_region_copy (area) : NULL;
  item->u.translate.dx = dx;
  item->u.translate.dy = dy;

  gdk_window_queue (window, item);
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow *window,
					GdkRegion *area)
{
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);
  item->type = GDK_WINDOW_QUEUE_ANTIEXPOSE;
  item->u.antiexpose.area = area;

  gdk_window_queue (window, item);

  return TRUE;
}

void
_gdk_window_process_expose (GdkWindow    *window,
			    gulong        serial,
			    GdkRectangle *area)
{
  GdkWindowImplX11 *impl;
  GdkRegion *invalidate_region = gdk_region_rectangle (area);
  GdkRegion *clip_region;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));
  impl = GDK_WINDOW_IMPL_X11 (GDK_WINDOW_OBJECT (window)->impl);

  if (display_x11->translate_queue)
    {
      GList *tmp_list = display_x11->translate_queue->head;
      
      while (tmp_list)
	{
	  GdkWindowQueueItem *item = tmp_list->data;
	  tmp_list = tmp_list->next;
	  
	  if (serial < item->serial)
	    {
	      if (item->window == window)
		{
		  if (item->type == GDK_WINDOW_QUEUE_TRANSLATE)
		    {
		      if (item->u.translate.area)
			{
			  GdkRegion *intersection;
			  
			  intersection = gdk_region_copy (invalidate_region);
			  gdk_region_intersect (intersection, item->u.translate.area);
			  gdk_region_subtract (invalidate_region, intersection);
			  gdk_region_offset (intersection, item->u.translate.dx, item->u.translate.dy);
			  gdk_region_union (invalidate_region, intersection);
			  gdk_region_destroy (intersection);
			}
		      else
			gdk_region_offset (invalidate_region, item->u.translate.dx, item->u.translate.dy);
		    }
		  else		/* anti-expose */
		    {
		      gdk_region_subtract (invalidate_region, item->u.antiexpose.area);
		    }
		}
	    }
	  else
	    {
	      queue_delete_link (display_x11->translate_queue, 
				 display_x11->translate_queue->head);
	      queue_item_free (item);
	    }
	}
    }

  clip_region = gdk_region_rectangle (&impl->position_info.clip_rect);
  gdk_region_intersect (invalidate_region, clip_region);

  if (!gdk_region_empty (invalidate_region))
    gdk_window_invalidate_region (window, invalidate_region, FALSE);
  
  gdk_region_destroy (invalidate_region);
  gdk_region_destroy (clip_region);
}

static void
gdk_window_clip_changed (GdkWindow *window, GdkRectangle *old_clip, GdkRectangle *new_clip)
{
  GdkWindowImplX11 *impl;
  GdkWindowObject *obj;
  GdkRegion *old_clip_region;
  GdkRegion *new_clip_region;
  
  if (((GdkWindowObject *)window)->input_only)
    return;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_X11 (obj->impl);
  
  old_clip_region = gdk_region_rectangle (old_clip);
  new_clip_region = gdk_region_rectangle (new_clip);

  /* We need to update this here because gdk_window_invalidate_region makes
   * use if it (through gdk_drawable_get_visible_region
   */
  impl->position_info.clip_rect = *new_clip;
  
  /* Trim invalid region of window to new clip rectangle
   */
  if (obj->update_area)
    gdk_region_intersect (obj->update_area, new_clip_region);

  /* Invalidate newly exposed portion of window
   */
  gdk_region_subtract (new_clip_region, old_clip_region);
  if (!gdk_region_empty (new_clip_region))
    {
      _gdk_x11_window_tmp_unset_bg (window, FALSE);;
      gdk_window_invalidate_region (window, new_clip_region, FALSE);
    }

  if (obj->parent)
    _gdk_x11_window_tmp_unset_bg ((GdkWindow *)obj->parent, FALSE);
  
  gdk_region_destroy (new_clip_region);
  gdk_region_destroy (old_clip_region);
}

#define __GDK_GEOMETRY_X11_C__
#include "gdkaliasdef.c"
