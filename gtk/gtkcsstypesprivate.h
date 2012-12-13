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

typedef union _GtkCssMatcher GtkCssMatcher;
typedef struct _GtkCssNode GtkCssNode;
typedef struct _GtkCssNodeDeclaration GtkCssNodeDeclaration;
typedef struct _GtkCssStyle GtkCssStyle;
typedef struct _GtkStyleProviderPrivate GtkStyleProviderPrivate; /* dummy typedef */

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
  GTK_CSS_CHANGE_PARENT_REGION            = GTK_CSS_CHANGE_PARENT_NAME,
  GTK_CSS_CHANGE_PARENT_POSITION          = (1 << 10),
  GTK_CSS_CHANGE_PARENT_STATE             = (1 << 11),
  GTK_CSS_CHANGE_PARENT_SIBLING_CLASS     = (1 << 12),
  GTK_CSS_CHANGE_PARENT_SIBLING_NAME      = (1 << 13),
  GTK_CSS_CHANGE_PARENT_SIBLING_POSITION  = (1 << 14),
  GTK_CSS_CHANGE_PARENT_SIBLING_STATE     = (1 << 15),
  /* add more */
  GTK_CSS_CHANGE_SOURCE                   = (1 << 16),
  GTK_CSS_CHANGE_PARENT_STYLE             = (1 << 17),
  GTK_CSS_CHANGE_TIMESTAMP                = (1 << 18),
  GTK_CSS_CHANGE_ANIMATIONS               = (1 << 19),

  GTK_CSS_CHANGE_RESERVED_BIT             = (1 << 31) /* Used internally in gtkcssselector.c */
} GtkCssChange;

#define GTK_CSS_CHANGE_ANY ((1 << 19) - 1)
#define GTK_CSS_CHANGE_ANY_SELF (GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_STATE)
#define GTK_CSS_CHANGE_ANY_SIBLING (GTK_CSS_CHANGE_SIBLING_CLASS | GTK_CSS_CHANGE_SIBLING_NAME | \
                                    GTK_CSS_CHANGE_SIBLING_POSITION | GTK_CSS_CHANGE_SIBLING_STATE)
#define GTK_CSS_CHANGE_ANY_PARENT (GTK_CSS_CHANGE_PARENT_CLASS | GTK_CSS_CHANGE_PARENT_SIBLING_CLASS | \
                                   GTK_CSS_CHANGE_PARENT_NAME | GTK_CSS_CHANGE_PARENT_SIBLING_NAME | \
                                   GTK_CSS_CHANGE_PARENT_POSITION | GTK_CSS_CHANGE_PARENT_SIBLING_POSITION | \
                                   GTK_CSS_CHANGE_PARENT_STATE | GTK_CSS_CHANGE_PARENT_SIBLING_STATE)

/*
 * GtkCssAffects:
 * @GTK_CSS_AFFECTS_FOREGROUND: The foreground rendering is affected.
 *   This does not include things that affect the font. For those,
 *   see @GTK_CSS_AFFECTS_FONT.
 * @GTK_CSS_AFFECTS_BACKGROUND: The background rendering is affected.
 * @GTK_CSS_AFFECTS_BORDER: The border styling is affected.
 * @GTK_CSS_AFFECTS_PANGO_LAYOUT: Font rendering is affected.
 * @GTK_CSS_AFFECTS_FONT: The font is affected and should be reloaded
 *   if it was cached.
 * @GTK_CSS_AFFECTS_TEXT: Text rendering is affected.
 * @GTK_CSS_AFFECTS_ICON: Icons and icon rendering is affected.
 * @GTK_CSS_AFFECTS_OUTLINE: The outline styling is affected. Outlines
 *   only affect elements that can be focused.
 * @GTK_CSS_AFFECTS_CLIP: Changes in this property may have an effect
 *   on the clipping area of the element. Changes in these properties
 *   should cause a reevaluation of the element's clip area.
 * @GTK_CSS_AFFECTS_SIZE: Changes in this property may have an effect
 *   on the allocated size of the element. Changes in these properties
 *   should cause a recomputation of the element's allocated size.
 *
 * The generic effects that a CSS property can have. If a value is
 * set, then the property will have an influence on that feature.
 *
 * Note that multiple values can be set.
 */
