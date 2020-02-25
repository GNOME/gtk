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

#define GTK_CSS_CHANGE_CLASS                          (1ULL <<  0)
#define GTK_CSS_CHANGE_NAME                           (1ULL <<  1)
#define GTK_CSS_CHANGE_ID                             (1ULL <<  2)
#define GTK_CSS_CHANGE_FIRST_CHILD                    (1ULL <<  3)
#define GTK_CSS_CHANGE_LAST_CHILD                     (1ULL <<  4)
#define GTK_CSS_CHANGE_NTH_CHILD                      (1ULL <<  5)
#define GTK_CSS_CHANGE_NTH_LAST_CHILD                 (1ULL <<  6)
#define GTK_CSS_CHANGE_STATE                          (1ULL <<  7)
#define GTK_CSS_CHANGE_SIBLING_CLASS                  (1ULL <<  8)
#define GTK_CSS_CHANGE_SIBLING_NAME                   (1ULL <<  9)
#define GTK_CSS_CHANGE_SIBLING_ID                     (1ULL << 10)
#define GTK_CSS_CHANGE_SIBLING_FIRST_CHILD            (1ULL << 11)
#define GTK_CSS_CHANGE_SIBLING_LAST_CHILD             (1ULL << 12)
#define GTK_CSS_CHANGE_SIBLING_NTH_CHILD              (1ULL << 13)
#define GTK_CSS_CHANGE_SIBLING_NTH_LAST_CHILD         (1ULL << 14)
#define GTK_CSS_CHANGE_SIBLING_STATE                  (1ULL << 15)
#define GTK_CSS_CHANGE_PARENT_CLASS                   (1ULL << 16)
#define GTK_CSS_CHANGE_PARENT_NAME                    (1ULL << 17)
#define GTK_CSS_CHANGE_PARENT_ID                      (1ULL << 18)
#define GTK_CSS_CHANGE_PARENT_FIRST_CHILD             (1ULL << 19)
#define GTK_CSS_CHANGE_PARENT_LAST_CHILD              (1ULL << 20)
#define GTK_CSS_CHANGE_PARENT_NTH_CHILD               (1ULL << 21)
#define GTK_CSS_CHANGE_PARENT_NTH_LAST_CHILD          (1ULL << 22)
#define GTK_CSS_CHANGE_PARENT_STATE                   (1ULL << 23)
#define GTK_CSS_CHANGE_PARENT_SIBLING_CLASS           (1ULL << 24)
#define GTK_CSS_CHANGE_PARENT_SIBLING_ID              (1ULL << 25)
#define GTK_CSS_CHANGE_PARENT_SIBLING_NAME            (1ULL << 26)
#define GTK_CSS_CHANGE_PARENT_SIBLING_FIRST_CHILD     (1ULL << 27)
#define GTK_CSS_CHANGE_PARENT_SIBLING_LAST_CHILD      (1ULL << 28)
#define GTK_CSS_CHANGE_PARENT_SIBLING_NTH_CHILD       (1ULL << 29)
#define GTK_CSS_CHANGE_PARENT_SIBLING_NTH_LAST_CHILD  (1ULL << 30)
#define GTK_CSS_CHANGE_PARENT_SIBLING_STATE           (1ULL << 31)

/* add more */
#define GTK_CSS_CHANGE_SOURCE                         (1ULL << 32)
#define GTK_CSS_CHANGE_PARENT_STYLE                   (1ULL << 33)
#define GTK_CSS_CHANGE_TIMESTAMP                      (1ULL << 34)
#define GTK_CSS_CHANGE_ANIMATIONS                     (1ULL << 35)

#define GTK_CSS_CHANGE_RESERVED_BIT                   (1ULL << 62) /* Used internally in gtkcssselector.c */

typedef guint64 GtkCssChange;

#define GTK_CSS_CHANGE_POSITION (GTK_CSS_CHANGE_FIRST_CHILD | GTK_CSS_CHANGE_LAST_CHILD | \
                                 GTK_CSS_CHANGE_NTH_CHILD | GTK_CSS_CHANGE_NTH_LAST_CHILD)
#define GTK_CSS_CHANGE_SIBLING_POSITION (GTK_CSS_CHANGE_SIBLING_FIRST_CHILD | GTK_CSS_CHANGE_SIBLING_LAST_CHILD | \
                                         GTK_CSS_CHANGE_SIBLING_NTH_CHILD | GTK_CSS_CHANGE_SIBLING_NTH_LAST_CHILD)
