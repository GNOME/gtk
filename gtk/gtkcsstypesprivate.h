/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_CSS_TYPES_PRIVATE_H__
#define __GTK_CSS_TYPES_PRIVATE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
  GTK_CSS_REPEAT_STYLE_NONE,
  GTK_CSS_REPEAT_STYLE_REPEAT,
  GTK_CSS_REPEAT_STYLE_ROUND,
  GTK_CSS_REPEAT_STYLE_SPACE
} GtkCssRepeatStyle;

typedef struct _GtkCssBorderCornerRadius GtkCssBorderCornerRadius;
typedef struct _GtkCssBorderRadius GtkCssBorderRadius;
typedef struct _GtkCssBorderImageRepeat GtkCssBorderImageRepeat;

struct _GtkCssBorderCornerRadius {
  double horizontal;
  double vertical;
};

struct _GtkCssBorderRadius {
  GtkCssBorderCornerRadius top_left;
  GtkCssBorderCornerRadius top_right;
  GtkCssBorderCornerRadius bottom_right;
  GtkCssBorderCornerRadius bottom_left;
};

struct _GtkCssBorderImageRepeat {
  GtkCssRepeatStyle vrepeat;
  GtkCssRepeatStyle hrepeat;
};

#define GTK_TYPE_CSS_BORDER_CORNER_RADIUS _gtk_css_border_corner_radius_get_type ()
#define GTK_TYPE_CSS_BORDER_RADIUS _gtk_css_border_radius_get_type ()
#define GTK_TYPE_CSS_BORDER_IMAGE_REPEAT _gtk_css_border_image_repeat_get_type ()

GType           _gtk_css_border_corner_radius_get_type          (void);
GType           _gtk_css_border_radius_get_type                 (void);
GType           _gtk_css_border_image_repeat_get_type           (void);

G_END_DECLS

#endif /* __GTK_CSS_TYPES_PRIVATE_H__ */
