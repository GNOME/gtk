/*
 * Copyright © 2021 Red Hat Inc.
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

#ifndef __GTK_CSS_IMAGE_FILTER_PRIVATE_H__
#define __GTK_CSS_IMAGE_FILTER_PRIVATE_H__

#include "gtk/gtkcssimageprivate.h"
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_FILTER           (gtk_css_image_filter_get_type ())
#define GTK_CSS_IMAGE_FILTER(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_FILTER, GtkCssImageFilter))
#define GTK_CSS_IMAGE_FILTER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_FILTER, GtkCssImageFilterClass))
#define GTK_IS_CSS_IMAGE_FILTER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_FILTER))
#define GTK_IS_CSS_IMAGE_FILTER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_FILTER))
#define GTK_CSS_IMAGE_FILTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_FILTER, GtkCssImageFilterClass))

typedef struct _GtkCssImageFilter           GtkCssImageFilter;
typedef struct _GtkCssImageFilterClass      GtkCssImageFilterClass;

struct _GtkCssImageFilter
{
  GtkCssImage parent;

  GtkCssImage *image;
  GtkCssValue *filter;
};

struct _GtkCssImageFilterClass
{
  GtkCssImageClass parent_class;
};

GType          gtk_css_image_filter_get_type              (void) G_GNUC_CONST;

GtkCssImage *  gtk_css_image_filter_new                  (GtkCssImage *image,
                                                          GtkCssValue *filter);

G_END_DECLS

#endif /* __GTK_CSS_IMAGE_FILTER_PRIVATE_H__ */
