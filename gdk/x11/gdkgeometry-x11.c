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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkinternals.h"
#include "gdkrectangle.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkwindow-x11.h"


typedef struct _GdkWindowQueueItem GdkWindowQueueItem;
typedef struct _GdkWindowParentPos GdkWindowParentPos;

struct _GdkWindowQueueItem
{
  GdkWindow *window;
  gulong serial;
  cairo_region_t *antiexpose_area;
};

void
_gdk_x11_window_move_resize_child (GdkWindow *window,
                                   gint       x,
                                   gint       y,
                                   gint       width,
                                   gint       height)
{
  GdkWindowImplX11 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (width * impl->window_scale > 65535 ||
      height * impl->window_scale > 65535)
    {
      g_warning ("Native children wider or taller than 65535 pixels are not supported");

      if (width * impl->window_scale > 65535)
        width = 65535 / impl->window_scale;
      if (height * impl->window_scale > 65535)
        height = 65535 / impl->window_scale;
    }

  window->x = x;
  window->y = y;
  window->width = width;
  window->height = height;

  /* We don't really care about origin overflow, because on overflow
   * the window won't be visible anyway and thus it will be shaped
   * to nothing
   */
  _gdk_x11_window_tmp_unset_parent_bg (window);
  _gdk_x11_window_tmp_unset_bg (window, TRUE);
  XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
                     GDK_WINDOW_XID (window),
                     (window->x + window->parent->abs_x) * impl->window_scale,
                     (window->y + window->parent->abs_y) * impl->window_scale,
                     width * impl->window_scale,
                     height * impl->window_scale);
  _gdk_x11_window_tmp_reset_parent_bg (window);
  _gdk_x11_window_tmp_reset_bg (window, TRUE);
}

static Bool
expose_serial_predicate (Display *xdisplay,
			 XEvent  *xev,
			 XPointer arg)
{
  gulong *serial = (gulong *)arg;

  if (xev->xany.type == Expose || xev->xany.type == GraphicsExpose)
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
  
  cairo_region_destroy (item->antiexpose_area);
  g_free (item);
}

void
_gdk_x11_display_free_translate_queue (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->translate_queue)
    {
      g_queue_foreach (display_x11->translate_queue, (GFunc)queue_item_free, NULL);
      g_queue_free (display_x11->translate_queue);
      display_x11->translate_queue = NULL;
    }
}

static void
gdk_window_queue (GdkWindow          *window,
		  GdkWindowQueueItem *item)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (GDK_WINDOW_DISPLAY (window));
  
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
	  
	  /* an overflow-safe (item->serial < serial) */
	  if (item->serial - serial > (gulong) G_MAXLONG)
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
	  
	  queue_delete_link (display_x11->translate_queue, tmp_list);
	  queue_item_free (item);

	  tmp_list = next;
	}
    }

  item->window = window;
  item->serial = NextRequest (GDK_WINDOW_XDISPLAY (window));
  
  g_object_add_weak_pointer (G_OBJECT (window),
			     (gpointer *)&(item->window));

  g_queue_push_tail (display_x11->translate_queue, item);
}

gboolean
_gdk_x11_window_queue_antiexpose (GdkWindow *window,
				  cairo_region_t *area)
{
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);
  item->antiexpose_area = area;

  gdk_window_queue (window, item);

  return TRUE;
}

void
_gdk_x11_window_process_expose (GdkWindow    *window,
                                gulong        serial,
                                GdkRectangle *area)
{
  cairo_region_t *invalidate_region = cairo_region_create_rectangle (area);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (GDK_WINDOW_DISPLAY (window));

  if (display_x11->translate_queue)
    {
      GList *tmp_list = display_x11->translate_queue->head;

      while (tmp_list)
        {
          GdkWindowQueueItem *item = tmp_list->data;
          GList *next = tmp_list->next;

          /* an overflow-safe (serial < item->serial) */
          if (serial - item->serial > (gulong) G_MAXLONG)
            {
              if (item->window == window)
		cairo_region_subtract (invalidate_region, item->antiexpose_area);
            }
          else
            {
              queue_delete_link (display_x11->translate_queue, tmp_list);
              queue_item_free (item);
            }
          tmp_list = next;
        }
    }

  if (!cairo_region_is_empty (invalidate_region))
    _gdk_window_invalidate_for_expose (window, invalidate_region);

  cairo_region_destroy (invalidate_region);
}
