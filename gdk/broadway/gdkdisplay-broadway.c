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

#include "gdkcairocontext-broadway.h"
#include "gdkdisplay.h"
#include "gdkeventsource.h"
#include "gdkmonitor-broadway.h"
#include "gdkseatdefaultprivate.h"
#include "gdkdevice-broadway.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include <gdk/gdktextureprivate.h>

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

  display->monitor = g_object_new (GDK_TYPE_BROADWAY_MONITOR,
                                   "display", display,
                                   NULL);
  gdk_monitor_set_manufacturer (display->monitor, "browser");
  gdk_monitor_set_model (display->monitor, "0");
}

static void
gdk_event_init (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (display);
  broadway_display->event_source = _gdk_broadway_event_source_new (display);
}

void
_gdk_broadway_display_size_changed (GdkDisplay                      *display,
                                    BroadwayInputScreenResizeNotify *msg)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);
  GdkMonitor *monitor;
  GdkRectangle size;
  GList *toplevels, *l;

  monitor = broadway_display->monitor;
  gdk_monitor_get_geometry (monitor, &size);

  if (msg->width == size.width && msg->height == size.height)
    return;

  gdk_monitor_set_size (monitor, msg->width, msg->height);
  gdk_monitor_set_physical_size (monitor, msg->width * 25.4 / 96, msg->height * 25.4 / 96);

  toplevels =  broadway_display->toplevels;
  for (l = toplevels; l != NULL; l = l->next)
    {
      GdkBroadwaySurface *toplevel = l->data;

      if (toplevel->maximized)
        gdk_broadway_surface_move_resize (GDK_SURFACE (toplevel),
                                          0, 0,
                                          msg->width, msg->height);
    }
}

static GdkDevice *
create_core_pointer (GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_BROADWAY_DEVICE,
                       "name", "Core Pointer",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_MOUSE,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", TRUE,
                       "display", display,
                       NULL);
}

static GdkDevice *
create_core_keyboard (GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_BROADWAY_DEVICE,
                       "name", "Core Keyboard",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       NULL);
}

static GdkDevice *
create_touchscreen (GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_BROADWAY_DEVICE,
                       "name", "Touchscreen",
                       "type", GDK_DEVICE_TYPE_SLAVE,
                       "input-source", GDK_SOURCE_TOUCHSCREEN,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       NULL);
}

GdkDisplay *
_gdk_broadway_display_open (const gchar *display_name)
{
  GdkDisplay *display;
  GdkBroadwayDisplay *broadway_display;
  GError *error = NULL;
  GdkSeat *seat;

  display = g_object_new (GDK_TYPE_BROADWAY_DISPLAY, NULL);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  broadway_display->core_pointer = create_core_pointer (display);
  broadway_display->core_keyboard = create_core_keyboard (display);
  broadway_display->touchscreen = create_touchscreen (display);

  _gdk_device_set_associated_device (broadway_display->core_pointer, broadway_display->core_keyboard);
  _gdk_device_set_associated_device (broadway_display->core_keyboard, broadway_display->core_pointer);
  _gdk_device_set_associated_device (broadway_display->touchscreen, broadway_display->core_pointer);
  _gdk_device_add_slave (broadway_display->core_pointer, broadway_display->touchscreen);

  seat = gdk_seat_default_new_for_master_pair (broadway_display->core_pointer,
                                               broadway_display->core_keyboard);
  gdk_display_add_seat (display, seat);
  gdk_seat_default_add_slave (GDK_SEAT_DEFAULT (seat), broadway_display->touchscreen);
  g_object_unref (seat);

  gdk_event_init (display);

  _gdk_broadway_display_init_dnd (display);

  if (display_name == NULL)
    display_name = g_getenv ("BROADWAY_DISPLAY");

  broadway_display->server = _gdk_broadway_server_new (display, display_name, &error);
  if (broadway_display->server == NULL)
    {
      g_printerr ("Unable to init Broadway server: %s\n", error->message);
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

static GdkSurface *
gdk_broadway_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}

static void
gdk_broadway_display_dispose (GObject *object)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (object);

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

  _gdk_broadway_cursor_display_finalize (GDK_DISPLAY(broadway_display));

  g_object_unref (broadway_display->monitor);

  G_OBJECT_CLASS (gdk_broadway_display_parent_class)->finalize (object);
}