#define GTK_CSS_CHANGE_PARENT_POSITION (GTK_CSS_CHANGE_PARENT_FIRST_CHILD | GTK_CSS_CHANGE_PARENT_LAST_CHILD | \
                                        GTK_CSS_CHANGE_PARENT_NTH_CHILD | GTK_CSS_CHANGE_PARENT_NTH_LAST_CHILD)
#define GTK_CSS_CHANGE_PARENT_SIBLING_POSITION (GTK_CSS_CHANGE_PARENT_SIBLING_FIRST_CHILD | GTK_CSS_CHANGE_PARENT_SIBLING_LAST_CHILD | \
                                                GTK_CSS_CHANGE_PARENT_SIBLING_NTH_CHILD | GTK_CSS_CHANGE_PARENT_SIBLING_NTH_LAST_CHILD)


#define GTK_CSS_CHANGE_ANY ((1 << 19) - 1)
#define GTK_CSS_CHANGE_ANY_SELF (GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_ID | GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_STATE)
#define GTK_CSS_CHANGE_ANY_SIBLING (GTK_CSS_CHANGE_SIBLING_CLASS | GTK_CSS_CHANGE_SIBLING_NAME | \
                                    GTK_CSS_CHANGE_SIBLING_ID | \
                                    GTK_CSS_CHANGE_SIBLING_POSITION | GTK_CSS_CHANGE_SIBLING_STATE)
#define GTK_CSS_CHANGE_ANY_PARENT (GTK_CSS_CHANGE_PARENT_CLASS | GTK_CSS_CHANGE_PARENT_SIBLING_CLASS | \
                                   GTK_CSS_CHANGE_PARENT_NAME | GTK_CSS_CHANGE_PARENT_SIBLING_NAME | \
                                   GTK_CSS_CHANGE_PARENT_ID | GTK_CSS_CHANGE_PARENT_SIBLING_ID | \
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
 * @GTK_CSS_AFFECTS_TEXT_ATTRS: Text attributes are affected.
 * @GTK_CSS_AFFECTS_ICON: Fullcolor icons and their rendering is affected.
 * @GTK_CSS_AFFECTS_SYMBOLIC_ICON: Symbolic icons and their rendering is affected.
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
  GTK_CSS_AFFECTS_TEXT_ATTRS = (1 << 5),
  GTK_CSS_AFFECTS_ICON = (1 << 6),
  GTK_CSS_AFFECTS_SYMBOLIC_ICON = (1 << 7),
  GTK_CSS_AFFECTS_OUTLINE = (1 << 8),
  GTK_CSS_AFFECTS_CLIP = (1 << 9),
  GTK_CSS_AFFECTS_SIZE = (1 << 10)
} GtkCssAffects;

#define GTK_CSS_AFFECTS_REDRAW (GTK_CSS_AFFECTS_FOREGROUND |    \
                                GTK_CSS_AFFECTS_BACKGROUND |    \
                                GTK_CSS_AFFECTS_BORDER |        \
                                GTK_CSS_AFFECTS_ICON |          \
                                GTK_CSS_AFFECTS_SYMBOLIC_ICON | \
                                GTK_CSS_AFFECTS_OUTLINE)

enum { /*< skip >*/
  GTK_CSS_PROPERTY_COLOR,
  GTK_CSS_PROPERTY_DPI,
  GTK_CSS_PROPERTY_FONT_SIZE,
  GTK_CSS_PROPERTY_ICON_THEME,
  GTK_CSS_PROPERTY_ICON_PALETTE,
  GTK_CSS_PROPERTY_BACKGROUND_COLOR,
  GTK_CSS_PROPERTY_FONT_FAMILY,
  GTK_CSS_PROPERTY_FONT_STYLE,
  GTK_CSS_PROPERTY_FONT_VARIANT,
  GTK_CSS_PROPERTY_FONT_WEIGHT,
  GTK_CSS_PROPERTY_FONT_STRETCH,
  GTK_CSS_PROPERTY_LETTER_SPACING,
  GTK_CSS_PROPERTY_TEXT_DECORATION_LINE,
  GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR,
  GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE,
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
  GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE,
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
  GTK_CSS_PROPERTY_ICON_EFFECT,
  GTK_CSS_PROPERTY_ENGINE,
  GTK_CSS_PROPERTY_GTK_KEY_BINDINGS,
  GTK_CSS_PROPERTY_CARET_COLOR,
  GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR,
  GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS,
  /* add more */
  GTK_CSS_PROPERTY_N_PROPERTIES
};

