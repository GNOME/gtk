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

#include <math.h>
#include <cairo.h>
#include <android/bitmap.h>
#include <android/native_window.h>

#include "gdkmemoryformatprivate.h"

#include "gdkandroidinit-private.h"
#include "gdkandroidsurface-private.h"
#include "gdkandroiddnd-private.h"

#include "gdkandroidcairocontext-private.h"

struct _GdkAndroidCairoContext
{
  GdkCairoContext parent_instance;

  // only valid between start and end frame
  union {
    struct {
      ANativeWindow *window;
      ARect bounds;
      ANativeWindow_Buffer buffer;
    } surface;
    struct {
      jintArray buffer;
    } drag;
  };
};

struct _GdkAndroidCairoContextClass
{
  GdkCairoContextClass parent_class;
};

G_DEFINE_TYPE (GdkAndroidCairoContext, gdk_android_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static void
gdk_android_cairo_context_begin_frame (GdkDrawContext      *draw_context,
                                       GdkDrawContextFrame *frame,
                                       gpointer             context_data,
                                       GdkColorState      **out_color_state,
                                       GdkMemoryDepth      *out_depth)
{
  GdkAndroidCairoContext *self = (GdkAndroidCairoContext *)draw_context;
  GdkCairoContextFrame *cframe = (GdkCairoContextFrame *) frame;
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 2);

  if (GDK_IS_ANDROID_SURFACE (surface))
    {
      GdkAndroidSurface *surface_impl = (GdkAndroidSurface *)surface;

      g_mutex_lock (&surface_impl->native_lock);

      if (surface_impl->native == NULL)
        {
          g_critical ("Native surface not available for current frame");
          g_mutex_unlock (&surface_impl->native_lock);
          gdk_cairo_context_frame_set_surface (cframe, cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0));
          goto cleanup;
        }

      self->surface.window = surface_impl->native;
      ANativeWindow_acquire (self->surface.window);

      cairo_rectangle_int_t bounds;
      cairo_region_get_extents (gdk_draw_context_frame_get_damage (frame), &bounds);

      self->surface.bounds = (ARect){
        .left = bounds.x,
        .top = bounds.y,
        .right = bounds.x + bounds.width,
        .bottom = bounds.y + bounds.height,
      };
      gint rc = ANativeWindow_lock (self->surface.window,
                                    &self->surface.buffer,
                                    &self->surface.bounds);
      if (rc < 0)
        {
          g_info ("failed to gain surface lock: %d", rc);
          g_clear_pointer (&self->surface.window, ANativeWindow_release);
          g_mutex_unlock (&surface_impl->native_lock);
          gdk_cairo_context_frame_set_surface (cframe, cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0));
          goto cleanup;
        }

      g_assert (self->surface.buffer.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);

      cairo_rectangle_int_t true_bounds = {
        .x = self->surface.bounds.left,
        .y = self->surface.bounds.top,
        .width = self->surface.bounds.right - self->surface.bounds.left,
        .height = self->surface.bounds.bottom - self->surface.bounds.top,
      };
      cairo_region_t *region = cairo_region_create_rectangle (&true_bounds);
      cairo_surface_t *cairo_surface;
      gdk_draw_context_frame_add_damage (frame, region);
      cairo_region_destroy (region);

      cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                  self->surface.bounds.right - self->surface.bounds.left,
                                                  self->surface.bounds.bottom - self->surface.bounds.top);
      cairo_surface_set_device_offset (cairo_surface,
                                       -self->surface.bounds.left,
                                       -self->surface.bounds.top);
      gdk_cairo_context_frame_set_surface (cframe, cairo_surface);
    }
  else if (GDK_IS_ANDROID_DRAG_SURFACE (surface))
    {
      GdkAndroidDragSurface *surface_impl = (GdkAndroidDragSurface *)surface;
      GdkAndroidSurface *initiator = (GdkAndroidSurface *)gdk_drag_get_surface ((GdkDrag *)surface_impl->drag);
      cairo_surface_t *cairo_surface;

      gint scaled_width = ceilf(surface->width * initiator->cfg.scale);
      gint scaled_height = ceilf(surface->height * initiator->cfg.scale);

      jintArray buffer = (*env)->NewIntArray(env, scaled_width * scaled_height);
      self->drag.buffer = (*env)->NewGlobalRef(env, buffer);
      jint* native_buffer = (*env)->GetIntArrayElements(env, self->drag.buffer, NULL);
      cairo_surface = cairo_image_surface_create_for_data((guchar *)native_buffer,
                                                          CAIRO_FORMAT_ARGB32,
                                                          scaled_width, scaled_height,
                                                          scaled_width*sizeof(jint));
      gdk_cairo_context_frame_set_surface (cframe, cairo_surface);

      cairo_rectangle_int_t bounds = {
        .x = 0,
        .y = 0,
        .width = scaled_width,
        .height = scaled_height,
      };
      cairo_region_t *region = cairo_region_create_rectangle (&bounds);
      gdk_draw_context_frame_add_damage (frame, region);
      cairo_region_destroy (region);
    }

cleanup:
  (*env)->PopLocalFrame (env, NULL);

  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
