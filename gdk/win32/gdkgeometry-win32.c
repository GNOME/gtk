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

/* gdkgeometry-win32.c: emulation of 32 bit coordinates within the
 * limits of Win32 GDI. Just a copy of the X11 version, more or less.
 * Probably totally bogus in functionality. Just a quick hack, to
 * get the thing to build. Need to write some test code for it.
 * Well, need to find out what it is supposed to do first ;-)
 *
 * The X11 version by Owen Taylor <otaylor@redhat.com>
 * Copyright Red Hat, Inc. 2000
 * Win32 hack by Tor Lillqvist <tml@iki.fi>
 */

#include "gdk.h"		/* For gdk_rectangle_intersect */
#include "gdkregion.h"
#include "gdkregion-generic.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"
#include "gdkdrawable-win32.h"
#include "gdkwindow-win32.h"

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
  gint win32_x;
  gint win32_y;
  GdkRectangle clip_rect;
};

static void gdk_window_compute_position   (GdkWindowImplWin32   *window,
					   GdkWindowParentPos   *parent_pos,
					   GdkWin32PositionInfo *info);
static void gdk_window_compute_parent_pos (GdkWindowImplWin32 *window,
					   GdkWindowParentPos *parent_pos);
static void gdk_window_premove            (GdkWindow          *window,
					   GdkWindowParentPos *parent_pos);
static void gdk_window_postmove           (GdkWindow          *window,
					   GdkWindowParentPos *parent_pos);
static void gdk_window_queue_translation  (GdkWindow          *window,
					   gint                dx,
					   gint                dy);
static void gdk_window_tmp_unset_bg       (GdkWindow          *window);
static void gdk_window_tmp_reset_bg       (GdkWindow          *window);
static void gdk_window_clip_changed       (GdkWindow          *window,
					   GdkRectangle       *old_clip,
					   GdkRectangle       *new_clip);

static GSList *translate_queue = NULL;

void
_gdk_windowing_window_get_offsets (GdkWindow *window,
				   gint      *x_offset,
				   gint      *y_offset)
{
  GdkWindowImplWin32 *impl =
    GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  *x_offset = impl->position_info.x_offset;
  *y_offset = impl->position_info.y_offset;
}

void
_gdk_window_init_position (GdkWindow *window)
{
  GdkWindowParentPos parent_pos;
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  gdk_window_compute_parent_pos (impl, &parent_pos);
  gdk_window_compute_position (impl, &parent_pos, &impl->position_info);
}

/**
 * gdk_window_scroll:
 * @window: a #GdkWindow
 * @dx: Amount to scroll in the X direction
 * @dy: Amount to scroll in the Y direction
 * 
 * Scroll the contents of its window, both pixels and children, by
 * the given amount. Portions of the window that the scroll operation
 * brings in from offscreen areas are invalidated. The invalidated
 * region may be bigger than what would strictly be necessary.
 * (For X11, a minimum area will be invalidated if the window has
 * no subwindows, or if the edges of the window's parent do not extend
 * beyond the edges of the window. In other cases, a multi-step process
 * is used to scroll the window which may produce temporary visual
 * artifacts and unnecessary invalidations.)
 **/
