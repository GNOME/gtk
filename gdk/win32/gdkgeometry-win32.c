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
 * limits of Win32 GDI. The idea of big window emulation is more or less
 * a copy of the X11 version, and the equvalent of guffaw scrolling
 * is ScrollWindowEx(). While we determine the invalidated region
 * ourself during scrolling, we do not pass SW_INVALIDATE to
 * ScrollWindowEx() to avoid a unnecessary WM_PAINT.
 *
 * Bits are always scrolled correctly by ScrollWindowEx(), but
 * some big children may hit the coordinate boundary (i.e.
 * win32_x/win32_y < -16383) after scrolling. They needed to be moved
 * back to the real position determined by gdk_window_compute_position().
 * This is handled in gdk_window_postmove().
 * 
 * The X11 version by Owen Taylor <otaylor@redhat.com>
 * Copyright Red Hat, Inc. 2000
 * Win32 hack by Tor Lillqvist <tml@iki.fi>
 * and Hans Breuer <hans@breuer.org>
 * Modified by Ivan, Wong Yat Cheung <email@ivanwong.info>
 * so that big window emulation finally works.
 */

#include <config.h>
#include "gdk.h"		/* For gdk_rectangle_intersect */
#include "gdkregion.h"
#include "gdkregion-generic.h"
#include "gdkprivate-win32.h"

#define SIZE_LIMIT 32767

typedef struct _GdkWindowParentPos GdkWindowParentPos;

struct _GdkWindowParentPos
{
  gint x;
  gint y;
  gint win32_x;
  gint win32_y;
  GdkRectangle clip_rect;
};

static void gdk_window_compute_position   (GdkWindowImplWin32     *window,
					   GdkWindowParentPos     *parent_pos,
					   GdkWin32PositionInfo   *info);
static void gdk_window_compute_parent_pos (GdkWindowImplWin32     *window,
					   GdkWindowParentPos     *parent_pos);

static void gdk_window_postmove           (GdkWindow          *window,
					   GdkWindowParentPos *parent_pos,
					   gboolean	      anti_scroll);
static void gdk_window_tmp_unset_bg       (GdkWindow          *window);
static void gdk_window_tmp_reset_bg       (GdkWindow          *window);
static GdkRegion *gdk_window_clip_changed (GdkWindow          *window,
					   GdkRectangle       *old_clip,
					   GdkRectangle       *new_clip);
static void gdk_window_post_scroll        (GdkWindow          *window,
			                   GdkRegion          *new_clip_region);

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

void
gdk_window_scroll (GdkWindow *window,
		   gint       dx,
		   gint       dy)
{
  GdkRegion *invalidate_region;
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GList *tmp_list;
  GdkWindowParentPos parent_pos;
  HRGN native_invalidate_region;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  GDK_NOTE (EVENTS, g_print ("gdk_window_scroll: %p %d,%d\n",
			     GDK_WINDOW_HWND (window), dx, dy));

  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);  

  if (dx == 0 && dy == 0)
    return;
  
  /* Move the current invalid region */
  if (obj->update_area)
    gdk_region_offset (obj->update_area, dx, dy);
  
  gdk_window_compute_parent_pos (impl, &parent_pos);

  parent_pos.x += obj->x;
  parent_pos.y += obj->y;
  parent_pos.win32_x += impl->position_info.x;
  parent_pos.win32_y += impl->position_info.y;
  parent_pos.clip_rect = impl->position_info.clip_rect;

  gdk_window_tmp_unset_bg (window);

  native_invalidate_region = CreateRectRgn (0, 0, 0, 0);
  if (native_invalidate_region == NULL)
    WIN32_API_FAILED ("CreateRectRgn");

  API_CALL (ScrollWindowEx, (GDK_WINDOW_HWND (window),
			     dx, dy, NULL, NULL,
			     native_invalidate_region, NULL, SW_SCROLLCHILDREN));

  if (impl->position_info.no_bg)
    gdk_window_tmp_reset_bg (window);
  
  tmp_list = obj->children;
  while (tmp_list)
    {
      GDK_WINDOW_OBJECT(tmp_list->data)->x += dx;
      GDK_WINDOW_OBJECT(tmp_list->data)->y += dy;
      gdk_window_postmove (tmp_list->data, &parent_pos, FALSE);
      tmp_list = tmp_list->next;
    }

  if (native_invalidate_region != NULL)
    {
      invalidate_region = _gdk_win32_hrgn_to_region (native_invalidate_region);
      gdk_region_offset (invalidate_region, impl->position_info.x_offset,
                         impl->position_info.y_offset);
      gdk_window_invalidate_region (window, invalidate_region, TRUE);
      gdk_region_destroy (invalidate_region);
      GDI_CALL (DeleteObject, (native_invalidate_region));
    }
}

