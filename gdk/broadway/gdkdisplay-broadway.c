/* GDK - The GIMP Drawing Kit
 * gdkdisplay-broadway.c
 * 
 * Copyright 2001 Sun Microsystems Inc.
 * Copyright (C) 2004 Nokia Corporation
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

#include "gdkdisplay-broadway.h"

#include "gdkdisplay.h"
#include "gdkeventsource.h"
#include "gdkscreen.h"
#include "gdkscreen-broadway.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager-broadway.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>

static void   gdk_broadway_display_dispose            (GObject            *object);
static void   gdk_broadway_display_finalize           (GObject            *object);

#if 0
#define DEBUG_WEBSOCKETS 1
#endif

G_DEFINE_TYPE (GdkBroadwayDisplay, gdk_broadway_display, GDK_TYPE_DISPLAY)

static void
gdk_broadway_display_init (GdkBroadwayDisplay *display)
{
  display->id_ht = g_hash_table_new (NULL, NULL);
}

static void
gdk_event_init (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (display);
  broadway_display->event_source = _gdk_broadway_event_source_new (display);
}

static void
gdk_broadway_display_init_input (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GList *list, *l;

  broadway_display = GDK_BROADWAY_DISPLAY (display);
  device_manager = gdk_display_get_device_manager (display);

  /* For backwards compatibility, just add
   * floating devices that are not keyboards.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING);

  for (l = list; l; l = l->next)
    {
      device = l->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
        continue;

      broadway_display->input_devices = g_list_prepend (broadway_display->input_devices,
                                                   g_object_ref (l->data));
    }

  g_list_free (list);

  /* Now set "core" pointer to the first
   * master device that is a pointer.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (l = list; l; l = l->next)
    {
      device = l->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
        continue;

      display->core_pointer = device;
      break;
    }

  /* Add the core pointer to the devices list */
  broadway_display->input_devices = g_list_prepend (broadway_display->input_devices,
                                               g_object_ref (display->core_pointer));

  g_list_free (list);
}

GdkDisplay *
_gdk_broadway_display_open (const gchar *display_name)
{
  GdkDisplay *display;
  GdkBroadwayDisplay *broadway_display;
  GError *error = NULL;

  display = g_object_new (GDK_TYPE_BROADWAY_DISPLAY, NULL);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  /* initialize the display's screens */
  broadway_display->screens = g_new (GdkScreen *, 1);
  broadway_display->screens[0] = _gdk_broadway_screen_new (display, 0);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  _gdk_broadway_screen_events_init (broadway_display->screens[0]);

  /*set the default screen */
  broadway_display->default_screen = broadway_display->screens[0];

  display->device_manager = _gdk_broadway_device_manager_new (display);

  gdk_event_init (display);

  gdk_broadway_display_init_input (display);
  _gdk_broadway_display_init_dnd (display);

  _gdk_broadway_screen_setup (broadway_display->screens[0]);

  if (display_name == NULL)
    display_name = g_getenv ("BROADWAY_DISPLAY");

  broadway_display->server = _gdk_broadway_server_new (display_name, &error);
  if (broadway_display->server == NULL)
    {
      g_printerr ("Unable to init server: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  g_signal_emit_by_name (display, "opened");

  return display;
}

static const gchar *
gdk_broadway_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return (gchar *) "Broadway";
}

static GdkScreen *
gdk_broadway_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_BROADWAY_DISPLAY (display)->default_screen;
}

static void
gdk_broadway_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

static void
gdk_broadway_display_sync (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);

  g_return_if_fail (GDK_IS_BROADWAY_DISPLAY (display));

  _gdk_broadway_server_sync (broadway_display->server);
}

static void
gdk_broadway_display_flush (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);

  g_return_if_fail (GDK_IS_BROADWAY_DISPLAY (display));

  _gdk_broadway_server_flush (broadway_display->server);
}

static gboolean
gdk_broadway_display_has_pending (GdkDisplay *display)
{
  return FALSE;
}

static GdkWindow *
gdk_broadway_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}

static void
gdk_broadway_display_dispose (GObject *object)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (object);

  g_list_foreach (broadway_display->input_devices, (GFunc) g_object_run_dispose, NULL);

  _gdk_screen_close (broadway_display->screens[0]);

  if (broadway_display->event_source)
    {
      g_source_destroy (broadway_display->event_source);
      g_source_unref (broadway_display->event_source);
      broadway_display->event_source = NULL;
    }

  G_OBJECT_CLASS (gdk_broadway_display_parent_class)->dispose (object);
}

