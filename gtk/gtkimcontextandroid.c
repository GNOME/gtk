/*
 * gtkimcontextandroid.c:
 * Copyright (C) 2024 Florian "sp1rit" <sp1rit@disroot.org>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * $Id:$
 */

#include "config.h"
#include <string.h>

#include "gtk/gtkimcontextandroid.h"
#include "gtk/gtkimmoduleprivate.h"
#include "gtk/gtkprivate.h"

#include "gdk/android/gdkandroid.h"
#include "gdk/android/gdkandroidinit-private.h"
#include "gdk/android/gdkandroidsurface-private.h"

#include "gtk/gtkimcontextsimple.h"

#define GTK_IM_CONTEXT_TYPE_ANDROID (gtk_im_context_android_get_type())
#define GTK_IM_CONTEXT_ANDROID(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_IM_CONTEXT_TYPE_ANDROID, GtkIMContextAndroid))
#define GTK_IM_CONTEXT_ANDROID_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_IM_CONTEXT_TYPE_ANDROID, GtkIMContextAndroidClass))

typedef struct _GtkIMContextAndroidClass
{
  GtkIMContextSimpleClass parent_class;
} GtkIMContextAndroidClass;

typedef struct _GtkIMContextAndroid
{
  GtkIMContextSimple parent;
  GdkSurface *client_surface;
  GtkWidget *client_widget;
  gboolean focused;
} GtkIMContextAndroid;

G_DEFINE_TYPE_WITH_CODE (GtkIMContextAndroid, gtk_im_context_android, GTK_TYPE_IM_CONTEXT_SIMPLE,
                         gtk_im_module_ensure_extension_point ();
                         g_io_extension_point_implement (GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "android", 0))

static void
gdk_im_context_android_update_ime_keyboard (GtkIMContextAndroid *self)
{
  if (!GDK_IS_ANDROID_SURFACE (self->client_surface))
    return;
  GdkAndroidSurface *surface = (GdkAndroidSurface *)self->client_surface;
  if (surface->surface == NULL)
    return;

  JNIEnv *env = gdk_android_display_get_env (gdk_surface_get_display (self->client_surface));
  (*env)->CallVoidMethod (env, surface->surface,
                          gdk_android_get_java_cache ()->surface.set_ime_keyboard_state,
                          self->focused);
}

static void
gtk_im_context_android_set_client_widget (GtkIMContext *context,
                                          GtkWidget    *widget)
{
  GtkIMContextAndroid *self = GTK_IM_CONTEXT_ANDROID (context);

  GTK_DEBUG (MODULES, "gtk_im_context_android_set_client_surface: %p", widget);

  self->client_widget = widget;
  self->client_surface = NULL;

  if (widget != NULL)
    {
      GtkNative *native = gtk_widget_get_native (widget);
      if (native != NULL)
        {
          self->client_surface = gtk_native_get_surface (native);
          gdk_im_context_android_update_ime_keyboard (self);
        }
    }

  if (GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->set_client_widget)
    GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->set_client_widget (context, widget);
}

static void
gtk_im_context_android_focus_in (GtkIMContext *context)
{
  GTK_DEBUG (MODULES, "gtk_im_context_android_focus_in");

  GtkIMContextAndroid *self = GTK_IM_CONTEXT_ANDROID (context);
  self->focused = TRUE;
  gdk_im_context_android_update_ime_keyboard (self);

  if (GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->focus_in)
    GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->focus_in (context);
}

static void
gtk_im_context_android_focus_out (GtkIMContext *context)
{
  GTK_DEBUG (MODULES, "gtk_im_context_android_focus_out");

  GtkIMContextAndroid *self = GTK_IM_CONTEXT_ANDROID (context);
  self->focused = FALSE;
  gdk_im_context_android_update_ime_keyboard (self);

  if (GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->focus_out)
    GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->focus_out (context);
}

static gboolean
gdk_im_context_activate_osk_keyboard (GtkIMContext *context,
                                      GdkEvent     *event)
{
  GtkIMContextAndroid *self = (GtkIMContextAndroid *)context;
  gdk_im_context_android_update_ime_keyboard (self);
  return self->focused;
}

static void
gtk_im_context_android_class_init (GtkIMContextAndroidClass *klass)
{
  GTK_DEBUG (MODULES, "gtk_im_context_android_class_init");
  GtkIMContextClass *context_class = GTK_IM_CONTEXT_CLASS (klass);

  context_class->set_client_widget = gtk_im_context_android_set_client_widget;
  context_class->focus_in = gtk_im_context_android_focus_in;
  context_class->focus_out = gtk_im_context_android_focus_out;
  context_class->activate_osk_with_event = gdk_im_context_activate_osk_keyboard;
}

static void
gtk_im_context_android_init (GtkIMContextAndroid *self)
{
  GTK_DEBUG (MODULES, "gtk_im_context_android_init");

  self->focused = FALSE;
}
