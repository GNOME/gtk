/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_CAIRO_PRIVATE_H__
#define __GDK_CAIRO_PRIVATE_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>

G_BEGIN_DECLS

gboolean        _gdk_cairo_surface_extents       (cairo_surface_t *surface,
                                                  GdkRectangle    *extents);

void            gdk_cairo_surface_paint_pixbuf   (cairo_surface_t *surface,
                                                  const GdkPixbuf *pixbuf);

cairo_region_t *gdk_cairo_region_from_clip       (cairo_t         *cr);


G_END_DECLS

#endif /* __GDK_CAIRO_PRIVATE_H__ */
