/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
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

static void gdk_window_compute_position   (GdkWindow            *window,
					   GdkWindowParentPos   *parent_pos,
					   GdkWin32PositionInfo *info);
static void gdk_window_compute_parent_pos (GdkWindow          *window,
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
  GdkWindowWin32Data *data = GDK_WINDOW_WIN32DATA (window);

  *x_offset = data->position_info.x_offset;
  *y_offset = data->position_info.y_offset;
}

void
_gdk_window_init_position (GdkWindow *window)
{
  GdkWindowWin32Data *data;
  GdkWindowParentPos parent_pos;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  data = GDK_WINDOW_WIN32DATA (window);

  gdk_window_compute_parent_pos (window, &parent_pos);
  gdk_window_compute_position (window, &parent_pos, &data->position_info);
}

void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GdkWin32PositionInfo new_info;
  GdkWindowParentPos parent_pos;
  GdkWindowWin32Data *data;
  RECT rect;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  gint dx, dy;
  gboolean is_move;
  gboolean is_resize;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  data = GDK_WINDOW_WIN32DATA (window);

  dx = x - private->x;
  dy = y - private->y;
  
  is_move = dx != 0 || dy != 0;
  is_resize = private->drawable.width != width || private->drawable.height != height;

  if (!is_move && !is_resize)
    return;
  
  private->x = x;
  private->y = y;
  private->drawable.width = width;
  private->drawable.height = height;

  gdk_window_compute_parent_pos (window, &parent_pos);
  gdk_window_compute_position (window, &parent_pos, &new_info);

  gdk_window_clip_changed (window, &data->position_info.clip_rect, &new_info.clip_rect);

  parent_pos.x += private->x;
  parent_pos.y += private->y;
  parent_pos.win32_x += new_info.x;
  parent_pos.win32_y += new_info.y;
  parent_pos.clip_rect = new_info.clip_rect;

  d_xoffset = new_info.x_offset - data->position_info.x_offset;
  d_yoffset = new_info.y_offset - data->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      gint new_x0, new_y0, new_x1, new_y1;

      gdk_window_set_static_gravities (window, TRUE);

      if (d_xoffset < 0 || d_yoffset < 0)
	gdk_window_queue_translation (window, MIN (d_xoffset, 0), MIN (d_yoffset, 0));
	
      if (d_xoffset < 0)
	{
	  new_x0 = data->position_info.x + d_xoffset;
	  new_x1 = data->position_info.x + data->position_info.width;
	}
      else
	{
	  new_x0 = data->position_info.x;
	  new_x1 = data->position_info.x + new_info.width + d_xoffset;
	}

      if (d_yoffset < 0)
	{
	  new_y0 = data->position_info.y + d_yoffset;
	  new_y1 = data->position_info.y + data->position_info.height;
	}
      else
	{
	  new_y0 = data->position_info.y;
	  new_y1 = data->position_info.y + new_info.height + d_yoffset;
	}
      
      if (!MoveWindow (GDK_DRAWABLE_XID (window),
		       new_x0, new_y0, new_x1 - new_x0, new_y1 - new_y0,
		       FALSE))
	  WIN32_API_FAILED ("MoveWindow");
      
      tmp_list = private->children;
      while (tmp_list)
	{
	  gdk_window_premove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      GetClientRect (GDK_DRAWABLE_XID (window), &rect);

      if (!MoveWindow (GDK_DRAWABLE_XID (window),
		       new_x0 + dx, new_y0 + dy,
		       rect.right - rect.left, rect.bottom - rect.top,
		       FALSE))
	  WIN32_API_FAILED ("MoveWindow");
      
      if (d_xoffset > 0 || d_yoffset > 0)
	gdk_window_queue_translation (window, MAX (d_xoffset, 0), MAX (d_yoffset, 0));
      
      if (!MoveWindow (GDK_DRAWABLE_XID (window),
		       new_info.x, new_info.y, new_info.width, new_info.height,
		       FALSE))
	  WIN32_API_FAILED ("MoveWindow");
      
      if (data->position_info.no_bg)
	gdk_window_tmp_reset_bg (window);

      if (!data->position_info.mapped && new_info.mapped && private->mapped)
	ShowWindow (GDK_DRAWABLE_XID (window), SW_SHOWNA);
      
      data->position_info = new_info;
      
      tmp_list = private->children;
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

      if (data->position_info.mapped && !new_info.mapped)
	ShowWindow (GDK_DRAWABLE_XID (window), SW_HIDE);
      
      tmp_list = private->children;
      while (tmp_list)
	{
	  gdk_window_premove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      if (is_resize)
	{
	  if (!MoveWindow (GDK_DRAWABLE_XID (window),
			   new_info.x, new_info.y, new_info.width, new_info.height,
			   FALSE))
	    WIN32_API_FAILED ("MoveWindow");
	}
      else
	{
	  GetClientRect (GDK_DRAWABLE_XID (window), &rect);
	  if (!MoveWindow (GDK_DRAWABLE_XID (window),
			   new_info.x, new_info.y,
			   rect.right - rect.left, rect.bottom - rect.top,
			   FALSE))
	    WIN32_API_FAILED ("MoveWindow");
	}

      tmp_list = private->children;
      while (tmp_list)
	{
	  gdk_window_postmove (tmp_list->data, &parent_pos);
	  tmp_list = tmp_list->next;
	}

      if (data->position_info.no_bg)
	gdk_window_tmp_reset_bg (window);

      if (!data->position_info.mapped && new_info.mapped && private->mapped)
	ShowWindow (GDK_DRAWABLE_XID (window), SW_SHOWNA);

      data->position_info = new_info;
    }
}