void
gdk_window_scroll (GdkWindow *window,
		   gint       dx,
		   gint       dy)
{
  gboolean can_guffaw_scroll = FALSE;
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);  
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* We can guffaw scroll if we are a child window, and the parent
   * does not extend beyond our edges.
   */
  
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
    {
      GdkWindowImplWin32 *parent_impl = GDK_WINDOW_IMPL_WIN32 (obj->parent->impl);  
      can_guffaw_scroll = (obj->x <= 0 &&
			   obj->y <= 0 &&
			   obj->x + impl->width >= parent_impl->width &&
			   obj->y + impl->height >= parent_impl->height);
    }

  if (!obj->children || !can_guffaw_scroll)
    {
      /* Use ScrollWindowEx, then move any children later
       */
      GList *tmp_list;
      GdkRegion *invalidate_region;
      GdkRectangle dest_rect;

      invalidate_region = gdk_region_rectangle (&impl->position_info.clip_rect);
      
      dest_rect = impl->position_info.clip_rect;
      dest_rect.x += dx;
      dest_rect.y += dy;
      gdk_rectangle_intersect (&dest_rect, &impl->position_info.clip_rect, &dest_rect);

      if (dest_rect.width > 0 && dest_rect.height > 0)
	{
	  GdkRegion *tmp_region;

	  tmp_region = gdk_region_rectangle (&dest_rect);
	  gdk_region_subtract (invalidate_region, tmp_region);
	  gdk_region_destroy (tmp_region);
	  
	  gdk_window_queue_translation (window, dx, dy);

	  if (ScrollWindowEx (GDK_WINDOW_HWND (window), dx, dy,
			      NULL, NULL, NULL, NULL, 0) == ERROR)
	    WIN32_API_FAILED ("ScrollWindowEx");
	}

      gdk_window_invalidate_region (window, invalidate_region, TRUE);
      gdk_region_destroy (invalidate_region);

      tmp_list = obj->children;
      while (tmp_list)
	{
	  GdkWindow * child = GDK_WINDOW (tmp_list->data);
	  
	  gdk_window_move (child, obj->x + dx, obj->y + dy);
	  
	  tmp_list = tmp_list->next;
	}
    }
  else
    {
      /* Guffaw scroll
       */
      g_warning ("gdk_window_scroll(): guffaw scrolling not yet implemented");
    }
}

void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkWin32PositionInfo new_info;
  GdkWindowParentPos parent_pos;
  RECT rect;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  gint dx, dy;
  gboolean is_move;
  gboolean is_resize;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  dx = x - obj->x;
  dy = y - obj->y;
  
  is_move = dx != 0 || dy != 0;
  is_resize = impl->width != width || impl->height != height;

  if (!is_move && !is_resize)
    return;
  
  obj->x = x;
  obj->y = y;
  impl->width = width;
  impl->height = height;

  gdk_window_compute_parent_pos (impl, &parent_pos);
  gdk_window_compute_position (impl, &parent_pos, &new_info);

  gdk_window_clip_changed (window, &impl->position_info.clip_rect, &new_info.clip_rect);

  parent_pos.x += obj->x;
  parent_pos.y += obj->y;
  parent_pos.win32_x += new_info.x;
  parent_pos.win32_y += new_info.y;
  parent_pos.clip_rect = new_info.clip_rect;

  d_xoffset = new_info.x_offset - impl->position_info.x_offset;
  d_yoffset = new_info.y_offset - impl->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      gint new_x0, new_y0, new_x1, new_y1;

      gdk_window_set_static_gravities (window, TRUE);

      if (d_xoffset < 0 || d_yoffset < 0)
	gdk_window_queue_translation (window, MIN (d_xoffset, 0), MIN (d_yoffset, 0));
	
      if (d_xoffset < 0)
	{
	  new_x0 = impl->position_info.x + d_xoffset;
	  new_x1 = impl->position_info.x + impl->position_info.width;
	}
      else
	{
	  new_x0 = impl->position_info.x;
	  new_x1 = impl->position_info.x + new_info.width + d_xoffset;
	}

      if (d_yoffset < 0)
	{
	  new_y0 = impl->position_info.y + d_yoffset;
	  new_y1 = impl->position_info.y + impl->position_info.height;
	}
      else
	{
	  new_y0 = impl->position_info.y;
	  new_y1 = impl->position_info.y + new_info.height + d_yoffset;
	}
      
      if (!MoveWindow (GDK_WINDOW_HWND (window),
		       new_x0, new_y0, new_x1 - new_x0, new_y1 - new_y0,
		       FALSE))
	  WIN32_API_FAILED ("MoveWindow");
      
      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_premove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      GetClientRect (GDK_WINDOW_HWND (window), &rect);

      if (!MoveWindow (GDK_WINDOW_HWND (window),
		       new_x0 + dx, new_y0 + dy,
		       rect.right - rect.left, rect.bottom - rect.top,
		       FALSE))
	  WIN32_API_FAILED ("MoveWindow");
      
      if (d_xoffset > 0 || d_yoffset > 0)
	gdk_window_queue_translation (window, MAX (d_xoffset, 0), MAX (d_yoffset, 0));
      
      if (!MoveWindow (GDK_WINDOW_HWND (window),
		       new_info.x, new_info.y, new_info.width, new_info.height,
		       FALSE))
	  WIN32_API_FAILED ("MoveWindow");
      
      if (impl->position_info.no_bg)
	gdk_window_tmp_reset_bg (window);

      if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
	ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNA);
      
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
	ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);
      
      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_premove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      /*
       * HB: Passing TRUE(=Redraw) to MoveWindow here fixes some
       * redraw problems with (e.g. testgtk main buttons)
       * scrolling. AFAIK the non flicker optimization would
       * be done by the GDI anyway, if the window is SW_HIDE.
       */
      if (is_resize)
	{
	  if (!MoveWindow (GDK_WINDOW_HWND (window),
			   new_info.x, new_info.y, new_info.width, new_info.height,
			   TRUE /*FALSE*/))
	    WIN32_API_FAILED ("MoveWindow");
	}
      else
	{
	  GetClientRect (GDK_WINDOW_HWND (window), &rect);
	  if (!MoveWindow (GDK_WINDOW_HWND (window),
			   new_info.x, new_info.y,
			   rect.right - rect.left, rect.bottom - rect.top,
			   TRUE /*FALSE*/))
	    WIN32_API_FAILED ("MoveWindow");
	}

      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_postmove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      if (impl->position_info.no_bg)
	gdk_window_tmp_reset_bg (window);

      if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
	ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNA);

      impl->position_info = new_info;
    }
}

