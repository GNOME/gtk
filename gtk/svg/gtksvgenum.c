/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "config.h"

#include "gtksvgenumprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgelementinternal.h"

typedef struct
{
  SvgValue base;
  unsigned int value;
  const char *name;
  size_t len;
} SvgEnum;

static gboolean
svg_enum_equal (const SvgValue *value0,
                const SvgValue *value1)
{
  const SvgEnum *n0 = (const SvgEnum *) value0;
  const SvgEnum *n1 = (const SvgEnum *) value1;

  return n0->value == n1->value;
}

static SvgValue *
svg_enum_interpolate (const SvgValue    *value0,
                      const SvgValue    *value1,
                      SvgComputeContext *context,
                      double             t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_enum_accumulate (const SvgValue    *value0,
                     const SvgValue    *value1,
                     SvgComputeContext *context,
                     int                n)
{
  return svg_value_ref ((SvgValue *) value0);
}

static void
svg_enum_print (const SvgValue *value,
                GString        *string)
{
  const SvgEnum *e = (const SvgEnum *) value;
  g_string_append (string, e->name);
}

unsigned int
svg_enum_get (const SvgValue *value)
{
  const SvgEnum *e = (const SvgEnum *) value;
  return e->value;
}

static SvgValue *
svg_enum_try_parse (const SvgEnum  values[],
                    unsigned int   n_values,
                    GtkCssParser  *parser)
{
  for (unsigned int i = 0; i < n_values; i++)
    {
      if (values[i].name &&
          gtk_css_parser_try_ident (parser, values[i].name))
        return svg_value_ref ((SvgValue *) &values[i]);
    }
  return NULL;
}

/* Note that name must be a string literal */
#define DEFINE_ENUM_VALUE(CLASS_NAME, value, name) \
  { { & SVG_ ## CLASS_NAME ## _CLASS, 0 }, value, name, sizeof (name) - 1, }

#define DEFINE_ENUM_VALUE_NO_NAME(CLASS_NAME, value) \
  { { & SVG_ ## CLASS_NAME ## _CLASS, 0 }, value, NULL, 0, }

#define DEF_E(CLASS_NAME, class_name, EnumType, resolve, ...) \
static const SvgValueClass SVG_ ## CLASS_NAME ## _CLASS = { \
  #CLASS_NAME, \
  svg_value_default_free, \
  svg_enum_equal, \
  svg_enum_interpolate, \
  svg_enum_accumulate, \
  svg_enum_print, \
  svg_value_default_distance, \
  resolve, \
}; \
\
static SvgEnum class_name ## _values[] = { \
  __VA_ARGS__ , \
}; \
\
SvgValue * \
svg_ ## class_name ## _new (EnumType value) \
{ \
  for (unsigned int i = 0; i < G_N_ELEMENTS (class_name ## _values); i++) \
    { \
      if (class_name ## _values[i].value == value) \
        return (SvgValue *) & class_name ## _values[i]; \
    } \
  g_assert_not_reached (); \
} \

#define DEF_E_PARSE(class_name, EnumType) \
SvgValue * \
svg_ ## class_name ## _try_parse (GtkCssParser *parser) \
{ \
  return svg_enum_try_parse (class_name ## _values, \
                             G_N_ELEMENTS (class_name ## _values), \
                             parser); \
} \
\
SvgValue * \
svg_ ## class_name ## _parse (GtkCssParser *parser) \
{ \
  SvgValue *value = svg_ ## class_name ## _try_parse (parser); \
  if (value == NULL) \
    gtk_css_parser_error_syntax (parser, "Unknown " #EnumType " value"); \
  return value; \
}

#define DEFINE_ENUM(CLASS_NAME, class_name, EnumType, ...) \
  DEF_E(CLASS_NAME, class_name, EnumType, svg_value_default_resolve, __VA_ARGS__) \
  DEF_E_PARSE(class_name, EnumType)

#define DEFINE_ENUM_NO_PARSE(CLASS_NAME, class_name, EnumType, ...) \
  DEF_E(CLASS_NAME, class_name, EnumType, svg_value_default_resolve, __VA_ARGS__)

#define DEFINE_ENUM_CUSTOM_RESOLVE(CLASS_NAME, class_name, EnumType, resolve, ...) \
  DEF_E(CLASS_NAME, class_name, EnumType, resolve, __VA_ARGS__) \
  DEF_E_PARSE(class_name, EnumType)

DEFINE_ENUM (FILL_RULE, fill_rule, GskFillRule,
  DEFINE_ENUM_VALUE (FILL_RULE, GSK_FILL_RULE_WINDING, "nonzero"),
  DEFINE_ENUM_VALUE (FILL_RULE, GSK_FILL_RULE_EVEN_ODD, "evenodd")
)

DEFINE_ENUM (MASK_TYPE, mask_type, GskMaskMode,
  DEFINE_ENUM_VALUE (MASK_TYPE, GSK_MASK_MODE_ALPHA, "alpha"),
  DEFINE_ENUM_VALUE (MASK_TYPE, GSK_MASK_MODE_LUMINANCE, "luminance")
)

DEFINE_ENUM (LINE_CAP, linecap, GskLineCap,
  DEFINE_ENUM_VALUE (LINE_CAP, GSK_LINE_CAP_BUTT, "butt"),
  DEFINE_ENUM_VALUE (LINE_CAP, GSK_LINE_CAP_ROUND, "round"),
  DEFINE_ENUM_VALUE (LINE_CAP, GSK_LINE_CAP_SQUARE, "square")
)

DEFINE_ENUM (LINE_JOIN, linejoin, GskLineJoin,
  DEFINE_ENUM_VALUE (LINE_JOIN, GSK_LINE_JOIN_MITER, "miter"),
  DEFINE_ENUM_VALUE (LINE_JOIN, GSK_LINE_JOIN_ROUND, "round"),
  DEFINE_ENUM_VALUE (LINE_JOIN, GSK_LINE_JOIN_BEVEL, "bevel")
)

DEFINE_ENUM_NO_PARSE (VISIBILITY, visibility, Visibility,
  DEFINE_ENUM_VALUE (VISIBILITY, VISIBILITY_HIDDEN, "hidden"),
  DEFINE_ENUM_VALUE (VISIBILITY, VISIBILITY_VISIBLE, "visible")
)

/* Accept other values too, but treat "collapse" as "hidden" */
SvgValue *
svg_visibility_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      Visibility visibility;

      if (strcmp (gtk_css_token_get_string (token), "hidden") == 0 ||
          strcmp (gtk_css_token_get_string (token), "collapse") == 0)
        visibility = VISIBILITY_HIDDEN;
      else
        visibility = VISIBILITY_VISIBLE;

      gtk_css_parser_skip (parser);

      return svg_visibility_new (visibility);
    }

  return NULL;
}

DEFINE_ENUM_NO_PARSE (DISPLAY, display, SvgDisplay,
  DEFINE_ENUM_VALUE (DISPLAY, DISPLAY_NONE, "none"),
  DEFINE_ENUM_VALUE (DISPLAY, DISPLAY_INLINE, "inline")
)

/* Accept other values too, but treat them all as "inline" */
SvgValue *
svg_display_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      SvgDisplay display;

      if (strcmp (gtk_css_token_get_string (token), "none") == 0)
        display = DISPLAY_NONE;
      else
        display = DISPLAY_INLINE;

      gtk_css_parser_skip (parser);

      return svg_display_new (display);
    }

  return NULL;
}

DEFINE_ENUM (SPREAD_METHOD, spread_method, GskRepeat,
  DEFINE_ENUM_VALUE (SPREAD_METHOD, GSK_REPEAT_PAD, "pad"),
  DEFINE_ENUM_VALUE (SPREAD_METHOD, GSK_REPEAT_REFLECT, "reflect"),
  DEFINE_ENUM_VALUE (SPREAD_METHOD, GSK_REPEAT_REPEAT, "repeat")
)

DEFINE_ENUM (COORD_UNITS, coord_units, CoordUnits,
  DEFINE_ENUM_VALUE (COORD_UNITS, COORD_UNITS_USER_SPACE_ON_USE, "userSpaceOnUse"),
  DEFINE_ENUM_VALUE (COORD_UNITS, COORD_UNITS_OBJECT_BOUNDING_BOX, "objectBoundingBox")
)

DEFINE_ENUM_NO_PARSE (PAINT_ORDER, paint_order, PaintOrder,
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_FILL_STROKE_MARKERS, "normal"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_FILL_STROKE_MARKERS, "fill stroke markers"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_FILL_MARKERS_STROKE, "fill markers stroke"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_STROKE_FILL_MARKERS, "stroke fill markers"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_STROKE_MARKERS_FILL, "stroke markers fill"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_MARKERS_FILL_STROKE, "markers fill stroke"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_MARKERS_STROKE_FILL, "markers stroke fill")
)

SvgValue *
svg_paint_order_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "normal"))
    return svg_paint_order_new (PAINT_ORDER_FILL_STROKE_MARKERS);

  if (gtk_css_parser_try_ident (parser, "fill"))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_try_ident (parser, "stroke"))
        {
          gtk_css_parser_skip_whitespace (parser);
          gtk_css_parser_try_ident (parser, "markers");
          return svg_paint_order_new (PAINT_ORDER_FILL_STROKE_MARKERS);
        }
      else if (gtk_css_parser_try_ident (parser, "markers"))
        {
          gtk_css_parser_skip_whitespace (parser);
          gtk_css_parser_try_ident (parser, "stroke");
          return svg_paint_order_new (PAINT_ORDER_FILL_MARKERS_STROKE);
        }
      else
        {
          return svg_paint_order_new (PAINT_ORDER_FILL_STROKE_MARKERS);
        }
    }
  else if (gtk_css_parser_try_ident (parser, "stroke"))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_try_ident (parser, "fill"))
        {
          gtk_css_parser_skip_whitespace (parser);
          gtk_css_parser_try_ident (parser, "markers");
          return svg_paint_order_new (PAINT_ORDER_STROKE_FILL_MARKERS);
        }
      else if (gtk_css_parser_try_ident (parser, "markers"))
        {
          gtk_css_parser_skip_whitespace (parser);
          gtk_css_parser_try_ident (parser, "fill");
          return svg_paint_order_new (PAINT_ORDER_STROKE_MARKERS_FILL);
        }
      else
        {
          return svg_paint_order_new (PAINT_ORDER_STROKE_FILL_MARKERS);
        }
    }
  else if (gtk_css_parser_try_ident (parser, "markers"))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_try_ident (parser, "fill"))
        {
          gtk_css_parser_skip_whitespace (parser);
          gtk_css_parser_try_ident (parser, "stroke");
          return svg_paint_order_new (PAINT_ORDER_MARKERS_FILL_STROKE);
        }
      else if (gtk_css_parser_try_ident (parser, "stroke"))
        {
          gtk_css_parser_skip_whitespace (parser);
          gtk_css_parser_try_ident (parser, "fill");
          return svg_paint_order_new (PAINT_ORDER_MARKERS_STROKE_FILL);
        }
      else
        {
          return svg_paint_order_new (PAINT_ORDER_MARKERS_FILL_STROKE);
        }
    }

  gtk_css_parser_error_syntax (parser, "Expected a paint order value");
  return NULL;
}

