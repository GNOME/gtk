/* GDK - The GIMP Drawing Kit
 * gdkdisplay-x11.c
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

static void   gdk_display_x11_dispose            (GObject            *object);
static void   gdk_display_x11_finalize           (GObject            *object);

G_DEFINE_TYPE (GdkDisplayX11, _gdk_display_x11, GDK_TYPE_DISPLAY)


static void
_gdk_display_x11_class_init (GdkDisplayX11Class * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gdk_display_x11_dispose;
  object_class->finalize = gdk_display_x11_finalize;
}

static void
_gdk_display_x11_init (GdkDisplayX11 *display)
{
}

static void
_gdk_event_init (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11;

  display_x11 = GDK_DISPLAY_X11 (display);
  display_x11->event_source = gdk_event_source_new (display);
}

static void
_gdk_input_init (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GList *list, *l;

  display_x11 = GDK_DISPLAY_X11 (display);
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

      display_x11->input_devices = g_list_prepend (display_x11->input_devices,
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
  display_x11->input_devices = g_list_prepend (display_x11->input_devices,
                                               g_object_ref (display->core_pointer));

  g_list_free (list);
}

GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  const char *sm_client_id;

  display = g_object_new (GDK_TYPE_DISPLAY_X11, NULL);
  display_x11 = GDK_DISPLAY_X11 (display);

  /* initialize the display's screens */
  display_x11->screens = g_new (GdkScreen *, 1);
  display_x11->screens[0] = _gdk_x11_screen_new (display, 0);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  _gdk_screen_x11_events_init (display_x11->screens[0]);

  /*set the default screen */
  display_x11->default_screen = display_x11->screens[0];

  display->device_manager = _gdk_device_manager_new (display);

  _gdk_event_init (display);

  sm_client_id = _gdk_get_sm_client_id ();
  if (sm_client_id)
    _gdk_windowing_display_set_sm_client_id (display, sm_client_id);

  _gdk_input_init (display);
  _gdk_dnd_init (display);

  _gdk_x11_screen_setup (display_x11->screens[0]);

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

  return GDK_DISPLAY_X11 (display)->screens[screen_num];
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_X11 (display)->default_screen;
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
gdk_x11_display_grab (GdkDisplay *display)
{
}

void
gdk_x11_display_ungrab (GdkDisplay *display)
{
}

static void
gdk_display_x11_dispose (GObject *object)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (object);

  g_list_foreach (display_x11->input_devices, (GFunc) g_object_run_dispose, NULL);

  _gdk_screen_close (display_x11->screens[0]);

  if (display_x11->event_source)
    {
      g_source_destroy (display_x11->event_source);
      g_source_unref (display_x11->event_source);
      display_x11->event_source = NULL;
    }

  G_OBJECT_CLASS (_gdk_display_x11_parent_class)->dispose (object);
}

static void
gdk_display_x11_finalize (GObject *object)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (object);

  /* Keymap */
  if (display_x11->keymap)
    g_object_unref (display_x11->keymap);

  _gdk_x11_cursor_display_finalize (GDK_DISPLAY_OBJECT(display_x11));

  /* Atom Hashtable */
  g_hash_table_destroy (display_x11->atom_from_virtual);
  g_hash_table_destroy (display_x11->atom_to_virtual);

  /* input GdkDevice list */
  g_list_foreach (display_x11->input_devices, (GFunc) g_object_unref, NULL);
  g_list_free (display_x11->input_devices);
  /* Free all GdkScreens */
  g_object_unref (display_x11->screens[0]);
  g_free (display_x11->screens);

  G_OBJECT_CLASS (_gdk_display_x11_parent_class)->finalize (object);
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
gdk_x11_display_get_user_time (GdkDisplay *display)
{
  return GDK_DISPLAY_X11 (display)->user_time;
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
 */
gboolean
gdk_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}

GList *
gdk_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_X11 (display)->input_devices;
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
