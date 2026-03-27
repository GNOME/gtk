/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgpangoutilsprivate.h"
#include "gsk/gskpathbuilder.h"

GskPath *
svg_pango_layout_to_path (PangoLayout *layout)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_path_t *path;
  GskPathBuilder *builder;
  PangoRectangle rect;

  pango_layout_get_pixel_extents (layout, &rect, NULL);
  surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA,
                                            &(cairo_rectangle_t) { rect.x, rect.y, rect.width, rect.height });

  cr = cairo_create (surface);
  pango_cairo_layout_path (cr, layout);
  path = cairo_copy_path (cr);
  builder = gsk_path_builder_new ();
  gsk_path_builder_add_cairo_path (builder, path);
  cairo_path_destroy (path);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return gsk_path_builder_free_to_path (builder);
}