DEFINE_ENUM (OFLOW, overflow, SvgOverflow,
  DEFINE_ENUM_VALUE (OFLOW, OVERFLOW_VISIBLE, "visible"),
  DEFINE_ENUM_VALUE (OFLOW, OVERFLOW_HIDDEN, "hidden"),
  DEFINE_ENUM_VALUE (OFLOW, OVERFLOW_AUTO, "auto")
)

DEFINE_ENUM (ISOLATION, isolation, Isolation,
  DEFINE_ENUM_VALUE (ISOLATION, ISOLATION_AUTO, "auto"),
  DEFINE_ENUM_VALUE (ISOLATION, ISOLATION_ISOLATE, "isolate")
)

DEFINE_ENUM (BLEND_MODE, blend_mode, GskBlendMode,
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_DEFAULT, "normal"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_MULTIPLY, "multiply"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_SCREEN, "screen"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_OVERLAY, "overlay"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_DARKEN, "darken"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_LIGHTEN, "lighten"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_COLOR_DODGE, "color-dodge"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_COLOR_BURN, "color-burn"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_HARD_LIGHT, "hard-light"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_SOFT_LIGHT, "soft-light"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_DIFFERENCE, "difference"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_EXCLUSION, "exclusion"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_COLOR, "color"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_HUE, "hue"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_SATURATION, "saturation"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_LUMINOSITY, "luminosity")
)