typedef enum {
  GTK_CSS_AFFECTS_FOREGROUND = (1 << 0),
  GTK_CSS_AFFECTS_BACKGROUND = (1 << 1),
  GTK_CSS_AFFECTS_BORDER = (1 << 2),
  GTK_CSS_AFFECTS_FONT = (1 << 3),
  GTK_CSS_AFFECTS_TEXT = (1 << 4),
  GTK_CSS_AFFECTS_ICON = (1 << 5),
  GTK_CSS_AFFECTS_OUTLINE = (1 << 6),
  GTK_CSS_AFFECTS_CLIP = (1 << 7),
  GTK_CSS_AFFECTS_SIZE = (1 << 8)
} GtkCssAffects;

enum { /*< skip >*/
  GTK_CSS_PROPERTY_COLOR,
  GTK_CSS_PROPERTY_DPI,
  GTK_CSS_PROPERTY_FONT_SIZE,
  GTK_CSS_PROPERTY_ICON_THEME,
  GTK_CSS_PROPERTY_BACKGROUND_COLOR,
  GTK_CSS_PROPERTY_FONT_FAMILY,
  GTK_CSS_PROPERTY_FONT_STYLE,
  GTK_CSS_PROPERTY_FONT_VARIANT,
  GTK_CSS_PROPERTY_FONT_WEIGHT,
  GTK_CSS_PROPERTY_FONT_STRETCH,
  GTK_CSS_PROPERTY_TEXT_SHADOW,
  GTK_CSS_PROPERTY_BOX_SHADOW,
  GTK_CSS_PROPERTY_MARGIN_TOP,
  GTK_CSS_PROPERTY_MARGIN_LEFT,
  GTK_CSS_PROPERTY_MARGIN_BOTTOM,
  GTK_CSS_PROPERTY_MARGIN_RIGHT,
  GTK_CSS_PROPERTY_PADDING_TOP,
  GTK_CSS_PROPERTY_PADDING_LEFT,
  GTK_CSS_PROPERTY_PADDING_BOTTOM,
  GTK_CSS_PROPERTY_PADDING_RIGHT,
  GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
  GTK_CSS_PROPERTY_BORDER_TOP_WIDTH,
  GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
  GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
  GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
  GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH,
  GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_STYLE,
  GTK_CSS_PROPERTY_OUTLINE_WIDTH,
  GTK_CSS_PROPERTY_OUTLINE_OFFSET,
  GTK_CSS_PROPERTY_OUTLINE_TOP_LEFT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_TOP_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_BOTTOM_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_BOTTOM_LEFT_RADIUS,
  GTK_CSS_PROPERTY_BACKGROUND_CLIP,
  GTK_CSS_PROPERTY_BACKGROUND_ORIGIN,
  GTK_CSS_PROPERTY_BACKGROUND_SIZE,
  GTK_CSS_PROPERTY_BACKGROUND_POSITION,
  GTK_CSS_PROPERTY_BORDER_TOP_COLOR,
  GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
  GTK_CSS_PROPERTY_BORDER_LEFT_COLOR,
  GTK_CSS_PROPERTY_OUTLINE_COLOR,
  GTK_CSS_PROPERTY_BACKGROUND_REPEAT,
  GTK_CSS_PROPERTY_BACKGROUND_IMAGE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT,
  GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH,
  GTK_CSS_PROPERTY_ICON_SOURCE,
  GTK_CSS_PROPERTY_ICON_SHADOW,
  GTK_CSS_PROPERTY_ICON_STYLE,
  GTK_CSS_PROPERTY_ICON_TRANSFORM,
  GTK_CSS_PROPERTY_MIN_WIDTH,
  GTK_CSS_PROPERTY_MIN_HEIGHT,
  GTK_CSS_PROPERTY_TRANSITION_PROPERTY,
  GTK_CSS_PROPERTY_TRANSITION_DURATION,
  GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION,
  GTK_CSS_PROPERTY_TRANSITION_DELAY,
  GTK_CSS_PROPERTY_ANIMATION_NAME,
  GTK_CSS_PROPERTY_ANIMATION_DURATION,
  GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION,
  GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT,
  GTK_CSS_PROPERTY_ANIMATION_DIRECTION,
  GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE,
  GTK_CSS_PROPERTY_ANIMATION_DELAY,
  GTK_CSS_PROPERTY_ANIMATION_FILL_MODE,
  GTK_CSS_PROPERTY_OPACITY,
  GTK_CSS_PROPERTY_GTK_IMAGE_EFFECT,
  GTK_CSS_PROPERTY_ENGINE,
  GTK_CSS_PROPERTY_GTK_KEY_BINDINGS,
  /* add more */
  GTK_CSS_PROPERTY_N_PROPERTIES
};

