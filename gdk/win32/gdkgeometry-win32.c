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
 * and Hans Breuer <hans@breuer.org>
 */

#include <config.h>
#include "gdk.h"		/* For gdk_rectangle_intersect */
#include "gdkregion.h"
#include "gdkregion-generic.h"
#include "gdkprivate-win32.h"

#define SIZE_LIMIT 32000

typedef struct _GdkWindowQueueItem GdkWindowQueueItem;
typedef struct _GdkWindowParentPos GdkWindowParentPos;

#if 0

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

#endif

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

#if 0
static GSList *translate_queue = NULL;
#endif

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

static void
gdk_window_copy_area_scroll (GdkWindow    *window,
			     GdkRectangle *dest_rect,
			     gint          dx,
			     gint          dy)
{
#if 0
  GdkWindowObject *obj = GDK_WINDOW_OBJECT (window);
  GList *tmp_list;
#endif

  GDK_NOTE (MISC, g_print ("gdk_window_copy_area_scroll: %p %s %d,%d\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_gdkrectangle_to_string (dest_rect),
			   dx, dy));

  if (dest_rect->width > 0 && dest_rect->height > 0)
    {
      RECT clipRect;
#ifdef G_ENABLE_DEBUG
      RECT update_rect;
#endif

      clipRect.left   = dest_rect->x;
      clipRect.top    = dest_rect->y;
      clipRect.right  = clipRect.left + dest_rect->width;
      clipRect.bottom = clipRect.top + dest_rect->height;

      if (dx < 0)
	clipRect.right -= dx;
      else
	clipRect.left -= dx;
      if (dy < 0)
	clipRect.bottom -= dy;
      else
	clipRect.top -= dy;

      gdk_window_queue_translation (window, dx, dy);

      if (!ScrollWindowEx (GDK_WINDOW_HWND (window),
                           dx, dy, /* in: scroll offsets */
                           NULL, /* in: scroll rect, NULL == entire client area */
                           &clipRect, /* in: restrict to */
                           NULL, /* in: update region */
                           NULL, /* out: update rect */
                           SW_INVALIDATE | SW_SCROLLCHILDREN))
        WIN32_API_FAILED ("ScrollWindowEx");

      GDK_NOTE (EVENTS,
		(GetUpdateRect (GDK_WINDOW_HWND (window), &update_rect, FALSE),
		 g_print ("gdk_window_copy_area_scroll: post-scroll update rect: %s\n",
			  _gdk_win32_rect_to_string (&update_rect))));
    }

#if 0 /* Not needed, ScrollWindowEx also scrolls the children. */
  tmp_list = obj->children;
  while (tmp_list)
    {
      GdkWindow *child = GDK_WINDOW (tmp_list->data);
      GdkWindowObject *child_obj = GDK_WINDOW_OBJECT (child);
	  
      gdk_window_move (child, child_obj->x + dx, child_obj->y + dy);
      
      tmp_list = tmp_list->next;
    }
#endif
}

static void
compute_intermediate_position (GdkWin32PositionInfo *position_info,
			       GdkWin32PositionInfo *new_info,
			       gint                  d_xoffset,
			       gint                  d_yoffset,
			       GdkRectangle         *new_position)
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

#if 0