DEFINE_ENUM (TEXT_ANCHOR, text_anchor, TextAnchor,
  DEFINE_ENUM_VALUE (TEXT_ANCHOR, TEXT_ANCHOR_START, "start"),
  DEFINE_ENUM_VALUE (TEXT_ANCHOR, TEXT_ANCHOR_MIDDLE, "middle"),
  DEFINE_ENUM_VALUE (TEXT_ANCHOR, TEXT_ANCHOR_END, "end")
)

DEFINE_ENUM (MARKER_UNITS, marker_units, MarkerUnits,
  DEFINE_ENUM_VALUE (MARKER_UNITS, MARKER_UNITS_STROKE_WIDTH, "strokeWidth"),
  DEFINE_ENUM_VALUE (MARKER_UNITS, MARKER_UNITS_USER_SPACE_ON_USE, "userSpaceOnUse")
)

DEFINE_ENUM (UNICODE_BIDI, unicode_bidi, UnicodeBidi,
  DEFINE_ENUM_VALUE (UNICODE_BIDI, UNICODE_BIDI_NORMAL, "normal"),
  DEFINE_ENUM_VALUE (UNICODE_BIDI, UNICODE_BIDI_EMBED, "embed"),
  DEFINE_ENUM_VALUE (UNICODE_BIDI, UNICODE_BIDI_OVERRIDE, "bidi-override")
)

