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

static gboolean
compare_surface (cairo_surface_t *a,
                 cairo_surface_t *b,
                 int width,
                 int height)
{
  unsigned char *data_a, *data_b;
  int stride_a, stride_b;
  int y;

  data_a = cairo_image_surface_get_data (a);
  stride_a = cairo_image_surface_get_stride (a);

  data_b = cairo_image_surface_get_data (b);
  stride_b = cairo_image_surface_get_stride (b);

  for (y = 0; y < height; y++)
    {
      if (memcmp (data_a, data_b, 4 * width) != 0)
        return FALSE;
      data_a += stride_a;
      data_b += stride_b;
    }

  return TRUE;
}

static gboolean
gdk_texture_equal (GdkTexture *a,
                   GdkTexture *b)
{
  cairo_surface_t *surface_a;
  cairo_surface_t *surface_b;
  gboolean res;

  if (a == b)
    return TRUE;

  if (a->width != b->width ||
      a->height != b->height)
    return FALSE;

  surface_a = gdk_texture_download_surface (a);
  surface_b = gdk_texture_download_surface (b);

  res = compare_surface (surface_a, surface_b, a->width, a->height);

  cairo_surface_destroy (surface_a);
  cairo_surface_destroy (surface_b);

  return res;
}

static guint
gdk_texture_hash (GdkTexture *self)
{
  cairo_surface_t *surface;
  unsigned char *data;
  int stride;
  guint32 *row;
  guint64 sum;
  int x, y, width, height;
  guint h;

  surface = gdk_texture_download_surface (self);
  data = cairo_image_surface_get_data (surface);
  stride = cairo_image_surface_get_stride (surface);

  width = MAX (self->width, 4);
  height = MAX (self->height, 4);

  sum = 0;
  for (y = 0; y < height; y++, data += stride)
    {
      row = (guint32 *)data;
      for (x = 0; x < width; x++)
        sum += row[x];
    }

  cairo_surface_destroy (surface);

  h = sum / (width * height);

  return h ^ self->width ^ (self->height << 16);
}

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
  display->texture_cache = g_hash_table_new ((GHashFunc)gdk_texture_hash, (GEqualFunc)gdk_texture_equal);
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
      GdkWindowImplBroadway *toplevel_impl = l->data;

      if (toplevel_impl->maximized)
        gdk_window_move_resize (toplevel_impl->wrapper, 0, 0, msg->width, msg->height);
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
  GdkTexture *in_cache;
  GList *textures;
} BroadwayTextureData;

static void
broadway_texture_data_notify (BroadwayTextureData *data,
			      GdkTexture *disposed_texture)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (data->display);

  if (data->in_cache == disposed_texture)
    {
      g_hash_table_remove (broadway_display->texture_cache, disposed_texture);
      data->in_cache = NULL;
    }

  g_object_set_data (G_OBJECT (disposed_texture), "broadway-data", NULL);

  data->textures = g_list_remove (data->textures, disposed_texture);
  if (data->textures == NULL)
    {
      gdk_broadway_server_release_texture (broadway_display->server, data->id);
      g_object_unref (data->display);
      g_free (data);
    }
  else if (data->in_cache == NULL)
    {
      GdkTexture *first = data->textures->data;

      data->in_cache = first;
      g_hash_table_replace (broadway_display->texture_cache,
			    data->in_cache, data->in_cache);
    }
}

guint32
gdk_broadway_display_ensure_texture (GdkDisplay *display,
                                     GdkTexture *texture)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);
  BroadwayTextureData *data;
  guint32 id;
  GdkTexture *cached;

  data = g_object_get_data (G_OBJECT (texture), "broadway-data");
  if (data != NULL)
    return data->id;

  cached = g_hash_table_lookup (broadway_display->texture_cache, texture);
  if (cached)
    data = g_object_get_data (G_OBJECT (cached), "broadway-data");

  if (data == NULL)
    {
      id = gdk_broadway_server_upload_texture (broadway_display->server, texture);

      data = g_new0 (BroadwayTextureData, 1);
      data->id = id;
      data->display = g_object_ref (display);

      data->in_cache = texture;
      g_hash_table_replace (broadway_display->texture_cache,
			    data->in_cache, data->in_cache);
    }

  data->textures = g_list_prepend (data->textures, texture);

  g_object_weak_ref (G_OBJECT (texture), (GWeakNotify)broadway_texture_data_notify, data);
  g_object_set_data (G_OBJECT (texture), "broadway-data", data);

  return data->id;
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
  display_class->get_default_cursor_size = _gdk_broadway_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_broadway_display_get_maximal_cursor_size;
  display_class->supports_cursor_alpha = _gdk_broadway_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_broadway_display_supports_cursor_color;

  display_class->get_next_serial = gdk_broadway_display_get_next_serial;
  display_class->notify_startup_complete = gdk_broadway_display_notify_startup_complete;
  display_class->create_window_impl = _gdk_broadway_display_create_window_impl;
  display_class->get_keymap = _gdk_broadway_display_get_keymap;
  display_class->get_selection_owner = _gdk_broadway_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_broadway_display_set_selection_owner;
  display_class->send_selection_notify = _gdk_broadway_display_send_selection_notify;
  display_class->get_selection_property = _gdk_broadway_display_get_selection_property;
  display_class->clear_selection_targets = gdk_broadway_display_clear_selection_targets;
  display_class->add_selection_targets = gdk_broadway_display_add_selection_targets;
  display_class->convert_selection = _gdk_broadway_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_broadway_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_broadway_display_utf8_to_string_target;

  display_class->get_n_monitors = gdk_broadway_display_get_n_monitors;
  display_class->get_monitor = gdk_broadway_display_get_monitor;
  display_class->get_primary_monitor = gdk_broadway_display_get_primary_monitor;
  display_class->get_setting = gdk_broadway_display_get_setting;
  display_class->get_last_seen_time = gdk_broadway_display_get_last_seen_time;
}
