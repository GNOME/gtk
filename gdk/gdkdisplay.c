/* GDK - The GIMP Drawing Kit
 * gdkdisplay.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkdisplay.h"
#include "gdkdisplayprivate.h"

#include "gdk-private.h"

#include "gdkdeviceprivate.h"
#include "gdkdisplaymanagerprivate.h"
#include "gdkevents.h"
#include "gdkwindowimpl.h"
#include "gdkinternals.h"
#include "gdkmarshalers.h"
#include "gdkscreen.h"

#include <math.h>
#include <glib.h>

/* for the use of round() */
#include "fallback-c89.c"

/**
 * SECTION:gdkdisplay
 * @Short_description: Controls a set of GdkScreens and their associated input devices
 * @Title: GdkDisplay
 *
 * #GdkDisplay objects purpose are two fold:
 *
 * - To manage and provide information about input devices (pointers and keyboards)
 *
 * - To manage and provide information about the available #GdkScreens
 *
 * GdkDisplay objects are the GDK representation of an X Display,
 * which can be described as a workstation consisting of
 * a keyboard, a pointing device (such as a mouse) and one or more
 * screens.
 * It is used to open and keep track of various GdkScreen objects
 * currently instantiated by the application. It is also used to
 * access the keyboard(s) and mouse pointer(s) of the display.
 *
 * Most of the input device handling has been factored out into
 * the separate #GdkDeviceManager object. Every display has a
 * device manager, which you can obtain using
 * gdk_display_get_device_manager().
 */


enum {
  OPENED,
  CLOSED,
  LAST_SIGNAL
};

static void gdk_display_dispose     (GObject         *object);
static void gdk_display_finalize    (GObject         *object);


static GdkAppLaunchContext *gdk_display_real_get_app_launch_context (GdkDisplay *display);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkDisplay, gdk_display, G_TYPE_OBJECT)

static void
gdk_display_real_make_default (GdkDisplay *display)
{
}

static void
device_removed_cb (GdkDeviceManager *device_manager,
                   GdkDevice        *device,
                   GdkDisplay       *display)
{
  g_hash_table_remove (display->multiple_click_info, device);
  g_hash_table_remove (display->device_grabs, device);
  g_hash_table_remove (display->pointers_info, device);

  /* FIXME: change core pointer and remove from device list */
}

static void
gdk_display_real_opened (GdkDisplay *display)
{
  GdkDeviceManager *device_manager;

  device_manager = gdk_display_get_device_manager (display);

  g_signal_connect (device_manager, "device-removed",
                    G_CALLBACK (device_removed_cb), display);

  _gdk_display_manager_add_display (gdk_display_manager_get (), display);
}

static void
gdk_display_real_event_data_copy (GdkDisplay     *display,
                                  const GdkEvent *src,
                                  GdkEvent       *dst)
{
}

static void
gdk_display_real_event_data_free (GdkDisplay     *display,
                                  GdkEvent       *dst)
{
}

static void
gdk_display_class_init (GdkDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_display_finalize;
  object_class->dispose = gdk_display_dispose;

  class->get_app_launch_context = gdk_display_real_get_app_launch_context;
  class->window_type = GDK_TYPE_WINDOW;

  class->opened = gdk_display_real_opened;
  class->make_default = gdk_display_real_make_default;
  class->event_data_copy = gdk_display_real_event_data_copy;
  class->event_data_free = gdk_display_real_event_data_free;

  /**
   * GdkDisplay::opened:
   * @display: the object on which the signal is emitted
   *
   * The ::opened signal is emitted when the connection to the windowing
   * system for @display is opened.
   */
  signals[OPENED] =
    g_signal_new (g_intern_static_string ("opened"),
		  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkDisplayClass, opened),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkDisplay::closed:
   * @display: the object on which the signal is emitted
   * @is_error: %TRUE if the display was closed due to an error
   *
   * The ::closed signal is emitted when the connection to the windowing
   * system for @display is closed.
   *
   * Since: 2.2
   */   
  signals[CLOSED] =
    g_signal_new (g_intern_static_string ("closed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkDisplayClass, closed),
		  NULL, NULL,
		  _gdk_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1,
		  G_TYPE_BOOLEAN);
}

static void
free_pointer_info (GdkPointerWindowInfo *info)
{
  if (info->toplevel_under_pointer)
    g_object_unref (info->toplevel_under_pointer);
  g_slice_free (GdkPointerWindowInfo, info);
}

static void
free_device_grab (GdkDeviceGrabInfo *info)
{
  g_object_unref (info->window);
  g_object_unref (info->native_window);
  g_free (info);
}

static gboolean
free_device_grabs_foreach (gpointer key,
                           gpointer value,
                           gpointer user_data)
{
  GList *list = value;

  g_list_free_full (list, (GDestroyNotify) free_device_grab);

  return TRUE;
}

static void
gdk_display_init (GdkDisplay *display)
{
  display->double_click_time = 250;
  display->double_click_distance = 5;

  display->touch_implicit_grabs = g_array_new (FALSE, FALSE, sizeof (GdkTouchGrabInfo));
  display->device_grabs = g_hash_table_new (NULL, NULL);
  display->motion_hint_info = g_hash_table_new_full (NULL, NULL, NULL,
                                                     (GDestroyNotify) g_free);

  display->pointers_info = g_hash_table_new_full (NULL, NULL, NULL,
                                                  (GDestroyNotify) free_pointer_info);

  display->multiple_click_info = g_hash_table_new_full (NULL, NULL, NULL,
                                                        (GDestroyNotify) g_free);

  display->rendering_mode = _gdk_rendering_mode;
}

static void
gdk_display_dispose (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY (object);
  GdkDeviceManager *device_manager;

  device_manager = gdk_display_get_device_manager (GDK_DISPLAY (object));

  _gdk_display_manager_remove_display (gdk_display_manager_get (), display);

  g_list_free_full (display->queued_events, (GDestroyNotify) gdk_event_free);
  display->queued_events = NULL;
  display->queued_tail = NULL;

  if (device_manager)
    {
      /* this is to make it drop devices which may require using the X
       * display and therefore can't be cleaned up in finalize.
       * It will also disconnect device_removed_cb
       */
      g_object_run_dispose (G_OBJECT (display->device_manager));
    }

  G_OBJECT_CLASS (gdk_display_parent_class)->dispose (object);
}

static void
gdk_display_finalize (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY (object);

  g_hash_table_foreach_remove (display->device_grabs,
                               free_device_grabs_foreach,
                               NULL);
  g_hash_table_destroy (display->device_grabs);

  g_array_free (display->touch_implicit_grabs, TRUE);

  g_hash_table_destroy (display->motion_hint_info);
  g_hash_table_destroy (display->pointers_info);
  g_hash_table_destroy (display->multiple_click_info);

  if (display->device_manager)
    g_object_unref (display->device_manager);

  G_OBJECT_CLASS (gdk_display_parent_class)->finalize (object);
}

/**
 * gdk_display_close:
 * @display: a #GdkDisplay
 *
 * Closes the connection to the windowing system for the given display,
 * and cleans up associated resources.
 *
 * Since: 2.2
 */
void
gdk_display_close (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (!display->closed)
    {
      display->closed = TRUE;
      
      g_signal_emit (display, signals[CLOSED], 0, FALSE);
      g_object_run_dispose (G_OBJECT (display));
      
      g_object_unref (display);
    }
}

/**
 * gdk_display_is_closed:
 * @display: a #GdkDisplay
 *
 * Finds out if the display has been closed.
 *
 * Returns: %TRUE if the display is closed.
 *
 * Since: 2.22
 */
gboolean
gdk_display_is_closed  (GdkDisplay  *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return display->closed;
}

/**
 * gdk_display_get_event:
 * @display: a #GdkDisplay
 * 
 * Gets the next #GdkEvent to be processed for @display, fetching events from the
 * windowing system if necessary.
 * 
 * Returns: (nullable): the next #GdkEvent to be processed, or %NULL
 * if no events are pending. The returned #GdkEvent should be freed
 * with gdk_event_free().
 *
 * Since: 2.2
 **/
GdkEvent*
gdk_display_get_event (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->event_pause_count == 0)
    GDK_DISPLAY_GET_CLASS (display)->queue_events (display);

  return _gdk_event_unqueue (display);
}

