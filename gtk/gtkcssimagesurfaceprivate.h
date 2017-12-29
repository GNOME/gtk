/*
 * Copyright © 2011 Red Hat Inc.
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_CSS_IMAGE_SURFACE_PRIVATE_H__
#define __GTK_CSS_IMAGE_SURFACE_PRIVATE_H__

#include "gtk/gtkcssimageprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_SURFACE           (_gtk_css_image_surface_get_type ())
#define GTK_CSS_IMAGE_SURFACE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_SURFACE, GtkCssImageSurface))
#define GTK_CSS_IMAGE_SURFACE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_SURFACE, GtkCssImageSurfaceClass))
#define GTK_IS_CSS_IMAGE_SURFACE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_SURFACE))
#define GTK_IS_CSS_IMAGE_SURFACE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_SURFACE))
#define GTK_CSS_IMAGE_SURFACE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_SURFACE, GtkCssImageSurfaceClass))

typedef struct _GtkCssImageSurface           GtkCssImageSurface;
typedef struct _GtkCssImageSurfaceClass      GtkCssImageSurfaceClass;

struct _GtkCssImageSurface
{
  GtkCssImage parent;

  cairo_surface_t *surface;             /* the surface we render - guaranteed to be an image surface */
  cairo_surface_t *cache;               /* the scaled surface - to avoid scaling every time we need to draw */
  double width;                         /* original cache width */
  double height;                        /* original cache height */
};

struct _GtkCssImageSurfaceClass
{
  GtkCssImageClass parent_class;
};

GType          _gtk_css_image_surface_get_type             (void) G_GNUC_CONST;

GtkCssImage *  _gtk_css_image_surface_new                  (cairo_surface_t *surface);
GtkCssImage *  _gtk_css_image_surface_new_for_pixbuf       (GdkPixbuf       *pixbuf);

G_END_DECLS

#endif /* __GTK_CSS_IMAGE_SURFACE_PRIVATE_H__ */
