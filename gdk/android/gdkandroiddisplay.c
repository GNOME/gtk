/*
 * Copyright (c) 2024 Florian "sp1rit" <sp1rit@disroot.org>
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

#include "config.h"

#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"

#include "gdkandroidinit-private.h"
#include "gdkandroidsurface-private.h"
#include "gdkandroidtoplevel-private.h"
#include "gdkandroidpopup-private.h"
#include "gdkandroidcairocontext-private.h"
#include "gdkandroidclipboard-private.h"
#include "gdkandroiddevice-private.h"
#include "gdkandroidkeymap-private.h"
#include "gdkandroidglcontext-private.h"

#include "gdkandroiddisplay-private.h"

#include <epoxy/egl.h>

G_DEFINE_ENUM_TYPE (GdkAndroidDisplayNightMode, gdk_android_display_night_mode,
                    G_DEFINE_ENUM_VALUE (GDK_ANDROID_DISPLAY_NIGHT_UNDEFINED, "undefined"),
                    G_DEFINE_ENUM_VALUE (GDK_ANDROID_DISPLAY_NIGHT_NO, "no"),
                    G_DEFINE_ENUM_VALUE (GDK_ANDROID_DISPLAY_NIGHT_YES, "yes"))

/**
 * GdkAndroidDisplay:
 *
 * The Android implementation of [class@Gdk.Display].
 *
 * In addition to the API provided by [class@Gdk.Display], this subclass
 * provides [method@Gdk.AndroidDisplay.get_env], which allows you to
 * interact with the [ART](https://source.android.com/docs/core/runtime)
 * with [JNI](https://docs.oracle.com/en/java/javase/17/docs/specs/jni).
 *
 * Since: 4.18
 */
G_DEFINE_TYPE (GdkAndroidDisplay, gdk_android_display, GDK_TYPE_DISPLAY)

enum
{
  PROP_NIGHT_MODE = 1,
  N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES] = { 0, };

static void
gdk_android_display_finalize (GObject *object)
{
  GdkAndroidDisplay *self = (GdkAndroidDisplay *) object;
  GdkDisplay *display = (GdkDisplay *) self;

  g_clear_object (&display->clipboard);

  if (g_hash_table_size (self->surfaces))
    g_warning ("Gdk.AndroidDisplay was finalized with active surfaces. This is not supposed to happen!");
  g_hash_table_unref (self->surfaces);
  g_object_unref (self->monitors);
  g_object_unref (self->seat);
  g_object_unref (self->keymap);

  if (g_hash_table_size (self->drags) > 0)
    // This is something that *shouldn't* be happening theoretically, but due
    // bad API design from Android, I could imagine that this could actually
    // occur.
    g_info ("Gdk.AndroidDisplay was finalized with active drags");
  g_hash_table_unref (self->drags);

  g_mutex_clear (&self->surface_lock);

  G_OBJECT_CLASS (gdk_android_display_parent_class)->finalize (object);
}

