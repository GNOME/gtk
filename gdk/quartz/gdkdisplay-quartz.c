/* gdkdisplay-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include "config.h"

#include "gdk.h"
#include "gdkdisplay-quartz.h"
#include "gdkprivate-quartz.h"
#include "gdkscreen-quartz.h"
#include "gdkdevicemanager-core.h"


static GdkWindow *
gdk_quartz_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  /* FIXME: Implement */

  return NULL;
}

GdkDeviceManager *
_gdk_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_DEVICE_MANAGER_CORE,
                       "display", display,
                       NULL);
}

GdkDisplay *
_gdk_quartz_display_open (const gchar *display_name)
{
  if (_gdk_display != NULL)
    return NULL;

  /* Initialize application */
  [NSApplication sharedApplication];

  _gdk_display = g_object_new (_gdk_quartz_display_get_type (), NULL);
  _gdk_display->device_manager = _gdk_device_manager_new (_gdk_display);

  _gdk_screen = _gdk_screen_quartz_new ();

  _gdk_quartz_visual_init (_gdk_screen);

  _gdk_windowing_window_init ();

  _gdk_quartz_events_init ();

  _gdk_quartz_input_init ();

#if 0
  /* FIXME: Remove the #if 0 when we have these functions */
  _gdk_quartz_dnd_init ();
#endif

  g_signal_emit_by_name (gdk_display_manager_get (),
			 "display_opened", _gdk_display);

  return _gdk_display;
}

static const gchar *
gdk_quartz_display_get_name (GdkDisplay *display)
{
  static gchar *display_name = NULL;

  if (!display_name)
    {
      GDK_QUARTZ_ALLOC_POOL;
      display_name = g_strdup ([[[NSHost currentHost] name] UTF8String]);
      GDK_QUARTZ_RELEASE_POOL;
    }

  return display_name;
}

static gint
gdk_quartz_display_get_n_screens (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return 1;
}

static GdkScreen *
gdk_quartz_display_get_screen (GdkDisplay *display,
                               gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return _gdk_screen;
}

static GdkScreen *
gdk_quartz_display_get_default_screen (GdkDisplay *display)
{
  return _gdk_screen;
}

static void
gdk_quartz_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  NSBeep();
}

static gboolean
gdk_quartz_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_request_selection_notification (GdkDisplay *display,
                                                   GdkAtom     selection)

{
  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_clipboard_persistence (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_shapes (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_input_shapes (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static void
gdk_quartz_display_store_clipboard (GdkDisplay    *display,
                                    GdkWindow     *clipboard_window,
                                    guint32        time_,
                                    const GdkAtom *targets,
                                    gint           n_targets)
{
  /* FIXME: Implement */
}


static gboolean
gdk_quartz_display_supports_composite (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

gulong
_gdk_quartz_display_get_next_serial (GdkDisplay *display)
{
  return 0;
}

G_DEFINE_TYPE (GdkDisplayQuartz, _gdk_display_quartz, GDK_TYPE_DISPLAY)

static void
_gdk_display_quartz_init (GdkDisplayQuartz *display)
{
  gdk_quartz_display_manager_add_display (gdk_display_nmanager_get (),
                                       GDK_DISPLAY_OBJECT (display));
}

static void
_gdk_display_quartz_dispose (GObject *object)
{
  _gdk_quartz_display_manager_remove_display (gdk_display_manager_get (),
                                              GDK_DISPLAY_OBJECT (object));

  G_OBJECT_CLASS (_gdk_display_quartz_parent_class)->dispose (object);
}

static void
_gdk_display_quartz_finalize (GObject *object)
{
  G_OBJECT_CLASS (_gdk_display_quartz_parent_class)->finalize (object);
}

static void
_gdk_display_quartz_class_init (GdkDisplayQuartz *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->finalize = _gdk_display_quartz_finalize;

  display_class->get_name = gdk_quartz_display_get_name;
  display_class->get_n_screens = gdk_quartz_display_get_n_screens;
  display_class->get_screen = gdk_quartz_display_get_screen;
  display_class->get_default_screen = gdk_quartz_display_get_default_screen;
  display_class->beep = gdk_quartz_display_beep;
  display_class->sync = _gdk_quartz_display_sync;
  display_class->flush = _gdk_quartz_display_flush;
  display_class->queue_events = _gdk_quartz_display_queue_events;
  display_class->has_pending = _gdk_quartz_display_has_pending;
  display_class->get_default_group = gdk_quartz_display_get_default_group;
  display_class->supports_selection_notification = gdk_quartz_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_quartz_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_quartz_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_quartz_display_store_clipboard;
  display_class->supports_shapes = gdk_quartz_display_supports_shapes;
  display_class->supports_input_shapes = gdk_quartz_display_supports_input_shapes;
  display_class->supports_composite = gdk_quartz_display_supports_composite;
  display_class->list_devices = _gdk_quartz_display_list_devices;
  display_class->send_client_message = _gdk_quartz_display_send_client_message;
  display_class->add_client_message_filter = _gdk_quartz_display_add_client_message_filter;
  display_class->get_drag_protocol = _gdk_quartz_display_get_drag_protocol;
  display_class->get_cursor_for_type = _gdk_quartz_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_quartz_display_get_cursor_for_name;
  display_class->get_cursor_for_pixbuf = _gdk_quartz_display_get_cursor_for_pixbuf;
  display_class->get_default_cursor_size = _gdk_quartz_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_quartz_display_get_maximal_cursor_size;
  display_class->supports_cursor_alpha = _gdk_quartz_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_quartz_display_supports_cursor_color;
  display_class->get_next_serial = gdk_quartz_display_get_next_serial;
  display_class->notify_startup_complete = _gdk_quartz_display_notify_startup_complete;
  display_class->event_data_copy = _gdk_quartz_display_event_data_copy;
  display_class->event_data_free = _gdk_quartz_display_event_data_free;

}