DEFINE_ENUM (DIRECTION, direction, PangoDirection,
  DEFINE_ENUM_VALUE (DIRECTION, PANGO_DIRECTION_LTR, "ltr"),
  DEFINE_ENUM_VALUE (DIRECTION, PANGO_DIRECTION_RTL, "rtl")
)

DEFINE_ENUM (WRITING_MODE, writing_mode, WritingMode,
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_HORIZONTAL_TB, "horizontal-tb"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_VERTICAL_RL, "vertical-rl"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_VERTICAL_LR, "vertical-lr"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_LR, "lr"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_LR_TB, "lr-tb"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_RL, "rl"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_RL_TB, "rl-tb"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_TB, "tb"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_TB_RL, "tb-rl")
)

DEFINE_ENUM (FONT_STYLE, font_style, PangoStyle,
  DEFINE_ENUM_VALUE (FONT_STYLE, PANGO_STYLE_NORMAL, "normal"),
  DEFINE_ENUM_VALUE (FONT_STYLE, PANGO_STYLE_OBLIQUE, "oblique"),
  DEFINE_ENUM_VALUE (FONT_STYLE, PANGO_STYLE_ITALIC, "italic")
)

DEFINE_ENUM (FONT_VARIANT, font_variant, PangoVariant,
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_NORMAL, "normal"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_SMALL_CAPS, "small-caps"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_ALL_SMALL_CAPS, "all-small-caps"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_PETITE_CAPS, "petite-caps"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_ALL_PETITE_CAPS, "all-petite-caps"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_UNICASE, "unicase"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_TITLE_CAPS, "titling-caps")
)

DEFINE_ENUM (FONT_STRETCH, font_stretch, PangoStretch,
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_ULTRA_CONDENSED, "ultra-condensed"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_EXTRA_CONDENSED, "extra-condensed"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_CONDENSED, "condensed"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_SEMI_CONDENSED, "semi-condensed"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_NORMAL, "normal"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_SEMI_EXPANDED, "semi-expanded"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_EXPANDED, "expanded"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_EXTRA_EXPANDED, "extra-expanded"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_ULTRA_EXPANDED, "ultra-expanded")
)

