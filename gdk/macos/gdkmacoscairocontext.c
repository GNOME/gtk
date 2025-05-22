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

#import "GdkMacosLayer.h"

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

static void
_gdk_macos_cairo_context_constructed(GObject *object)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)object;
  GdkSurface *surface;
  NSView *view;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self));
  view = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface));

  [view setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawNever];
  [view setLayer:[GdkMacosLayer layer]];
  [view setWantsLayer:YES];
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
   */
  image_surface = cairo_image_surface_create_for_data (data,
                                                       CAIRO_FORMAT_ARGB32,
                                                       width,
                                                       height,
                                                       stride);

  /* The buffer should already be locked at this point, and will
   * be unlocked as part of end_frame.
   */

  if (!(cr = cairo_create (image_surface)))
    goto failure;

  /* Clip to the current damage region */
  if (damage != NULL)
    {
      gdk_cairo_region (cr, damage);
      cairo_clip (cr);
    }

  if (!opaque)
    {
      cairo_save (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_paint (cr);

      cairo_restore (cr);
    }

failure:
  cairo_surface_destroy (image_surface);

  return cr;
}

static void
copy_surface_data (GdkMacosBuffer       *from,
                   GdkMacosBuffer       *to,
                   const cairo_region_t *region)
{
  const guint8 *from_base;
  guint8 *to_base;
  guint from_stride;
  guint to_stride;
  guint n_rects;

  g_assert (GDK_IS_MACOS_BUFFER (from));
  g_assert (GDK_IS_MACOS_BUFFER (to));
  g_assert (region != NULL);
  g_assert (!cairo_region_is_empty (region));

  from_base = _gdk_macos_buffer_get_data (from);
  from_stride = _gdk_macos_buffer_get_stride (from);

  to_base = _gdk_macos_buffer_get_data (to);
  to_stride = _gdk_macos_buffer_get_stride (to);

  n_rects = cairo_region_num_rectangles (region);

  for (guint i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      int y2;

      cairo_region_get_rectangle (region, i, &rect);

      y2 = rect.y + rect.height;

      for (int y = rect.y; y < y2; y++)
        memcpy (&to_base[y * to_stride + rect.x * 4],
                &from_base[y * from_stride + rect.x * 4],
                rect.width * 4);
    }
}

static void
_gdk_macos_cairo_context_begin_frame (GdkDrawContext  *draw_context,
                                      gpointer         context_data,
                                      GdkMemoryDepth   depth,
                                      cairo_region_t  *region,
                                      GdkColorState  **out_color_state,
                                      GdkMemoryDepth  *out_depth)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkMacosBuffer *buffer;
  GdkMacosSurface *surface;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  [CATransaction begin];
  [CATransaction setDisableActions:YES];

  surface = GDK_MACOS_SURFACE (gdk_draw_context_get_surface (draw_context));
  buffer = _gdk_macos_surface_get_buffer (surface);

  _gdk_macos_buffer_set_damage (buffer, region);
  _gdk_macos_buffer_set_flipped (buffer, FALSE);

  _gdk_macos_buffer_lock (buffer);

  /* If there is damage that was on the previous frame that is not on
   * this frame, we need to copy that rendered region over to the back
   * buffer so that when swapping buffers, we still have that content.
   * This is done with a read-only lock on the IOSurface to avoid
   * invalidating the buffer contents.
   */
  if (surface->front != NULL)
    {
      const cairo_region_t *previous = _gdk_macos_buffer_get_damage (surface->front);

      if (previous != NULL)
        {
          cairo_region_t *copy;

          copy = cairo_region_copy (previous);
          cairo_region_subtract (copy, region);

          if (!cairo_region_is_empty (copy))
            {
              _gdk_macos_buffer_read_lock (surface->front);
              copy_surface_data (surface->front, buffer, copy);
              _gdk_macos_buffer_read_unlock (surface->front);
            }

          cairo_region_destroy (copy);
        }
    }

  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
_gdk_macos_cairo_context_end_frame (GdkDrawContext *draw_context,
                                    gpointer        context_data,
                                    cairo_region_t *painted)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkMacosSurface *surface;
  GdkMacosBuffer *buffer;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  surface = GDK_MACOS_SURFACE (gdk_draw_context_get_surface (draw_context));
  buffer = _gdk_macos_surface_get_buffer (surface);

  _gdk_macos_buffer_unlock (buffer);

  _gdk_macos_surface_swap_buffers (surface, painted);

  [CATransaction commit];
}

static void
_gdk_macos_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
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
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

  draw_context_class->begin_frame = _gdk_macos_cairo_context_begin_frame;
  draw_context_class->end_frame = _gdk_macos_cairo_context_end_frame;
  draw_context_class->empty_frame = _gdk_macos_cairo_context_empty_frame;
  draw_context_class->surface_resized = _gdk_macos_cairo_context_surface_resized;

  cairo_context_class->cairo_create = _gdk_macos_cairo_context_cairo_create;

  g_object_class->constructed = _gdk_macos_cairo_context_constructed;
}

static void
_gdk_macos_cairo_context_init (GdkMacosCairoContext *self)
{
}