static void
gdk_window_guffaw_scroll (GdkWindow    *window,
			  gint          dx,
			  gint          dy)
{
  GdkWindowObject *obj = GDK_WINDOW_OBJECT (window);
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  gint d_xoffset = -dx;
  gint d_yoffset = -dy;
  GdkRectangle new_position;
  GdkWin32PositionInfo new_info;
  GdkWindowParentPos parent_pos;
  GList *tmp_list;
  
  GDK_NOTE (EVENTS, g_print ("gdk_window_guffaw_scroll: %p %d,%d\n",
			     GDK_WINDOW_HWND (window), dx, dy));

  gdk_window_compute_parent_pos (impl, &parent_pos);
  gdk_window_compute_position (impl, &parent_pos, &new_info);

  parent_pos.x += obj->x;
  parent_pos.y += obj->y;
  parent_pos.win32_x += new_info.x;
  parent_pos.win32_y += new_info.y;
  parent_pos.clip_rect = new_info.clip_rect;

  gdk_window_tmp_unset_bg (window);

  if (d_xoffset < 0 || d_yoffset < 0)
    gdk_window_queue_translation (window, MIN (d_xoffset, 0), MIN (d_yoffset, 0));
	
  gdk_window_set_static_gravities (window, TRUE);

  compute_intermediate_position (&impl->position_info, &new_info, d_xoffset, d_yoffset,
				 &new_position);
  
  /* XXX: this is only translating the X11 code. Don't know why the
   *  window movement needs to be done in three steps there, and here ??
   */
  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                     new_position.x, new_position.y,
		     new_position.width, new_position.height,
                     SWP_NOACTIVATE | SWP_NOZORDER))
    WIN32_API_FAILED ("SetWindowPos");
  
  tmp_list = obj->children;
  while (tmp_list)
    {
      GDK_WINDOW_OBJECT(tmp_list->data)->x -= d_xoffset;
      GDK_WINDOW_OBJECT(tmp_list->data)->y -= d_yoffset;

      gdk_window_premove (tmp_list->data, &parent_pos);
      tmp_list = tmp_list->next;
    }
  
  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                     new_position.x - d_xoffset, new_position.y - d_yoffset, 1, 1,
                     SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE))
    WIN32_API_FAILED ("SetWindowPos");

  if (d_xoffset > 0 || d_yoffset > 0)
    gdk_window_queue_translation (window, MAX (d_xoffset, 0), MAX (d_yoffset, 0));
  
  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                     impl->position_info.x, impl->position_info.y,
                     impl->position_info.width, impl->position_info.height,
                     SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE))
    WIN32_API_FAILED ("SetWindowPos");
  
  if (impl->position_info.no_bg)
    gdk_window_tmp_reset_bg (window);
  
  impl->position_info = new_info;
  
  tmp_list = obj->children;
  while (tmp_list)
    {
      gdk_window_postmove (tmp_list->data, &parent_pos);
      tmp_list = tmp_list->next;
    }
}

#endif

void
gdk_window_scroll (GdkWindow *window,
		   gint       dx,
		   gint       dy)
{
#if 0
  gboolean can_guffaw_scroll = FALSE;
#endif
  GdkRegion *invalidate_region;
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkRectangle dest_rect;
  
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
    }
  
  gdk_window_invalidate_region (window, invalidate_region, TRUE);
  gdk_region_destroy (invalidate_region);
#if 0
  /* We can guffaw scroll if we are a child window, and the parent
   * does not extend beyond our edges. Otherwise, we use XCopyArea, then
   * move any children later
   */
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
    {
      GdkWindowImplWin32 *parent_impl = GDK_WINDOW_IMPL_WIN32 (obj->parent->impl);  
      can_guffaw_scroll = ((dx == 0 || (obj->x <= 0 && obj->x + impl->width >= parent_impl->width)) &&
			   (dy == 0 || (obj->y <= 0 && obj->y + impl->height >= parent_impl->height)));
    }

  if (!obj->children || !can_guffaw_scroll)
    gdk_window_copy_area_scroll (window, &dest_rect, dx, dy);
  else
    gdk_window_guffaw_scroll (window, dx, dy);
#else
  gdk_window_copy_area_scroll (window, &dest_rect, dx, dy);