static void
gdk_window_compute_position (GdkWindow            *window,
			     GdkWindowParentPos   *parent_pos,
			     GdkWin32PositionInfo *info)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  int parent_x_offset;
  int parent_y_offset;
  
  info->big = FALSE;
  
  if (private->drawable.width <= 32768)
    {
      info->width = private->drawable.width;
      info->x = parent_pos->x + private->x - parent_pos->win32_x;
    }
  else
    {
      info->big = TRUE;
      info->width = 32768;
      if (parent_pos->x + private->x < -16384)
	{
	  if (parent_pos->x + private->x + private->drawable.width < 16384)
	    info->x = parent_pos->x + private->x + private->drawable.width - 32768 - parent_pos->win32_x;
	  else
	    info->x = -16384 - parent_pos->win32_y;
	}
      else
	info->x = parent_pos->x + private->x - parent_pos->win32_x;
    }

  if (private->drawable.height <= 32768)
    {
      info->height = private->drawable.height;
      info->y = parent_pos->y + private->y - parent_pos->win32_y;
    }
  else
    {
      info->big = TRUE;
      info->height = 32768;
      if (parent_pos->y + private->y < -16384)
	{
	  if (parent_pos->y + private->y + private->drawable.height < 16384)
	    info->y = parent_pos->y + private->y + private->drawable.height - 32768 - parent_pos->win32_y;
	  else
	    info->y = -16384 - parent_pos->win32_y;
	}
      else
	info->y = parent_pos->y + private->y - parent_pos->win32_y;
    }

  parent_x_offset = parent_pos->win32_x - parent_pos->x;
  parent_y_offset = parent_pos->win32_y - parent_pos->y;
  
  info->x_offset = parent_x_offset + info->x - private->x;
  info->y_offset = parent_y_offset + info->y - private->y;

  /* We don't considering the clipping of toplevel windows and their immediate children
   * by their parents, and simply always map those windows.
   */
  if (parent_pos->clip_rect.width == G_MAXINT)
    info->mapped = TRUE;
  /* Check if the window would wrap around into the visible space in either direction */
  else if (info->x + parent_x_offset < parent_pos->clip_rect.x + parent_pos->clip_rect.width - 65536 ||
      info->x + info->width + parent_x_offset > parent_pos->clip_rect.x + 65536 ||
      info->y + parent_y_offset < parent_pos->clip_rect.y + parent_pos->clip_rect.height - 65536 ||
      info->y + info->width + parent_y_offset  > parent_pos->clip_rect.y + 65536)
    info->mapped = FALSE;
  else
    info->mapped = TRUE;

  info->no_bg = FALSE;

  if (GDK_DRAWABLE_TYPE (private) == GDK_WINDOW_CHILD)
    {
      info->clip_rect.x = private->x;
      info->clip_rect.y = private->y;
      info->clip_rect.width = private->drawable.width;
      info->clip_rect.height = private->drawable.height;
      
      gdk_rectangle_intersect (&info->clip_rect, &parent_pos->clip_rect, &info->clip_rect);

      info->clip_rect.x -= private->x;
      info->clip_rect.y -= private->y;
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
gdk_window_compute_parent_pos (GdkWindow          *window,
			       GdkWindowParentPos *parent_pos)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GdkWindowWin32Data *data;
  GdkRectangle tmp_clip;
  
  int clip_xoffset = 0;
  int clip_yoffset = 0;

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

  private = (GdkWindowPrivate *)private->parent;
  while (private && private->drawable.window_type == GDK_WINDOW_CHILD)
    {
      data = (GdkWindowWin32Data *)private->drawable.klass_data;

      tmp_clip.x = - clip_xoffset;
      tmp_clip.y = - clip_yoffset;
      tmp_clip.width = private->drawable.width;
      tmp_clip.height = private->drawable.height;

      gdk_rectangle_intersect (&parent_pos->clip_rect, &tmp_clip, &parent_pos->clip_rect);

      parent_pos->x += private->x;
      parent_pos->y += private->y;
      parent_pos->win32_x += data->position_info.x;
      parent_pos->win32_y += data->position_info.y;

      clip_xoffset += private->x;
      clip_yoffset += private->y;

      private = (GdkWindowPrivate *)private->parent;
    }
}

