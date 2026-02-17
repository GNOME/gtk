/* gdkdrmcairocontext.c
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

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cairo.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "gdkdrmcairocontext-private.h"
#include "gdkdrmdisplay-private.h"
#include "gdkdrmmonitor-private.h"
#include "gdkdrmsurface-private.h"

#define DRM_BPP 32

typedef struct
{
  guint32 handle;
  guint32 pitch;
  guint64 size;
  guint32 fb_id;
  void   *map_ptr;
} DrmCairoBuffer;

struct _GdkDrmCairoContext
{
  GdkCairoContext parent_instance;

  DrmCairoBuffer buffers[2];
  int            front_index;   /* 0 or 1 - which buffer is on screen */
  cairo_surface_t *paint_surface;
  int            width;
  int            height;
};

struct _GdkDrmCairoContextClass
{
  GdkCairoContextClass parent_class;
};

G_DEFINE_FINAL_TYPE (GdkDrmCairoContext, _gdk_drm_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static void
drm_cairo_buffer_clear (GdkDrmDisplay *display,
                        DrmCairoBuffer *buf)
{
  int fd = display->drm_fd;

  if (buf->map_ptr)
    {
      munmap (buf->map_ptr, buf->size);
      buf->map_ptr = NULL;
    }
  if (buf->fb_id)
    {
      drmModeRmFB (fd, buf->fb_id);
      buf->fb_id = 0;
    }
  if (buf->handle)
    {
      drmModeDestroyDumbBuffer (fd, buf->handle);
      buf->handle = 0;
    }
  buf->pitch = 0;
  buf->size = 0;
}

static gboolean
drm_cairo_buffer_allocate (GdkDrmDisplay *display,
                           DrmCairoBuffer *buf,
                           int            width,
                           int            height)
{
  int fd = display->drm_fd;
  guint32 handle;
  guint32 pitch;
  guint64 size;
  guint64 offset;
  uint32_t fb_id;
  int ret;

  ret = drmModeCreateDumbBuffer (fd, width, height, DRM_BPP, 0,
                                  &handle, &pitch, &size);
  if (ret != 0)
    return FALSE;

  buf->handle = handle;
  buf->pitch = pitch;
  buf->size = size;

  ret = drmModeMapDumbBuffer (fd, handle, &offset);
  if (ret != 0)
    {
      drmModeDestroyDumbBuffer (fd, handle);
      buf->handle = 0;
      return FALSE;
    }

  buf->map_ptr = mmap (0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
  if (buf->map_ptr == MAP_FAILED)
    {
      buf->map_ptr = NULL;
      drmModeDestroyDumbBuffer (fd, handle);
      buf->handle = 0;
      return FALSE;
    }

  ret = drmModeAddFB2 (fd, width, height, DRM_FORMAT_ARGB8888,
                       (uint32_t[]){ handle, 0, 0, 0 },
                       (uint32_t[]){ pitch, 0, 0, 0 },
                       (uint32_t[]){ 0, 0, 0, 0 },
                       &fb_id, 0);
  if (ret != 0)
    {
      munmap (buf->map_ptr, size);
      buf->map_ptr = NULL;
      drmModeDestroyDumbBuffer (fd, handle);
      buf->handle = 0;
      return FALSE;
    }
  buf->fb_id = fb_id;
  return TRUE;
}

static void
drm_cairo_context_ensure_buffers (GdkDrmCairoContext *self,
                                  int                 width,
                                  int                 height)
{
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (self)));

  if (self->width == width && self->height == height)
    return;

  drm_cairo_buffer_clear (display, &self->buffers[0]);
  drm_cairo_buffer_clear (display, &self->buffers[1]);

  if (width <= 0 || height <= 0)
    {
      self->width = 0;
      self->height = 0;
      return;
    }

  if (!drm_cairo_buffer_allocate (display, &self->buffers[0], width, height) ||
      !drm_cairo_buffer_allocate (display, &self->buffers[1], width, height))
    return;

  self->width = width;
  self->height = height;
  self->front_index = 0;
}

static cairo_t *
_gdk_drm_cairo_context_cairo_create (GdkCairoContext *cairo_context)
{
  GdkDrmCairoContext *self = GDK_DRM_CAIRO_CONTEXT (cairo_context);

  if (self->paint_surface)
    return cairo_create (self->paint_surface);
  return NULL;
}

