/*
 * Copyright © 2012 Red Hat Inc.
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

#ifndef __GTK_CSS_IMAGE_BUILTIN_PRIVATE_H__
#define __GTK_CSS_IMAGE_BUILTIN_PRIVATE_H__

#include "gtk/gtkcssimageprivate.h"
#include "gtk/gtkicontheme.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_BUILTIN           (gtk_css_image_builtin_get_type ())
#define GTK_CSS_IMAGE_BUILTIN(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_BUILTIN, GtkCssImageBuiltin))
#define GTK_CSS_IMAGE_BUILTIN_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_BUILTIN, GtkCssImageBuiltinClass))
#define GTK_IS_CSS_IMAGE_BUILTIN(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_BUILTIN))
#define GTK_IS_CSS_IMAGE_BUILTIN_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_BUILTIN))
#define GTK_CSS_IMAGE_BUILTIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_BUILTIN, GtkCssImageBuiltinClass))

typedef struct _GtkCssImageBuiltin           GtkCssImageBuiltin;
typedef struct _GtkCssImageBuiltinClass      GtkCssImageBuiltinClass;

struct _GtkCssImageBuiltin
{
  GtkCssImage   parent;

  GdkRGBA       fg_color;
  GdkRGBA       bg_color;
};

struct _GtkCssImageBuiltinClass
{
  GtkCssImageClass parent_class;
};

GType          gtk_css_image_builtin_get_type              (void) G_GNUC_CONST;

GtkCssImage *  gtk_css_image_builtin_new                   (void);

void           gtk_css_image_builtin_draw                  (GtkCssImage                 *image,
                                                            cairo_t                     *cr,
                                                            double                       width,
                                                            double                       height,
                                                            GtkCssImageBuiltinType       image_type);
void           gtk_css_image_builtin_snapshot              (GtkCssImage                 *image,
                                                            GtkSnapshot                 *snapshot,
                                                            double                       width,
                                                            double                       height,
                                                            GtkCssImageBuiltinType       image_type);

G_END_DECLS

#endif /* __GTK_CSS_IMAGE_BUILTIN_PRIVATE_H__ */
