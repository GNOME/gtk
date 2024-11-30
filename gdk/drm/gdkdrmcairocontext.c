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

#include <cairo.h>

#include "gdkdrmcairocontext-private.h"
#include "gdkdrmsurface-private.h"

struct _GdkDrmCairoContext
{
  GdkCairoContext parent_instance;
};

struct _GdkDrmCairoContextClass
{
  GdkCairoContextClass parent_class;
};

G_DEFINE_FINAL_TYPE (GdkDrmCairoContext, _gdk_drm_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static cairo_t *
_gdk_drm_cairo_context_cairo_create (GdkCairoContext *cairo_context)
{
  return NULL;
}

static void
_gdk_drm_cairo_context_begin_frame (GdkDrawContext  *draw_context,
                                    GdkMemoryDepth   depth,
                                    cairo_region_t  *region,
                                    GdkColorState  **out_color_state,
                                    GdkMemoryDepth  *out_depth)
{
  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
_gdk_drm_cairo_context_end_frame (GdkDrawContext *draw_context,
                                  cairo_region_t *painted)
{
}

static void
_gdk_drm_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
}

static void
_gdk_drm_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  g_assert (GDK_IS_DRM_CAIRO_CONTEXT (draw_context));
}

static void
_gdk_drm_cairo_context_class_init (GdkDrmCairoContextClass *klass)
{
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  draw_context_class->begin_frame = _gdk_drm_cairo_context_begin_frame;
  draw_context_class->end_frame = _gdk_drm_cairo_context_end_frame;
  draw_context_class->empty_frame = _gdk_drm_cairo_context_empty_frame;
  draw_context_class->surface_resized = _gdk_drm_cairo_context_surface_resized;

  cairo_context_class->cairo_create = _gdk_drm_cairo_context_cairo_create;
}

static void
_gdk_drm_cairo_context_init (GdkDrmCairoContext *self)
{
}
