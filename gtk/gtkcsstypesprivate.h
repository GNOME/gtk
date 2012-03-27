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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CSS_TYPES_PRIVATE_H__
#define __GTK_CSS_TYPES_PRIVATE_H__

#include <glib-object.h>
#include <gtk/gtkstylecontext.h>

G_BEGIN_DECLS

typedef enum { /*< skip >*/
  GTK_CSS_CHANGE_CLASS                    = (1 <<  0),
  GTK_CSS_CHANGE_NAME                     = (1 <<  1),
  GTK_CSS_CHANGE_ID                       = GTK_CSS_CHANGE_NAME,
  GTK_CSS_CHANGE_REGION                   = GTK_CSS_CHANGE_NAME,
  GTK_CSS_CHANGE_POSITION                 = (1 <<  2),
  GTK_CSS_CHANGE_STATE                    = (1 <<  3),
  GTK_CSS_CHANGE_SIBLING_CLASS            = (1 <<  4),
  GTK_CSS_CHANGE_SIBLING_NAME             = (1 <<  5),
  GTK_CSS_CHANGE_SIBLING_POSITION         = (1 <<  6),
  GTK_CSS_CHANGE_SIBLING_STATE            = (1 <<  7),
  GTK_CSS_CHANGE_PARENT_CLASS             = (1 <<  8),
  GTK_CSS_CHANGE_PARENT_NAME              = (1 <<  9),
  GTK_CSS_CHANGE_PARENT_POSITION          = (1 << 10),
  GTK_CSS_CHANGE_PARENT_STATE             = (1 << 11),
  GTK_CSS_CHANGE_PARENT_SIBLING_CLASS     = (1 << 12),
  GTK_CSS_CHANGE_PARENT_SIBLING_NAME      = (1 << 13),
  GTK_CSS_CHANGE_PARENT_SIBLING_POSITION  = (1 << 14),
  GTK_CSS_CHANGE_PARENT_SIBLING_STATE     = (1 << 15),
  /* add more */
} GtkCssChange;

#define GTK_CSS_CHANGE_ANY ((1 << 16) - 1)
#define GTK_CSS_CHANGE_ANY_SELF (GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_STATE)
#define GTK_CSS_CHANGE_ANY_SIBLING (GTK_CSS_CHANGE_SIBLING_CLASS | GTK_CSS_CHANGE_SIBLING_NAME | \
                                    GTK_CSS_CHANGE_SIBLING_POSITION | GTK_CSS_CHANGE_SIBLING_STATE)
#define GTK_CSS_CHANGE_ANY_PARENT (GTK_CSS_CHANGE_PARENT_CLASS | GTK_CSS_CHANGE_PARENT_SIBLING_CLASS | \
                                   GTK_CSS_CHANGE_PARENT_NAME | GTK_CSS_CHANGE_PARENT_SIBLING_NAME | \
                                   GTK_CSS_CHANGE_PARENT_POSITION | GTK_CSS_CHANGE_PARENT_SIBLING_POSITION | \
                                   GTK_CSS_CHANGE_PARENT_STATE | GTK_CSS_CHANGE_PARENT_SIBLING_STATE)

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

typedef enum /*< skip >*/ {
  /* CSS term: <number> */
  GTK_CSS_NUMBER,
  /* CSS term: <percentage> */
  GTK_CSS_PERCENT,
  /* CSS term: <length> */
  GTK_CSS_PX,
  GTK_CSS_PT,
  GTK_CSS_EM,
  GTK_CSS_EX,
  GTK_CSS_PC,
  GTK_CSS_IN,
  GTK_CSS_CM,
  GTK_CSS_MM,
  /* CSS term: <angle> */
  GTK_CSS_RAD,
  GTK_CSS_DEG,
  GTK_CSS_GRAD,
  GTK_CSS_TURN
} GtkCssUnit;

typedef struct _GtkCssNumber GtkCssNumber;
typedef struct _GtkCssBackgroundSize GtkCssBackgroundSize;
typedef struct _GtkCssBackgroundPosition GtkCssBackgroundPosition;
typedef struct _GtkCssBorderCornerRadius GtkCssBorderCornerRadius;
typedef struct _GtkCssBorderImageRepeat GtkCssBorderImageRepeat;

struct _GtkCssNumber {
  gdouble        value;
  GtkCssUnit     unit;
};

struct _GtkCssBackgroundSize {
  GtkCssNumber width;  /* 0 means auto here */
  GtkCssNumber height; /* 0 means auto here */
  guint cover :1;
  guint contain :1;
};

struct _GtkCssBackgroundPosition {
  GtkCssNumber x;
  GtkCssNumber y;
};

struct _GtkCssBorderCornerRadius {
  GtkCssNumber horizontal;
  GtkCssNumber vertical;
};

struct _GtkCssBorderImageRepeat {
  GtkCssBorderRepeatStyle vrepeat;
  GtkCssBorderRepeatStyle hrepeat;
};

#define GTK_TYPE_CSS_BACKGROUND_SIZE _gtk_css_background_size_get_type ()
#define GTK_TYPE_CSS_BACKGROUND_POSITION _gtk_css_background_position_get_type ()
#define GTK_TYPE_CSS_BORDER_CORNER_RADIUS _gtk_css_border_corner_radius_get_type ()
#define GTK_TYPE_CSS_BORDER_IMAGE_REPEAT _gtk_css_border_image_repeat_get_type ()

GType           _gtk_css_background_size_get_type               (void);
GType           _gtk_css_background_position_get_type           (void);
GType           _gtk_css_border_corner_radius_get_type          (void);
GType           _gtk_css_border_image_repeat_get_type           (void);

GtkCssChange    _gtk_css_change_for_sibling                      (GtkCssChange       match);
GtkCssChange    _gtk_css_change_for_child                        (GtkCssChange       match);

#define GTK_CSS_NUMBER_INIT(_value,_unit) { (_value), (_unit) }
void            _gtk_css_number_init                            (GtkCssNumber       *number,
                                                                 double              value,
                                                                 GtkCssUnit          unit);
gboolean        _gtk_css_number_equal                           (const GtkCssNumber *one,
                                                                 const GtkCssNumber *two);
double          _gtk_css_number_get                             (const GtkCssNumber *number,
                                                                 double              one_hundred_percent);
gboolean        _gtk_css_number_compute                         (GtkCssNumber       *dest,
                                                                 const GtkCssNumber *src,
                                                                 GtkStyleContext    *context);
void            _gtk_css_number_print                           (const GtkCssNumber *number,
                                                                 GString            *string);


G_END_DECLS

#endif /* __GTK_CSS_TYPES_PRIVATE_H__ */