typedef enum /*< skip >*/ {
  GTK_CSS_BLEND_MODE_COLOR,
  GTK_CSS_BLEND_MODE_COLOR_BURN,
  GTK_CSS_BLEND_MODE_COLOR_DODGE,
  GTK_CSS_BLEND_MODE_DARKEN,
  GTK_CSS_BLEND_MODE_DIFFERENCE,
  GTK_CSS_BLEND_MODE_EXCLUSION,
  GTK_CSS_BLEND_MODE_HARD_LIGHT,
  GTK_CSS_BLEND_MODE_HUE,
  GTK_CSS_BLEND_MODE_LIGHTEN,
  GTK_CSS_BLEND_MODE_LUMINOSITY,
  GTK_CSS_BLEND_MODE_MULTIPLY,
  GTK_CSS_BLEND_MODE_NORMAL,
  GTK_CSS_BLEND_MODE_OVERLAY,
  GTK_CSS_BLEND_MODE_SATURATE,
  GTK_CSS_BLEND_MODE_SCREEN,
  GTK_CSS_BLEND_MODE_SOFT_LIGHT
} GtkCssBlendMode;

typedef enum /*< skip >*/ {
  GTK_CSS_IMAGE_BUILTIN_NONE,
  GTK_CSS_IMAGE_BUILTIN_CHECK,
  GTK_CSS_IMAGE_BUILTIN_CHECK_INCONSISTENT,
  GTK_CSS_IMAGE_BUILTIN_OPTION,
  GTK_CSS_IMAGE_BUILTIN_OPTION_INCONSISTENT,
  GTK_CSS_IMAGE_BUILTIN_ARROW_UP,
  GTK_CSS_IMAGE_BUILTIN_ARROW_DOWN,
  GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT,
  GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT,
  GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_LEFT,
  GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT,
  GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_RIGHT,
  GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT,
  GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_LEFT_EXPANDED,
  GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT_EXPANDED,
  GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_RIGHT_EXPANDED,
  GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT_EXPANDED,
  GTK_CSS_IMAGE_BUILTIN_GRIP_TOPLEFT,
  GTK_CSS_IMAGE_BUILTIN_GRIP_TOP,
  GTK_CSS_IMAGE_BUILTIN_GRIP_TOPRIGHT,
  GTK_CSS_IMAGE_BUILTIN_GRIP_RIGHT,
  GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOMRIGHT,
  GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOM,
  GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOMLEFT,
  GTK_CSS_IMAGE_BUILTIN_GRIP_LEFT,
  GTK_CSS_IMAGE_BUILTIN_PANE_SEPARATOR,
  GTK_CSS_IMAGE_BUILTIN_HANDLE,
  GTK_CSS_IMAGE_BUILTIN_SPINNER
} GtkCssImageBuiltinType;

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
  GTK_CSS_ICON_EFFECT_NONE,
  GTK_CSS_ICON_EFFECT_HIGHLIGHT,
  GTK_CSS_ICON_EFFECT_DIM
} GtkCssIconEffect;

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

typedef enum /*< skip >*/ {
  GTK_CSS_TEXT_DECORATION_LINE_NONE,
  GTK_CSS_TEXT_DECORATION_LINE_UNDERLINE,
  GTK_CSS_TEXT_DECORATION_LINE_LINE_THROUGH
} GtkTextDecorationLine;

typedef enum /*< skip >*/ {
  GTK_CSS_TEXT_DECORATION_STYLE_SOLID,
  GTK_CSS_TEXT_DECORATION_STYLE_DOUBLE,
  GTK_CSS_TEXT_DECORATION_STYLE_WAVY
} GtkTextDecorationStyle;

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
  GTK_CSS_DIMENSION_PERCENTAGE,
  GTK_CSS_DIMENSION_NUMBER,
  GTK_CSS_DIMENSION_LENGTH,
  GTK_CSS_DIMENSION_ANGLE,
  GTK_CSS_DIMENSION_TIME
} GtkCssDimension;

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
  GTK_CSS_REM,
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

cairo_operator_t        _gtk_css_blend_mode_get_operator         (GtkCssBlendMode    mode);

GtkCssChange            _gtk_css_change_for_sibling              (GtkCssChange       match);
GtkCssChange            _gtk_css_change_for_child                (GtkCssChange       match);

GtkCssDimension         gtk_css_unit_get_dimension               (GtkCssUnit         unit);

char *                  gtk_css_change_to_string                 (GtkCssChange       change);
void                    gtk_css_change_print                     (GtkCssChange       change,
                                                                  GString           *string);

/* for lack of better place to put it */
/* mirror what cairo does */
#define gtk_rgba_is_clear(rgba) ((rgba)->alpha < ((double)0x00ff / (double)0xffff))

G_END_DECLS

#endif /* __GTK_CSS_TYPES_PRIVATE_H__ */