/**
 * gdk_display_peek_event:
 * @display: a #GdkDisplay 
 * 
 * Gets a copy of the first #GdkEvent in the @display’s event queue, without
 * removing the event from the queue.  (Note that this function will
 * not get more events from the windowing system.  It only checks the events
 * that have already been moved to the GDK event queue.)
 * 
 * Returns: (nullable): a copy of the first #GdkEvent on the event
 * queue, or %NULL if no events are in the queue. The returned
 * #GdkEvent should be freed with gdk_event_free().
 *
 * Since: 2.2
 **/
GdkEvent*
gdk_display_peek_event (GdkDisplay *display)
{
  GList *tmp_list;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  tmp_list = _gdk_event_queue_find_first (display);
  
  if (tmp_list)
    return gdk_event_copy (tmp_list->data);
  else
    return NULL;
}

/**
 * gdk_display_put_event:
 * @display: a #GdkDisplay
 * @event: a #GdkEvent.
 *
 * Appends a copy of the given event onto the front of the event
 * queue for @display.
 *
 * Since: 2.2
 **/
void
gdk_display_put_event (GdkDisplay     *display,
		       const GdkEvent *event)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (event != NULL);

  _gdk_event_queue_append (display, gdk_event_copy (event));
  /* If the main loop is blocking in a different thread, wake it up */
  g_main_context_wakeup (NULL); 
}

/**
 * gdk_display_pointer_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g. %GDK_CURRENT_TIME).
 *
 * Release any pointer grab.
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_device_ungrab(), together with gdk_device_grab()
 *             instead.
 */
void
gdk_display_pointer_ungrab (GdkDisplay *display,
			    guint32     time_)
{
  GdkDeviceManager *device_manager;
  GList *devices, *dev;
  GdkDevice *device;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  /* FIXME: Should this be generic to all backends? */
  /* FIXME: What happens with extended devices? */
  for (dev = devices; dev; dev = dev->next)
    {
      device = dev->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
        continue;

      gdk_device_ungrab (device, time_);
    }

  g_list_free (devices);
}

/**
 * gdk_display_keyboard_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g #GDK_CURRENT_TIME).
 *
 * Release any keyboard grab
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_device_ungrab(), together with gdk_device_grab()
 *             instead.
 */
void
gdk_display_keyboard_ungrab (GdkDisplay *display,
			     guint32     time)
{
  GdkDeviceManager *device_manager;
  GList *devices, *dev;
  GdkDevice *device;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  /* FIXME: Should this be generic to all backends? */
  /* FIXME: What happens with extended devices? */
  for (dev = devices; dev; dev = dev->next)
    {
      device = dev->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
        continue;

      gdk_device_ungrab (device, time);
    }

  g_list_free (devices);
}

/**
 * gdk_beep:
 * 
 * Emits a short beep on the default display.
 **/
void
gdk_beep (void)
{
  gdk_display_beep (gdk_display_get_default ());
}

/**
 * gdk_flush:
 *
 * Flushes the output buffers of all display connections and waits
 * until all requests have been processed.
 * This is rarely needed by applications.
 */
void
gdk_flush (void)
{
  GSList *list, *l;

  list = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (l = list; l; l = l->next)
    {
      GdkDisplay *display = l->data;

      GDK_DISPLAY_GET_CLASS (display)->sync (display);
    }

  g_slist_free (list);
}

void
_gdk_display_enable_motion_hints (GdkDisplay *display,
                                  GdkDevice  *device)
{
  gulong *device_serial, serial;

  device_serial = g_hash_table_lookup (display->motion_hint_info, device);

  if (!device_serial)
    {
      device_serial = g_new0 (gulong, 1);
      *device_serial = G_MAXULONG;
      g_hash_table_insert (display->motion_hint_info, device, device_serial);
    }

  if (*device_serial != 0)
    {
      serial = _gdk_display_get_next_serial (display);
      /* We might not actually generate the next request, so
	 make sure this triggers always, this may cause it to
	 trigger slightly too early, but this is just a hint
	 anyway. */
      if (serial > 0)
	serial--;
      if (serial < *device_serial)
	*device_serial = serial;
    }
}

/**
 * gdk_display_get_pointer:
 * @display: a #GdkDisplay
 * @screen: (out) (allow-none) (transfer none): location to store the screen that the
 *          cursor is on, or %NULL.
 * @x: (out) (allow-none): location to store root window X coordinate of pointer, or %NULL.
 * @y: (out) (allow-none): location to store root window Y coordinate of pointer, or %NULL.
 * @mask: (out) (allow-none): location to store current modifier mask, or %NULL
 *
 * Gets the current location of the pointer and the current modifier
 * mask for a given display.
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_device_get_position() instead.
 **/
void
gdk_display_get_pointer (GdkDisplay      *display,
			 GdkScreen      **screen,
			 gint            *x,
			 gint            *y,
			 GdkModifierType *mask)
{
  GdkScreen *default_screen;
  GdkWindow *root;
  gdouble tmp_x, tmp_y;
  GdkModifierType tmp_mask;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (gdk_display_is_closed (display))
    return;

  default_screen = gdk_display_get_default_screen (display);

  /* We call _gdk_device_query_state() here manually instead of
   * gdk_device_get_position() because we care about the modifier mask */

  _gdk_device_query_state (display->core_pointer,
                           gdk_screen_get_root_window (default_screen),
                           &root, NULL,
                           &tmp_x, &tmp_y,
                           NULL, NULL,
                           &tmp_mask);

  if (screen)
    *screen = gdk_window_get_screen (root);
  if (x)
    *x = round (tmp_x);
  if (y)
    *y = round (tmp_y);
  if (mask)
    *mask = tmp_mask;
}