static void
gdk_android_display_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GdkAndroidDisplay *self = (GdkAndroidDisplay *) object;
  switch (prop_id)
    {
    case PROP_NIGHT_MODE:
      g_value_set_enum (value, gdk_android_display_get_night_mode (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static const char *
gdk_android_display_get_name (GdkDisplay *display)
{
  return "AndroidDisplay";
}

static void
gdk_android_display_beep (GdkDisplay *display)
{
  // TODO: use ToneGenerator
}

static void
gdk_android_display_sync (GdkDisplay *display)
{
  // NOP
}
static void
gdk_android_display_flush (GdkDisplay *display)
{
  // NOP
}

static void
gdk_android_display_queue_events (GdkDisplay *display)
{
  // NOP
}

static GdkGLContext *
gdk_android_display_init_gl (GdkDisplay *display, GError **error)
{
  if (!gdk_display_init_egl (display, EGL_PLATFORM_ANDROID_KHR, EGL_DEFAULT_DISPLAY, TRUE, error))
    {
      g_warning ("GdkAndroidDisplay.init_egl failed");
      return NULL;
    }
  return g_object_new (GDK_TYPE_ANDROID_GL_CONTEXT, "display", display, NULL);
}

static gulong
gdk_android_display_get_next_serial (GdkDisplay *display)
{
  return 0;
}

static void
gdk_android_display_notify_startup_complete (GdkDisplay *display, const char *startup_id)
{
  g_debug ("Android Startup (%s) complete", startup_id);
}

static GdkKeymap *
gdk_android_display_get_keymap (GdkDisplay *display)
{
  GdkAndroidDisplay *self = (GdkAndroidDisplay *) display;
  return self->keymap;
}

static GdkSeat *
gdk_android_display_get_default_seat (GdkDisplay *display)
{
  GdkAndroidDisplay *self = (GdkAndroidDisplay *) display;
  return (GdkSeat *) self->seat;
}

static GListModel *
gdk_android_display_get_monitors (GdkDisplay *display)
{
  GdkAndroidDisplay *self = GDK_ANDROID_DISPLAY (display);
  return G_LIST_MODEL (self->monitors);
}

static gboolean
gdk_android_display_get_setting (GdkDisplay *display,
                                 const char *name,
                                 GValue     *value)
{
  GdkAndroidDisplay *self = (GdkAndroidDisplay *) display;
  if (g_strcmp0 (name, "gtk-application-prefer-dark-theme") == 0)
    {
      g_value_set_boolean (value, self->night_mode == GDK_ANDROID_DISPLAY_NIGHT_YES);
      return TRUE;
    }
  else if (g_strcmp0 (name, "gtk-decoration-layout") == 0)
    {
      g_value_set_string (value, ":");
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
gdk_android_display_class_init (GdkAndroidDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->finalize = gdk_android_display_finalize;
  object_class->get_property = gdk_android_display_get_property;

  display_class->toplevel_type = GDK_TYPE_ANDROID_TOPLEVEL;
  display_class->popup_type = GDK_TYPE_ANDROID_POPUP;
  display_class->cairo_context_type = GDK_TYPE_ANDROID_CAIRO_CONTEXT;
  display_class->vk_context_type = G_TYPE_NONE;
  display_class->vk_extension_name = NULL;

  display_class->get_name = gdk_android_display_get_name;
  display_class->beep = gdk_android_display_beep;
  display_class->sync = gdk_android_display_sync;
  display_class->flush = gdk_android_display_flush;
  display_class->queue_events = gdk_android_display_queue_events;

  display_class->init_gl = gdk_android_display_init_gl;

  display_class->get_next_serial = gdk_android_display_get_next_serial;
  display_class->notify_startup_complete = gdk_android_display_notify_startup_complete;
  display_class->get_keymap = gdk_android_display_get_keymap;

  display_class->get_default_seat = gdk_android_display_get_default_seat;
  display_class->get_monitors = gdk_android_display_get_monitors;

  display_class->get_setting = gdk_android_display_get_setting;

  /**
   * GdkAndroidDisplay:night-mode: (getter get_night_mode)
   *
   * The current night mode state of the Android UI configuration.
   *
   * Since: 4.18
   */
  obj_properties[PROP_NIGHT_MODE] = g_param_spec_enum ("night-mode", NULL, NULL,
                                                       GDK_TYPE_ANDROID_DISPLAY_NIGHT_MODE, GDK_ANDROID_DISPLAY_NIGHT_UNDEFINED,
                                                       G_PARAM_STATIC_STRINGS | G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

static void
gdk_android_display_init (GdkAndroidDisplay *self)
{
  g_mutex_init (&self->surface_lock);

  GdkDisplay *display = (GdkDisplay *) self;
  display->clipboard = gdk_android_clipboard_new (display);

  self->monitors = g_list_store_new (GDK_TYPE_ANDROID_MONITOR);
  self->surfaces = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
  self->seat = gdk_android_seat_new ((GdkDisplay *) self);
  gdk_display_add_seat (display, (GdkSeat *) self->seat);
  self->keymap = g_object_new (GDK_TYPE_ANDROID_KEYMAP, NULL);

  self->drags = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  gdk_display_set_composited (display, TRUE);
  gdk_display_set_input_shapes (display, TRUE);
  gdk_display_set_rgba (display, TRUE);
  gdk_display_set_shadow_width (display, FALSE);
}

static GdkAndroidDisplay *gdk_android_display = NULL;
GdkDisplay *
_gdk_android_display_open (const char *display_name)
{
  if (gdk_android_display != NULL)
    return NULL;

  if (!gdk_android_get_env ())
    {
      g_debug ("Unable to open Android display, as it was unprepared");
      return NULL;
    }

  display_name = display_name ? display_name : "";
  GDK_DEBUG (MISC, "opening display %s", display_name);

  gdk_android_display = g_object_new (GDK_TYPE_ANDROID_DISPLAY, NULL);
  g_object_add_weak_pointer (G_OBJECT (gdk_android_display), (gpointer *) &gdk_android_display);

  gdk_display_emit_opened (GDK_DISPLAY (gdk_android_display));

  return GDK_DISPLAY (gdk_android_display);
}

/**
 * gdk_android_display_get_env: (skip)
 * @self: (transfer none): the display
 *
 * Get the thread-local pointer to the JNI function table that is needed
 * to interact with the Java virtual machine.
 *
 * Returns: pointer to thread-local JNI function table
 *
 * Since: 4.18
 */
JNIEnv *
gdk_android_display_get_env (GdkDisplay *self)
{
  return gdk_android_get_env ();
}

GdkAndroidDisplay *
gdk_android_display_get_display_instance (void)
{
  return gdk_android_display;
}

GdkAndroidSurface *
gdk_android_display_get_surface_from_identifier (GdkAndroidDisplay *self, glong identifier)
{
  g_mutex_lock (&self->surface_lock);
  GdkAndroidSurface *ret = g_hash_table_lookup (self->surfaces, (gpointer) identifier);
  if (ret)
    g_object_ref (ret);
  g_mutex_unlock (&self->surface_lock);
  return ret;
}

void
gdk_android_display_add_surface (GdkAndroidDisplay *self, GdkAndroidSurface *surface)
{
  g_mutex_lock (&self->surface_lock);
  g_hash_table_insert (self->surfaces, surface, g_object_ref (surface));
  g_mutex_unlock (&self->surface_lock);
}

/**
 * gdk_android_display_get_night_mode: (get-property night-mode)
 * @self: (transfer none): the display
 *
 * Get the night mode setting of the Android UI configuration.
 *
 * Returns: current night mode setting
 *
 * Since: 4.18
 */
GdkAndroidDisplayNightMode
gdk_android_display_get_night_mode (GdkAndroidDisplay *self)
{
  g_return_val_if_fail (GDK_IS_ANDROID_DISPLAY (self), GDK_ANDROID_DISPLAY_NIGHT_UNDEFINED);
  return self->night_mode;
}

void
gdk_android_display_update_night_mode (GdkAndroidDisplay *self, jobject context)
{
  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 5);
  jobject resources = (*env)->CallObjectMethod (env, context, gdk_android_get_java_cache ()->a_context.get_resources);
  jobject configuration = (*env)->CallObjectMethod (env, resources, gdk_android_get_java_cache ()->a_resources.get_configuration);
  jint ui = (*env)->GetIntField (env, configuration, gdk_android_get_java_cache ()->a_configuration.ui);
  (*env)->PopLocalFrame (env, NULL);

  GdkAndroidDisplayNightMode night_mode = GDK_ANDROID_DISPLAY_NIGHT_UNDEFINED;
  if (ui & gdk_android_get_java_cache ()->a_configuration.ui_night_yes)
    night_mode = GDK_ANDROID_DISPLAY_NIGHT_YES;
  else if (ui & gdk_android_get_java_cache ()->a_configuration.ui_night_no)
    night_mode = GDK_ANDROID_DISPLAY_NIGHT_NO;

  if (self->night_mode == night_mode)
    return;
  self->night_mode = night_mode;
  g_debug ("night mode changed");
  gdk_display_setting_changed ((GdkDisplay *) self, "gtk-application-prefer-dark-theme");
  g_object_notify_by_pspec ((GObject *) self, obj_properties[PROP_NIGHT_MODE]);
}
