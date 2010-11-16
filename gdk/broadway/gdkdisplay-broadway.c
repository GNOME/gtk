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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkdisplay-broadway.h"

#include "gdkdisplay.h"
#include "gdkeventsource.h"
#include "gdkscreen.h"
#include "gdkscreen-broadway.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void   gdk_display_broadway_dispose            (GObject            *object);
static void   gdk_display_broadway_finalize           (GObject            *object);

G_DEFINE_TYPE (GdkDisplayBroadway, _gdk_display_broadway, GDK_TYPE_DISPLAY)


static void
_gdk_display_broadway_class_init (GdkDisplayBroadwayClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gdk_display_broadway_dispose;
  object_class->finalize = gdk_display_broadway_finalize;
}

static void
_gdk_display_broadway_init (GdkDisplayBroadway *display)
{
}

static void
_gdk_event_init (GdkDisplay *display)
{
  GdkDisplayBroadway *display_broadway;

  display_broadway = GDK_DISPLAY_BROADWAY (display);
  display_broadway->event_source = gdk_event_source_new (display);
}

static void
_gdk_input_init (GdkDisplay *display)
{
  GdkDisplayBroadway *display_broadway;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GList *list, *l;

  display_broadway = GDK_DISPLAY_BROADWAY (display);
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

      display_broadway->input_devices = g_list_prepend (display_broadway->input_devices,
                                                   g_object_ref (l->data));
    }

  g_list_free (list);

  /* Now set "core" pointer to the first
   * master device that is a pointer.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (l = list; l; l = l->next)
    {
      device = list->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
        continue;

      display->core_pointer = device;
      break;
    }

  /* Add the core pointer to the devices list */
  display_broadway->input_devices = g_list_prepend (display_broadway->input_devices,
                                               g_object_ref (display->core_pointer));

  g_list_free (list);
}

GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  GdkDisplay *display;
  GdkDisplayBroadway *display_broadway;
  const char *sm_client_id;

  display = g_object_new (GDK_TYPE_DISPLAY_BROADWAY, NULL);
  display_broadway = GDK_DISPLAY_BROADWAY (display);

  /* initialize the display's screens */
  display_broadway->screens = g_new (GdkScreen *, 1);
  display_broadway->screens[0] = _gdk_broadway_screen_new (display, 0);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  _gdk_screen_broadway_events_init (display_broadway->screens[0]);

  /*set the default screen */
  display_broadway->default_screen = display_broadway->screens[0];

  display->device_manager = _gdk_device_manager_new (display);

  _gdk_event_init (display);

  sm_client_id = _gdk_get_sm_client_id ();
  if (sm_client_id)
    _gdk_windowing_display_set_sm_client_id (display, sm_client_id);

  _gdk_input_init (display);
  _gdk_dnd_init (display);

  _gdk_broadway_screen_setup (display_broadway->screens[0]);

  g_signal_emit_by_name (display, "opened");
  g_signal_emit_by_name (gdk_display_manager_get (), "display-opened", display);

  return display;
}


G_CONST_RETURN gchar *
gdk_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return (gchar *) "Broadway";
}

gint
gdk_display_get_n_screens (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return 1;
}

GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return GDK_DISPLAY_BROADWAY (display)->screens[screen_num];
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_BROADWAY (display)->default_screen;
}

void
gdk_device_ungrab (GdkDevice  *device,
                   guint32     time_)
{
}

void
gdk_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

void
gdk_display_sync (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

}

void
gdk_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

}

GdkWindow *
gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}

void
gdk_broadway_display_grab (GdkDisplay *display)
{
}

void
gdk_broadway_display_ungrab (GdkDisplay *display)
{
}

static void
gdk_display_broadway_dispose (GObject *object)
{
  GdkDisplayBroadway *display_broadway = GDK_DISPLAY_BROADWAY (object);

  g_list_foreach (display_broadway->input_devices, (GFunc) g_object_run_dispose, NULL);

  _gdk_screen_close (display_broadway->screens[0]);

  if (display_broadway->event_source)
    {
      g_source_destroy (display_broadway->event_source);
      g_source_unref (display_broadway->event_source);
      display_broadway->event_source = NULL;
    }

  G_OBJECT_CLASS (_gdk_display_broadway_parent_class)->dispose (object);
}

static void
gdk_display_broadway_finalize (GObject *object)
{
  GdkDisplayBroadway *display_broadway = GDK_DISPLAY_BROADWAY (object);

  /* Keymap */
  if (display_broadway->keymap)
    g_object_unref (display_broadway->keymap);

  _gdk_broadway_cursor_display_finalize (GDK_DISPLAY_OBJECT(display_broadway));

  /* Atom Hashtable */
  g_hash_table_destroy (display_broadway->atom_from_virtual);
  g_hash_table_destroy (display_broadway->atom_to_virtual);

  /* input GdkDevice list */
  g_list_foreach (display_broadway->input_devices, (GFunc) g_object_unref, NULL);
  g_list_free (display_broadway->input_devices);
  /* Free all GdkScreens */
  g_object_unref (display_broadway->screens[0]);
  g_free (display_broadway->screens);

  G_OBJECT_CLASS (_gdk_display_broadway_parent_class)->finalize (object);
}

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
}

void
gdk_notify_startup_complete (void)
{
}

void
gdk_notify_startup_complete_with_id (const gchar* startup_id)
{
}

gboolean
gdk_display_supports_selection_notification (GdkDisplay *display)
{
  return FALSE;
}

gboolean
gdk_display_request_selection_notification (GdkDisplay *display,
					    GdkAtom     selection)

{
    return FALSE;
}

gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

void
gdk_display_store_clipboard (GdkDisplay    *display,
			     GdkWindow     *clipboard_window,
			     guint32        time_,
			     const GdkAtom *targets,
			     gint           n_targets)
{
}

guint32
gdk_broadway_display_get_user_time (GdkDisplay *display)
{
  return GDK_DISPLAY_BROADWAY (display)->user_time;
}

gboolean
gdk_display_supports_shapes (GdkDisplay *display)
{
  return FALSE;
}

gboolean
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}

gboolean
gdk_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}

GList *
gdk_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_BROADWAY (display)->input_devices;
}

gboolean
gdk_event_send_client_message_for_display (GdkDisplay     *display,
					   GdkEvent       *event,
					   GdkNativeWindow winid)
{
  return FALSE;
}

void
gdk_display_add_client_message_filter (GdkDisplay   *display,
				       GdkAtom       message_type,
				       GdkFilterFunc func,
				       gpointer      data)
{
}

void
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
}

void
gdk_flush (void)
{
  GSList *tmp_list = _gdk_displays;

  while (tmp_list)
    {
      gdk_display_flush (GDK_DISPLAY_OBJECT (tmp_list->data));
      tmp_list = tmp_list->next;
    }
}

gulong
_gdk_windowing_window_get_next_serial (GdkDisplay *display)
{
  return 0;
}