#endif
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
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);
  
  GDK_NOTE (MISC, g_print ("_gdk_window_move_resize_child: %s@+%d+%d %dx%d@+%d+%d\n",
			   _gdk_win32_drawable_description (window),
			   obj->x, obj->y,
			   width, height, x, y));

  dx = x - obj->x;
  dy = y - obj->y;
  
  is_move = dx != 0 || dy != 0;
  is_resize = impl->width != width || impl->height != height;

  if (!is_move && !is_resize)
    {
      GDK_NOTE (MISC, g_print ("...neither move or resize\n"));
      return;
    }
  
  GDK_NOTE (MISC, g_print ("...%s%s\n", is_move ? "is_move " : "", is_resize ? "is_resize" : ""));

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
      GdkRectangle new_position;

      GDK_NOTE (MISC, g_print ("...d_xoffset=%d d_yoffset=%d\n", d_xoffset, d_yoffset));

      gdk_window_set_static_gravities (window, TRUE);

      if (d_xoffset < 0 || d_yoffset < 0)
	gdk_window_queue_translation (window, MIN (d_xoffset, 0), MIN (d_yoffset, 0));

      compute_intermediate_position (&impl->position_info, &new_info, d_xoffset, d_yoffset,
				     &new_position);
      
      GDK_NOTE (MISC, g_print ("...SetWindowPos(%p,%s)\n",
			       GDK_WINDOW_HWND (window),
			       _gdk_win32_gdkrectangle_to_string (&new_position)));
      if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                         new_position.x, new_position.y, 
                         new_position.width, new_position.height,
                         SWP_NOACTIVATE | SWP_NOZORDER))
        WIN32_API_FAILED ("SetWindowPos");
        
      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_premove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      GDK_NOTE (MISC, g_print ("...SetWindowPos(%p,0x0@+%d+%d)\n",
			       GDK_WINDOW_HWND (window),
			       new_position.x + dx, new_position.y + dy));
      if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                         new_position.x + dx, new_position.y + dy, 0, 0,
                         SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_NOREDRAW))
        WIN32_API_FAILED ("SetWindowPos");
      
      if (d_xoffset > 0 || d_yoffset > 0)
	gdk_window_queue_translation (window, MAX (d_xoffset, 0), MAX (d_yoffset, 0));
      
      GDK_NOTE (MISC, g_print ("...SetWindowPos(%p,%dx%d@+%d+%d)\n",
			       GDK_WINDOW_HWND (window),
			       new_info.width, new_info.height,
			       new_info.x, new_info.y));
      if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                         new_info.x, new_info.y, 
                         new_info.width, new_info.height,
                         SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW))
        WIN32_API_FAILED ("SetWindowPos");
      
      if (impl->position_info.no_bg)
	gdk_window_tmp_reset_bg (window);

      if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
	{
	  GDK_NOTE (MISC, g_print ("...ShowWindow(%p, SW_SHOWNA)\n",
				   GDK_WINDOW_HWND (window)));
	  ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNA);
	}
      
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
	{
	  GDK_NOTE (MISC, g_print ("...ShowWindow(%p, SW_HIDE)\n",
				   GDK_WINDOW_HWND (window)));
	  ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);
	}
      
      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_premove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      GDK_NOTE (MISC, g_print ("...SetWindowPos(%p,%dx%d@+%d+%d)\n",
			       GDK_WINDOW_HWND (window),
			       new_info.width, new_info.height,
			       new_info.x, new_info.y));
      if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                         new_info.x, new_info.y, 
                         new_info.width, new_info.height,
                         SWP_NOACTIVATE | SWP_NOZORDER | 
                         (is_move ? 0 : SWP_NOMOVE) |
                         (is_resize ? 0 : SWP_NOSIZE)))
        WIN32_API_FAILED ("SetWindowPos");

      tmp_list = obj->children;
      while (tmp_list)
	{
	  gdk_window_postmove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      if (impl->position_info.no_bg)
	gdk_window_tmp_reset_bg (window);

      if (!impl->position_info.mapped && new_info.mapped && GDK_WINDOW_IS_MAPPED (obj))
	{
	  GDK_NOTE (MISC, g_print ("...ShowWindow(%p, SW_SHOWNA)\n",
				   GDK_WINDOW_HWND (window)));
	  ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNA);
	}

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
      GdkRectangle new_position;

      if (d_xoffset < 0 || d_yoffset < 0)
	gdk_window_queue_translation (window, MIN (d_xoffset, 0), MIN (d_yoffset, 0));

      compute_intermediate_position (&impl->position_info, &new_info, d_xoffset, d_yoffset,
				     &new_position);

      GDK_NOTE (MISC, g_print ("gdk_window_premove: %s@+%d+%d\n"
			       "...SetWindowPos(%s)\n",
			       _gdk_win32_drawable_description (window),
			       obj->x, obj->y,
			       _gdk_win32_gdkrectangle_to_string (&new_position)));

      if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                         new_position.x, new_position.y, 
                         new_position.width, new_position.height,
		       SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE))
        WIN32_API_FAILED ("SetWindowPos");
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
	
      GDK_NOTE (MISC, g_print ("gdk_window_postmove: %s@+%d+%d\n"
			       "...SetWindowPos(%dx%d@+%d+%d)\n",
			       _gdk_win32_drawable_description (window),
			       obj->x, obj->y,
			       new_info.width, new_info.height,
			       new_info.x, new_info.y));

      if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
                         new_info.x, new_info.y, 
                         new_info.width, new_info.height,
                         SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE))
        WIN32_API_FAILED ("SetWindowPos");
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

