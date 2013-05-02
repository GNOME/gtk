/*
 * Copyright © 2013 Red Hat Inc.
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
 * Authors: Alexander Larsson <alexl@gnome.org>
 */

#ifndef __GTK_PIXEL_CACHE_PRIVATE_H__
#define __GTK_PIXEL_CACHE_PRIVATE_H__

#include <glib-object.h>
#include <gtkwidget.h>

G_BEGIN_DECLS

typedef struct _GtkPixelCache           GtkPixelCache;

typedef void (*GtkPixelCacheDrawFunc) (cairo_t *cr,
				       gpointer user_data);

GtkPixelCache *_gtk_pixel_cache_new        (void);
void           _gtk_pixel_cache_free       (GtkPixelCache         *cache);
void           _gtk_pixel_cache_invalidate (GtkPixelCache         *cache,
					    cairo_region_t        *region);
void           _gtk_pixel_cache_draw       (GtkPixelCache         *cache,
					    cairo_t               *cr,
					    GdkWindow             *window,
					    cairo_rectangle_int_t *view_rect,
					    cairo_rectangle_int_t *canvas_rect,
					    GtkPixelCacheDrawFunc  draw,
					    gpointer               user_data);


G_END_DECLS

#endif /* __GTK_PIXEL_CACHE_PRIVATE_H__ */
