/* gdkdrmglcontext.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkconfig.h"

#include "gdkdrmglcontext-private.h"
#include "gdkdrmsurface-private.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_DEFINE_FINAL_TYPE (GdkDrmGLContext, gdk_drm_gl_context, GDK_TYPE_GL_CONTEXT)

static GdkGLAPI
gdk_drm_gl_context_real_realize (GdkGLContext  *context,
                                 GError       **error)
{
  GdkDrmGLContext *self = (GdkDrmGLContext *)context;

  g_assert (GDK_IS_DRM_GL_CONTEXT (self));

  return GDK_GL_API_GL;
}

static void
gdk_drm_gl_context_begin_frame (GdkDrawContext  *context,
                                GdkMemoryDepth   depth,
                                cairo_region_t  *region,
                                GdkColorState  **out_color_state,
                                GdkMemoryDepth  *out_depth)
{
  GdkDrmGLContext *self = (GdkDrmGLContext *)context;

  g_assert (GDK_IS_DRM_GL_CONTEXT (self));
}

static void
gdk_drm_gl_context_end_frame (GdkDrawContext *context,
                              cairo_region_t *painted)
{
  GdkDrmGLContext *self = GDK_DRM_GL_CONTEXT (context);

  g_assert (GDK_IS_DRM_GL_CONTEXT (self));

  GDK_DRAW_CONTEXT_CLASS (gdk_drm_gl_context_parent_class)->end_frame (context, painted);
}

static void
gdk_drm_gl_context_empty_frame (GdkDrawContext *draw_context)
{
}

static void
gdk_drm_gl_context_surface_resized (GdkDrawContext *draw_context)
{
}

static gboolean
gdk_drm_gl_context_clear_current (GdkGLContext *context)
{
  GdkDrmGLContext *self = GDK_DRM_GL_CONTEXT (context);

  g_return_val_if_fail (GDK_IS_DRM_GL_CONTEXT (self), FALSE);

  return TRUE;
}

static gboolean
gdk_drm_gl_context_is_current (GdkGLContext *context)
{
  GdkDrmGLContext *self = GDK_DRM_GL_CONTEXT (context);

  return TRUE;
}

static gboolean
gdk_drm_gl_context_make_current (GdkGLContext *context,
                                 gboolean      surfaceless)
{
  GdkDrmGLContext *self = GDK_DRM_GL_CONTEXT (context);

  g_return_val_if_fail (GDK_IS_DRM_GL_CONTEXT (self), FALSE);

  return TRUE;
}

static cairo_region_t *
gdk_drm_gl_context_get_damage (GdkGLContext *context)
{
  GdkDrmGLContext *self = (GdkDrmGLContext *)context;

  g_assert (GDK_IS_DRM_GL_CONTEXT (self));

  return NULL;
}

static guint
gdk_drm_gl_context_get_default_framebuffer (GdkGLContext *context)
{
  return 0;
}

static void
gdk_drm_gl_context_dispose (GObject *gobject)
{
  GdkDrmGLContext *self = GDK_DRM_GL_CONTEXT (gobject);

  G_OBJECT_CLASS (gdk_drm_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_drm_gl_context_class_init (GdkDrmGLContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkGLContextClass *gl_class = GDK_GL_CONTEXT_CLASS (klass);

  object_class->dispose = gdk_drm_gl_context_dispose;

  draw_context_class->begin_frame = gdk_drm_gl_context_begin_frame;
  draw_context_class->end_frame = gdk_drm_gl_context_end_frame;
  draw_context_class->empty_frame = gdk_drm_gl_context_empty_frame;
  draw_context_class->surface_resized = gdk_drm_gl_context_surface_resized;

  gl_class->get_damage = gdk_drm_gl_context_get_damage;
  gl_class->clear_current = gdk_drm_gl_context_clear_current;
  gl_class->is_current = gdk_drm_gl_context_is_current;
  gl_class->make_current = gdk_drm_gl_context_make_current;
  gl_class->realize = gdk_drm_gl_context_real_realize;
  gl_class->get_default_framebuffer = gdk_drm_gl_context_get_default_framebuffer;

  gl_class->backend_type = GDK_GL_EGL;
}

static void
gdk_drm_gl_context_init (GdkDrmGLContext *self)
{
}

G_GNUC_END_IGNORE_DEPRECATIONS

GdkGLContext *
_gdk_drm_gl_context_new (GdkDrmDisplay  *display,
                         GError        **error)
{
  g_return_val_if_fail (GDK_IS_DRM_DISPLAY (display), NULL);

  return g_object_new (GDK_TYPE_DRM_GL_CONTEXT,
                       "display", display,
                       NULL);
}