DEFINE_ENUM (EDGE_MODE, edge_mode, EdgeMode,
  DEFINE_ENUM_VALUE (EDGE_MODE, EDGE_MODE_DUPLICATE, "duplicate"),
  DEFINE_ENUM_VALUE (EDGE_MODE, EDGE_MODE_WRAP, "wrap"),
  DEFINE_ENUM_VALUE (EDGE_MODE, EDGE_MODE_NONE, "none")
)

DEFINE_ENUM (BLEND_COMPOSITE, blend_composite, BlendComposite,
  DEFINE_ENUM_VALUE_NO_NAME (BLEND_COMPOSITE, BLEND_COMPOSITE),
  DEFINE_ENUM_VALUE (BLEND_COMPOSITE, BLEND_NO_COMPOSITE, "no-composite")
)

DEFINE_ENUM (COLOR_MATRIX_TYPE, color_matrix_type, ColorMatrixType,
  DEFINE_ENUM_VALUE (COLOR_MATRIX_TYPE, COLOR_MATRIX_TYPE_MATRIX, "matrix"),
  DEFINE_ENUM_VALUE (COLOR_MATRIX_TYPE, COLOR_MATRIX_TYPE_SATURATE, "saturate"),
  DEFINE_ENUM_VALUE (COLOR_MATRIX_TYPE, COLOR_MATRIX_TYPE_HUE_ROTATE, "hueRotate"),
  DEFINE_ENUM_VALUE (COLOR_MATRIX_TYPE, COLOR_MATRIX_TYPE_LUMINANCE_TO_ALPHA, "luminanceToAlpha")
)

DEFINE_ENUM (COMPOSITE_OPERATOR, composite_operator, CompositeOperator,
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_OVER, "over"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_IN, "in"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_OUT, "out"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_ATOP, "atop"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_XOR, "xor"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_LIGHTER, "lighter"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_ARITHMETIC, "arithmetic")
)

DEFINE_ENUM (RGBA_CHANNEL, rgba_channel, GdkColorChannel,
  DEFINE_ENUM_VALUE (RGBA_CHANNEL, GDK_COLOR_CHANNEL_RED, "R"),
  DEFINE_ENUM_VALUE (RGBA_CHANNEL, GDK_COLOR_CHANNEL_GREEN, "G"),
  DEFINE_ENUM_VALUE (RGBA_CHANNEL, GDK_COLOR_CHANNEL_BLUE, "B"),
  DEFINE_ENUM_VALUE (RGBA_CHANNEL, GDK_COLOR_CHANNEL_ALPHA, "A")
)

DEFINE_ENUM (COMPONENT_TRANSFER_TYPE, component_transfer_type, ComponentTransferType,
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_IDENTITY, "identity"),
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_TABLE, "table"),
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_DISCRETE, "discrete"),
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_LINEAR, "linear"),
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_GAMMA, "gamma")
)

DEFINE_ENUM (VECTOR_EFFECT, vector_effect, VectorEffect,
  DEFINE_ENUM_VALUE (VECTOR_EFFECT, VECTOR_EFFECT_NONE, "none"),
  DEFINE_ENUM_VALUE (VECTOR_EFFECT, VECTOR_EFFECT_NON_SCALING_STROKE, "non-scaling-stroke")
)