/**
 * gdk_display_get_window_at_pointer:
 * @display: a #GdkDisplay
 * @win_x: (out) (allow-none): return location for x coordinate of the pointer location relative
 *    to the window origin, or %NULL
 * @win_y: (out) (allow-none): return location for y coordinate of the pointer location relative
 &    to the window origin, or %NULL
 *
 * Obtains the window underneath the mouse pointer, returning the location
 * of the pointer in that window in @win_x, @win_y for @screen. Returns %NULL
 * if the window under the mouse pointer is not known to GDK (for example, 
 * belongs to another application).
 *
 * Returns: (nullable) (transfer none): the window under the mouse
 *   pointer, or %NULL
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_device_get_window_at_position() instead.
 **/
GdkWindow *
gdk_display_get_window_at_pointer (GdkDisplay *display,
				   gint       *win_x,
				   gint       *win_y)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return gdk_device_get_window_at_position (display->core_pointer, win_x, win_y);
}

static void
generate_grab_broken_event (GdkWindow *window,
                            GdkDevice *device,
			    gboolean   implicit,
			    GdkWindow *grab_window)
{
  g_return_if_fail (window != NULL);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkEvent *event;

      event = gdk_event_new (GDK_GRAB_BROKEN);
      event->grab_broken.window = g_object_ref (window);
      event->grab_broken.send_event = FALSE;
      event->grab_broken.implicit = implicit;
      event->grab_broken.grab_window = grab_window;
      gdk_event_set_device (event, device);
      event->grab_broken.keyboard = (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD) ? TRUE : FALSE;

      gdk_event_put (event);
      gdk_event_free (event);
    }
}

GdkDeviceGrabInfo *
_gdk_display_get_last_device_grab (GdkDisplay *display,
                                   GdkDevice  *device)
{
  GList *l;

  l = g_hash_table_lookup (display->device_grabs, device);

  if (l)
    {
      l = g_list_last (l);
      return l->data;
    }

  return NULL;
}

GdkDeviceGrabInfo *
_gdk_display_add_device_grab (GdkDisplay       *display,
                              GdkDevice        *device,
                              GdkWindow        *window,
                              GdkWindow        *native_window,
                              GdkGrabOwnership  grab_ownership,
                              gboolean          owner_events,
                              GdkEventMask      event_mask,
                              unsigned long     serial_start,
                              guint32           time,
                              gboolean          implicit)
{
  GdkDeviceGrabInfo *info, *other_info;
  GList *grabs, *l;

  info = g_new0 (GdkDeviceGrabInfo, 1);

  info->window = g_object_ref (window);
  info->native_window = g_object_ref (native_window);
  info->serial_start = serial_start;
  info->serial_end = G_MAXULONG;
  info->owner_events = owner_events;
  info->event_mask = event_mask;
  info->time = time;
  info->implicit = implicit;
  info->ownership = grab_ownership;

  grabs = g_hash_table_lookup (display->device_grabs, device);

  /* Find the first grab that has a larger start time (if any) and insert
   * before that. I.E we insert after already existing grabs with same
   * start time */
  for (l = grabs; l != NULL; l = l->next)
    {
      other_info = l->data;

      if (info->serial_start < other_info->serial_start)
	break;
    }

  grabs = g_list_insert_before (grabs, l, info);

  /* Make sure the new grab end before next grab */
  if (l)
    {
      other_info = l->data;
      info->serial_end = other_info->serial_start;
    }

  /* Find any previous grab and update its end time */
  l = g_list_find (grabs, info);
  l = l->prev;
  if (l)
    {
      other_info = l->data;
      other_info->serial_end = serial_start;
    }

  g_hash_table_insert (display->device_grabs, device, grabs);

  return info;
}

static void
_gdk_display_break_touch_grabs (GdkDisplay *display,
                                GdkDevice  *device,
                                GdkWindow  *new_grab_window)
{
  guint i;

  for (i = 0; i < display->touch_implicit_grabs->len; i++)
    {
      GdkTouchGrabInfo *info;

      info = &g_array_index (display->touch_implicit_grabs,
                             GdkTouchGrabInfo, i);

      if (info->device == device && info->window != new_grab_window)
        generate_grab_broken_event (GDK_WINDOW (info->window),
                                    device, TRUE, new_grab_window);
    }
}

void
_gdk_display_add_touch_grab (GdkDisplay       *display,
                             GdkDevice        *device,
                             GdkEventSequence *sequence,
                             GdkWindow        *window,
                             GdkWindow        *native_window,
                             GdkEventMask      event_mask,
                             unsigned long     serial,
                             guint32           time)
{
  GdkTouchGrabInfo info;

  info.device = device;
  info.sequence = sequence;
  info.window = g_object_ref (window);
  info.native_window = g_object_ref (native_window);
  info.serial = serial;
  info.event_mask = event_mask;
  info.time = time;

  g_array_append_val (display->touch_implicit_grabs, info);
}

gboolean
_gdk_display_end_touch_grab (GdkDisplay       *display,
                             GdkDevice        *device,
                             GdkEventSequence *sequence)
{
  guint i;

  for (i = 0; i < display->touch_implicit_grabs->len; i++)
    {
      GdkTouchGrabInfo *info;

      info = &g_array_index (display->touch_implicit_grabs,
                             GdkTouchGrabInfo, i);

      if (info->device == device && info->sequence == sequence)
        {
          g_array_remove_index_fast (display->touch_implicit_grabs, i);
          return TRUE;
        }
    }

  return FALSE;
}

/* _gdk_synthesize_crossing_events only works inside one toplevel.
   This function splits things into two calls if needed, converting the
   coordinates to the right toplevel */
static void
synthesize_crossing_events (GdkDisplay      *display,
                            GdkDevice       *device,
                            GdkDevice       *source_device,
			    GdkWindow       *src_window,
			    GdkWindow       *dest_window,
			    GdkCrossingMode  crossing_mode,
			    guint32          time,
			    gulong           serial)
{
  GdkWindow *src_toplevel, *dest_toplevel;
  GdkModifierType state;
  double x, y;

  if (src_window)
    src_toplevel = gdk_window_get_toplevel (src_window);
  else
    src_toplevel = NULL;
  if (dest_window)
    dest_toplevel = gdk_window_get_toplevel (dest_window);
  else
    dest_toplevel = NULL;

  if (src_toplevel == NULL && dest_toplevel == NULL)
    return;
  
  if (src_toplevel == NULL ||
      src_toplevel == dest_toplevel)
    {
      /* Same toplevels */
      gdk_window_get_device_position_double (dest_toplevel,
                                             device,
                                             &x, &y, &state);
      _gdk_synthesize_crossing_events (display,
				       src_window,
				       dest_window,
                                       device, source_device,
				       crossing_mode,
				       x, y, state,
				       time,
				       NULL,
				       serial, FALSE);
    }
  else if (dest_toplevel == NULL)
    {
      gdk_window_get_device_position_double (src_toplevel,
                                             device,
                                             &x, &y, &state);
      _gdk_synthesize_crossing_events (display,
                                       src_window,
                                       NULL,
                                       device, source_device,
                                       crossing_mode,
                                       x, y, state,
                                       time,
                                       NULL,
                                       serial, FALSE);
    }
  else
    {
      /* Different toplevels */
      gdk_window_get_device_position_double (src_toplevel,
                                             device,
                                             &x, &y, &state);
      _gdk_synthesize_crossing_events (display,
				       src_window,
				       NULL,
                                       device, source_device,
				       crossing_mode,
				       x, y, state,
				       time,
				       NULL,
				       serial, FALSE);
      gdk_window_get_device_position_double (dest_toplevel,
                                             device,
                                             &x, &y, &state);
      _gdk_synthesize_crossing_events (display,
				       NULL,
				       dest_window,
                                       device, source_device,
				       crossing_mode,
				       x, y, state,
				       time,
				       NULL,
				       serial, FALSE);
    }
}

