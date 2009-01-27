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

#include "config.h"
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

void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkWindowImplX11 *impl;
  GdkWindowObject *obj;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window)); 

  impl = GDK_WINDOW_IMPL_X11 (GDK_WINDOW_OBJECT (window)->impl);
  obj = GDK_WINDOW_OBJECT (window);

  if (width > 65535 ||
      height > 65535)
    {
      g_warning ("Native children wider or taller than 65535 pixels are not supported");
      
      if (width > 65535)
	width = 65535;
      if (height > 65535)
	height = 65535;
    }
  
  obj->x = x;
  obj->y = y;
  obj->width = width;
  obj->height = height;

  /* We don't really care about origin overflow, because on overflow
     the window won't be visible anyway and thus it will be shaped
     to nothing */

  
  _gdk_x11_window_tmp_unset_parent_bg (window);
  _gdk_x11_window_tmp_unset_bg (window, TRUE);
  XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     obj->x + obj->parent->abs_x,
		     obj->y + obj->parent->abs_y,
		     width, height);
  _gdk_x11_window_tmp_reset_parent_bg (window);
  _gdk_x11_window_tmp_reset_bg (window, TRUE);
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

void
_gdk_x11_window_queue_translation (GdkWindow *window,
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
_gdk_x11_window_queue_antiexpose (GdkWindow *window,
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
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));
  impl = GDK_WINDOW_IMPL_X11 (GDK_WINDOW_OBJECT (window)->impl);

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
	      queue_delete_link (display_x11->translate_queue, tmp_list);
	      queue_item_free (item);
	    }
	  tmp_list = next;
	}
    }

  if (!gdk_region_empty (invalidate_region))
    _gdk_window_invalidate_for_expose (window, invalidate_region);
  
  gdk_region_destroy (invalidate_region);
}

#define __GDK_GEOMETRY_X11_C__
#include "gdkaliasdef.c"