static void
gdk_window_compute_position (GdkWindowImplWin32   *window,
			     GdkWindowParentPos   *parent_pos,
			     GdkWin32PositionInfo *info)
{
  GdkWindowObject *wrapper;
  int parent_x_offset;
  int parent_y_offset;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (window));

  wrapper = GDK_WINDOW_OBJECT (GDK_DRAWABLE_IMPL_WIN32 (window)->wrapper);

  info->big = FALSE;
  
  if (window->width <= 32768)
    {
      info->width = window->width;
      info->x = parent_pos->x + wrapper->x - parent_pos->win32_x;
    }
  else
    {
      info->big = TRUE;
      info->width = 32768;
      if (parent_pos->x + wrapper->x < -16384)
	{
	  if (parent_pos->x + wrapper->x + window->width < 16384)
	    info->x = parent_pos->x + wrapper->x + window->width - 32768 - parent_pos->win32_x;
	  else
	    info->x = -16384 - parent_pos->win32_y;
	}
      else
	info->x = parent_pos->x + wrapper->x - parent_pos->win32_x;
    }

  if (window->height <= 32768)
    {
      info->height = window->height;
      info->y = parent_pos->y + wrapper->y - parent_pos->win32_y;
    }
  else
    {
      info->big = TRUE;
      info->height = 32768;
      if (parent_pos->y + wrapper->y < -16384)
	{
	  if (parent_pos->y + wrapper->y + window->height < 16384)
	    info->y = parent_pos->y + wrapper->y + window->height - 32768 - parent_pos->win32_y;
	  else
	    info->y = -16384 - parent_pos->win32_y;
	}
      else
	info->y = parent_pos->y + wrapper->y - parent_pos->win32_y;
    }

  parent_x_offset = parent_pos->win32_x - parent_pos->x;
  parent_y_offset = parent_pos->win32_y - parent_pos->y;
  
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
gdk_window_compute_parent_pos (GdkWindowImplWin32 *window,
			       GdkWindowParentPos *parent_pos)
{
  GdkWindowObject *wrapper;
  GdkWindowObject *parent;
  GdkRectangle tmp_clip;
  
  int clip_xoffset = 0;
  int clip_yoffset = 0;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (window));

  wrapper = GDK_WINDOW_OBJECT (GDK_DRAWABLE_IMPL_WIN32 (window)->wrapper);
  
  parent_pos->x = 0;
  parent_pos->y = 0;
  parent_pos->win32_x = 0;
  parent_pos->win32_y = 0;

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
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (parent->impl);
      
      tmp_clip.x = - clip_xoffset;
      tmp_clip.y = - clip_yoffset;
      tmp_clip.width = impl->width;
      tmp_clip.height = impl->height;

      gdk_rectangle_intersect (&parent_pos->clip_rect, &tmp_clip, &parent_pos->clip_rect);

      parent_pos->x += parent->x;
      parent_pos->y += parent->y;
      parent_pos->win32_x += impl->position_info.x;
      parent_pos->win32_y += impl->position_info.y;

      clip_xoffset += parent->x;
      clip_yoffset += parent->y;

      parent = (GdkWindowObject *)parent->parent;
    }
}