static GdkWindow *
get_current_toplevel (GdkDisplay      *display,
                      GdkDevice       *device,
                      int             *x_out,
                      int             *y_out,
		      GdkModifierType *state_out)
{
  GdkWindow *pointer_window;
  gdouble x, y;
  GdkModifierType state;

  pointer_window = _gdk_device_window_at_position (device, &x, &y, &state, TRUE);

  if (pointer_window != NULL &&
      (GDK_WINDOW_DESTROYED (pointer_window) ||
       GDK_WINDOW_TYPE (pointer_window) == GDK_WINDOW_ROOT ||
       GDK_WINDOW_TYPE (pointer_window) == GDK_WINDOW_FOREIGN))
    pointer_window = NULL;

  *x_out = round (x);
  *y_out = round (y);
  *state_out = state;

  return pointer_window;
}

static void
switch_to_pointer_grab (GdkDisplay        *display,
                        GdkDevice         *device,
                        GdkDevice         *source_device,
			GdkDeviceGrabInfo *grab,
			GdkDeviceGrabInfo *last_grab,
			guint32            time,
			gulong             serial)
{
  GdkWindow *src_window, *pointer_window, *new_toplevel;
  GdkPointerWindowInfo *info;
  GList *old_grabs;
  GdkModifierType state;
  int x = 0, y = 0;

  /* Temporarily unset pointer to make sure we send the crossing events below */
  old_grabs = g_hash_table_lookup (display->device_grabs, device);
  g_hash_table_steal (display->device_grabs, device);
  info = _gdk_display_get_pointer_info (display, device);

  if (grab)
    {
      /* New grab is in effect */

      /* We need to generate crossing events for the grab.
       * However, there are never any crossing events for implicit grabs
       * TODO: ... Actually, this could happen if the pointer window
       *           doesn't have button mask so a parent gets the event...
       */
      if (!grab->implicit)
	{
	  /* We send GRAB crossing events from the window under the pointer to the
	     grab window. Except if there is an old grab then we start from that */
	  if (last_grab)
	    src_window = last_grab->window;
	  else
	    src_window = info->window_under_pointer;

	  if (src_window != grab->window)
            synthesize_crossing_events (display, device, source_device,
                                        src_window, grab->window,
                                        GDK_CROSSING_GRAB, time, serial);

	  /* !owner_event Grabbing a window that we're not inside, current status is
	     now NULL (i.e. outside grabbed window) */
	  if (!grab->owner_events && info->window_under_pointer != grab->window)
	    _gdk_display_set_window_under_pointer (display, device, NULL);
	}

      grab->activated = TRUE;
    }

  if (last_grab)
    {
      new_toplevel = NULL;

      if (grab == NULL /* ungrab */ ||
	  (!last_grab->owner_events && grab->owner_events) /* switched to owner_events */ )
	{
	  /* We force check what window we're in, and update the toplevel_under_pointer info,
	   * as that won't get told of this change with toplevel enter events.
	   */
	  if (info->toplevel_under_pointer)
	    g_object_unref (info->toplevel_under_pointer);
	  info->toplevel_under_pointer = NULL;

          /* Ungrabbed slave devices don't have a position by
           * itself, rather depend on its master pointer, so
           * it doesn't make sense to track any position for
           * these after the grab
           */
          if (grab || gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_SLAVE)
            new_toplevel = get_current_toplevel (display, device, &x, &y, &state);

	  if (new_toplevel)
	    {
	      /* w is now toplevel and x,y in toplevel coords */
	      info->toplevel_under_pointer = g_object_ref (new_toplevel);
	      info->toplevel_x = x;
	      info->toplevel_y = y;
	      info->state = state;
	    }
	}

      if (grab == NULL) /* Ungrabbed, send events */
	{
          /* If the source device is a touch device, do not
           * propagate any enter event yet, until one is
           * synthesized when needed.
           */
          if (source_device &&
              (gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN))
            info->need_touch_press_enter = TRUE;

          pointer_window = NULL;

          if (new_toplevel &&
              !info->need_touch_press_enter)
            {
              /* Find (possibly virtual) child window */
              pointer_window =
                _gdk_window_find_descendant_at (new_toplevel,
                                                x, y,
                                                NULL, NULL);
            }

	  if (!info->need_touch_press_enter &&
	      pointer_window != last_grab->window)
            synthesize_crossing_events (display, device, source_device,
                                        last_grab->window, pointer_window,
                                        GDK_CROSSING_UNGRAB, time, serial);

	  /* We're now ungrabbed, update the window_under_pointer */
	  _gdk_display_set_window_under_pointer (display, device, pointer_window);
	}
    }

  g_hash_table_insert (display->device_grabs, device, old_grabs);
}

void
_gdk_display_device_grab_update (GdkDisplay *display,
                                 GdkDevice  *device,
                                 GdkDevice  *source_device,
                                 gulong      current_serial)
{
  GdkDeviceGrabInfo *current_grab, *next_grab;
  GList *grabs;
  guint32 time;

  time = display->last_event_time;
  grabs = g_hash_table_lookup (display->device_grabs, device);

  while (grabs != NULL)
    {
      current_grab = grabs->data;

      if (current_grab->serial_start > current_serial)
	return; /* Hasn't started yet */

      if (current_grab->serial_end > current_serial)
	{
	  /* This one hasn't ended yet.
	     its the currently active one or scheduled to be active */

	  if (!current_grab->activated)
            {
              if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
                switch_to_pointer_grab (display, device, source_device, current_grab, NULL, time, current_serial);
            }

	  break;
	}

      next_grab = NULL;
      if (grabs->next)
	{
	  /* This is the next active grab */
	  next_grab = grabs->next->data;

	  if (next_grab->serial_start > current_serial)
	    next_grab = NULL; /* Actually its not yet active */
	}

      if (next_grab)
        _gdk_display_break_touch_grabs (display, device, next_grab->window);

      if ((next_grab == NULL && current_grab->implicit_ungrab) ||
          (next_grab != NULL && current_grab->window != next_grab->window))
        generate_grab_broken_event (GDK_WINDOW (current_grab->window),
                                    device,
                                    current_grab->implicit,
                                    next_grab? next_grab->window : NULL);

      /* Remove old grab */
      grabs = g_list_delete_link (grabs, grabs);
      g_hash_table_insert (display->device_grabs, device, grabs);

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
        switch_to_pointer_grab (display, device, source_device,
                                next_grab, current_grab,
                                time, current_serial);

      free_device_grab (current_grab);
    }
}