gdk_android_cairo_context_end_frame (GdkDrawContext      *draw_context,
                                     GdkDrawContextFrame *frame,
                                     gpointer             context_data)
{
  GdkAndroidCairoContext *self = (GdkAndroidCairoContext *)draw_context;
  GdkCairoContextFrame *cframe = (GdkCairoContextFrame *) frame;
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);

  if (GDK_IS_ANDROID_SURFACE (surface))
    {
      GdkAndroidSurface *surface_impl = (GdkAndroidSurface *)surface;

      if (self->surface.window)
        {
          // for self->buffer.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM
          const guint bpp = 4;
          cairo_surface_t *cairo_surface;

          cairo_surface = gdk_cairo_context_frame_get_surface (cframe);
          cairo_surface_flush (cairo_surface);
          gint width = cairo_image_surface_get_width (cairo_surface);
          gint height = cairo_image_surface_get_height (cairo_surface);
          if (width > 0 && height > 0)
            gdk_memory_convert (&((guchar *) self->surface.buffer.bits)[(self->surface.buffer.stride * self->surface.bounds.top + self->surface.bounds.left) * bpp],
                                // TODO: figure out if the android buffer actually is PREMULTIPLIED or not
                                &GDK_MEMORY_LAYOUT_SIMPLE (GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, width, height, self->surface.buffer.stride * bpp),
                                GDK_COLOR_STATE_SRGB,
                                cairo_image_surface_get_data (cairo_surface),
                                &GDK_MEMORY_LAYOUT_SIMPLE (GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, width, height, cairo_image_surface_get_stride (cairo_surface)),
                                GDK_COLOR_STATE_SRGB);

          ANativeWindow_unlockAndPost (self->surface.window);
          //g_debug("Andoroid.CairoContext (%s): pushed frame", G_OBJECT_TYPE_NAME (surface));
          g_clear_pointer (&self->surface.window, ANativeWindow_release);

          g_mutex_unlock (&surface_impl->native_lock);
        }

      gdk_cairo_context_frame_set_surface (cframe, NULL);
    }
  else if (GDK_IS_ANDROID_DRAG_SURFACE (surface))
    {
      GdkAndroidDragSurface *surface_impl = (GdkAndroidDragSurface *)surface;
      GdkAndroidSurface *initiator = (GdkAndroidSurface *)gdk_drag_get_surface ((GdkDrag *)surface_impl->drag);
      cairo_surface_t *cairo_surface;

      JNIEnv *env = gdk_android_get_env();
      (*env)->PushLocalFrame(env, 4);

      cairo_surface = gdk_cairo_context_frame_get_surface (cframe);
      cairo_surface_flush (cairo_surface);
      jint* native_buffer = (jint *)cairo_image_surface_get_data (cairo_surface);
      (*env)->ReleaseIntArrayElements(env, self->drag.buffer, native_buffer, 0);

      g_info("New DragShadow: actual %dx%d", cairo_image_surface_get_width (cairo_surface), cairo_image_surface_get_height (cairo_surface));

      jobject bitmap = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_bitmap.klass,
                                                       gdk_android_get_java_cache ()->a_bitmap.create_from_array,
                                                       self->drag.buffer,
                                                       cairo_image_surface_get_width (cairo_surface),
                                                       cairo_image_surface_get_height (cairo_surface),
                                                       gdk_android_get_java_cache ()->a_bitmap.argb8888);
      jobject shadow = (*env)->NewObject (env,
                                          gdk_android_get_java_cache ()->clipboard_bitmap_drag_shadow.klass,
                                          gdk_android_get_java_cache ()->clipboard_bitmap_drag_shadow.constructor,
                                          initiator->surface,
                                          bitmap, surface_impl->hot_x, surface_impl->hot_y);

      (*env)->CallVoidMethod (env, initiator->surface,
                              gdk_android_get_java_cache()->surface.update_dnd,
                              shadow);

      (*env)->DeleteGlobalRef(env, self->drag.buffer);
      self->drag.buffer = NULL;

      gdk_cairo_context_frame_set_surface (cframe, NULL);

      (*env)->PopLocalFrame(env, NULL);
    }
}

static void
gdk_android_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 2);

  if (GDK_IS_ANDROID_SURFACE (surface))
    {
      GdkAndroidSurface *surface_impl = (GdkAndroidSurface *)surface;

      jobject surface_holder = (*env)->CallObjectMethod (env, surface_impl->surface, gdk_android_get_java_cache ()->surface.get_holder);
      jobject canvas = (*env)->CallObjectMethod (env, surface_holder, gdk_android_get_java_cache ()->a_surfaceholder.lock_canvas);
      (*env)->CallVoidMethod (env, surface_holder, gdk_android_get_java_cache ()->a_surfaceholder.unlock_canvas_and_post, canvas);
    }
  else if (GDK_IS_ANDROID_DRAG_SURFACE (surface))
    {
      // noop
    }

  (*env)->PopLocalFrame (env, NULL);
}

static void
gdk_android_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  g_assert (GDK_IS_ANDROID_CAIRO_CONTEXT (draw_context));

  /* Do nothing, next begin_frame will get new buffer */
}

static void
gdk_android_cairo_context_class_init (GdkAndroidCairoContextClass *klass)
{
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  draw_context_class->begin_frame = gdk_android_cairo_context_begin_frame;
  draw_context_class->end_frame = gdk_android_cairo_context_end_frame;
  draw_context_class->empty_frame = gdk_android_cairo_context_empty_frame;
  draw_context_class->surface_resized = gdk_android_cairo_context_surface_resized;
}

static void
gdk_android_cairo_context_init (GdkAndroidCairoContext *self)
{
}