static void
gdk_broadway_display_notify_startup_complete (GdkDisplay  *display,
					      const gchar *startup_id)
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

static int
gdk_broadway_display_get_n_monitors (GdkDisplay *display)
{
  return 1;
}

static GdkMonitor *
gdk_broadway_display_get_monitor (GdkDisplay *display,
                                  int         monitor_num)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);

  if (monitor_num == 0)
    return broadway_display->monitor;

  return NULL;
}

static GdkMonitor *
gdk_broadway_display_get_primary_monitor (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);

  return broadway_display->monitor;
}

static gboolean
gdk_broadway_display_get_setting (GdkDisplay *display,
                                  const char *name,
                                  GValue     *value)
{
  return FALSE;
}

static guint32
gdk_broadway_display_get_last_seen_time (GdkDisplay *display)
{
  return _gdk_broadway_server_get_last_seen_time (GDK_BROADWAY_DISPLAY (display)->server);
}

typedef struct {
  int id;
  GdkDisplay *display;
  GList *textures;
} BroadwayTextureData;

static void
broadway_texture_data_free (BroadwayTextureData *data)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (data->display);

  gdk_broadway_server_release_texture (broadway_display->server, data->id);
  g_object_unref (data->display);
  g_free (data);
}

guint32
gdk_broadway_display_ensure_texture (GdkDisplay *display,
                                     GdkTexture *texture)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);
  BroadwayTextureData *data;

  data = g_object_get_data (G_OBJECT (texture), "broadway-data");
  if (data == NULL)
    {
      guint32 id = gdk_broadway_server_upload_texture (broadway_display->server, texture);

      data = g_new0 (BroadwayTextureData, 1);
      data->id = id;
      data->display = g_object_ref (display);
     g_object_set_data_full (G_OBJECT (texture), "broadway-data", data, (GDestroyNotify)broadway_texture_data_free);
    }

  return data->id;
}

static gboolean
flush_idle (gpointer data)
{
  GdkDisplay *display = data;
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);

  broadway_display->idle_flush_id = 0;
  gdk_display_flush (display);

  return FALSE;
}

void
gdk_broadway_display_flush_in_idle (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);

  if (broadway_display->idle_flush_id == 0)
    {
      broadway_display->idle_flush_id = g_idle_add (flush_idle, g_object_ref (display));
      g_source_set_name_by_id (broadway_display->idle_flush_id, "[gtk] flush_idle");
    }
}


static void
gdk_broadway_display_class_init (GdkBroadwayDisplayClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_broadway_display_dispose;
  object_class->finalize = gdk_broadway_display_finalize;

  display_class->cairo_context_type = GDK_TYPE_BROADWAY_CAIRO_CONTEXT;

  display_class->get_name = gdk_broadway_display_get_name;
  display_class->beep = gdk_broadway_display_beep;
  display_class->sync = gdk_broadway_display_sync;
  display_class->flush = gdk_broadway_display_flush;
  display_class->has_pending = gdk_broadway_display_has_pending;
  display_class->queue_events = _gdk_broadway_display_queue_events;
  display_class->get_default_group = gdk_broadway_display_get_default_group;
  display_class->supports_shapes = gdk_broadway_display_supports_shapes;
  display_class->supports_input_shapes = gdk_broadway_display_supports_input_shapes;

  display_class->get_next_serial = gdk_broadway_display_get_next_serial;
  display_class->notify_startup_complete = gdk_broadway_display_notify_startup_complete;
  display_class->create_surface = _gdk_broadway_display_create_surface;
  display_class->get_keymap = _gdk_broadway_display_get_keymap;
  display_class->text_property_to_utf8_list = _gdk_broadway_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_broadway_display_utf8_to_string_target;

  display_class->get_n_monitors = gdk_broadway_display_get_n_monitors;
  display_class->get_monitor = gdk_broadway_display_get_monitor;
  display_class->get_primary_monitor = gdk_broadway_display_get_primary_monitor;
  display_class->get_setting = gdk_broadway_display_get_setting;
  display_class->get_last_seen_time = gdk_broadway_display_get_last_seen_time;
}
