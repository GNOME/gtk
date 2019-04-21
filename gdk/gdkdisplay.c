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

#include "gdkintl.h"
#include "gdk-private.h"

#include "gdkclipboardprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplaymanagerprivate.h"
#include "gdkevents.h"
#include "gdksurfaceimpl.h"
#include "gdkinternals.h"
#include "gdkmonitorprivate.h"

#include <math.h>
#include <glib.h>

/* for the use of round() */
#include "fallback-c89.c"

/**
 * SECTION:gdkdisplay
 * @Short_description: Controls a set of monitors and their associated input devices
 * @Title: GdkDisplay
 *
 * GdkDisplay objects are the GDK representation of a workstation.
 *
 * Their purpose are two-fold:
 * - To manage and provide information about input devices (pointers, keyboards, etc)
 * - To manage and provide information about output devices (monitors, projectors, etc)
 *
 * Most of the input device handling has been factored out into separate #GdkSeat
 * objects. Every display has a one or more seats, which can be accessed with
 * gdk_display_get_default_seat() and gdk_display_list_seats().
 *
 * Output devices are represented by #GdkMonitor objects, which can be accessed
 * with gdk_display_get_monitor() and similar APIs.
 */

/**
 * GdkDisplay:
 *
 * The GdkDisplay struct contains only private fields and should not
 * be accessed directly.
 */
enum
{
  PROP_0,
  PROP_COMPOSITED,
  PROP_RGBA,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

enum {
  OPENED,
  CLOSED,
  SEAT_ADDED,
  SEAT_REMOVED,
  MONITOR_ADDED,
  MONITOR_REMOVED,
  SETTING_CHANGED,
  LAST_SIGNAL
};

static void gdk_display_dispose     (GObject         *object);
static void gdk_display_finalize    (GObject         *object);
static void gdk_display_put_event_nocopy (GdkDisplay *display,
                                          GdkEvent   *event);


static GdkAppLaunchContext *gdk_display_real_get_app_launch_context (GdkDisplay *display);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkDisplay, gdk_display, G_TYPE_OBJECT)

static void
gdk_display_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdkDisplay *display = GDK_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_COMPOSITED:
      g_value_set_boolean (value, gdk_display_is_composited (display));
      break;

    case PROP_RGBA:
      g_value_set_boolean (value, gdk_display_is_rgba (display));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_display_real_make_default (GdkDisplay *display)
{
}