static GList *
grab_list_find (GList  *grabs,
                gulong  serial)
{
  GdkDeviceGrabInfo *grab;

  while (grabs)
    {
      grab = grabs->data;

      if (serial >= grab->serial_start && serial < grab->serial_end)
	return grabs;

      grabs = grabs->next;
    }

  return NULL;
}

static GList *
find_device_grab (GdkDisplay *display,
                   GdkDevice  *device,
                   gulong      serial)
{
  GList *l;

  l = g_hash_table_lookup (display->device_grabs, device);
  return grab_list_find (l, serial);
}

GdkDeviceGrabInfo *
_gdk_display_has_device_grab (GdkDisplay *display,
                              GdkDevice  *device,
                              gulong      serial)
{
  GList *l;

  l = find_device_grab (display, device, serial);
  if (l)
    return l->data;

  return NULL;
}

GdkTouchGrabInfo *
_gdk_display_has_touch_grab (GdkDisplay       *display,
                             GdkDevice        *device,
                             GdkEventSequence *sequence,
                             gulong            serial)
{
  guint i;

  for (i = 0; i < display->touch_implicit_grabs->len; i++)
    {
      GdkTouchGrabInfo *info;

      info = &g_array_index (display->touch_implicit_grabs,
                             GdkTouchGrabInfo, i);

      if (info->device == device && info->sequence == sequence)
        {
          if (serial >= info->serial)
            return info;
          else
            return NULL;
        }
    }

  return NULL;
}

/* Returns true if last grab was ended
 * If if_child is non-NULL, end the grab only if the grabbed
 * window is the same as if_child or a descendant of it */
gboolean
_gdk_display_end_device_grab (GdkDisplay *display,
                              GdkDevice  *device,
                              gulong      serial,
                              GdkWindow  *if_child,
                              gboolean    implicit)
{
  GdkDeviceGrabInfo *grab;
  GList *l;

  l = find_device_grab (display, device, serial);

  if (l == NULL)
    return FALSE;

  grab = l->data;
  if (grab &&
      (if_child == NULL ||
       _gdk_window_event_parent_of (if_child, grab->window)))
    {
      grab->serial_end = serial;
      grab->implicit_ungrab = implicit;
      return l->next == NULL;
    }
  
  return FALSE;
}

/* Returns TRUE if device events are not blocked by any grab */
gboolean
_gdk_display_check_grab_ownership (GdkDisplay *display,
                                   GdkDevice  *device,
                                   gulong      serial)
{
  GHashTableIter iter;
  gpointer key, value;
  GdkGrabOwnership higher_ownership, device_ownership;
  gboolean device_is_keyboard;

  g_hash_table_iter_init (&iter, display->device_grabs);
  higher_ownership = device_ownership = GDK_OWNERSHIP_NONE;
  device_is_keyboard = (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkDeviceGrabInfo *grab;
      GdkDevice *dev;
      GList *grabs;

      dev = key;
      grabs = value;
      grabs = grab_list_find (grabs, serial);

      if (!grabs)
        continue;

      /* Discard device if it's not of the same type */
      if ((device_is_keyboard && gdk_device_get_source (dev) != GDK_SOURCE_KEYBOARD) ||
          (!device_is_keyboard && gdk_device_get_source (dev) == GDK_SOURCE_KEYBOARD))
        continue;

      grab = grabs->data;

      if (dev == device)
        device_ownership = grab->ownership;
      else
        {
          if (grab->ownership > higher_ownership)
            higher_ownership = grab->ownership;
        }
    }

  if (higher_ownership > device_ownership)
    {
      /* There's a higher priority ownership
       * going on for other device(s)
       */
      return FALSE;
    }

  return TRUE;
}

GdkPointerWindowInfo *
_gdk_display_get_pointer_info (GdkDisplay *display,
                               GdkDevice  *device)
{
  GdkPointerWindowInfo *info;

  if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  if (G_UNLIKELY (!device))
    return NULL;

  info = g_hash_table_lookup (display->pointers_info, device);

  if (G_UNLIKELY (!info))
    {
      info = g_slice_new0 (GdkPointerWindowInfo);
      g_hash_table_insert (display->pointers_info, device, info);
    }

  return info;
}

void
_gdk_display_pointer_info_foreach (GdkDisplay                   *display,
                                   GdkDisplayPointerInfoForeach  func,
                                   gpointer                      user_data)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, display->pointers_info);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkPointerWindowInfo *info = value;
      GdkDevice *device = key;

      (func) (display, device, info, user_data);
    }
}

/*< private >
 * gdk_device_grab_info:
 * @display: the display for which to get the grab information
 * @device: device to get the grab information from
 * @grab_window: (out) (transfer none): location to store current grab window
 * @owner_events: (out): location to store boolean indicating whether
 *   the @owner_events flag to gdk_keyboard_grab() or
 *   gdk_pointer_grab() was %TRUE.
 *
 * Determines information about the current keyboard grab.
 * This is not public API and must not be used by applications.
 *
 * Returns: %TRUE if this application currently has the
 *  keyboard grabbed.
 */
gboolean
gdk_device_grab_info (GdkDisplay  *display,
                      GdkDevice   *device,
                      GdkWindow  **grab_window,
                      gboolean    *owner_events)
{
  GdkDeviceGrabInfo *info;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  info = _gdk_display_get_last_device_grab (display, device);

  if (info)
    {
      if (grab_window)
        *grab_window = info->window;
      if (owner_events)
        *owner_events = info->owner_events;

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gdk_device_grab_info_libgtk_only:
 * @display: the display for which to get the grab information
 * @device: device to get the grab information from
 * @grab_window: (out) (transfer none): location to store current grab window
 * @owner_events: (out): location to store boolean indicating whether
 *   the @owner_events flag to gdk_keyboard_grab() or
 *   gdk_pointer_grab() was %TRUE.
 *
 * Determines information about the current keyboard grab.
 * This is not public API and must not be used by applications.
 *
 * Returns: %TRUE if this application currently has the
 *  keyboard grabbed.
 *
 * Deprecated: 3.16: The symbol was never meant to be used outside
 *   of GTK+
 */
gboolean
gdk_device_grab_info_libgtk_only (GdkDisplay  *display,
                                  GdkDevice   *device,
                                  GdkWindow  **grab_window,
                                  gboolean    *owner_events)
{
  return gdk_device_grab_info (display, device, grab_window, owner_events);
}

/**
 * gdk_display_pointer_is_grabbed:
 * @display: a #GdkDisplay
 *
 * Test if the pointer is grabbed.
 *
 * Returns: %TRUE if an active X pointer grab is in effect
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_display_device_is_grabbed() instead.
 */
gboolean
gdk_display_pointer_is_grabbed (GdkDisplay *display)
{
  GdkDeviceManager *device_manager;
  GList *devices, *dev;
  GdkDevice *device;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);

  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (dev = devices; dev; dev = dev->next)
    {
      device = dev->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_MOUSE &&
          gdk_display_device_is_grabbed (display, device))
        {
          g_list_free (devices);
          return TRUE;
        }
    }

  g_list_free (devices);

  return FALSE;
}