static void
gdk_window_premove (GdkWindow          *window,
		    GdkWindowParentPos *parent_pos)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkWin32PositionInfo new_info;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  GdkWindowParentPos this_pos;
  
  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);
  
  gdk_window_compute_position (impl, parent_pos, &new_info);

  gdk_window_clip_changed (window, &impl->position_info.clip_rect, &new_info.clip_rect);

  this_pos.x = parent_pos->x + obj->x;
  this_pos.y = parent_pos->y + obj->y;
  this_pos.win32_x = parent_pos->win32_x + new_info.x;
  this_pos.win32_y = parent_pos->win32_y + new_info.y;
  this_pos.clip_rect = new_info.clip_rect;

  if (impl->position_info.mapped && !new_info.mapped)
    ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);

  d_xoffset = new_info.x_offset - impl->position_info.x_offset;
  d_yoffset = new_info.y_offset - impl->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      gint new_x0, new_y0, new_x1, new_y1;

      if (d_xoffset < 0 || d_yoffset < 0)
	gdk_window_queue_translation (window, MIN (d_xoffset, 0), MIN (d_yoffset, 0));
	
      if (d_xoffset < 0)
	{
	  new_x0 = impl->position_info.x + d_xoffset;
	  new_x1 = impl->position_info.x + impl->position_info.width;
	}
      else
	{
	  new_x0 = impl->position_info.x;
	  new_x1 = impl->position_info.x + new_info.width + d_xoffset;
	}

      if (d_yoffset < 0)
	{
	  new_y0 = impl->position_info.y + d_yoffset;
	  new_y1 = impl->position_info.y + impl->position_info.height;
	}
      else
	{
	  new_y0 = impl->position_info.y;
	  new_y1 = impl->position_info.y + new_info.height + d_yoffset;
	}

      if (!MoveWindow (GDK_WINDOW_HWND (window),
		       new_x0, new_y0, new_x1 - new_x0, new_y1 - new_y0,
		       FALSE))
	WIN32_API_FAILED ("MoveWindow");
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
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkWin32PositionInfo new_info;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  GdkWindowParentPos this_pos;
  
  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);
  
  gdk_window_compute_position (impl, parent_pos, &new_info);

  this_pos.x = parent_pos->x + obj->x;
  this_pos.y = parent_pos->y + obj->y;
  this_pos.win32_x = parent_pos->win32_x + new_info.x;
  this_pos.win32_y = parent_pos->win32_y + new_info.y;
  this_pos.clip_rect = new_info.clip_rect;

  d_xoffset = new_info.x_offset - impl->position_info.x_offset;
  d_yoffset = new_info.y_offset - impl->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      if (d_xoffset > 0 || d_yoffset > 0)
	gdk_window_queue_translation (window, MAX (d_xoffset, 0), MAX (d_yoffset, 0));
	
      if (!MoveWindow (GDK_WINDOW_HWND (window),
		       new_info.x, new_info.y, new_info.width, new_info.height,
		       FALSE))
	WIN32_API_FAILED ("MoveWindow");
    }

  if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNA);

  if (impl->position_info.no_bg)
    gdk_window_tmp_reset_bg (window);

  impl->position_info = new_info;

  tmp_list = obj->children;
  while (tmp_list)
    {
      gdk_window_postmove (tmp_list->data, &this_pos);
      tmp_list = tmp_list->next;
    }
}

static void
gdk_window_queue_translation (GdkWindow *window,
			      gint       dx,
			      gint       dy)
{
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);
  item->window = window;
  item->serial = GetMessageTime ();
  item->type = GDK_WINDOW_QUEUE_TRANSLATE;
  item->u.translate.dx = dx;
  item->u.translate.dy = dy;

  GDK_NOTE (EVENTS, g_print ("gdk_window_queue_translation %#x %ld %d,%d\n",
			     (guint) GDK_WINDOW_HWND (window),
			     item->serial,
			     dx, dy));

  gdk_drawable_ref (window);
  translate_queue = g_slist_append (translate_queue, item);
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow *window,
					GdkRegion *area)
{
#if 0
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);

  item->window = window;
  item->serial = GetMessageTime ();
  item->type = GDK_WINDOW_QUEUE_ANTIEXPOSE;
  item->u.antiexpose.area = area;

  GDK_NOTE (EVENTS, g_print ("_gdk_windowing_window_queue_antiexpose %#x %ld %dx%d@+%d+%d\n",
			     (guint) GDK_WINDOW_HWND (window),
			     item->serial,
			     area->extents.x2 - area->extents.x1,
			     area->extents.y2 - area->extents.y1,
			     area->extents.x1, area->extents.y1));

  gdk_drawable_ref (window);
  translate_queue = g_slist_append (translate_queue, item);

  return TRUE;