typedef enum /*< skip >*/ {
  GTK_CSS_AREA_BORDER_BOX,
  GTK_CSS_AREA_PADDING_BOX,
  GTK_CSS_AREA_CONTENT_BOX
} GtkCssArea;

typedef enum /*< skip >*/ {
  GTK_CSS_DIRECTION_NORMAL,
  GTK_CSS_DIRECTION_REVERSE,
  GTK_CSS_DIRECTION_ALTERNATE,
  GTK_CSS_DIRECTION_ALTERNATE_REVERSE
} GtkCssDirection;

typedef enum /*< skip >*/ {
  GTK_CSS_PLAY_STATE_RUNNING,
  GTK_CSS_PLAY_STATE_PAUSED
} GtkCssPlayState;

typedef enum /*< skip >*/ {
  GTK_CSS_FILL_NONE,
  GTK_CSS_FILL_FORWARDS,
  GTK_CSS_FILL_BACKWARDS,
  GTK_CSS_FILL_BOTH
} GtkCssFillMode;

typedef enum /*< skip >*/ {
  GTK_CSS_IMAGE_EFFECT_NONE,
  GTK_CSS_IMAGE_EFFECT_HIGHLIGHT,
  GTK_CSS_IMAGE_EFFECT_DIM
} GtkCssImageEffect;

typedef enum /*< skip >*/ {
  GTK_CSS_ICON_STYLE_REQUESTED,
  GTK_CSS_ICON_STYLE_REGULAR,
  GTK_CSS_ICON_STYLE_SYMBOLIC
} GtkCssIconStyle;

typedef enum /*< skip >*/ {
  /* relative font sizes */
  GTK_CSS_FONT_SIZE_SMALLER,
  GTK_CSS_FONT_SIZE_LARGER,
  /* absolute font sizes */
  GTK_CSS_FONT_SIZE_XX_SMALL,
  GTK_CSS_FONT_SIZE_X_SMALL,
  GTK_CSS_FONT_SIZE_SMALL,
  GTK_CSS_FONT_SIZE_MEDIUM,
  GTK_CSS_FONT_SIZE_LARGE,
  GTK_CSS_FONT_SIZE_X_LARGE,
  GTK_CSS_FONT_SIZE_XX_LARGE
} GtkCssFontSize;

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
  GTK_CSS_TURN,
  /* CSS term: <time> */
  GTK_CSS_S,
  GTK_CSS_MS,
} GtkCssUnit;

GtkCssChange            _gtk_css_change_for_sibling              (GtkCssChange       match);
GtkCssChange            _gtk_css_change_for_child                (GtkCssChange       match);

/* for lack of better place to put it */
/* mirror what cairo does */
#define gtk_rgba_is_clear(rgba) ((rgba)->alpha < ((double)0x00ff / (double)0xffff))

G_END_DECLS

#endif /* __GTK_CSS_TYPES_PRIVATE_H__ */