/**
 * gdk_display_device_is_grabbed:
 * @display: a #GdkDisplay
 * @device: a #GdkDevice
 *
 * Returns %TRUE if there is an ongoing grab on @device for @display.
 *
 * Returns: %TRUE if there is a grab in effect for @device.
 **/
gboolean
gdk_display_device_is_grabbed (GdkDisplay *display,
                               GdkDevice  *device)
{
  GdkDeviceGrabInfo *info;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), TRUE);

  /* What we're interested in is the steady state (ie last grab),
     because we're interested e.g. if we grabbed so that we
     can ungrab, even if our grab is not active just yet. */
  info = _gdk_display_get_last_device_grab (display, device);

  return (info && !info->implicit);
}

/**
 * gdk_display_get_device_manager:
 * @display: a #GdkDisplay.
 *
 * Returns the #GdkDeviceManager associated to @display.
 *
 * Returns: (nullable) (transfer none): A #GdkDeviceManager, or
 *          %NULL. This memory is owned by GDK and must not be freed
 *          or unreferenced.
 *
 * Since: 3.0
 **/
GdkDeviceManager *
gdk_display_get_device_manager (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return display->device_manager;
}

/**
 * gdk_display_get_name:
 * @display: a #GdkDisplay
 *
 * Gets the name of the display.
 *
 * Returns: a string representing the display name. This string is owned
 * by GDK and should not be modified or freed.
 *
 * Since: 2.2
 */
const gchar *
gdk_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_name (display);
}

/**
 * gdk_display_get_n_screens:
 * @display: a #GdkDisplay
 *
 * Gets the number of screen managed by the @display.
 *
 * Returns: number of screens.
 *
 * Since: 2.2
 *
 * Deprecated: 3.10: The number of screens is always 1.
 */
gint
gdk_display_get_n_screens (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return 1;
}

/**
 * gdk_display_get_screen:
 * @display: a #GdkDisplay
 * @screen_num: the screen number
 *
 * Returns a screen object for one of the screens of the display.
 *
 * Returns: (transfer none): the #GdkScreen object
 *
 * Since: 2.2
 */
GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return gdk_display_get_default_screen (display);
}

/**
 * gdk_display_get_default_screen:
 * @display: a #GdkDisplay
 *
 * Get the default #GdkScreen for @display.
 *
 * Returns: (transfer none): the default #GdkScreen object for @display
 *
 * Since: 2.2
 */
GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_default_screen (display);
}

/**
 * gdk_display_beep:
 * @display: a #GdkDisplay
 *
 * Emits a short beep on @display
 *
 * Since: 2.2
 */
void
gdk_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->beep (display);
}

/**
 * gdk_display_sync:
 * @display: a #GdkDisplay
 *
 * Flushes any requests queued for the windowing system and waits until all
 * requests have been handled. This is often used for making sure that the
 * display is synchronized with the current state of the program. Calling
 * gdk_display_sync() before gdk_error_trap_pop() makes sure that any errors
 * generated from earlier requests are handled before the error trap is
 * removed.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 *
 * Since: 2.2
 */
void
gdk_display_sync (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->sync (display);
}

/**
 * gdk_display_flush:
 * @display: a #GdkDisplay
 *
 * Flushes any requests queued for the windowing system; this happens automatically
 * when the main loop blocks waiting for new events, but if your application
 * is drawing without returning control to the main loop, you may need
 * to call this function explicitly. A common case where this function
 * needs to be called is when an application is executing drawing commands
 * from a thread other than the thread where the main loop is running.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 *
 * Since: 2.4
 */
void
gdk_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->flush (display);
}

/**
 * gdk_display_get_default_group:
 * @display: a #GdkDisplay
 *
 * Returns the default group leader window for all toplevel windows
 * on @display. This window is implicitly created by GDK.
 * See gdk_window_set_group().
 *
 * Returns: (transfer none): The default group leader window
 * for @display
 *
 * Since: 2.4
 **/
GdkWindow *
gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_default_group (display);
}

/**
 * gdk_display_supports_selection_notification:
 * @display: a #GdkDisplay
 *
 * Returns whether #GdkEventOwnerChange events will be
 * sent when the owner of a selection changes.
 *
 * Returns: whether #GdkEventOwnerChange events will
 *               be sent.
 *
 * Since: 2.6
 **/
gboolean
gdk_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->supports_selection_notification (display);
}

/**
 * gdk_display_request_selection_notification:
 * @display: a #GdkDisplay
 * @selection: the #GdkAtom naming the selection for which
 *             ownership change notification is requested
 *
 * Request #GdkEventOwnerChange events for ownership changes
 * of the selection named by the given atom.
 *
 * Returns: whether #GdkEventOwnerChange events will
 *               be sent.
 *
 * Since: 2.6
 **/
gboolean
gdk_display_request_selection_notification (GdkDisplay *display,
					    GdkAtom     selection)

{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->request_selection_notification (display, selection);
}

/**
 * gdk_display_supports_clipboard_persistence:
 * @display: a #GdkDisplay
 *
 * Returns whether the speicifed display supports clipboard
 * persistance; i.e. if it’s possible to store the clipboard data after an
 * application has quit. On X11 this checks if a clipboard daemon is
 * running.
 *
 * Returns: %TRUE if the display supports clipboard persistance.
 *
 * Since: 2.6
 */
gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->supports_clipboard_persistence (display);
}

/**
 * gdk_display_store_clipboard:
 * @display:          a #GdkDisplay
 * @clipboard_window: a #GdkWindow belonging to the clipboard owner
 * @time_:            a timestamp
 * @targets:          (array length=n_targets): an array of targets
 *                    that should be saved, or %NULL
 *                    if all available targets should be saved.
 * @n_targets:        length of the @targets array
 *
 * Issues a request to the clipboard manager to store the
 * clipboard data. On X11, this is a special program that works
 * according to the
 * [FreeDesktop Clipboard Specification](http://www.freedesktop.org/Standards/clipboard-manager-spec).
 *
 * Since: 2.6
 */
void
gdk_display_store_clipboard (GdkDisplay    *display,
			     GdkWindow     *clipboard_window,
			     guint32        time_,
			     const GdkAtom *targets,
			     gint           n_targets)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->store_clipboard (display, clipboard_window, time_, targets, n_targets);
}