static void
gdk_window_premove (GdkWindow          *window,
		    GdkWindowParentPos *parent_pos)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GdkWindowWin32Data *data = GDK_WINDOW_WIN32DATA (window);
  GdkWin32PositionInfo new_info;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  GdkWindowParentPos this_pos;
  
  gdk_window_compute_position (window, parent_pos, &new_info);

  gdk_window_clip_changed (window, &data->position_info.clip_rect, &new_info.clip_rect);

  this_pos.x = parent_pos->x + private->x;
  this_pos.y = parent_pos->y + private->y;
  this_pos.win32_x = parent_pos->win32_x + new_info.x;
  this_pos.win32_y = parent_pos->win32_y + new_info.y;
  this_pos.clip_rect = new_info.clip_rect;

  if (data->position_info.mapped && !new_info.mapped)
    ShowWindow (GDK_DRAWABLE_XID (window), SW_HIDE);

  d_xoffset = new_info.x_offset - data->position_info.x_offset;
  d_yoffset = new_info.y_offset - data->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      gint new_x0, new_y0, new_x1, new_y1;

      if (d_xoffset < 0 || d_yoffset < 0)
	gdk_window_queue_translation (window, MIN (d_xoffset, 0), MIN (d_yoffset, 0));
	
      if (d_xoffset < 0)
	{
	  new_x0 = data->position_info.x + d_xoffset;
	  new_x1 = data->position_info.x + data->position_info.width;
	}
      else
	{
	  new_x0 = data->position_info.x;
	  new_x1 = data->position_info.x + new_info.width + d_xoffset;
	}

      if (d_yoffset < 0)
	{
	  new_y0 = data->position_info.y + d_yoffset;
	  new_y1 = data->position_info.y + data->position_info.height;
	}
      else
	{
	  new_y0 = data->position_info.y;
	  new_y1 = data->position_info.y + new_info.height + d_yoffset;
	}

      if (!MoveWindow (GDK_DRAWABLE_XID (window),
		       new_x0, new_y0, new_x1 - new_x0, new_y1 - new_y0,
		       FALSE))
	WIN32_API_FAILED ("MoveWindow");
    }

  tmp_list = private->children;
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
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GdkWindowWin32Data *data = (GdkWindowWin32Data *)private->drawable.klass_data;
  GdkWin32PositionInfo new_info;
  GList *tmp_list;
  gint d_xoffset, d_yoffset;
  GdkWindowParentPos this_pos;
  
  gdk_window_compute_position (window, parent_pos, &new_info);

  this_pos.x = parent_pos->x + private->x;
  this_pos.y = parent_pos->y + private->y;
  this_pos.win32_x = parent_pos->win32_x + new_info.x;
  this_pos.win32_y = parent_pos->win32_y + new_info.y;
  this_pos.clip_rect = new_info.clip_rect;

  d_xoffset = new_info.x_offset - data->position_info.x_offset;
  d_yoffset = new_info.y_offset - data->position_info.y_offset;
  
  if (d_xoffset != 0 || d_yoffset != 0)
    {
      if (d_xoffset > 0 || d_yoffset > 0)
	gdk_window_queue_translation (window, MAX (d_xoffset, 0), MAX (d_yoffset, 0));
	
      if (!MoveWindow (GDK_DRAWABLE_XID (window),
		       new_info.x, new_info.y, new_info.width, new_info.height,
		       FALSE))
	WIN32_API_FAILED ("MoveWindow");
    }

  if (!data->position_info.mapped && new_info.mapped && private->mapped)
    ShowWindow (GDK_DRAWABLE_XID (window), SW_SHOWNA);

  if (data->position_info.no_bg)
    gdk_window_tmp_reset_bg (window);

  data->position_info = new_info;

  tmp_list = private->children;
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

  GDK_NOTE (EVENTS, g_print ("gdk_window_queue_translation %#x %d %d,%d\n",
			     GDK_DRAWABLE_XID (window),
			     item->serial,
			     dx, dy));

  gdk_drawable_ref (window);
  translate_queue = g_slist_append (translate_queue, item);
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow *window,
					GdkRegion *area)
{
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);

  item->window = window;
  item->serial = GetMessageTime ();
  item->type = GDK_WINDOW_QUEUE_ANTIEXPOSE;
  item->u.antiexpose.area = area;

  GDK_NOTE (EVENTS, g_print ("_gdk_windowing_window_queue_antiexpose %#x %d %dx%d@+%d+%d\n",
			     GDK_DRAWABLE_XID (window),
			     item->serial,
			     area->extents.x2 - area->extents.x1,
			     area->extents.y2 - area->extents.y1,
			     area->extents.x1, area->extents.y1));

  gdk_drawable_ref (window);
  translate_queue = g_slist_append (translate_queue, item);

  return TRUE;
}