static void
gdk_display_real_opened (GdkDisplay *display)
{
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

static GdkSeat *
gdk_display_real_get_default_seat (GdkDisplay *display)
{
  if (!display->seats)
    return NULL;

  return display->seats->data;
}

static GdkMonitor *
gdk_display_real_get_primary_monitor (GdkDisplay *display)
{
  return gdk_display_get_monitor (display, 0);
}

static void
gdk_display_class_init (GdkDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_display_finalize;
  object_class->dispose = gdk_display_dispose;
  object_class->get_property = gdk_display_get_property;

  class->get_app_launch_context = gdk_display_real_get_app_launch_context;
  class->surface_type = GDK_TYPE_SURFACE;

  class->opened = gdk_display_real_opened;
  class->make_default = gdk_display_real_make_default;
  class->event_data_copy = gdk_display_real_event_data_copy;
  class->event_data_free = gdk_display_real_event_data_free;
  class->get_default_seat = gdk_display_real_get_default_seat;

  class->get_primary_monitor = gdk_display_real_get_primary_monitor;

  /**
   * GdkDisplay:composited:
   *
   * %TRUE if the display properly composits the alpha channel.
   * See gdk_display_is_composited() for details.
   */
  props[PROP_COMPOSITED] =
    g_param_spec_boolean ("composited",
                          P_("Composited"),
                          P_("Composited"),
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDisplay:rgba:
   *
   * %TRUE if the display supports an alpha channel. See gdk_display_is_rgba()
   * for details.
   */
  props[PROP_RGBA] =
    g_param_spec_boolean ("rgba",
                          P_("RGBA"),
                          P_("RGBA"),
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

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
   */   
  signals[CLOSED] =
    g_signal_new (g_intern_static_string ("closed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkDisplayClass, closed),
		  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1,
		  G_TYPE_BOOLEAN);

  /**
   * GdkDisplay::seat-added:
   * @display: the object on which the signal is emitted
   * @seat: the seat that was just added
   *
   * The ::seat-added signal is emitted whenever a new seat is made
   * known to the windowing system.
   */
  signals[SEAT_ADDED] =
    g_signal_new (g_intern_static_string ("seat-added"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GDK_TYPE_SEAT);

  /**
   * GdkDisplay::seat-removed:
   * @display: the object on which the signal is emitted
   * @seat: the seat that was just removed
   *
   * The ::seat-removed signal is emitted whenever a seat is removed
   * by the windowing system.
   */
  signals[SEAT_REMOVED] =
    g_signal_new (g_intern_static_string ("seat-removed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GDK_TYPE_SEAT);

  /**
   * GdkDisplay::monitor-added:
   * @display: the objedct on which the signal is emitted
   * @monitor: the monitor that was just added
   *
   * The ::monitor-added signal is emitted whenever a monitor is
   * added.
   */
  signals[MONITOR_ADDED] =
    g_signal_new (g_intern_static_string ("monitor-added"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GDK_TYPE_MONITOR);

  /**
   * GdkDisplay::monitor-removed:
   * @display: the object on which the signal is emitted
   * @monitor: the monitor that was just removed
   *
   * The ::monitor-removed signal is emitted whenever a monitor is
   * removed.
   */
  signals[MONITOR_REMOVED] =
    g_signal_new (g_intern_static_string ("monitor-removed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GDK_TYPE_MONITOR);

  /**
   * GdkDisplay::setting-changed:
   * @display: the object on which the signal is emitted
   * @setting: the name of the setting that changed
   *
   * The ::setting-changed signal is emitted whenever a setting
   * changes its value.
   */
  signals[SETTING_CHANGED] =
    g_signal_new (g_intern_static_string ("setting-changed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL,
                  NULL,
		  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
free_pointer_info (GdkPointerSurfaceInfo *info)
{
  g_clear_object (&info->surface_under_pointer);
  g_slice_free (GdkPointerSurfaceInfo, info);
}

static void
free_device_grab (GdkDeviceGrabInfo *info)
{
  g_object_unref (info->surface);
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

  display->device_grabs = g_hash_table_new (NULL, NULL);

  display->pointers_info = g_hash_table_new_full (NULL, NULL, NULL,
                                                  (GDestroyNotify) free_pointer_info);

  g_queue_init (&display->queued_events);

  display->debug_flags = _gdk_debug_flags;

  display->composited = TRUE;
  display->rgba = TRUE;
}

static void
gdk_display_dispose (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY (object);

  _gdk_display_manager_remove_display (gdk_display_manager_get (), display);

  g_queue_clear (&display->queued_events);

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

  g_hash_table_destroy (display->pointers_info);

  g_list_free_full (display->seats, g_object_unref);

  G_OBJECT_CLASS (gdk_display_parent_class)->finalize (object);
}

/**
 * gdk_display_close:
 * @display: a #GdkDisplay
 *
 * Closes the connection to the windowing system for the given display,
 * and cleans up associated resources.
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
 * Returns: (nullable) (transfer full): the next #GdkEvent to be processed,
 *   or %NULL if no events are pending
 */
GdkEvent *
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
 * Returns: (nullable) (transfer full): the first #GdkEvent on the
 *   event queue
 **/
GdkEvent *
gdk_display_peek_event (GdkDisplay *display)
{
  GList *tmp_list;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  tmp_list = _gdk_event_queue_find_first (display);
  
  if (tmp_list != NULL)
    return g_object_ref (tmp_list->data);

  return NULL;
}

static void
gdk_display_put_event_nocopy (GdkDisplay *display,
                              GdkEvent   *event)
{
  _gdk_event_queue_append (display, event);
}

/**
 * gdk_display_put_event:
 * @display: a #GdkDisplay
 * @event: a #GdkEvent.
 *
 * Appends a copy of the given event onto the front of the event
 * queue for @display.
 **/
void
gdk_display_put_event (GdkDisplay     *display,
		       const GdkEvent *event)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (event != NULL);

  gdk_display_put_event_nocopy (display, gdk_event_copy (event));
}

static void
generate_grab_broken_event (GdkDisplay *display,
                            GdkSurface  *surface,
                            GdkDevice  *device,
			    gboolean    implicit,
			    GdkSurface  *grab_surface)
{
  g_return_if_fail (surface != NULL);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      GdkEvent *event;

      event = gdk_event_new (GDK_GRAB_BROKEN);
      event->any.surface = g_object_ref (surface);
      event->any.send_event = FALSE;
      event->grab_broken.implicit = implicit;
      event->grab_broken.grab_surface = grab_surface;
      gdk_event_set_device (event, device);
      event->grab_broken.keyboard = (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD) ? TRUE : FALSE;

      gdk_display_put_event_nocopy (display, event);
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
                              GdkSurface        *surface,
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

  info->surface = g_object_ref (surface);
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

static GdkSurface *
get_current_toplevel (GdkDisplay      *display,
                      GdkDevice       *device,
                      int             *x_out,
                      int             *y_out,
		      GdkModifierType *state_out)
{
  GdkSurface *pointer_surface;
  gdouble x, y;
  GdkModifierType state;

  pointer_surface = _gdk_device_surface_at_position (device, &x, &y, &state, TRUE);

  if (pointer_surface != NULL &&
      GDK_SURFACE_DESTROYED (pointer_surface))
    pointer_surface = NULL;

  *x_out = round (x);
  *y_out = round (y);
  *state_out = state;

  return pointer_surface;
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
  GdkSurface *new_toplevel;
  GdkPointerSurfaceInfo *info;
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
      if (!grab->implicit)
	{
	  /* !owner_event Grabbing a surface that we're not inside, current status is
	     now NULL (i.e. outside grabbed surface) */
	  if (!grab->owner_events && info->surface_under_pointer != grab->surface)
	    _gdk_display_set_surface_under_pointer (display, device, NULL);
	}

      grab->activated = TRUE;
    }

  if (last_grab)
    {
      new_toplevel = NULL;

      if (grab == NULL /* ungrab */ ||
	  (!last_grab->owner_events && grab->owner_events) /* switched to owner_events */ )
	{
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
              _gdk_display_set_surface_under_pointer (display, device, new_toplevel);
	      info->toplevel_x = x;
	      info->toplevel_y = y;
	      info->state = state;
	    }
	}

      if (grab == NULL) /* Ungrabbed, send events */
	{
	  /* We're now ungrabbed, update the surface_under_pointer */
	  _gdk_display_set_surface_under_pointer (display, device, new_toplevel);
	}
    }

  g_hash_table_insert (display->device_grabs, device, old_grabs);
}

void
_gdk_display_update_last_event (GdkDisplay     *display,
                                const GdkEvent *event)
{
  if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
    display->last_event_time = gdk_event_get_time (event);
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

      if ((next_grab == NULL && current_grab->implicit_ungrab) ||
          (next_grab != NULL && current_grab->surface != next_grab->surface))
        generate_grab_broken_event (display, GDK_SURFACE (current_grab->surface),
                                    device,
                                    current_grab->implicit,
                                    next_grab? next_grab->surface : NULL);

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

/* Returns true if last grab was ended
 * If if_child is non-NULL, end the grab only if the grabbed
 * surface is the same as if_child or a descendant of it */
gboolean
_gdk_display_end_device_grab (GdkDisplay *display,
                              GdkDevice  *device,
                              gulong      serial,
                              GdkSurface  *if_child,
                              gboolean    implicit)
{
  GdkDeviceGrabInfo *grab;
  GList *l;

  l = find_device_grab (display, device, serial);

  if (l == NULL)
    return FALSE;

  grab = l->data;
  if (grab && (if_child == NULL || if_child == grab->surface))
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

GdkPointerSurfaceInfo *
_gdk_display_get_pointer_info (GdkDisplay *display,
                               GdkDevice  *device)
{
  GdkPointerSurfaceInfo *info;

  if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  if (G_UNLIKELY (!device))
    return NULL;

  info = g_hash_table_lookup (display->pointers_info, device);

  if (G_UNLIKELY (!info))
    {
      info = g_slice_new0 (GdkPointerSurfaceInfo);
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
      GdkPointerSurfaceInfo *info = value;
      GdkDevice *device = key;

      (func) (display, device, info, user_data);
    }
}

/*< private >
 * gdk_device_grab_info:
 * @display: the display for which to get the grab information
 * @device: device to get the grab information from
 * @grab_surface: (out) (transfer none): location to store current grab surface
 * @owner_events: (out): location to store boolean indicating whether
 *   the @owner_events flag to gdk_device_grab() was %TRUE.
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
                      GdkSurface  **grab_surface,
                      gboolean    *owner_events)
{
  GdkDeviceGrabInfo *info;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  info = _gdk_display_get_last_device_grab (display, device);

  if (info)
    {
      if (grab_surface)
        *grab_surface = info->surface;
      if (owner_events)
        *owner_events = info->owner_events;

      return TRUE;
    }
  else
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
 * gdk_display_get_name:
 * @display: a #GdkDisplay
 *
 * Gets the name of the display.
 *
 * Returns: a string representing the display name. This string is owned
 * by GDK and should not be modified or freed.
 */
const gchar *
gdk_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_name (display);
}

/**
 * gdk_display_beep:
 * @display: a #GdkDisplay
 *
 * Emits a short beep on @display
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
 * Returns the default group leader surface for all toplevel surfaces
 * on @display. This surface is implicitly created by GDK.
 * See gdk_surface_set_group().
 *
 * Returns: (transfer none): The default group leader surface
 * for @display
 **/
GdkSurface *
gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_default_group (display);
}

/**
 * gdk_display_get_clipboard:
 * @display: a #GdkDisplay
 *
 * Gets the clipboard used for copy/paste operations.
 *
 * Returns: (transfer none): the display's clipboard
 */
GdkClipboard *
gdk_display_get_clipboard (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->clipboard == NULL)
    display->clipboard = gdk_clipboard_new (display);

  return display->clipboard;
}

/**
 * gdk_display_get_primary_clipboard:
 * @display: a #GdkDisplay
 *
 * Gets the clipboard used for the primary selection. On backends where the
 * primary clipboard is not supported natively, GDK emulates this clipboard
 * locally.
 *
 * Returns: (transfer none): the primary clipboard
 */
GdkClipboard *
gdk_display_get_primary_clipboard (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->primary_clipboard == NULL)
    display->primary_clipboard = gdk_clipboard_new (display);

  return display->primary_clipboard;
}

/**
 * gdk_display_supports_shapes:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if gdk_surface_shape_combine_mask() can
 * be used to create shaped windows on @display.
 *
 * Returns: %TRUE if shaped windows are supported
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
 * Returns %TRUE if gdk_surface_input_shape_combine_mask() can
 * be used to modify the input shape of surfaces on @display.
 *
 * Returns: %TRUE if surfaces with modified input shape are supported
 */
gboolean
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->supports_input_shapes (display);
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
 */
gboolean
gdk_display_has_pending (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->has_pending (display);
}

gulong
_gdk_display_get_next_serial (GdkDisplay *display)
{
  return GDK_DISPLAY_GET_CLASS (display)->get_next_serial (display);
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
 */
void
gdk_display_notify_startup_complete (GdkDisplay  *display,
                                     const gchar *startup_id)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->notify_startup_complete (display, startup_id);
}

/**
 * gdk_display_get_startup_notification_id:
 * @display: a #GdkDisplay
 *
 * Gets the startup notification ID for a Wayland display, or %NULL
 * if no ID has been defined.
 *
 * Returns: the startup notification ID for @display, or %NULL
 */
const gchar *
gdk_display_get_startup_notification_id (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (GDK_DISPLAY_GET_CLASS (display)->get_startup_notification_id == NULL)
    return NULL;

  return GDK_DISPLAY_GET_CLASS (display)->get_startup_notification_id (display);
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
gdk_display_create_surface_impl (GdkDisplay       *display,
                                 GdkSurface       *surface,
                                 GdkSurface       *real_parent)
{
  GDK_DISPLAY_GET_CLASS (display)->create_surface_impl (display,
                                                        surface,
                                                        real_parent);
}

GdkSurface *
_gdk_display_create_surface (GdkDisplay *display)
{
  return g_object_new (GDK_DISPLAY_GET_CLASS (display)->surface_type,
                       "display", display,
                       NULL);
}

/**
 * gdk_display_get_keymap:
 * @display: the #GdkDisplay
 *
 * Returns the #GdkKeymap attached to @display.
 *
 * Returns: (transfer none): the #GdkKeymap attached to @display.
 */
GdkKeymap *
gdk_display_get_keymap (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_keymap (display);
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

GdkDebugFlags
gdk_display_get_debug_flags (GdkDisplay *display)
{
  return display ? display->debug_flags : _gdk_debug_flags;
}

void
gdk_display_set_debug_flags (GdkDisplay    *display,
                             GdkDebugFlags  flags)
{
  display->debug_flags = flags;
}

/**
 * gdk_display_is_composited:
 * @display: a #GdkDisplay
 *
 * Returns whether surfaces can reasonably be expected to have
 * their alpha channel drawn correctly on the screen. Check
 * gdk_display_is_rgba() for wether the display supports an
 * alpha channel.
 *
 * On X11 this function returns whether a compositing manager is
 * compositing on @display.
 *
 * On modern displays, this value is always %TRUE.
 *
 * Returns: Whether surfaces with RGBA visuals can reasonably be
 * expected to have their alpha channels drawn correctly on the screen.
 **/
gboolean
gdk_display_is_composited (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return display->composited;
}

void
gdk_display_set_composited (GdkDisplay *display,
                            gboolean    composited)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (display->composited == composited)
    return;

  display->composited = composited;

  g_object_notify_by_pspec (G_OBJECT (display), props[PROP_COMPOSITED]);
}

/**
 * gdk_display_is_rgba:
 * @display: a #GdkDisplay
 *
 * Returns wether surfaces on this @display are created with an
 * alpha channel.
 *
 * Even if a %TRUE is returned, it is possible that the
 * surface’s alpha channel won’t be honored when displaying the
 * surface on the screen: in particular, for X an appropriate
 * windowing manager and compositing manager must be running to
 * provide appropriate display. Use gdk_display_is_composited()
 * to check if that is the case.
 *
 * For setting an overall opacity for a top-level surface, see
 * gdk_surface_set_opacity().
 *
 * On modern displays, this value is always %TRUE.
 *
 * Returns: %TRUE if surfaces are created with an alpha channel or
 *     %FALSE if the display does not support this functionality.
 **/
gboolean
gdk_display_is_rgba (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return display->rgba;
}

void
gdk_display_set_rgba (GdkDisplay *display,
                      gboolean    rgba)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (display->rgba == rgba)
    return;

  display->rgba = rgba;

  g_object_notify_by_pspec (G_OBJECT (display), props[PROP_RGBA]);
}

static void
device_removed_cb (GdkSeat    *seat,
                   GdkDevice  *device,
                   GdkDisplay *display)
{
  g_hash_table_remove (display->device_grabs, device);
  g_hash_table_remove (display->pointers_info, device);

  /* FIXME: change core pointer and remove from device list */
}

void
gdk_display_add_seat (GdkDisplay *display,
                      GdkSeat    *seat)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GDK_IS_SEAT (seat));

  display->seats = g_list_append (display->seats, g_object_ref (seat));
  g_signal_emit (display, signals[SEAT_ADDED], 0, seat);

  g_signal_connect (seat, "device-removed", G_CALLBACK (device_removed_cb), display);
}

void
gdk_display_remove_seat (GdkDisplay *display,
                         GdkSeat    *seat)
{
  GList *link;

  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GDK_IS_SEAT (seat));

  g_signal_handlers_disconnect_by_func (seat, G_CALLBACK (device_removed_cb), display);

  link = g_list_find (display->seats, seat);

  if (link)
    {
      display->seats = g_list_remove_link (display->seats, link);
      g_signal_emit (display, signals[SEAT_REMOVED], 0, seat);
      g_object_unref (link->data);
      g_list_free (link);
    }
}

/**
 * gdk_display_get_default_seat:
 * @display: a #GdkDisplay
 *
 * Returns the default #GdkSeat for this display.
 *
 * Returns: (transfer none): the default seat.
 **/
GdkSeat *
gdk_display_get_default_seat (GdkDisplay *display)
{
  GdkDisplayClass *display_class;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  display_class = GDK_DISPLAY_GET_CLASS (display);

  return display_class->get_default_seat (display);
}

/**
 * gdk_display_list_seats:
 * @display: a #GdkDisplay
 *
 * Returns the list of seats known to @display.
 *
 * Returns: (transfer container) (element-type GdkSeat): the
 *          list of seats known to the #GdkDisplay
 **/
GList *
gdk_display_list_seats (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return g_list_copy (display->seats);
}

/**
 * gdk_display_get_n_monitors:
 * @display: a #GdkDisplay
 *
 * Gets the number of monitors that belong to @display.
 *
 * The returned number is valid until the next emission of the
 * #GdkDisplay::monitor-added or #GdkDisplay::monitor-removed signal.
 *
 * Returns: the number of monitors
 */
int
gdk_display_get_n_monitors (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  if (GDK_DISPLAY_GET_CLASS (display)->get_n_monitors == NULL)
    return 1;

  return GDK_DISPLAY_GET_CLASS (display)->get_n_monitors (display);
}

/**
 * gdk_display_get_monitor:
 * @display: a #GdkDisplay
 * @monitor_num: number of the monitor
 *
 * Gets a monitor associated with this display.
 *
 * Returns: (nullable) (transfer none): the #GdkMonitor, or %NULL if
 *    @monitor_num is not a valid monitor number
 */
GdkMonitor *
gdk_display_get_monitor (GdkDisplay *display,
                         gint        monitor_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_monitor (display, monitor_num);
}

/**
 * gdk_display_get_primary_monitor:
 * @display: a #GdkDisplay
 *
 * Gets the primary monitor for the display.
 *
 * The primary monitor is considered the monitor where the “main desktop”
 * lives. While normal application surfaces typically allow the window
 * manager to place the surfaces, specialized desktop applications
 * such as panels should place themselves on the primary monitor.
 *
 * If no monitor is the designated primary monitor, any monitor
 * (usually the first) may be returned. To make sure there is a dedicated
 * primary monitor, use gdk_monitor_is_primary() on the returned monitor.
 *
 * Returns: (transfer none): the primary monitor, or any monitor if no
 *     primary monitor is configured by the user
 */
GdkMonitor *
gdk_display_get_primary_monitor (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_primary_monitor (display);
}

/**
 * gdk_display_get_monitor_at_point:
 * @display: a #GdkDisplay
 * @x: the x coordinate of the point
 * @y: the y coordinate of the point
 *
 * Gets the monitor in which the point (@x, @y) is located,
 * or a nearby monitor if the point is not in any monitor.
 *
 * Returns: (transfer none): the monitor containing the point
 */
GdkMonitor *
gdk_display_get_monitor_at_point (GdkDisplay *display,
                                  int         x,
                                  int         y)
{
  GdkMonitor *nearest = NULL;
  int nearest_dist = G_MAXINT;
  int n_monitors, i;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  n_monitors = gdk_display_get_n_monitors (display);
  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GdkRectangle geometry;
      int dist_x, dist_y, dist;

      monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_geometry (monitor, &geometry);

      if (x < geometry.x)
        dist_x = geometry.x - x;
      else if (geometry.x + geometry.width <= x)
        dist_x = x - (geometry.x + geometry.width) + 1;
      else
        dist_x = 0;

      if (y < geometry.y)
        dist_y = geometry.y - y;
      else if (geometry.y + geometry.height <= y)
        dist_y = y - (geometry.y + geometry.height) + 1;
      else
        dist_y = 0;

      dist = dist_x + dist_y;
      if (dist < nearest_dist)
        {
          nearest_dist = dist;
          nearest = monitor;
        }

      if (nearest_dist == 0)
        break;
    }

  return nearest;
}

/**
 * gdk_display_get_monitor_at_surface:
 * @display: a #GdkDisplay
 * @surface: a #GdkSurface
 *
 * Gets the monitor in which the largest area of @surface
 * resides, or a monitor close to @surface if it is outside
 * of all monitors.
 *
 * Returns: (transfer none): the monitor with the largest overlap with @surface
 */
GdkMonitor *
gdk_display_get_monitor_at_surface (GdkDisplay *display,
                                   GdkSurface  *surface)
{
  GdkRectangle win;
  int n_monitors, i;
  int area = 0;
  GdkMonitor *best = NULL;
  GdkDisplayClass *class;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  class = GDK_DISPLAY_GET_CLASS (display);
  if (class->get_monitor_at_surface)
    {
      best = class->get_monitor_at_surface (display, surface);

      if (best)
        return best;
    }

  gdk_surface_get_geometry (surface, &win.x, &win.y, &win.width, &win.height);
  gdk_surface_get_origin (surface, &win.x, &win.y);

  n_monitors = gdk_display_get_n_monitors (display);
  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GdkRectangle mon, intersect;
      int overlap;

      monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_geometry (monitor, &mon);
      gdk_rectangle_intersect (&win, &mon, &intersect);
      overlap = intersect.width *intersect.height;
      if (overlap > area)
        {
          area = overlap;
          best = monitor;
        }
    }

  if (best)
    return best;

  return gdk_display_get_monitor_at_point (display,
                                           win.x + win.width / 2,
                                           win.y + win.height / 2);
}

void
gdk_display_monitor_added (GdkDisplay *display,
                           GdkMonitor *monitor)
{
  g_signal_emit (display, signals[MONITOR_ADDED], 0, monitor);
}

void
gdk_display_monitor_removed (GdkDisplay *display,
                             GdkMonitor *monitor)
{
  g_signal_emit (display, signals[MONITOR_REMOVED], 0, monitor);
  gdk_monitor_invalidate (monitor);
}

void
gdk_display_emit_opened (GdkDisplay *display)
{
  g_signal_emit (display, signals[OPENED], 0);
}

/**
 * gdk_display_get_setting:
 * @display: a #GdkDisplay
 * @name: the name of the setting
 * @value: location to store the value of the setting
 *
 * Retrieves a desktop-wide setting such as double-click time
 * for the @display.
 *
 * Returns: %TRUE if the setting existed and a value was stored
 *   in @value, %FALSE otherwise
 */
gboolean
gdk_display_get_setting (GdkDisplay *display,
                         const char *name,
                         GValue     *value)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->get_setting (display, name, value);
}

void
gdk_display_setting_changed (GdkDisplay       *display,
                             const char       *name)
{
  g_signal_emit (display, signals[SETTING_CHANGED], 0, name);
}

guint32
gdk_display_get_last_seen_time (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), GDK_CURRENT_TIME);

  if (GDK_DISPLAY_GET_CLASS (display)->get_last_seen_time)
    return GDK_DISPLAY_GET_CLASS (display)->get_last_seen_time (display);

  return GDK_CURRENT_TIME;
}

void
gdk_display_set_double_click_time (GdkDisplay *display,
                                   guint       msec)
{
  display->double_click_time = msec;
}

void
gdk_display_set_double_click_distance (GdkDisplay *display,
                                       guint       distance)
{
  display->double_click_distance = distance;
}

void
gdk_display_set_cursor_theme (GdkDisplay *display,
                              const char *name,
                              int         size)
{
  if (GDK_DISPLAY_GET_CLASS (display)->set_cursor_theme)
    GDK_DISPLAY_GET_CLASS (display)->set_cursor_theme (display, name, size);
}