static void
_gdk_drm_cairo_context_begin_frame (GdkDrawContext  *draw_context,
                                    GdkMemoryDepth   depth,
                                    cairo_region_t  *region,
                                    GdkColorState  **out_color_state,
                                    GdkMemoryDepth  *out_depth)
{
  GdkDrmCairoContext *self = GDK_DRM_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  int width, height;
  int back_index;
  DrmCairoBuffer *back;

  gdk_surface_get_geometry (surface, NULL, NULL, &width, &height);

  drm_cairo_context_ensure_buffers (self, width, height);

  if (self->width == 0 || self->height == 0)
    {
      *out_color_state = GDK_COLOR_STATE_SRGB;
      *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
      return;
    }

  back_index = 1 - self->front_index;
  back = &self->buffers[back_index];

  if (self->paint_surface)
    cairo_surface_destroy (self->paint_surface);

  self->paint_surface = cairo_image_surface_create_for_data (back->map_ptr,
                                                             CAIRO_FORMAT_ARGB32,
                                                             self->width,
                                                             self->height,
                                                             back->pitch);
  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
_gdk_drm_cairo_context_end_frame (GdkDrawContext *draw_context,
                                  cairo_region_t *painted)
{
  GdkDrmCairoContext *self = GDK_DRM_CAIRO_CONTEXT (draw_context);
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_draw_context_get_display (draw_context));
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  GdkMonitor *monitor;
  GdkDrmMonitor *drm_monitor;
  guint32 crtc_id;
  int back_index;
  DrmCairoBuffer *back;
  drmModeCrtc *crtc = NULL;
  const drmModeModeInfo *mode;
  int ret;

  if (self->paint_surface)
    {
      cairo_surface_destroy (self->paint_surface);
      self->paint_surface = NULL;
    }

  if (self->width == 0 || self->height == 0)
    return;

  monitor = gdk_display_get_monitor_at_surface (gdk_draw_context_get_display (draw_context), surface);
  if (!monitor || !GDK_IS_DRM_MONITOR (monitor))
    return;
  drm_monitor = GDK_DRM_MONITOR (monitor);
  crtc_id = _gdk_drm_monitor_get_crtc_id (drm_monitor);
  if (crtc_id == 0)
    return;

  back_index = 1 - self->front_index;
  back = &self->buffers[back_index];

  _gdk_drm_display_wait_page_flip (display, crtc_id);

  if (!g_hash_table_contains (display->crtc_initialized, GUINT_TO_POINTER (crtc_id)))
    {
      guint32 connector_id = _gdk_drm_monitor_get_connector_id (drm_monitor);
      mode = _gdk_drm_monitor_get_mode (drm_monitor);
      crtc = drmModeGetCrtc (display->drm_fd, crtc_id);
      if (crtc && mode->clock && connector_id)
        {
          ret = drmModeSetCrtc (display->drm_fd, crtc_id, back->fb_id, 0, 0,
                               &connector_id, 1, (drmModeModeInfo *) mode);
          (void) ret;
        }
      if (crtc)
        drmModeFreeCrtc (crtc);
      g_hash_table_add (display->crtc_initialized, GUINT_TO_POINTER (crtc_id));
    }
  else
    {
      ret = drmModePageFlip (display->drm_fd, crtc_id, back->fb_id,
                             DRM_MODE_PAGE_FLIP_EVENT,
                             (void *) (uintptr_t) crtc_id);
      if (ret == 0)
        _gdk_drm_display_mark_page_flip_pending (display, crtc_id);
    }

  self->front_index = back_index;
}

static void
_gdk_drm_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
}

static void
_gdk_drm_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkDrmCairoContext *self = GDK_DRM_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  int width, height;

  gdk_surface_get_geometry (surface, NULL, NULL, &width, &height);
  drm_cairo_context_ensure_buffers (self, width, height);
}

static void
_gdk_drm_cairo_context_dispose (GObject *object)
{
  GdkDrmCairoContext *self = GDK_DRM_CAIRO_CONTEXT (object);
  GdkDrmDisplay *display = GDK_DRM_DISPLAY (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (self)));

  if (self->paint_surface)
    {
      cairo_surface_destroy (self->paint_surface);
      self->paint_surface = NULL;
    }
  drm_cairo_buffer_clear (display, &self->buffers[0]);
  drm_cairo_buffer_clear (display, &self->buffers[1]);
  self->width = 0;
  self->height = 0;

  G_OBJECT_CLASS (_gdk_drm_cairo_context_parent_class)->dispose (object);
}

static void
_gdk_drm_cairo_context_class_init (GdkDrmCairoContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  object_class->dispose = _gdk_drm_cairo_context_dispose;

  draw_context_class->begin_frame = _gdk_drm_cairo_context_begin_frame;
  draw_context_class->end_frame = _gdk_drm_cairo_context_end_frame;
  draw_context_class->empty_frame = _gdk_drm_cairo_context_empty_frame;
  draw_context_class->surface_resized = _gdk_drm_cairo_context_surface_resized;

  cairo_context_class->cairo_create = _gdk_drm_cairo_context_cairo_create;
}

static void
_gdk_drm_cairo_context_init (GdkDrmCairoContext *self)
{
  memset (self->buffers, 0, sizeof (self->buffers));
  self->front_index = 0;
  self->paint_surface = NULL;
  self->width = 0;
  self->height = 0;
}
