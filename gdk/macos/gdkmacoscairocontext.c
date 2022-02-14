/*
 * Copyright © 2016 Benjamin Otte
 * Copyright © 2020 Red Hat, Inc.
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

#include "gdkconfig.h"

#include <cairo.h>
#include <QuartzCore/QuartzCore.h>
#include <CoreGraphics/CoreGraphics.h>

#include "gdkmacosbuffer-private.h"
#include "gdkmacoscairocontext-private.h"
#include "gdkmacossurface-private.h"

struct _GdkMacosCairoContext
{
  GdkCairoContext parent_instance;
};

struct _GdkMacosCairoContextClass
{
  GdkCairoContextClass parent_class;
};

G_DEFINE_TYPE (GdkMacosCairoContext, _gdk_macos_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static const cairo_user_data_key_t buffer_key;

static void
unlock_buffer (gpointer data)
{
  GdkMacosBuffer *buffer = data;

  g_assert (GDK_IS_MACOS_BUFFER (buffer));

  _gdk_macos_buffer_unlock (buffer);
  g_clear_object (&buffer);
}

static cairo_t *
_gdk_macos_cairo_context_cairo_create (GdkCairoContext *cairo_context)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)cairo_context;
  const cairo_region_t *damage;
  cairo_surface_t *image_surface;
  GdkMacosBuffer *buffer;
  GdkSurface *surface;
  NSWindow *nswindow;
  cairo_t *cr;
  gpointer data;
  double scale;
  guint width;
  guint height;
  guint stride;
  gboolean opaque;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self));
  nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));
  opaque = [nswindow isOpaque];

  buffer = _gdk_macos_surface_get_buffer (GDK_MACOS_SURFACE (surface));
  damage = _gdk_macos_buffer_get_damage (buffer);
  width = _gdk_macos_buffer_get_width (buffer);
  height = _gdk_macos_buffer_get_height (buffer);
  scale = _gdk_macos_buffer_get_device_scale (buffer);
  stride = _gdk_macos_buffer_get_stride (buffer);
  data = _gdk_macos_buffer_get_data (buffer);

  /* Instead of forcing cairo to do everything through a CGContext,
   * we just use an image surface backed by an IOSurfaceRef mapped
   * into user-space. We can then use pixman which is quite fast as
   * far as software rendering goes.
   *
   * Additionally, cairo_quartz_surface_t can't handle a number of
   * tricks that the GSK cairo renderer does with border nodes and
   * shadows, so an image surface is necessary for that.
   *
   * Since our IOSurfaceRef is width*scale-by-height*scale, we undo
   * the scaling using cairo_surface_set_device_scale() so the renderer
   * just thinks it's on a 2x scale surface for HiDPI.
   */
  image_surface = cairo_image_surface_create_for_data (data,
                                                       CAIRO_FORMAT_ARGB32,
                                                       width,
                                                       height,
                                                       stride);
  cairo_surface_set_device_scale (image_surface, scale, scale);

  /* Lock the buffer so we can modify it safely */
  _gdk_macos_buffer_lock (buffer);
  cairo_surface_set_user_data (image_surface,
                               &buffer_key,
                               g_object_ref (buffer),
                               unlock_buffer);

  if (!(cr = cairo_create (image_surface)))
    goto failure;

  /* Clip to the current damage region */
  if (damage != NULL)
    {
      gdk_cairo_region (cr, damage);
      cairo_clip (cr);
    }

  /* If we have some exposed transparent area in the damage region,
   * we need to clear the existing content first to leave an transparent
   * area for cairo. We use (surface_bounds or damage)-(opaque) to get
   * the smallest set of rectangles we need to clear as it's expensive.
   */
  if (!opaque)
    {
      cairo_region_t *transparent;
      cairo_rectangle_int_t r = { 0, 0, width/scale, height/scale };

      cairo_save (cr);

      if (damage != NULL)
        cairo_region_get_extents (damage, &r);
      transparent = cairo_region_create_rectangle (&r);
      if (surface->opaque_region)
        cairo_region_subtract (transparent, surface->opaque_region);

      if (!cairo_region_is_empty (transparent))
        {
          gdk_cairo_region (cr, transparent);
          cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
          cairo_fill (cr);
        }

      cairo_region_destroy (transparent);
      cairo_restore (cr);
    }

failure:
  cairo_surface_destroy (image_surface);

  return cr;
}

static void
_gdk_macos_cairo_context_begin_frame (GdkDrawContext *draw_context,
                                      gboolean        prefers_high_depth,
                                      cairo_region_t *region)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkMacosBuffer *buffer;
  GdkSurface *surface;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  [CATransaction begin];
  [CATransaction setDisableActions:YES];

  surface = gdk_draw_context_get_surface (draw_context);
  buffer = _gdk_macos_surface_get_buffer (GDK_MACOS_SURFACE (surface));

  _gdk_macos_buffer_set_damage (buffer, region);
  _gdk_macos_buffer_set_flipped (buffer, FALSE);
}

static void
_gdk_macos_cairo_context_end_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *painted)
{
  GdkMacosBuffer *buffer;
  GdkSurface *surface;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (draw_context));

  surface = gdk_draw_context_get_surface (draw_context);
  buffer = _gdk_macos_surface_get_buffer (GDK_MACOS_SURFACE (surface));

  _gdk_macos_surface_swap_buffers (GDK_MACOS_SURFACE (surface), painted);
  _gdk_macos_buffer_set_damage (buffer, NULL);

  [CATransaction commit];
}

static void
_gdk_macos_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (draw_context));

  /* Do nothing, next begin_frame will get new buffer */
}

static void
_gdk_macos_cairo_context_class_init (GdkMacosCairoContextClass *klass)
{
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  draw_context_class->begin_frame = _gdk_macos_cairo_context_begin_frame;
  draw_context_class->end_frame = _gdk_macos_cairo_context_end_frame;
  draw_context_class->surface_resized = _gdk_macos_cairo_context_surface_resized;

  cairo_context_class->cairo_create = _gdk_macos_cairo_context_cairo_create;
}

static void
_gdk_macos_cairo_context_init (GdkMacosCairoContext *self)
{
}