static void
gdk_broadway_display_finalize (GObject *object)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (object);

  /* Keymap */
  if (broadway_display->keymap)
    g_object_unref (broadway_display->keymap);

  _gdk_broadway_cursor_display_finalize (GDK_DISPLAY_OBJECT(broadway_display));

  /* input GdkDevice list */
  g_list_free_full (broadway_display->input_devices, g_object_unref);
  /* Free all GdkScreens */
  g_object_unref (broadway_display->screens[0]);
  g_free (broadway_display->screens);

  G_OBJECT_CLASS (gdk_broadway_display_parent_class)->finalize (object);
}

static void
gdk_broadway_display_notify_startup_complete (GdkDisplay  *display,
					      const gchar *startup_id)
{
}

static gboolean
gdk_broadway_display_supports_selection_notification (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_broadway_display_request_selection_notification (GdkDisplay *display,
						     GdkAtom     selection)

{
    return FALSE;
}

static gboolean
gdk_broadway_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_broadway_display_store_clipboard (GdkDisplay    *display,
				      GdkWindow     *clipboard_window,
				      guint32        time_,
				      const GdkAtom *targets,
				      gint           n_targets)
{
}

static gboolean
gdk_broadway_display_supports_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_broadway_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_broadway_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}

static GList *
gdk_broadway_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_BROADWAY_DISPLAY (display)->input_devices;
}

static gulong
gdk_broadway_display_get_next_serial (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  return _gdk_broadway_server_get_next_serial (broadway_display->server);
}

void
gdk_broadway_display_show_keyboard (GdkBroadwayDisplay *display)
{
  g_return_if_fail (GDK_IS_BROADWAY_DISPLAY (display));

  _gdk_broadway_server_set_show_keyboard (display->server, TRUE);
}

void
gdk_broadway_display_hide_keyboard (GdkBroadwayDisplay *display)
{
  g_return_if_fail (GDK_IS_BROADWAY_DISPLAY (display));

  _gdk_broadway_server_set_show_keyboard (display->server, FALSE);
}

static void
gdk_broadway_display_class_init (GdkBroadwayDisplayClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_broadway_display_dispose;
  object_class->finalize = gdk_broadway_display_finalize;

  display_class->window_type = GDK_TYPE_BROADWAY_WINDOW;

  display_class->get_name = gdk_broadway_display_get_name;
  display_class->get_default_screen = gdk_broadway_display_get_default_screen;
  display_class->beep = gdk_broadway_display_beep;
  display_class->sync = gdk_broadway_display_sync;
  display_class->flush = gdk_broadway_display_flush;
  display_class->has_pending = gdk_broadway_display_has_pending;
  display_class->queue_events = _gdk_broadway_display_queue_events;
  display_class->get_default_group = gdk_broadway_display_get_default_group;
  display_class->supports_selection_notification = gdk_broadway_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_broadway_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_broadway_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_broadway_display_store_clipboard;
  display_class->supports_shapes = gdk_broadway_display_supports_shapes;
  display_class->supports_input_shapes = gdk_broadway_display_supports_input_shapes;
  display_class->supports_composite = gdk_broadway_display_supports_composite;
  display_class->list_devices = gdk_broadway_display_list_devices;
  display_class->get_cursor_for_type = _gdk_broadway_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_broadway_display_get_cursor_for_name;
  display_class->get_cursor_for_surface = _gdk_broadway_display_get_cursor_for_surface;
  display_class->get_default_cursor_size = _gdk_broadway_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_broadway_display_get_maximal_cursor_size;
  display_class->supports_cursor_alpha = _gdk_broadway_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_broadway_display_supports_cursor_color;

  display_class->before_process_all_updates = _gdk_broadway_display_before_process_all_updates;
  display_class->after_process_all_updates = _gdk_broadway_display_after_process_all_updates;
  display_class->get_next_serial = gdk_broadway_display_get_next_serial;
  display_class->notify_startup_complete = gdk_broadway_display_notify_startup_complete;
  display_class->create_window_impl = _gdk_broadway_display_create_window_impl;
  display_class->get_keymap = _gdk_broadway_display_get_keymap;
  display_class->get_selection_owner = _gdk_broadway_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_broadway_display_set_selection_owner;
  display_class->send_selection_notify = _gdk_broadway_display_send_selection_notify;
  display_class->get_selection_property = _gdk_broadway_display_get_selection_property;
  display_class->convert_selection = _gdk_broadway_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_broadway_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_broadway_display_utf8_to_string_target;
}

