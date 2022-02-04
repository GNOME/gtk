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

#include <CoreGraphics/CoreGraphics.h>

#import "GdkMacosCairoView.h"

#include "gdkmacoscairocontext-private.h"
#include "gdkmacossurface-private.h"

struct _GdkMacosCairoContext
{
  GdkCairoContext  parent_instance;

  cairo_surface_t *window_surface;
  cairo_t         *cr;
};

struct _GdkMacosCairoContextClass
{
  GdkCairoContextClass parent_class;
};

G_DEFINE_TYPE (GdkMacosCairoContext, _gdk_macos_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static cairo_surface_t *
create_cairo_surface_for_surface (GdkSurface *surface)
{
  static const cairo_user_data_key_t buffer_key;
  cairo_surface_t *cairo_surface;
  guint8 *data;
  cairo_format_t format;
  size_t size;
  size_t rowstride;
  size_t width;
  size_t height;
  int scale;

  g_assert (GDK_IS_MACOS_SURFACE (surface));

  /* We use a cairo image surface here instead of a quartz surface because
   * we get strange artifacts with the quartz surface such as empty
   * cross-fades when hovering buttons. For performance, we want to be using
   * GL rendering so there isn't much point here as correctness is better.
   *
   * Additionally, so we can take avantage of faster paths in Core
   * Graphics, we want our data pointer to be 16-byte aligned and our rows
   * to be 16-byte aligned or we risk errors below us. Normally, cairo
   * image surface does not guarantee the later, which means we could end
   * up doing some costly copies along the way to compositing.
   */

  if ([GDK_MACOS_SURFACE (surface)->window isOpaque])
    format = CAIRO_FORMAT_RGB24;
  else
    format = CAIRO_FORMAT_ARGB32;

  scale = gdk_surface_get_scale_factor (surface);
  width = scale * gdk_surface_get_width (surface);
  height = scale * gdk_surface_get_height (surface);
  rowstride = (cairo_format_stride_for_width (format, width) + 0xF) & ~0xF;
  size = rowstride * height;
  data = g_malloc0 (size);
  cairo_surface = cairo_image_surface_create_for_data (data, format, width, height, rowstride);
  cairo_surface_set_user_data (cairo_surface, &buffer_key, data, g_free);
  cairo_surface_set_device_scale (cairo_surface, scale, scale);



  return cairo_surface;
}

static cairo_t *
_gdk_macos_cairo_context_cairo_create (GdkCairoContext *cairo_context)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)cairo_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  if (self->cr != NULL)
    return cairo_reference (self->cr);

  return cairo_create (self->window_surface);
}

static void
_gdk_macos_cairo_context_begin_frame (GdkDrawContext *draw_context,
                                      gboolean        prefers_high_depth,
                                      cairo_region_t *region)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkSurface *surface;
  NSWindow *nswindow;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  surface = gdk_draw_context_get_surface (draw_context);
  nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));

  if (self->window_surface == NULL)
    self->window_surface = create_cairo_surface_for_surface (surface);

  self->cr = cairo_create (self->window_surface);

  if (![nswindow isOpaque])
    {
      cairo_save (self->cr);
      gdk_cairo_region (self->cr, region);
      cairo_set_source_rgba (self->cr, 0, 0, 0, 0);
      cairo_set_operator (self->cr, CAIRO_OPERATOR_SOURCE);
      cairo_fill (self->cr);
      cairo_restore (self->cr);
    }
}

static void
_gdk_macos_cairo_context_end_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *painted)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkSurface *surface;
  NSView *nsview;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));
  g_assert (self->window_surface != NULL);

  surface = gdk_draw_context_get_surface (draw_context);
  nsview = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface));

  g_clear_pointer (&self->cr, cairo_destroy);

  if (GDK_IS_MACOS_CAIRO_VIEW (nsview))
    [(GdkMacosCairoView *)nsview setCairoSurface:self->window_surface
                                      withDamage:painted];
}

static void
_gdk_macos_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  g_clear_pointer (&self->window_surface, cairo_surface_destroy);
}

static void
_gdk_macos_cairo_context_dispose (GObject *object)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)object;

  g_clear_pointer (&self->window_surface, cairo_surface_destroy);

  G_OBJECT_CLASS (_gdk_macos_cairo_context_parent_class)->dispose (object);
}

static void
_gdk_macos_cairo_context_class_init (GdkMacosCairoContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  object_class->dispose = _gdk_macos_cairo_context_dispose;

  draw_context_class->begin_frame = _gdk_macos_cairo_context_begin_frame;
  draw_context_class->end_frame = _gdk_macos_cairo_context_end_frame;
  draw_context_class->surface_resized = _gdk_macos_cairo_context_surface_resized;

  cairo_context_class->cairo_create = _gdk_macos_cairo_context_cairo_create;
}

static void
_gdk_macos_cairo_context_init (GdkMacosCairoContext *self)
{
}
