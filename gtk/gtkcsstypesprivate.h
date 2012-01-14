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
  GTK_CSS_INHERIT,
  GTK_CSS_INITIAL
} GtkCssSpecialValue;

/* We encode horizontal and vertical repeat in one enum value.
 * This eases parsing and storage, but you need to be aware that
 * you have to "unpack" this value.
 */
#define GTK_CSS_BACKGROUND_REPEAT_SHIFT (8)
#define GTK_CSS_BACKGROUND_REPEAT_MASK ((1 << GTK_CSS_BACKGROUND_REPEAT_SHIFT) - 1)
#define GTK_CSS_BACKGROUND_HORIZONTAL(repeat) ((repeat) & GTK_CSS_BACKGROUND_REPEAT_MASK)
#define GTK_CSS_BACKGROUND_VERTICAL(repeat) (((repeat) >> GTK_CSS_BACKGROUND_REPEAT_SHIFT) & GTK_CSS_BACKGROUND_REPEAT_MASK)
typedef enum /*< enum >*/
{
  GTK_CSS_BACKGROUND_INVALID, /*< skip >*/
  GTK_CSS_BACKGROUND_REPEAT, /* start at one so we know if a value has been set */
  GTK_CSS_BACKGROUND_SPACE,
  GTK_CSS_BACKGROUND_ROUND,
  GTK_CSS_BACKGROUND_NO_REPEAT,
  /* need to hardcode the numer or glib-mkenums makes us into a flags type */
  GTK_CSS_BACKGROUND_REPEAT_X = 1025,
  GTK_CSS_BACKGROUND_REPEAT_Y = 260
} GtkCssBackgroundRepeat;

typedef enum {
  GTK_CSS_REPEAT_STYLE_STRETCH,
  GTK_CSS_REPEAT_STYLE_REPEAT,
  GTK_CSS_REPEAT_STYLE_ROUND,
  GTK_CSS_REPEAT_STYLE_SPACE
} GtkCssBorderRepeatStyle;

typedef enum {
  GTK_CSS_AREA_BORDER_BOX,
  GTK_CSS_AREA_PADDING_BOX,
  GTK_CSS_AREA_CONTENT_BOX
} GtkCssArea;

/* for the order in arrays */
typedef enum /*< skip >*/ {
  GTK_CSS_TOP,
  GTK_CSS_RIGHT,
  GTK_CSS_BOTTOM,
  GTK_CSS_LEFT
} GtkCssSide;

typedef enum /*< skip >*/ {
  GTK_CSS_TOP_LEFT,
  GTK_CSS_TOP_RIGHT,
  GTK_CSS_BOTTOM_RIGHT,
  GTK_CSS_BOTTOM_LEFT
} GtkCssCorner;

typedef struct _GtkCssBorderCornerRadius GtkCssBorderCornerRadius;
typedef struct _GtkCssBorderImageRepeat GtkCssBorderImageRepeat;

struct _GtkCssBorderCornerRadius {
  double horizontal;
  double vertical;
};

struct _GtkCssBorderImageRepeat {
  GtkCssBorderRepeatStyle vrepeat;
  GtkCssBorderRepeatStyle hrepeat;
};

#define GTK_TYPE_CSS_BORDER_CORNER_RADIUS _gtk_css_border_corner_radius_get_type ()
#define GTK_TYPE_CSS_BORDER_RADIUS _gtk_css_border_radius_get_type ()
#define GTK_TYPE_CSS_BORDER_IMAGE_REPEAT _gtk_css_border_image_repeat_get_type ()

GType           _gtk_css_border_corner_radius_get_type          (void);
GType           _gtk_css_border_radius_get_type                 (void);
GType           _gtk_css_border_image_repeat_get_type           (void);

G_END_DECLS

#endif /* __GTK_CSS_TYPES_PRIVATE_H__ */
