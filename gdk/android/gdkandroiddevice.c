/*
 * Copyright (c) 2024-2025 Florian "sp1rit" <sp1rit@disroot.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>

#include "gdkeventsprivate.h"

#include "gdkandroidinit-private.h"
#include "gdkandroiddisplay-private.h"
#include "gdkandroidutils-private.h"

#include "gdkandroiddevice-private.h"

struct _GdkAndroidDeviceClass
{
  GdkDeviceClass parent_class;
};

/**
 * GdkAndroidDevice:
 *
 * The Android implementation of [class@Gdk.Device].
 *
 * Since: 4.18
 */
G_DEFINE_TYPE (GdkAndroidDevice, gdk_android_device, GDK_TYPE_DEVICE)

static void
gdk_android_device_finalize (GObject *object)
{
  GdkAndroidDevice *self = (GdkAndroidDevice *) object;
  if (self->last)
    g_object_remove_weak_pointer ((GObject *) self->last, (gpointer *) &self->last);
  G_OBJECT_CLASS (gdk_android_device_parent_class)->finalize (object);
}

static void
gdk_android_device_constructed (GObject *object)
{
  G_OBJECT_CLASS (gdk_android_device_parent_class)->constructed (object);
}

static void
gdk_android_device_set_surface_cursor (GdkDevice  *device,
                                       GdkSurface *surface,
                                       GdkCursor  *cursor)
{
  GdkAndroidSurface *surface_impl = GDK_ANDROID_SURFACE (surface);

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 5);

  if (cursor == NULL)
    {
      (*env)->CallVoidMethod (env, surface_impl->surface, gdk_android_get_java_cache ()->surface.drop_cursor_icon);
      goto exit;
    }

  const gchar *name = gdk_cursor_get_name (cursor);
  if (name)
    {
      if (g_hash_table_contains (gdk_android_get_java_cache ()->a_pointericon.gdk_type_mapping, name))
        {
          gint icon = GPOINTER_TO_INT (g_hash_table_lookup (gdk_android_get_java_cache ()->a_pointericon.gdk_type_mapping,
                                                            name));
          (*env)->CallVoidMethod (env, surface_impl->surface, gdk_android_get_java_cache ()->surface.set_cursor_from_id, icon);
        }
      else
        {
          (*env)->CallVoidMethod (env, surface_impl->surface, gdk_android_get_java_cache ()->surface.drop_cursor_icon);
        }
      goto exit;
    }

  GdkTexture *texture = gdk_cursor_get_texture (cursor);
  if (texture)
    {
      gint width = gdk_texture_get_width (texture);
      gint height = gdk_texture_get_height (texture);

      jintArray jbuffer = (*env)->NewIntArray (env,
                                               width * height);

      jint *native_buffer = (*env)->GetIntArrayElements (env, jbuffer, NULL);
      GdkTextureDownloader *downloader = gdk_texture_downloader_new (texture);
      gdk_texture_downloader_set_format (downloader, GDK_MEMORY_DEFAULT);
      gdk_texture_downloader_download_into (downloader, (guchar *) native_buffer, width * sizeof (jint));
      gdk_texture_downloader_free (downloader);
      (*env)->ReleaseIntArrayElements (env, jbuffer, native_buffer, 0);

      jobject bitmap = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_bitmap.klass,
                                                       gdk_android_get_java_cache ()->a_bitmap.create_from_array,
                                                       jbuffer, width, height, gdk_android_get_java_cache ()->a_bitmap.argb8888);
      (*env)->CallVoidMethod (env, surface_impl->surface, gdk_android_get_java_cache ()->surface.set_cursor_from_bitmap,
                              bitmap, gdk_cursor_get_hotspot_x (cursor), gdk_cursor_get_hotspot_y (cursor));
      goto exit;
    }

  gdk_android_device_set_surface_cursor (device, surface, gdk_cursor_get_fallback (cursor));

exit:
  (*env)->PopLocalFrame (env, NULL);
}

static GdkGrabStatus
gdk_android_device_grab (GdkDevice *device, GdkSurface *surface,
                         gboolean owner_events, GdkEventMask event_mask,
                         GdkSurface *confine_to, GdkCursor *cursor,
                         guint32 time_)
{
  /* Should remain empty */
  return GDK_GRAB_SUCCESS;
}

