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
#include "gdkdrmdisplay-private.h"
#include "gdkdrmmonitor-private.h"
#include "gdkdrmsurface-private.h"

#include "gdkdisplayprivate.h"
#include "gdkglcontextprivate.h"
#include "gdksurfaceprivate.h"

#include <drm_fourcc.h>
#include <gbm.h>
#include <xf86drmMode.h>

#ifdef HAVE_EGL
#include <epoxy/egl.h>
#endif

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_DEFINE_FINAL_TYPE (GdkDrmGLContext, gdk_drm_gl_context, GDK_TYPE_GL_CONTEXT)

#ifdef HAVE_EGL

static void
drm_gl_context_ensure_gbm_surface (GdkDrmGLContext *self,
                                   int              width,
                                   int              height)
{
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (self)));
  struct gbm_device *gbm = display->gbm_device;

  if (self->gbm_surface && self->width == width && self->height == height)
    return;

  if (self->gbm_surface)
    {
      if (self->previous_bo)
        {
          gbm_surface_release_buffer (self->gbm_surface, self->previous_bo);
          self->previous_bo = NULL;
        }
      if (self->previous_fb_id)
        {
          drmModeRmFB (display->drm_fd, self->previous_fb_id);
          self->previous_fb_id = 0;
        }
      gbm_surface_destroy (self->gbm_surface);
      self->gbm_surface = NULL;
    }

  self->width = 0;
  self->height = 0;

  if (width <= 0 || height <= 0 || !gbm)
    return;

  self->gbm_surface = gbm_surface_create (gbm, width, height,
                                          GBM_FORMAT_XRGB8888,
                                          GBM_BO_USE_SCANOUT);
  if (self->gbm_surface)
    {
      self->width = width;
      self->height = height;
    }
}

static GdkGLAPI
gdk_drm_gl_context_real_realize (GdkGLContext  *context,
                                 GError       **error)
{
  return gdk_gl_context_realize_egl (context, error);
}

static void
gdk_drm_gl_context_begin_frame (GdkDrawContext  *context,
                                GdkMemoryDepth   depth,
                                cairo_region_t  *region,
                                GdkColorState  **out_color_state,
                                GdkMemoryDepth  *out_depth)
{
  GdkDrmGLContext *self = GDK_DRM_GL_CONTEXT (context);
  GdkSurface *surface = gdk_draw_context_get_surface (context);
  int width, height;

  gdk_surface_get_geometry (surface, NULL, NULL, &width, &height);
  drm_gl_context_ensure_gbm_surface (self, width, height);

  if (self->gbm_surface)
    gdk_surface_set_egl_native_window (surface, self->gbm_surface);

  GDK_DRAW_CONTEXT_CLASS (gdk_drm_gl_context_parent_class)->begin_frame (context,
                                                                         depth,
                                                                         region,
                                                                         out_color_state,
                                                                         out_depth);
}

static void
gdk_drm_gl_context_end_frame (GdkDrawContext *context,
                              cairo_region_t *painted)
{
  GdkDrmGLContext *self = GDK_DRM_GL_CONTEXT (context);
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_draw_context_get_display (context));
  GdkSurface *surface = gdk_draw_context_get_surface (context);
  GdkMonitor *monitor;
  GdkDrmMonitor *drm_monitor;
  guint32 crtc_id;
  struct gbm_bo *bo;
  guint32 fb_id = 0;
  uint32_t handle, stride;
  int ret;
  drmModeCrtc *crtc = NULL;
  const drmModeModeInfo *mode;

  GDK_DRAW_CONTEXT_CLASS (gdk_drm_gl_context_parent_class)->end_frame (context, painted);

  if (!self->gbm_surface || self->width == 0 || self->height == 0)
    return;

  bo = gbm_surface_lock_front_buffer (self->gbm_surface);
  if (!bo)
    return;

  if (self->previous_bo)
    {
      gbm_surface_release_buffer (self->gbm_surface, self->previous_bo);
      self->previous_bo = NULL;
    }
  if (self->previous_fb_id)
    {
      drmModeRmFB (display->drm_fd, self->previous_fb_id);
      self->previous_fb_id = 0;
    }

#ifdef __linux__
  handle = gbm_bo_get_handle(bo).u32;
  stride = gbm_bo_get_stride(bo);
#else
  handle = gbm_bo_get_handle(bo).u32;
  stride = gbm_bo_get_stride_for_plane(bo, 0);