void
gdk_window_move_region (GdkWindow *window,
			GdkRegion *region,
			gint       dx,
			gint       dy)
{
  GdkRegion *invalidate_region;
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkRectangle src_rect, dest_rect;
  HRGN hrgn;
  RECT clipRect, destRect;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);  

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
  
  /* no guffaw scroll on win32 */
  hrgn = _gdk_win32_gdkregion_to_hrgn(invalidate_region, 0, 0);
  gdk_region_destroy (invalidate_region);
  destRect.left = dest_rect.y;
  destRect.top = dest_rect.x;
  destRect.right = dest_rect.x + dest_rect.width;
  destRect.bottom = dest_rect.y + dest_rect.height;
  clipRect.left = src_rect.y;
  clipRect.top = src_rect.x;
  clipRect.right = src_rect.x + src_rect.width;
  clipRect.bottom = src_rect.y + src_rect.height;

  g_print ("ScrollWindowEx(%d, %d, ...) - if you see this work, remove trace;)\n", dx, dy);
  API_CALL(ScrollWindowEx, (GDK_WINDOW_HWND (window),
                       dx, dy, /* in: scroll offsets */
                       NULL, /* in: scroll rect, NULL == entire client area */
                       &clipRect, /* in: restrict to */
                       hrgn, /* in: update region */
                       NULL, /* out: update rect */
                       SW_INVALIDATE));
  API_CALL(DeleteObject, (hrgn));
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
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  gint dx, dy;
  gboolean is_move;
  gboolean is_resize;
  GdkRegion *new_clip_region;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);
  
  GDK_NOTE (MISC, g_print ("_gdk_window_move_resize_child: %s@%+d%+d %dx%d@%+d%+d\n",
			   _gdk_win32_drawable_description (window),
			   obj->x, obj->y,
			   width, height, x, y));

  dx = x - obj->x;
  dy = y - obj->y;
  
  is_move = dx != 0 || dy != 0;
  is_resize = impl->width != width || impl->height != height;

  if (!is_move && !is_resize)
    {
      GDK_NOTE (MISC, g_print ("... neither move or resize\n"));
      return;
    }
  
  GDK_NOTE (MISC, g_print ("... %s%s\n",
			   is_move ? "is_move " : "",
			   is_resize ? "is_resize" : ""));

  obj->x = x;
  obj->y = y;
  impl->width = width;
  impl->height = height;

  gdk_window_compute_parent_pos (impl, &parent_pos);
  gdk_window_compute_position (impl, &parent_pos, &new_info);

  new_clip_region =
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
      GDK_NOTE (MISC, g_print ("... d_offset=%+d%+d\n", d_xoffset, d_yoffset));

      if (!ScrollWindowEx (GDK_WINDOW_HWND (window),
			   -d_xoffset, -d_yoffset, /* in: scroll offsets */
			   NULL, /* in: scroll rect, NULL == entire client area */
			   NULL, /* in: restrict to */
			   NULL, /* in: update region */
			   NULL, /* out: update rect */
			   SW_SCROLLCHILDREN))
	WIN32_API_FAILED ("ScrollWindowEx");

      if (dx != d_xoffset || dy != d_yoffset || is_resize)
	{
	  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%d,%d,"
				   "NOACTIVATE|NOZORDER%s%s)\n",
				   GDK_WINDOW_HWND (window),
				   new_info.x, new_info.y, 
				   new_info.width, new_info.height,
				   (is_move ? "" : "|NOMOVE"),
				   (is_resize ? "" : "|NOSIZE")));

	  API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
				   new_info.x, new_info.y, 
				   new_info.width, new_info.height,
				   SWP_NOACTIVATE | SWP_NOZORDER | 
				   (is_move ? 0 : SWP_NOMOVE) |
				   (is_resize ? 0 : SWP_NOSIZE)));
	}

      if (impl->position_info.no_bg)
	gdk_window_tmp_reset_bg (window);

      if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
	{
	  GDK_NOTE (MISC, g_print ("... ShowWindow(%p, SW_SHOWNA)\n",
				   GDK_WINDOW_HWND (window)));
	  ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNA);
	}

      impl->position_info = new_info;
      
      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_postmove (tmp_list->data, &parent_pos, FALSE);
	  tmp_list = tmp_list->next;
	}
    }
  else
    {
      if (impl->position_info.mapped && !new_info.mapped)
	{
	  GDK_NOTE (MISC, g_print ("... ShowWindow(%p, SW_HIDE)\n",
				   GDK_WINDOW_HWND (window)));
	  ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);
	}
      
      GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%d,%d,"
			       "NOACTIVATE|NOZORDER%s%s)\n",
			       GDK_WINDOW_HWND (window),
			       new_info.x, new_info.y, 
			       new_info.width, new_info.height,
			       (is_move ? "" : "|NOMOVE"),
			       (is_resize ? "" : "|NOSIZE")));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
			       new_info.x, new_info.y, 
			       new_info.width, new_info.height,
			       SWP_NOACTIVATE | SWP_NOZORDER | 
			       (is_move ? 0 : SWP_NOMOVE) |
			       (is_resize ? 0 : SWP_NOSIZE)));

      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_postmove (tmp_list->data, &parent_pos, FALSE);
	  tmp_list = tmp_list->next;
	}

      if (impl->position_info.no_bg)
	gdk_window_tmp_reset_bg (window);

      if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
	{
	  GDK_NOTE (MISC, g_print ("... ShowWindow(%p, SW_SHOWNA)\n",
				   GDK_WINDOW_HWND (window)));
	  ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNA);
	}

      impl->position_info = new_info;
    }
  if (new_clip_region)
    gdk_window_post_scroll (window, new_clip_region);
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
  
  if (window->width <= SIZE_LIMIT)
    {
      info->width = window->width;
      info->x = parent_pos->x + wrapper->x - parent_pos->win32_x;
    }
  else
    {
      info->big = TRUE;
      info->width = SIZE_LIMIT;
      if (parent_pos->x + wrapper->x < -(SIZE_LIMIT/2))
	{
	  if (parent_pos->x + wrapper->x + window->width < (SIZE_LIMIT/2))
	    info->x = parent_pos->x + wrapper->x + window->width - info->width - parent_pos->win32_x;
	  else
	    info->x = -(SIZE_LIMIT/2) - parent_pos->win32_x;
	}
      else
	info->x = parent_pos->x + wrapper->x - parent_pos->win32_x;
    }

  if (window->height <= SIZE_LIMIT)
    {
      info->height = window->height;
      info->y = parent_pos->y + wrapper->y - parent_pos->win32_y;
    }
  else
    {
      info->big = TRUE;
      info->height = SIZE_LIMIT;
      if (parent_pos->y + wrapper->y < -(SIZE_LIMIT/2))
	{
	  if (parent_pos->y + wrapper->y + window->height < (SIZE_LIMIT/2))
	    info->y = parent_pos->y + wrapper->y + window->height - info->height - parent_pos->win32_y;
	  else
	    info->y = -(SIZE_LIMIT/2) - parent_pos->win32_y;
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
gdk_window_postmove (GdkWindow          *window,
		     GdkWindowParentPos *parent_pos,
		     gboolean	         anti_scroll)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkWin32PositionInfo new_info;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  GdkWindowParentPos this_pos;
  GdkRegion *new_clip_region;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);
  
  gdk_window_compute_position (impl, parent_pos, &new_info);

  new_clip_region =
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
  
  if (anti_scroll || (anti_scroll = d_xoffset != 0 || d_yoffset != 0))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_postmove: %s@%+d%+d\n"
			       "... SetWindowPos(%p,NULL,%d,%d,0,0,"
			       "NOREDRAW|NOZORDER|NOACTIVATE|NOSIZE)\n",
			       _gdk_win32_drawable_description (window),
			       obj->x, obj->y,
			       GDK_WINDOW_HWND (window),
			       new_info.x, new_info.y));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
			       new_info.x, new_info.y, 
			       0, 0,
			       SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
    }

  if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNA);

  if (impl->position_info.no_bg)
    gdk_window_tmp_reset_bg (window);

  impl->position_info = new_info;

  if (new_clip_region)
    gdk_window_post_scroll (window, new_clip_region);

  tmp_list = obj->children;
  while (tmp_list)
    {
      gdk_window_postmove (tmp_list->data, &this_pos, anti_scroll);
      tmp_list = tmp_list->next;
    }
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow *window,
					GdkRegion *area)
{
  HRGN hrgn = _gdk_win32_gdkregion_to_hrgn (area, 0, 0);

  GDK_NOTE (EVENTS, g_print ("_gdk_windowing_window_queue_antiexpose: ValidateRgn %p %s\n",
			     GDK_WINDOW_HWND (window),
			     _gdk_win32_gdkregion_to_string (area)));

  ValidateRgn (GDK_WINDOW_HWND (window), hrgn);

  DeleteObject (hrgn);

  return FALSE;
}