static void
gdk_android_device_ungrab (GdkDevice *device, guint32 time_)
{
  GdkDisplay *display = gdk_device_get_display (device);
  GdkDeviceGrabInfo *grab = _gdk_display_get_last_device_grab (display, device);
  if (grab != NULL)
    grab->serial_end = grab->serial_start;

  g_debug ("Ungrabbing: %s", device->name);
  _gdk_display_device_grab_update (display, device, 0);
}

static GdkSurface *
gdk_android_device_surface_at_position (GdkDevice *device,
                                        double *win_x, double *win_y,
                                        GdkModifierType *mask)
{
  GdkAndroidDevice *self = (GdkAndroidDevice *) device;
  // TODO: in what space are those values supposed to be?
  if (win_x)
    *win_x = self->last_x;
  if (win_y)
    *win_y = self->last_y;
  if (mask)
    *mask = self->last_mods;
  return (GdkSurface *) self->last;
}

static void
gdk_android_device_class_init (GdkAndroidDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  object_class->finalize = gdk_android_device_finalize;
  object_class->constructed = gdk_android_device_constructed;

  device_class->set_surface_cursor = gdk_android_device_set_surface_cursor;
  device_class->grab = gdk_android_device_grab;
  device_class->ungrab = gdk_android_device_ungrab;
  device_class->surface_at_position = gdk_android_device_surface_at_position;
}

static void
gdk_android_device_init (GdkAndroidDevice *self)
{
}

void
gdk_android_device_maybe_update_surface (GdkAndroidDevice  *self,
                                         GdkAndroidSurface *new_surface,
                                         GdkModifierType    new_mods,
                                         guint32            timestamp,
                                         gfloat             x,
                                         gfloat             y)
{
  GdkModifierType old_mods = self->last_mods;
  self->last_x = x;
  self->last_y = y;
  self->last_mods = new_mods;
  if (self->last == new_surface || self->button_state != 0)
    return;
  GdkDisplay *display = gdk_device_get_display ((GdkDevice *) self);
  if (self->last)
    {
      GdkEvent *ev = gdk_crossing_event_new (GDK_LEAVE_NOTIFY, (GdkSurface *) self->last, (GdkDevice *) self,
                                             timestamp, old_mods,
                                             self->last_x, self->last_y,
                                             GDK_CROSSING_NORMAL, GDK_NOTIFY_UNKNOWN);
      gdk_android_seat_consume_event (display, ev);
      g_object_remove_weak_pointer ((GObject *) self->last, (gpointer *) &self->last);
    }
  self->last = new_surface;
  g_object_add_weak_pointer ((GObject *) self->last, (gpointer *) &self->last);
  GdkEvent *ev = gdk_crossing_event_new (GDK_ENTER_NOTIFY, (GdkSurface *) new_surface, (GdkDevice *) self,
                                         timestamp, new_mods,
                                         x, y,
                                         GDK_CROSSING_NORMAL, GDK_NOTIFY_UNKNOWN);
  gdk_android_seat_consume_event (display, ev);
}

void
gdk_android_device_keyboard_maybe_update_surface_focus (GdkAndroidDevice  *self,
                                                        GdkAndroidSurface *new_surface)
{
  g_return_if_fail (((GdkDevice *)self)->source == GDK_SOURCE_KEYBOARD);
  if (self->last == new_surface)
    return;
  GdkDisplay *display = gdk_device_get_display ((GdkDevice *) self);
  if (self->last)
    {
      GdkEvent *ev = gdk_focus_event_new ((GdkSurface *)self->last, (GdkDevice *)self, FALSE);
      gdk_android_seat_consume_event (display, ev);
      g_object_remove_weak_pointer ((GObject *) self->last, (gpointer *) &self->last);
    }
  self->last = new_surface;
  g_object_add_weak_pointer ((GObject *) self->last, (gpointer *) &self->last);
  GdkEvent *ev = gdk_focus_event_new ((GdkSurface *)self->last, (GdkDevice *)self, TRUE);
  gdk_android_seat_consume_event (display, ev);
}