#if 0

static void
gdk_window_queue_append (GdkWindow          *window,
			 GdkWindowQueueItem *item)
{
  if (g_slist_length (translate_queue) >= 128)
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

      GDK_NOTE (EVENTS, g_print ("gdk_window_queue_append: length >= 128\n"));
      _gdk_window_process_expose (window, _gdk_win32_get_next_tick (0),
				  &impl->position_info.clip_rect);
    }

  item->window = window;
  item->serial = GetTickCount ();

  GDK_NOTE (EVENTS, (g_print ("gdk_window_queue_append: %s %p %ld ",
			      (item->type == GDK_WINDOW_QUEUE_TRANSLATE ?
			       "TRANSLATE" : "ANTIEXPOSE"),
			      GDK_WINDOW_HWND (window),
			      item->serial),
		     (item->type == GDK_WINDOW_QUEUE_TRANSLATE ?
		      g_print ("%d,%d\n",
			       item->u.translate.dx, item->u.translate.dy) :
		      g_print ("%s\n",
			       _gdk_win32_gdkregion_to_string (item->u.antiexpose.area)))));

  g_object_ref (window);
  translate_queue = g_slist_append (translate_queue, item) ;
}

#endif

static void
gdk_window_queue_translation (GdkWindow *window,
			      gint       dx,
			      gint       dy)
{
#if 0
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);
  item->type = GDK_WINDOW_QUEUE_TRANSLATE;
  item->u.translate.dx = dx;
  item->u.translate.dy = dy;

  gdk_window_queue_append (window, item);
#endif
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
#if 0
  GSList *tmp_list = translate_queue;
#endif
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  
  GDK_NOTE (EVENTS, g_print ("_gdk_window_process_expose: %p %s\n",
			     GDK_WINDOW_HWND (window),
			     _gdk_win32_gdkregion_to_string (invalidate_region)));
#if 0
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
		  GDK_NOTE (EVENTS, g_print ("...item %ld xlating region by %d,%d\n",
					     item->serial,
					     item->u.translate.dx, item->u.translate.dy));
		  gdk_region_offset (invalidate_region, - item->u.translate.dx, - item->u.translate.dy);
		}
	      else		/* anti-expose */
		{
#ifdef G_ENABLE_DEBUG
		  GdkRectangle rect;

		  GDK_NOTE (EVENTS,
			    (gdk_region_get_clipbox (item->u.antiexpose.area, &rect),
			     g_print ("...item %ld antiexposing %s\n",
				      item->serial,
				      _gdk_win32_gdkrectangle_to_string (&rect))));
#endif
		  gdk_region_subtract (invalidate_region, item->u.antiexpose.area);
		}
	    }
	}
      else
	{
	  GSList *tmp_link = translate_queue;
	  
	  GDK_NOTE (EVENTS, g_print ("...item %ld being removed\n", item->serial));

	  translate_queue = g_slist_remove_link (translate_queue, translate_queue);
	  g_object_unref (item->window);

	  if (item->type == GDK_WINDOW_QUEUE_ANTIEXPOSE)
	    gdk_region_destroy (item->u.antiexpose.area);
	  
	  g_free (item);
	  g_slist_free_1 (tmp_link);
	}
    }

  GDK_NOTE (EVENTS, g_print ("...queue length now %d\n", g_slist_length (translate_queue)));
#endif
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
      gdk_window_tmp_unset_bg (window);
      GDK_NOTE (EVENTS,
		g_print ("gdk_window_clip_changed: invalidating region: %s\n",
			 _gdk_win32_gdkregion_to_string (new_clip_region)));
      gdk_window_invalidate_region (window, new_clip_region, FALSE);
    }

  gdk_region_destroy (new_clip_region);
  gdk_region_destroy (old_clip_region);
}