#endif

  ret = drmModeAddFB2 (display->drm_fd, self->width, self->height,
                       DRM_FORMAT_XRGB8888,
                       (uint32_t[]){ handle, 0, 0, 0 },
                       (uint32_t[]){ stride, 0, 0, 0 },
                       (uint32_t[]){ 0, 0, 0, 0 },
                       &fb_id, 0);
  if (ret != 0)
    {
      gbm_surface_release_buffer (self->gbm_surface, bo);
      return;
    }

  monitor = gdk_display_get_monitor_at_surface (gdk_draw_context_get_display (context), surface);
  if (!monitor || !GDK_IS_DRM_MONITOR (monitor))
    {
      gbm_surface_release_buffer (self->gbm_surface, bo);
      drmModeRmFB (display->drm_fd, fb_id);
      return;
    }
  drm_monitor = GDK_DRM_MONITOR (monitor);
  crtc_id = _gdk_drm_monitor_get_crtc_id (drm_monitor);
  if (crtc_id == 0)
    {
      gbm_surface_release_buffer (self->gbm_surface, bo);
      drmModeRmFB (display->drm_fd, fb_id);
      return;
    }

  _gdk_drm_display_wait_page_flip (display, crtc_id);

  if (!g_hash_table_contains (display->crtc_initialized, GUINT_TO_POINTER (crtc_id)))
    {
      guint32 connector_id = _gdk_drm_monitor_get_connector_id (drm_monitor);
      mode = _gdk_drm_monitor_get_mode (drm_monitor);
      crtc = drmModeGetCrtc (display->drm_fd, crtc_id);
      if (crtc && mode->clock && connector_id)
        {
          ret = drmModeSetCrtc (display->drm_fd, crtc_id, fb_id, 0, 0,
                                &connector_id, 1, (drmModeModeInfo *) mode);
          (void) ret;
        }
      if (crtc)
        drmModeFreeCrtc (crtc);
      g_hash_table_add (display->crtc_initialized, GUINT_TO_POINTER (crtc_id));
    }
  else
    {
      ret = drmModePageFlip (display->drm_fd, crtc_id, fb_id,
                             DRM_MODE_PAGE_FLIP_EVENT,
                             (void *) (uintptr_t) crtc_id);
      if (ret != 0)
        {
          drmModeRmFB (display->drm_fd, fb_id);
          gbm_surface_release_buffer (self->gbm_surface, bo);
          return;
        }
      _gdk_drm_display_mark_page_flip_pending (display, crtc_id);
    }

  self->previous_bo = bo;
  self->previous_fb_id = fb_id;
}

static void
gdk_drm_gl_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkDrmGLContext *self = GDK_DRM_GL_CONTEXT (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  int width, height;

  gdk_surface_get_geometry (surface, NULL, NULL, &width, &height);
  drm_gl_context_ensure_gbm_surface (self, width, height);
}

static gboolean
gdk_drm_gl_context_make_current (GdkGLContext *context,
                                gboolean      surfaceless)
{
  if (surfaceless)
    return GDK_GL_CONTEXT_CLASS (gdk_drm_gl_context_parent_class)->make_current (context, TRUE);
  return GDK_GL_CONTEXT_CLASS (gdk_drm_gl_context_parent_class)->make_current (context, FALSE);
}

static gboolean
gdk_drm_gl_context_clear_current (GdkGLContext *context)
{
  return GDK_GL_CONTEXT_CLASS (gdk_drm_gl_context_parent_class)->clear_current (context);
}

static gboolean
gdk_drm_gl_context_is_current (GdkGLContext *context)
{
  return GDK_GL_CONTEXT_CLASS (gdk_drm_gl_context_parent_class)->is_current (context);
}

#else

static GdkGLAPI
gdk_drm_gl_context_real_realize (GdkGLContext  *context,
                                 GError       **error)
{
  g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                       "EGL support not compiled in");
  return 0;
}

static void
gdk_drm_gl_context_begin_frame (GdkDrawContext  *context,
                                GdkMemoryDepth   depth,
                                cairo_region_t  *region,
                                GdkColorState  **out_color_state,
                                GdkMemoryDepth  *out_depth)
{
  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
gdk_drm_gl_context_end_frame (GdkDrawContext *context,
                              cairo_region_t *painted)
{
  GDK_DRAW_CONTEXT_CLASS (gdk_drm_gl_context_parent_class)->end_frame (context, painted);
}

static void
gdk_drm_gl_context_surface_resized (GdkDrawContext *draw_context)
{
}

static gboolean
gdk_drm_gl_context_make_current (GdkGLContext *context,
                                gboolean      surfaceless)
{
  return TRUE;
}

static gboolean
gdk_drm_gl_context_clear_current (GdkGLContext *context)
{
  return TRUE;
}

static gboolean
gdk_drm_gl_context_is_current (GdkGLContext *context)
{
  return TRUE;
}

#endif

static void
gdk_drm_gl_context_empty_frame (GdkDrawContext *draw_context)
{
}

static cairo_region_t *
gdk_drm_gl_context_get_damage (GdkGLContext *context)
{
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
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (self)));

#ifdef HAVE_EGL
  if (self->gbm_surface)
    {
      if (self->previous_bo)
        {
          gbm_surface_release_buffer (self->gbm_surface, self->previous_bo);
          self->previous_bo = NULL;
        }
      if (self->previous_fb_id)
        {
          drmModeRmFB (display->drm_fd, self->previous_fb_id);
          self->previous_fb_id = 0;
        }
      gbm_surface_destroy (self->gbm_surface);
      self->gbm_surface = NULL;
    }
#endif

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
  self->gbm_surface = NULL;
  self->width = 0;
  self->height = 0;
  self->previous_bo = NULL;
  self->previous_fb_id = 0;
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