void
_gdk_window_process_expose (GdkWindow    *window,
			    gulong        serial,
			    GdkRectangle *area)
{
  GdkWindowWin32Data *data = GDK_WINDOW_WIN32DATA (window);
  GdkRegion *invalidate_region = gdk_region_rectangle (area);
  GdkRegion *clip_region;

  GSList *tmp_list = translate_queue;

  GDK_NOTE (EVENTS, g_print ("_gdk_window_process_expose %#x %d %dx%d@+%d+%d\n",
			     GDK_DRAWABLE_XID (window), serial,
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

  clip_region = gdk_region_rectangle (&data->position_info.clip_rect);
  gdk_region_intersect (invalidate_region, clip_region);

  if (!gdk_region_empty (invalidate_region))
    gdk_window_invalidate_region (window, invalidate_region, FALSE);

  gdk_region_destroy (invalidate_region);
  gdk_region_destroy (clip_region);
}

static void
gdk_window_tmp_unset_bg (GdkWindow *window)
{
  /* ??? */
}

static void
gdk_window_tmp_reset_bg (GdkWindow *window)
{
  /* ??? */
}

static void
gdk_window_clip_changed (GdkWindow *window, GdkRectangle *old_clip, GdkRectangle *new_clip)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  GdkRegion *old_clip_region;
  GdkRegion *new_clip_region;

  if (private->input_only)
    return;
    
  old_clip_region = gdk_region_rectangle (old_clip);
  new_clip_region = gdk_region_rectangle (new_clip);

  /* Trim invalid region of window to new clip rectangle
   */
  if (private->update_area)
    gdk_region_intersect (private->update_area, new_clip_region);

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

