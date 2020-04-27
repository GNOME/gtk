/*
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

#include "gdkmacoscairocontext-private.h"

struct _GdkMacosCairoContext
{
  GdkCairoContext parent_instance;
};

struct _GdkMacosCairoContextClass
{
  GdkCairoContextClass parent_class;
};

G_DEFINE_TYPE (GdkMacosCairoContext, _gdk_macos_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static cairo_t *
_gdk_macos_cairo_context_cairo_create (GdkCairoContext *cairo_context)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)cairo_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  return NULL;
}

static void
_gdk_macos_cairo_context_begin_frame (GdkDrawContext *draw_context,
                                      cairo_region_t *region)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

}

static void
_gdk_macos_cairo_context_end_frame (GdkDrawContext *draw_context,
                                   cairo_region_t  *painted)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

}

static void
_gdk_macos_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

}

static void
_gdk_macos_cairo_context_dispose (GObject *object)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)object;

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