/**
 * gdk_display_supports_shapes:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if gdk_window_shape_combine_mask() can
 * be used to create shaped windows on @display.
 *
 * Returns: %TRUE if shaped windows are supported
 *
 * Since: 2.10
 */
gboolean
gdk_display_supports_shapes (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->supports_shapes (display);
}

/**
 * gdk_display_supports_input_shapes:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if gdk_window_input_shape_combine_mask() can
 * be used to modify the input shape of windows on @display.
 *
 * Returns: %TRUE if windows with modified input shape are supported
 *
 * Since: 2.10
 */
gboolean
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->supports_input_shapes (display);
}

/**
 * gdk_display_supports_composite:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if gdk_window_set_composited() can be used
 * to redirect drawing on the window using compositing.
 *
 * Currently this only works on X11 with XComposite and
 * XDamage extensions available.
 *
 * Returns: %TRUE if windows may be composited.
 *
 * Since: 2.12
 *
 * Deprecated: 3.16: Compositing is an outdated technology that
 *   only ever worked on X11.
 */
gboolean
gdk_display_supports_composite (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->supports_composite (display);
}

/**
 * gdk_display_list_devices:
 * @display: a #GdkDisplay
 *
 * Returns the list of available input devices attached to @display.
 * The list is statically allocated and should not be freed.
 *
 * Returns: (transfer none) (element-type GdkDevice):
 *     a list of #GdkDevice
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_device_manager_list_devices() instead.
 **/
GList *
gdk_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->list_devices (display);
}

static GdkAppLaunchContext *
gdk_display_real_get_app_launch_context (GdkDisplay *display)
{
  GdkAppLaunchContext *ctx;

  ctx = g_object_new (GDK_TYPE_APP_LAUNCH_CONTEXT,
                      "display", display,
                      NULL);

  return ctx;
}

/**
 * gdk_display_get_app_launch_context:
 * @display: a #GdkDisplay
 *
 * Returns a #GdkAppLaunchContext suitable for launching
 * applications on the given display.
 *
 * Returns: (transfer full): a new #GdkAppLaunchContext for @display.
 *     Free with g_object_unref() when done
 *
 * Since: 3.0
 */
GdkAppLaunchContext *
gdk_display_get_app_launch_context (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_app_launch_context (display);
}

/**
 * gdk_display_open:
 * @display_name: the name of the display to open
 *
 * Opens a display.
 *
 * Returns: (nullable) (transfer none): a #GdkDisplay, or %NULL if the
 *     display could not be opened
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  return gdk_display_manager_open_display (gdk_display_manager_get (),
                                           display_name);
}

/**
 * gdk_display_has_pending:
 * @display: a #GdkDisplay
 *
 * Returns whether the display has events that are waiting
 * to be processed.
 *
 * Returns: %TRUE if there are events ready to be processed.
 *
 * Since: 3.0
 */
gboolean
gdk_display_has_pending (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->has_pending (display);
}

/**
 * gdk_display_supports_cursor_alpha:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if cursors can use an 8bit alpha channel
 * on @display. Otherwise, cursors are restricted to bilevel
 * alpha (i.e. a mask).
 *
 * Returns: whether cursors can have alpha channels.
 *
 * Since: 2.4
 */
gboolean
gdk_display_supports_cursor_alpha (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->supports_cursor_alpha (display);
}

/**
 * gdk_display_supports_cursor_color:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if multicolored cursors are supported
 * on @display. Otherwise, cursors have only a forground
 * and a background color.
 *
 * Returns: whether cursors can have multiple colors.
 *
 * Since: 2.4
 */
gboolean
gdk_display_supports_cursor_color (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->supports_cursor_color (display);
}

/**
 * gdk_display_get_default_cursor_size:
 * @display: a #GdkDisplay
 *
 * Returns the default size to use for cursors on @display.
 *
 * Returns: the default cursor size.
 *
 * Since: 2.4
 */
guint
gdk_display_get_default_cursor_size (GdkDisplay *display)
{
  guint width, height;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  GDK_DISPLAY_GET_CLASS (display)->get_default_cursor_size (display,
                                                            &width,
                                                            &height);

  return MIN (width, height);
}

/**
 * gdk_display_get_maximal_cursor_size:
 * @display: a #GdkDisplay
 * @width: (out): the return location for the maximal cursor width
 * @height: (out): the return location for the maximal cursor height
 *
 * Gets the maximal size to use for cursors on @display.
 *
 * Since: 2.4
 */
void
gdk_display_get_maximal_cursor_size (GdkDisplay *display,
                                     guint       *width,
                                     guint       *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->get_maximal_cursor_size (display,
                                                            width,
                                                            height);
}

/**
 * gdk_display_warp_pointer:
 * @display: a #GdkDisplay
 * @screen: the screen of @display to warp the pointer to
 * @x: the x coordinate of the destination
 * @y: the y coordinate of the destination
 *
 * Warps the pointer of @display to the point @x,@y on
 * the screen @screen, unless the pointer is confined
 * to a window by a grab, in which case it will be moved
 * as far as allowed by the grab. Warping the pointer
 * creates events as if the user had moved the mouse
 * instantaneously to the destination.
 *
 * Note that the pointer should normally be under the
 * control of the user. This function was added to cover
 * some rare use cases like keyboard navigation support
 * for the color picker in the #GtkColorSelectionDialog.
 *
 * Since: 2.8
 *
 * Deprecated: 3.0: Use gdk_device_warp() instead.
 */
void
gdk_display_warp_pointer (GdkDisplay *display,
                          GdkScreen  *screen,
                          gint        x,
                          gint        y)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  gdk_device_warp (display->core_pointer,
                   screen,
                   x, y);
}

gulong
_gdk_display_get_next_serial (GdkDisplay *display)
{
  return GDK_DISPLAY_GET_CLASS (display)->get_next_serial (display);
}


/**
 * gdk_notify_startup_complete:
 *
 * Indicates to the GUI environment that the application has finished
 * loading. If the applications opens windows, this function is
 * normally called after opening the application’s initial set of
 * windows.
 *
 * GTK+ will call this function automatically after opening the first
 * #GtkWindow unless gtk_window_set_auto_startup_notification() is called
 * to disable that feature.
 *
 * Since: 2.2
 **/
void
gdk_notify_startup_complete (void)
{
  gdk_notify_startup_complete_with_id (NULL);
}

/**
 * gdk_notify_startup_complete_with_id:
 * @startup_id: a startup-notification identifier, for which
 *     notification process should be completed
 *
 * Indicates to the GUI environment that the application has
 * finished loading, using a given identifier.
 *
 * GTK+ will call this function automatically for #GtkWindow
 * with custom startup-notification identifier unless
 * gtk_window_set_auto_startup_notification() is called to
 * disable that feature.
 *
 * Since: 2.12
 */
void
gdk_notify_startup_complete_with_id (const gchar* startup_id)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();
  if (display)
    gdk_display_notify_startup_complete (display, startup_id);
}