static SvgValue *
svg_font_size_resolve (const SvgValue    *value,
                       SvgProperty        attr,
                       unsigned int       idx,
                       SvgElement        *shape,
                       SvgComputeContext *context)
{
  const SvgEnum *font_size = (const SvgEnum *) value;

  g_assert (attr == SVG_PROPERTY_FONT_SIZE);

  if (font_size->value == FONT_SIZE_XX_SMALL)
    return svg_number_new (DEFAULT_FONT_SIZE * 3. / 5.);
  else if (font_size->value == FONT_SIZE_X_SMALL)
    return svg_number_new (DEFAULT_FONT_SIZE * 3. / 4.);
  else if (font_size->value == FONT_SIZE_SMALL)
    return svg_number_new (DEFAULT_FONT_SIZE * 8. / 9.);
  else if (font_size->value == FONT_SIZE_MEDIUM)
    return svg_number_new (DEFAULT_FONT_SIZE);
  else if (font_size->value == FONT_SIZE_LARGE)
    return svg_number_new (DEFAULT_FONT_SIZE * 6. / 5.);
  else if (font_size->value == FONT_SIZE_X_LARGE)
    return svg_number_new (DEFAULT_FONT_SIZE * 3. / 2.);
  else if (font_size->value == FONT_SIZE_XX_LARGE)
    return svg_number_new (DEFAULT_FONT_SIZE * 2.);
  else
    {
      double parent_size;

      if (context->parent)
        parent_size = svg_number_get (context->parent->current[SVG_PROPERTY_FONT_SIZE], 100);
      else
        parent_size = DEFAULT_FONT_SIZE;

      if (font_size->value == FONT_SIZE_SMALLER)
        return svg_number_new (parent_size / 1.2);
      else if (font_size->value == FONT_SIZE_LARGER)
        return svg_number_new (parent_size * 1.2);
      else
        g_assert_not_reached ();
    }
}

DEFINE_ENUM_CUSTOM_RESOLVE (FONT_SIZE, font_size, FontSize, svg_font_size_resolve,
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_SMALLER,  "smaller"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_LARGER,   "larger"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_XX_SMALL, "xx-small"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_X_SMALL,  "x-small"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_SMALL,    "small"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_MEDIUM,   "medium"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_LARGE,    "large"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_X_LARGE,  "x-large"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_XX_LARGE, "xx-large")
)

static SvgValue *
svg_font_weight_resolve (const SvgValue    *value,
                         SvgProperty        attr,
                         unsigned int       idx,
                         SvgElement        *shape,
                         SvgComputeContext *context)
{
  const SvgEnum *font_weight = (const SvgEnum *) value;

  g_assert (attr == SVG_PROPERTY_FONT_WEIGHT);

  if (font_weight->value == FONT_WEIGHT_NORMAL)
    return svg_number_new (PANGO_WEIGHT_NORMAL);
  else if (font_weight->value == FONT_WEIGHT_BOLD)
    return svg_number_new (PANGO_WEIGHT_BOLD);
  else
    {
      double parent_weight;

      if (context->parent)
        parent_weight = svg_number_get (context->parent->current[SVG_PROPERTY_FONT_WEIGHT], 100);
      else
        parent_weight = PANGO_WEIGHT_NORMAL;

      if (font_weight->value == FONT_WEIGHT_BOLDER)
        {
          if (parent_weight < 350)
            return svg_number_new (400);
          else if (parent_weight < 550)
            return svg_number_new (700);
          else if (parent_weight < 900)
            return svg_number_new (900);
          else
            return svg_number_new (parent_weight);
        }
      else if (font_weight->value == FONT_WEIGHT_LIGHTER)
        {
          if (parent_weight > 750)
            return svg_number_new (700);
          else if (parent_weight > 550)
            return svg_number_new (400);
          else if (parent_weight > 100)
            return svg_number_new (100);
          else
            return svg_number_new (parent_weight);
        }
      else
        g_assert_not_reached ();
    }
}

DEFINE_ENUM_CUSTOM_RESOLVE (FONT_WEIGHT, font_weight, FontWeight, svg_font_weight_resolve,
  DEFINE_ENUM_VALUE (FONT_WEIGHT, FONT_WEIGHT_NORMAL,  "normal"),
  DEFINE_ENUM_VALUE (FONT_WEIGHT, FONT_WEIGHT_BOLD,    "bold"),
  DEFINE_ENUM_VALUE (FONT_WEIGHT, FONT_WEIGHT_BOLDER,  "bolder"),
  DEFINE_ENUM_VALUE (FONT_WEIGHT, FONT_WEIGHT_LIGHTER, "lighter")
)

static SvgValue * svg_color_interpolation_resolve (const SvgValue    *value,
                                                   SvgProperty        attr,
                                                   unsigned int       idx,
                                                   SvgElement        *shape,
                                                   SvgComputeContext *context);