void
_gdk_window_process_expose (GdkWindow *window,
			    GdkRegion *invalidate_region)
{
  GdkWindowImplWin32 *impl;
  GdkRegion *clip_region;
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  
  GDK_NOTE (EVENTS, g_print ("_gdk_window_process_expose: %p %s\n",
			     GDK_WINDOW_HWND (window),
			     _gdk_win32_gdkregion_to_string (invalidate_region)));
  clip_region = gdk_region_rectangle (&impl->position_info.clip_rect);
  gdk_region_intersect (invalidate_region, clip_region);

  if (!gdk_region_empty (invalidate_region))
    gdk_window_invalidate_region (window, invalidate_region, FALSE);
  
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

  /*
   * The X version sets background = None to avoid updateing for a moment.
   * Not sure if this could really emulate it.
   */
  if (obj->bg_pixmap != GDK_NO_BG)
    /* handled in WM_ERASEBKGRND proceesing */;
}

static void
gdk_window_tmp_reset_bg (GdkWindow *window)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  impl->position_info.no_bg = FALSE;
}

static GdkRegion *
gdk_window_clip_changed (GdkWindow    *window,
			 GdkRectangle *old_clip,
			 GdkRectangle *new_clip)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkRegion *old_clip_region;
  GdkRegion *new_clip_region;
  
  if (((GdkWindowObject *)window)->input_only)
    return NULL;

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
    gdk_window_tmp_unset_bg (window);
  else
    {
      gdk_region_destroy (new_clip_region);
      new_clip_region = NULL;
    }
  gdk_region_destroy (old_clip_region);

  return new_clip_region;
}

static void
gdk_window_post_scroll (GdkWindow    *window,
			GdkRegion    *new_clip_region)
{
  GDK_NOTE (EVENTS,
	    g_print ("gdk_window_clip_changed: invalidating region: %s\n",
		     _gdk_win32_gdkregion_to_string (new_clip_region)));

  gdk_window_invalidate_region (window, new_clip_region, FALSE);
  gdk_region_destroy (new_clip_region);
}
