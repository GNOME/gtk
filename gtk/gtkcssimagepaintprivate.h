/*
 * Copyright © 2018 Benjamin Otte
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

#ifndef __GTK_CSS_IMAGE_PAINT_PRIVATE_H__
#define __GTK_CSS_IMAGE_PAINT_PRIVATE_H__

#include "gtk/gtkcssimageprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_PAINT           (gtk_css_image_paint_get_type ())
#define GTK_CSS_IMAGE_PAINT(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_PAINT, GtkCssImagePaint))
#define GTK_CSS_IMAGE_PAINT_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_PAINT, GtkCssImagePaintClass))
#define GTK_IS_CSS_IMAGE_PAINT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_PAINT))
#define GTK_IS_CSS_IMAGE_PAINT_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_PAINT))
#define GTK_CSS_IMAGE_PAINT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_PAINT, GtkCssImagePaintClass))

typedef struct _GtkCssImagePaint           GtkCssImagePaint;
typedef struct _GtkCssImagePaintClass      GtkCssImagePaintClass;

struct _GtkCssImagePaint
{
  GtkCssImage parent;

  char          *name;                /* the name of the paint */
};

struct _GtkCssImagePaintClass
{
  GtkCssImageClass parent_class;
};

GType           gtk_css_image_paint_get_type                    (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GTK_CSS_IMAGE_PAINT_PRIVATE_H__ */