/**
 * gdk_display_notify_startup_complete:
 * @display: a #GdkDisplay
 * @startup_id: a startup-notification identifier, for which
 *     notification process should be completed
 *
 * Indicates to the GUI environment that the application has
 * finished loading, using a given identifier.
 *
 * GTK+ will call this function automatically for #GtkWindow
 * with custom startup-notification identifier unless
 * gtk_window_set_auto_startup_notification() is called to
 * disable that feature.
 *
 * Since: 3.0
 */
void
gdk_display_notify_startup_complete (GdkDisplay  *display,
                                     const gchar *startup_id)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->notify_startup_complete (display, startup_id);
}

void
_gdk_display_pause_events (GdkDisplay *display)
{
  display->event_pause_count++;
}

void
_gdk_display_unpause_events (GdkDisplay *display)
{
  g_return_if_fail (display->event_pause_count > 0);

  display->event_pause_count--;
}

void
_gdk_display_event_data_copy (GdkDisplay     *display,
                              const GdkEvent *event,
                              GdkEvent       *new_event)
{
  GDK_DISPLAY_GET_CLASS (display)->event_data_copy (display, event, new_event);
}

void
_gdk_display_event_data_free (GdkDisplay *display,
                              GdkEvent   *event)
{
  GDK_DISPLAY_GET_CLASS (display)->event_data_free (display, event);
}

void
_gdk_display_create_window_impl (GdkDisplay       *display,
                                 GdkWindow        *window,
                                 GdkWindow        *real_parent,
                                 GdkScreen        *screen,
                                 GdkEventMask      event_mask,
                                 GdkWindowAttr    *attributes,
                                 gint              attributes_mask)
{
  GDK_DISPLAY_GET_CLASS (display)->create_window_impl (display,
                                                       window,
                                                       real_parent,
                                                       screen,
                                                       event_mask,
                                                       attributes,
                                                       attributes_mask);
}

GdkWindow *
_gdk_display_create_window (GdkDisplay *display)
{
  return g_object_new (GDK_DISPLAY_GET_CLASS (display)->window_type, NULL);
}

/**
 * gdk_keymap_get_for_display:
 * @display: the #GdkDisplay.
 *
 * Returns the #GdkKeymap attached to @display.
 *
 * Returns: (transfer none): the #GdkKeymap attached to @display.
 *
 * Since: 2.2
 */
GdkKeymap*
gdk_keymap_get_for_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_keymap (display);
}

typedef struct _GdkGlobalErrorTrap  GdkGlobalErrorTrap;

struct _GdkGlobalErrorTrap
{
  GSList *displays;
};

static GQueue gdk_error_traps = G_QUEUE_INIT;

/**
 * gdk_error_trap_push:
 *
 * This function allows X errors to be trapped instead of the normal
 * behavior of exiting the application. It should only be used if it
 * is not possible to avoid the X error in any other way. Errors are
 * ignored on all #GdkDisplay currently known to the
 * #GdkDisplayManager. If you don’t care which error happens and just
 * want to ignore everything, pop with gdk_error_trap_pop_ignored().
 * If you need the error code, use gdk_error_trap_pop() which may have
 * to block and wait for the error to arrive from the X server.
 *
 * This API exists on all platforms but only does anything on X.
 *
 * You can use gdk_x11_display_error_trap_push() to ignore errors
 * on only a single display.
 *
 * ## Trapping an X error
 *
 * |[<!-- language="C" -->
 * gdk_error_trap_push ();
 *
 *  // ... Call the X function which may cause an error here ...
 *
 *
 * if (gdk_error_trap_pop ())
 *  {
 *    // ... Handle the error here ...
 *  }
 * ]|
 */
void
gdk_error_trap_push (void)
{
  GdkDisplayManager *manager;
  GdkDisplayClass *class;
  GdkGlobalErrorTrap *trap;
  GSList *l;

  manager = gdk_display_manager_get ();
  class = GDK_DISPLAY_GET_CLASS (gdk_display_manager_get_default_display (manager));

  if (class->push_error_trap == NULL)
    return;

  trap = g_slice_new (GdkGlobalErrorTrap);
  trap->displays = gdk_display_manager_list_displays (manager);

  g_slist_foreach (trap->displays, (GFunc) g_object_ref, NULL);
  for (l = trap->displays; l != NULL; l = l->next)
    {
      class->push_error_trap (l->data);
    }

  g_queue_push_head (&gdk_error_traps, trap);
}

static gint
gdk_error_trap_pop_internal (gboolean need_code)
{
  GdkDisplayManager *manager;
  GdkDisplayClass *class;
  GdkGlobalErrorTrap *trap;
  gint result;
  GSList *l;

  manager = gdk_display_manager_get ();
  class = GDK_DISPLAY_GET_CLASS (gdk_display_manager_get_default_display (manager));

  if (class->pop_error_trap == NULL)
    return 0;

  trap = g_queue_pop_head (&gdk_error_traps);

  g_return_val_if_fail (trap != NULL, 0);

  result = 0;
  for (l = trap->displays; l != NULL; l = l->next)
    {
      gint code = 0;

      code = class->pop_error_trap (l->data, !need_code);

      /* we use the error on the last display listed, why not. */
      if (code != 0)
        result = code;
    }

  g_slist_free_full (trap->displays, g_object_unref);
  g_slice_free (GdkGlobalErrorTrap, trap);

  return result;
}

/**
 * gdk_error_trap_pop_ignored:
 *
 * Removes an error trap pushed with gdk_error_trap_push(), but
 * without bothering to wait and see whether an error occurred.  If an
 * error arrives later asynchronously that was triggered while the
 * trap was pushed, that error will be ignored.
 *
 * Since: 3.0
 */
void
gdk_error_trap_pop_ignored (void)
{
  gdk_error_trap_pop_internal (FALSE);
}

/**
 * gdk_error_trap_pop:
 *
 * Removes an error trap pushed with gdk_error_trap_push().
 * May block until an error has been definitively received
 * or not received from the X server. gdk_error_trap_pop_ignored()
 * is preferred if you don’t need to know whether an error
 * occurred, because it never has to block. If you don't
 * need the return value of gdk_error_trap_pop(), use
 * gdk_error_trap_pop_ignored().
 *
 * Prior to GDK 3.0, this function would not automatically
 * sync for you, so you had to gdk_flush() if your last
 * call to Xlib was not a blocking round trip.
 *
 * Returns: X error code or 0 on success
 */
gint
gdk_error_trap_pop (void)
{
  return gdk_error_trap_pop_internal (TRUE);
}

/*< private >
 * gdk_display_make_gl_context_current:
 * @display: a #GdkDisplay
 * @context: (optional): a #GdkGLContext, or %NULL
 *
 * Makes the given @context the current GL context, or unsets
 * the current GL context if @context is %NULL.
 */
gboolean
gdk_display_make_gl_context_current (GdkDisplay   *display,
                                     GdkGLContext *context)
{
  return GDK_DISPLAY_GET_CLASS (display)->make_gl_context_current (display, context);
}
