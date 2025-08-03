/*
 * Copyright (c) 2024-2025 Marvin Wissfeld
 * Copyright (c) 2025 Florian "sp1rit" <sp1rit@disroot.org>
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

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"

#include "gdkandroid.h"

#include "gdkandroidinit-private.h"
#include "gdkandroidsurface-private.h"
#include "gdkandroiddnd-private.h"
#include "gdkandroidglcontext-private.h"

#include <epoxy/egl.h>

struct _GdkAndroidGLContext
{
  GdkGLContext parent_instance;
};

struct _GdkAndroidGLContextClass
{
  GdkGLContextClass parent_class;
};

G_DEFINE_TYPE (GdkAndroidGLContext, gdk_android_gl_context, GDK_TYPE_GL_CONTEXT)

static void
gdk_android_gl_context_begin_frame (GdkDrawContext  *draw_context,
                                    gpointer         context_data,
                                    GdkMemoryDepth   depth,
                                    cairo_region_t  *region,
                                    GdkColorState  **out_color_state,
                                    GdkMemoryDepth  *out_depth)
{
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  g_return_if_fail (GDK_IS_ANDROID_DRAG_SURFACE (surface) == FALSE);

  GDK_DRAW_CONTEXT_CLASS (gdk_android_gl_context_parent_class)->begin_frame (draw_context, context_data, depth, region, out_color_state, out_depth);
}

static void
gdk_android_gl_context_end_frame (GdkDrawContext *draw_context,
                                  gpointer        context_data,
                                  cairo_region_t *painted)
{
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  g_return_if_fail (GDK_IS_ANDROID_DRAG_SURFACE (surface) == FALSE);

  GDK_DRAW_CONTEXT_CLASS (gdk_android_gl_context_parent_class)->end_frame (draw_context, context_data, painted);
}

static void
gdk_android_gl_context_empty_frame (GdkDrawContext *draw_context)
{}

static gboolean
gdk_android_gl_context_surface_attach (GdkDrawContext  *context,
                                       GError         **error)
{
  GdkSurface *surface = gdk_draw_context_get_surface (context);

  gdk_gl_context_set_egl_native_window (GDK_GL_CONTEXT (context),
                                        GDK_ANDROID_SURFACE (surface)->native);

  return TRUE;
}

static void
gdk_android_gl_context_surface_resized (GdkDrawContext  *context)
{
  GdkSurface *surface = gdk_draw_context_get_surface (context);
  /*
   * For some reason, not all surface changes (e.g. fullscreening) cause
   * the OS to do the surfaceDestroyed / surfaceCreated cycle, but we
   * still have to recreate the EGL surface for those cases.
   */
  gdk_gl_context_set_egl_native_window ((GdkGLContext *)context,
                                        GDK_ANDROID_SURFACE (surface)->native);
}

static void
gdk_android_gl_context_surface_detach (GdkDrawContext  *context)
{
  gdk_gl_context_set_egl_native_window ((GdkGLContext *)context,
                                        NULL);
}

static GLuint
gdk_android_gl_context_get_default_framebuffer (GdkGLContext *gl_context)
{
  GdkSurface *surface = gdk_gl_context_get_surface (gl_context);
  g_return_val_if_fail (GDK_IS_ANDROID_DRAG_SURFACE (surface) == FALSE, 0);

  return GDK_GL_CONTEXT_CLASS (gdk_android_gl_context_parent_class)->get_default_framebuffer (gl_context);
}


static GdkGLAPI
gdk_android_gl_context_realize (GdkGLContext  *gl_context,
                                GError       **error)
{
  GdkSurface *surface = gdk_gl_context_get_surface (gl_context);

  if (GDK_IS_ANDROID_DRAG_SURFACE (surface))
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                   "%s does currently not support OpenGL", G_OBJECT_TYPE_NAME (surface));
      return 0;
    }

  return GDK_GL_CONTEXT_CLASS (gdk_android_gl_context_parent_class)->realize (gl_context, error);
}

static void
gdk_android_gl_context_class_init (GdkAndroidGLContextClass *class)
{
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (class);
  GdkGLContextClass *gl_context_class = GDK_GL_CONTEXT_CLASS (class);

  draw_context_class->begin_frame = gdk_android_gl_context_begin_frame;
  draw_context_class->end_frame = gdk_android_gl_context_end_frame;
  draw_context_class->empty_frame = gdk_android_gl_context_empty_frame;
  draw_context_class->surface_attach = gdk_android_gl_context_surface_attach;
  draw_context_class->surface_resized = gdk_android_gl_context_surface_resized;
  draw_context_class->surface_detach = gdk_android_gl_context_surface_detach;
  gl_context_class->backend_type = GDK_GL_EGL;
  gl_context_class->get_default_framebuffer = gdk_android_gl_context_get_default_framebuffer;
  gl_context_class->realize = gdk_android_gl_context_realize;
}

static void
gdk_android_gl_context_init (GdkAndroidGLContext *self)
{}

/**
 * gdk_android_display_get_egl_display:
 * @self: (transfer none): the display
 *
 * Retrieves the EGL display connection object for the given GDK display.
 *
 * Returns: (nullable): the EGL display
 *
 * Since: 4.18
 */
gpointer
gdk_android_display_get_egl_display (GdkAndroidDisplay *self)
{
  g_return_val_if_fail (GDK_IS_ANDROID_DISPLAY (self), NULL);
  return gdk_display_get_egl_display ((GdkDisplay *)self);
}