DEFINE_ENUM_CUSTOM_RESOLVE (COLOR_INTERPOLATION, color_interpolation, ColorInterpolation, svg_color_interpolation_resolve,
  DEFINE_ENUM_VALUE (COLOR_INTERPOLATION, COLOR_INTERPOLATION_AUTO,   "auto"),
  DEFINE_ENUM_VALUE (COLOR_INTERPOLATION, COLOR_INTERPOLATION_SRGB,   "sRGB"),
  DEFINE_ENUM_VALUE (COLOR_INTERPOLATION, COLOR_INTERPOLATION_LINEAR, "linearRGB")
)

static SvgValue *
svg_color_interpolation_resolve (const SvgValue    *value,
                                 SvgProperty        attr,
                                 unsigned int       idx,
                                 SvgElement        *shape,
                                 SvgComputeContext *context)
{
  if (svg_enum_get (value) == COLOR_INTERPOLATION_AUTO)
    return svg_color_interpolation_new (COLOR_INTERPOLATION_SRGB);
  else
    return svg_value_ref ((SvgValue *) value);
}

DEFINE_ENUM (TRANSFORM_BOX, transform_box, TransformBox,
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_CONTENT_BOX, "content-box"),
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_BORDER_BOX, "border-box"),
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_FILL_BOX, "fill-box"),
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_STROKE_BOX, "stroke-box"),
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_VIEW_BOX, "view-box")
)

DEFINE_ENUM (SHAPE_RENDERING, shape_rendering, ShapeRendering,
  DEFINE_ENUM_VALUE (SHAPE_RENDERING, SHAPE_RENDERING_AUTO, "auto"),
  DEFINE_ENUM_VALUE (SHAPE_RENDERING, SHAPE_RENDERING_OPTIMIZE_SPEED, "optimizeSpeed"),
  DEFINE_ENUM_VALUE (SHAPE_RENDERING, SHAPE_RENDERING_CRISP_EDGES, "crispEdges"),
  DEFINE_ENUM_VALUE (SHAPE_RENDERING, SHAPE_RENDERING_GEOMETRIC_PRECISION, "geometricPrecision")
)

DEFINE_ENUM (TEXT_RENDERING, text_rendering, TextRendering,
  DEFINE_ENUM_VALUE (TEXT_RENDERING, TEXT_RENDERING_AUTO, "auto"),
  DEFINE_ENUM_VALUE (TEXT_RENDERING, TEXT_RENDERING_OPTIMIZE_SPEED, "optimizeSpeed"),
  DEFINE_ENUM_VALUE (TEXT_RENDERING, TEXT_RENDERING_OPTIMIZE_LEGIBILITY, "optimizeLegibility"),
  DEFINE_ENUM_VALUE (TEXT_RENDERING, TEXT_RENDERING_GEOMETRIC_PRECISION , "geometricPrecision")
)

DEFINE_ENUM (IMAGE_RENDERING, image_rendering, ImageRendering,
  DEFINE_ENUM_VALUE (IMAGE_RENDERING, IMAGE_RENDERING_AUTO, "auto"),
  DEFINE_ENUM_VALUE (IMAGE_RENDERING, IMAGE_RENDERING_OPTIMIZE_QUALITY, "optimizeQuality"),
  DEFINE_ENUM_VALUE (IMAGE_RENDERING, IMAGE_RENDERING_OPTIMIZE_SPEED, "optimizeSpeed")
)

DEFINE_ENUM (POINTER_EVENTS, pointer_events, PointerEvents,
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_AUTO, "auto"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_BOUNDING_BOX, "bounding-box"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_VISIBLE_PAINTED, "visiblePainted"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_VISIBLE_FILL, "visibleFill"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_VISIBLE_STROKE, "visibleStroke"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_VISIBLE, "visible"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_PAINTED, "painted"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_FILL, "fill"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_STROKE, "stroke"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_ALL, "all"),
  DEFINE_ENUM_VALUE (POINTER_EVENTS, POINTER_EVENTS_NONE, "none")
)