#else
  GdkRectangle r;
  HRGN hrgn;

  gdk_region_get_clipbox (area, &r);
  hrgn = CreateRectRgn(r.x, r.y, r.width+1, r.height+1);

  g_return_val_if_fail (area != NULL, FALSE);

  GDK_NOTE (MISC, g_print ("_gdk_windowing_window_queue_antiexpose %#x\n",
			   (guint) GDK_WINDOW_HWND (window)));

  /* HB: not quite sure if this is the right thing to do.
   * (Region not to be proceesed by next WM_PAINT)
   */
  ValidateRgn(GDK_WINDOW_HWND (window), hrgn);
  DeleteObject(hrgn);
  return TRUE;
#endif
}

void
_gdk_window_process_expose (GdkWindow    *window,
			    gulong        serial,
			    GdkRectangle *area)
{
  GdkWindowImplWin32 *impl;
  GdkRegion *invalidate_region = gdk_region_rectangle (area);
  GdkRegion *clip_region;
  GSList *tmp_list = translate_queue;

  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  
  GDK_NOTE (EVENTS, g_print ("_gdk_window_process_expose %#x %ld %dx%d@+%d+%d\n",
			     (guint) GDK_WINDOW_HWND (window), serial,
			     area->width, area->height, area->x, area->y));

  while (tmp_list)
    {
      GdkWindowQueueItem *item = tmp_list->data;
      tmp_list = tmp_list->next;

      if (serial < item->serial)
	{
	  if (item->window == window)
	    {
	      if (item->type == GDK_WINDOW_QUEUE_TRANSLATE)
		gdk_region_offset (invalidate_region, - item->u.translate.dx, - item->u.translate.dy);
	      else		/* anti-expose */
		gdk_region_subtract (invalidate_region, item->u.antiexpose.area);
	    }
	}
      else
	{
	  GSList *tmp_link = translate_queue;
	  
	  translate_queue = g_slist_remove_link (translate_queue, translate_queue);
	  gdk_drawable_unref (item->window);

	  if (item->type == GDK_WINDOW_QUEUE_ANTIEXPOSE)
	    gdk_region_destroy (item->u.antiexpose.area);
	  
	  g_free (item);
	  g_slist_free_1 (tmp_link);
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
gdk_window_tmp_unset_bg (GdkWindow *window)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  impl->position_info.no_bg = TRUE;

  if (obj->bg_pixmap != GDK_NO_BG)
    /* ??? */;
}

static void
gdk_window_tmp_reset_bg (GdkWindow *window)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  impl->position_info.no_bg = FALSE;

  if (obj->bg_pixmap == GDK_NO_BG)
    return;
  
  if (obj->bg_pixmap)
    {
      /* ??? */
    }
  else
    {
      /* ??? */
    }
}

static void
gdk_window_clip_changed (GdkWindow    *window,
			 GdkRectangle *old_clip,
			 GdkRectangle *new_clip)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkRegion *old_clip_region;
  GdkRegion *new_clip_region;

  if (((GdkWindowObject *)window)->input_only)
    return;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);
  
  old_clip_region = gdk_region_rectangle (old_clip);
  new_clip_region = gdk_region_rectangle (new_clip);

  /* Trim invalid region of window to new clip rectangle
   */
  if (obj->update_area)
    gdk_region_intersect (obj->update_area, new_clip_region);

  /* Invalidate newly exposed portion of window
   */
  gdk_region_subtract (new_clip_region, old_clip_region);
  if (!gdk_region_empty (new_clip_region))
    {
      gdk_window_tmp_unset_bg (window);
      gdk_window_invalidate_region (window, new_clip_region, FALSE);
    }

  gdk_region_destroy (new_clip_region);
  gdk_region_destroy (old_clip_region);
}
