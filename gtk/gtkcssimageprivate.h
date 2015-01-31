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

#ifndef __GTK_CSS_IMAGE_PRIVATE_H__
#define __GTK_CSS_IMAGE_PRIVATE_H__

#include <cairo.h>
#include <glib-object.h>

#include "gtk/gtkcssparserprivate.h"
#include "gtk/gtkcsstypesprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE           (_gtk_css_image_get_type ())
#define GTK_CSS_IMAGE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE, GtkCssImage))
#define GTK_CSS_IMAGE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE, GtkCssImageClass))
#define GTK_IS_CSS_IMAGE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE))
#define GTK_IS_CSS_IMAGE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE))
#define GTK_CSS_IMAGE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE, GtkCssImageClass))

typedef struct _GtkCssImage           GtkCssImage;
typedef struct _GtkCssImageClass      GtkCssImageClass;

struct _GtkCssImage
{
  GObject parent;
};

struct _GtkCssImageClass
{
  GObjectClass parent_class;

  /* width of image or 0 if it has no width (optional) */
  int          (* get_width)                       (GtkCssImage                *image);
  /* height of image or 0 if it has no height (optional) */
  int          (* get_height)                      (GtkCssImage                *image);
  /* aspect ratio (width / height) of image or 0 if it has no aspect ratio (optional) */
  double       (* get_aspect_ratio)                (GtkCssImage                *image);

  /* create "computed value" in CSS terms, returns a new reference */
  GtkCssImage *(* compute)                         (GtkCssImage                *image,
                                                    guint                       property_id,
                                                    GtkStyleProviderPrivate    *provider,
                                                    GtkCssStyle                *style,
                                                    GtkCssStyle                *parent_style,
                                                    GtkCssDependencies         *dependencies);
  /* compare two images for equality */
  gboolean     (* equal)                           (GtkCssImage                *image1,
                                                    GtkCssImage                *image2);
  /* transition between start and end image (end may be NULL), returns new reference (optional) */
  GtkCssImage *(* transition)                      (GtkCssImage                *start,
                                                    GtkCssImage                *end,
                                                    guint                       property_id,
                                                    double                      progress);

  /* draw to 0,0 with the given width and height */
  void         (* draw)                            (GtkCssImage                *image,
                                                    cairo_t                    *cr,
                                                    double                      width,
                                                    double                      height);
  /* parse CSS, return TRUE on success */
  gboolean     (* parse)                           (GtkCssImage                *image,
                                                    GtkCssParser               *parser);
  /* print to CSS */
  void         (* print)                           (GtkCssImage                *image,
                                                    GString                    *string);
};

GType          _gtk_css_image_get_type             (void) G_GNUC_CONST;

gboolean       _gtk_css_image_can_parse            (GtkCssParser               *parser);
GtkCssImage *  _gtk_css_image_new_parse            (GtkCssParser               *parser);

int            _gtk_css_image_get_width            (GtkCssImage                *image);
int            _gtk_css_image_get_height           (GtkCssImage                *image);
double         _gtk_css_image_get_aspect_ratio     (GtkCssImage                *image);

GtkCssImage *  _gtk_css_image_compute              (GtkCssImage                *image,
                                                    guint                       property_id,
                                                    GtkStyleProviderPrivate    *provider,
                                                    GtkCssStyle                *style,
                                                    GtkCssStyle                *parent_style,
                                                    GtkCssDependencies         *dependencies);
gboolean       _gtk_css_image_equal                (GtkCssImage                *image1,
                                                    GtkCssImage                *image2);
GtkCssImage *  _gtk_css_image_transition           (GtkCssImage                *start,
                                                    GtkCssImage                *end,
                                                    guint                       property_id,
                                                    double                      progress);

void           _gtk_css_image_draw                 (GtkCssImage                *image,
                                                    cairo_t                    *cr,
                                                    double                      width,
                                                    double                      height);
void           _gtk_css_image_print                (GtkCssImage                *image,
                                                    GString                    *string);

void           _gtk_css_image_get_concrete_size    (GtkCssImage                *image,
                                                    double                      specified_width,
                                                    double                      specified_height,
                                                    double                      default_width,
                                                    double                      default_height,
                                                    double                     *concrete_width,
                                                    double                     *concrete_height);
cairo_surface_t *
               _gtk_css_image_get_surface          (GtkCssImage                *image,
                                                    cairo_surface_t            *target,
                                                    int                         surface_width,
                                                    int                         surface_height);

G_END_DECLS

#endif /* __GTK_CSS_IMAGE_PRIVATE_H__ */
