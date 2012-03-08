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

#include "config.h"

#include "gtkcssshorthandpropertyprivate.h"

#include <cairo-gobject.h>
#include <math.h>

#include "gtkcssimageprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylepropertiesprivate.h"
#include "gtksymboliccolorprivate.h"
#include "gtktypebuiltins.h"
#include "gtkcssvalueprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

/*** PARSING ***/

static gboolean
value_is_done_parsing (GtkCssParser *parser)
{
  return _gtk_css_parser_is_eof (parser) ||
         _gtk_css_parser_begins_with (parser, ';') ||
         _gtk_css_parser_begins_with (parser, '}');
}

static gboolean
parse_four_numbers (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GtkCssNumberParseFlags   flags)
{
  GtkCssNumber numbers[4];
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (!_gtk_css_parser_has_number (parser))
        break;

      if (!_gtk_css_parser_read_number (parser,
                                        &numbers[i], 
                                        flags))
        return FALSE;
    }

  if (i == 0)
    {
      _gtk_css_parser_error (parser, "Expected a length");
      return FALSE;
    }

  for (; i < 4; i++)
    {
      numbers[i] = numbers[(i - 1) >> 1];
    }

  for (i = 0; i < 4; i++)
    {
      g_value_init (&values[i], GTK_TYPE_CSS_NUMBER);
      g_value_set_boxed (&values[i], &numbers[i]);
    }

  return TRUE;
}

static gboolean
parse_margin (GtkCssShorthandProperty *shorthand,
              GValue                  *values,
              GtkCssParser            *parser,
              GFile                   *base)
{
  return parse_four_numbers (shorthand,
                             values,
                             parser,
                             GTK_CSS_NUMBER_AS_PIXELS
                             | GTK_CSS_PARSE_LENGTH);
}

static gboolean
parse_padding (GtkCssShorthandProperty *shorthand,
               GValue                  *values,
               GtkCssParser            *parser,
               GFile                   *base)
{
  return parse_four_numbers (shorthand,
                             values,
                             parser,
                             GTK_CSS_POSITIVE_ONLY
                             | GTK_CSS_NUMBER_AS_PIXELS
                             | GTK_CSS_PARSE_LENGTH);
}

static gboolean
parse_border_width (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GFile                   *base)
{
  return parse_four_numbers (shorthand,
                             values,
                             parser,
                             GTK_CSS_POSITIVE_ONLY
                             | GTK_CSS_NUMBER_AS_PIXELS
                             | GTK_CSS_PARSE_LENGTH);
}

static gboolean 
parse_border_radius (GtkCssShorthandProperty *shorthand,
                     GValue                  *values,
                     GtkCssParser            *parser,
                     GFile                   *base)
{
  GtkCssBorderCornerRadius borders[4];
  guint i;

  for (i = 0; i < G_N_ELEMENTS (borders); i++)
    {
      if (!_gtk_css_parser_has_number (parser))
        break;
      if (!_gtk_css_parser_read_number (parser,
                                        &borders[i].horizontal,
                                        GTK_CSS_POSITIVE_ONLY
                                        | GTK_CSS_PARSE_PERCENT
                                        | GTK_CSS_NUMBER_AS_PIXELS
                                        | GTK_CSS_PARSE_LENGTH))
        return FALSE;
    }

  if (i == 0)
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }

  /* The magic (i - 1) >> 1 below makes it take the correct value
   * according to spec. Feel free to check the 4 cases */
  for (; i < G_N_ELEMENTS (borders); i++)
    borders[i].horizontal = borders[(i - 1) >> 1].horizontal;

  if (_gtk_css_parser_try (parser, "/", TRUE))
    {
      for (i = 0; i < G_N_ELEMENTS (borders); i++)
        {
          if (!_gtk_css_parser_has_number (parser))
            break;
          if (!_gtk_css_parser_read_number (parser,
                                            &borders[i].vertical,
                                            GTK_CSS_POSITIVE_ONLY
                                            | GTK_CSS_PARSE_PERCENT
                                            | GTK_CSS_NUMBER_AS_PIXELS
                                            | GTK_CSS_PARSE_LENGTH))
            return FALSE;
        }

      if (i == 0)
        {
          _gtk_css_parser_error (parser, "Expected a number");
          return FALSE;
        }

      for (; i < G_N_ELEMENTS (borders); i++)
        borders[i].vertical = borders[(i - 1) >> 1].vertical;

    }
  else
    {
      for (i = 0; i < G_N_ELEMENTS (borders); i++)
        borders[i].vertical = borders[i].horizontal;
    }

  for (i = 0; i < G_N_ELEMENTS (borders); i++)
    {
      g_value_init (&values[i], GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
      g_value_set_boxed (&values[i], &borders[i]);
    }

  return TRUE;
}

static gboolean 
parse_border_color (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GFile                   *base)
{
  GtkSymbolicColor *symbolic;
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (_gtk_css_parser_try (parser, "currentcolor", TRUE))
        {
          symbolic = gtk_symbolic_color_ref (_gtk_symbolic_color_get_current_color ());
        }
      else
        {
          symbolic = _gtk_css_parser_read_symbolic_color (parser);
          if (symbolic == NULL)
            return FALSE;
        }

      g_value_init (&values[i], GTK_TYPE_SYMBOLIC_COLOR);
      g_value_set_boxed (&values[i], symbolic);

      if (value_is_done_parsing (parser))
        break;
    }

  for (i++; i < 4; i++)
    {
      g_value_init (&values[i], G_VALUE_TYPE (&values[(i - 1) >> 1]));
      g_value_copy (&values[(i - 1) >> 1], &values[i]);
    }

  return TRUE;
}

static gboolean
parse_border_style (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GFile                   *base)
{
  GtkBorderStyle styles[4];
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (!_gtk_css_parser_try_enum (parser, GTK_TYPE_BORDER_STYLE, (int *)&styles[i]))
        break;
    }

  if (i == 0)
    {
      _gtk_css_parser_error (parser, "Expected a border style");
      return FALSE;
    }

  for (; i < G_N_ELEMENTS (styles); i++)
    styles[i] = styles[(i - 1) >> 1];

  for (i = 0; i < G_N_ELEMENTS (styles); i++)
    {
      g_value_init (&values[i], GTK_TYPE_BORDER_STYLE);
      g_value_set_enum (&values[i], styles[i]);
    }

  return TRUE;
}

static gboolean
parse_border_image (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GFile                   *base)
{
  GtkCssImage *image;
  
  if (_gtk_css_parser_try (parser, "none", TRUE))
    image = NULL;
  else
    {
      image = _gtk_css_image_new_parse (parser, base);
      if (!image)
        return FALSE;
    }
  g_value_init (&values[0], GTK_TYPE_CSS_IMAGE);
  g_value_set_object (&values[0], image);

  if (value_is_done_parsing (parser))
    return TRUE;

  g_value_init (&values[1], GTK_TYPE_BORDER);
  if (!_gtk_css_style_parse_value (&values[1], parser, base))
    return FALSE;

  if (_gtk_css_parser_try (parser, "/", TRUE))
    {
      g_value_init (&values[2], GTK_TYPE_BORDER);
      if (!_gtk_css_style_parse_value (&values[2], parser, base))
        return FALSE;
    }

  if (value_is_done_parsing (parser))
    return TRUE;

  g_value_init (&values[3], GTK_TYPE_CSS_BORDER_IMAGE_REPEAT);
  if (!_gtk_css_style_parse_value (&values[3], parser, base))
    return FALSE;

  return TRUE;
}

static gboolean
parse_border_side (GtkCssShorthandProperty *shorthand,
                   GValue                  *values,
                   GtkCssParser            *parser,
                   GFile                   *base)
{
  int style;

  do
  {
    if (!G_IS_VALUE (&values[0]) &&
         _gtk_css_parser_has_number (parser))
      {
        GtkCssNumber number;
        if (!_gtk_css_parser_read_number (parser,
                                          &number,
                                          GTK_CSS_POSITIVE_ONLY
                                          | GTK_CSS_NUMBER_AS_PIXELS
                                          | GTK_CSS_PARSE_LENGTH))
          return FALSE;

        g_value_init (&values[0], GTK_TYPE_CSS_NUMBER);
        g_value_set_boxed (&values[0], &number);
      }
    else if (!G_IS_VALUE (&values[1]) &&
             _gtk_css_parser_try_enum (parser, GTK_TYPE_BORDER_STYLE, &style))
      {
        g_value_init (&values[1], GTK_TYPE_BORDER_STYLE);
        g_value_set_enum (&values[1], style);
      }
    else if (!G_IS_VALUE (&values[2]))
      {
        GtkSymbolicColor *symbolic;

        symbolic = _gtk_css_parser_read_symbolic_color (parser);
        if (symbolic == NULL)
          return FALSE;

        g_value_init (&values[2], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_take_boxed (&values[2], symbolic);
      }
    else
      {
        /* We parsed everything and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error */
        break;
      }
  }
  while (!value_is_done_parsing (parser));

  return TRUE;
}

static gboolean
parse_border (GtkCssShorthandProperty *shorthand,
              GValue                  *values,
              GtkCssParser            *parser,
              GFile                   *base)
{
  int style;

  do
  {
    if (!G_IS_VALUE (&values[0]) &&
         _gtk_css_parser_has_number (parser))
      {
        GtkCssNumber number;
        if (!_gtk_css_parser_read_number (parser,
                                          &number,
                                          GTK_CSS_POSITIVE_ONLY
                                          | GTK_CSS_NUMBER_AS_PIXELS
                                          | GTK_CSS_PARSE_LENGTH))
          return FALSE;

        g_value_init (&values[0], GTK_TYPE_CSS_NUMBER);
        g_value_init (&values[1], GTK_TYPE_CSS_NUMBER);
        g_value_init (&values[2], GTK_TYPE_CSS_NUMBER);
        g_value_init (&values[3], GTK_TYPE_CSS_NUMBER);
        g_value_set_boxed (&values[0], &number);
        g_value_set_boxed (&values[1], &number);
        g_value_set_boxed (&values[2], &number);
        g_value_set_boxed (&values[3], &number);
      }
    else if (!G_IS_VALUE (&values[4]) &&
             _gtk_css_parser_try_enum (parser, GTK_TYPE_BORDER_STYLE, &style))
      {
        g_value_init (&values[4], GTK_TYPE_BORDER_STYLE);
        g_value_init (&values[5], GTK_TYPE_BORDER_STYLE);
        g_value_init (&values[6], GTK_TYPE_BORDER_STYLE);
        g_value_init (&values[7], GTK_TYPE_BORDER_STYLE);
        g_value_set_enum (&values[4], style);
        g_value_set_enum (&values[5], style);
        g_value_set_enum (&values[6], style);
        g_value_set_enum (&values[7], style);
      }
    else if (!G_IS_VALUE (&values[8]))
      {
        GtkSymbolicColor *symbolic;

        symbolic = _gtk_css_parser_read_symbolic_color (parser);
        if (symbolic == NULL)
          return FALSE;

        g_value_init (&values[8], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_init (&values[9], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_init (&values[10], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_init (&values[11], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_set_boxed (&values[8], symbolic);
        g_value_set_boxed (&values[9], symbolic);
        g_value_set_boxed (&values[10], symbolic);
        g_value_take_boxed (&values[11], symbolic);
      }
    else
      {
        /* We parsed everything and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error */
        break;
      }
  }
  while (!value_is_done_parsing (parser));

  /* Note that border-image values are not set: according to the spec
     they just need to be reset when using the border shorthand */

  return TRUE;
}

static gboolean
parse_font (GtkCssShorthandProperty *shorthand,
            GValue                  *values,
            GtkCssParser            *parser,
            GFile                   *base)
{
  PangoFontDescription *desc;
  guint mask;
  char *str;

  str = _gtk_css_parser_read_value (parser);
  if (str == NULL)
    return FALSE;

  desc = pango_font_description_from_string (str);
  g_free (str);

  mask = pango_font_description_get_set_fields (desc);

  if (mask & PANGO_FONT_MASK_FAMILY)
    {
      GPtrArray *strv = g_ptr_array_new ();

      g_ptr_array_add (strv, g_strdup (pango_font_description_get_family (desc)));
      g_ptr_array_add (strv, NULL);
      g_value_init (&values[0], G_TYPE_STRV);
      g_value_take_boxed (&values[0], g_ptr_array_free (strv, FALSE));
    }
  if (mask & PANGO_FONT_MASK_STYLE)
    {
      g_value_init (&values[1], PANGO_TYPE_STYLE);
      g_value_set_enum (&values[1], pango_font_description_get_style (desc));
    }
  if (mask & PANGO_FONT_MASK_VARIANT)
    {
      g_value_init (&values[2], PANGO_TYPE_VARIANT);
      g_value_set_enum (&values[2], pango_font_description_get_variant (desc));
    }
  if (mask & PANGO_FONT_MASK_WEIGHT)
    {
      g_value_init (&values[3], PANGO_TYPE_WEIGHT);
      g_value_set_enum (&values[3], pango_font_description_get_weight (desc));
    }
  if (mask & PANGO_FONT_MASK_SIZE)
    {
      g_value_init (&values[4], G_TYPE_DOUBLE);
      g_value_set_double (&values[4],
                          (double) pango_font_description_get_size (desc) / PANGO_SCALE);
    }

  pango_font_description_free (desc);

  return TRUE;
}

static gboolean
parse_background (GtkCssShorthandProperty *shorthand,
                  GValue                  *values,
                  GtkCssParser            *parser,
                  GFile                   *base)
{
  int enum_value;

  do
    {
      /* the image part */
      if (!G_IS_VALUE (&values[0]) &&
          (_gtk_css_parser_has_prefix (parser, "none") ||
           _gtk_css_image_can_parse (parser)))
        {
          GtkCssImage *image;

          if (_gtk_css_parser_try (parser, "none", TRUE))
            image = NULL;
          else
            {
              image = _gtk_css_image_new_parse (parser, base);
              if (image == NULL)
                return FALSE;
            }

          g_value_init (&values[0], GTK_TYPE_CSS_IMAGE);
          g_value_take_object (&values[0], image);
        }
      else if (!G_IS_VALUE (&values[1]) &&
               _gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BACKGROUND_REPEAT, &enum_value))
        {
          if (enum_value <= GTK_CSS_BACKGROUND_REPEAT_MASK)
            {
              int vertical;

              if (_gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BACKGROUND_REPEAT, &vertical))
                {
                  if (vertical >= GTK_CSS_BACKGROUND_REPEAT_MASK)
                    {
                      _gtk_css_parser_error (parser, "Not a valid 2nd value for border-repeat");
                      return FALSE;
                    }
                  else
                    enum_value |= vertical << GTK_CSS_BACKGROUND_REPEAT_SHIFT;
                }
              else
                enum_value |= enum_value << GTK_CSS_BACKGROUND_REPEAT_SHIFT;
            }

          g_value_init (&values[1], GTK_TYPE_CSS_BACKGROUND_REPEAT);
          g_value_set_enum (&values[1], enum_value);
        }
      else if ((!G_IS_VALUE (&values[2]) || !G_IS_VALUE (&values[3])) &&
               _gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_AREA, &enum_value))
        {
          guint idx = !G_IS_VALUE (&values[2]) ? 2 : 3;
          g_value_init (&values[idx], GTK_TYPE_CSS_AREA);
          g_value_set_enum (&values[idx], enum_value);
        }
      else if (!G_IS_VALUE (&values[4]))
        {
          GtkSymbolicColor *symbolic;
          
          symbolic = _gtk_css_parser_read_symbolic_color (parser);
          if (symbolic == NULL)
            return FALSE;

          g_value_init (&values[4], GTK_TYPE_SYMBOLIC_COLOR);
          g_value_take_boxed (&values[4], symbolic);
        }
      else
        {
          /* We parsed everything and there's still stuff left?
           * Pretend we didn't notice and let the normal code produce
           * a 'junk at end of value' error */
          break;
        }
    }
  while (!value_is_done_parsing (parser));

  return TRUE;
}

/*** PACKING ***/

static void
unpack_border (GtkCssShorthandProperty *shorthand,
               GtkStyleProperties      *props,
               GtkStateFlags            state,
               const GValue            *value)
{
  GValue v = G_VALUE_INIT;
  GtkBorder *border = g_value_get_boxed (value);

  g_value_init (&v, G_TYPE_INT);

  g_value_set_int (&v, border->top);
  _gtk_style_property_assign (GTK_STYLE_PROPERTY (_gtk_css_shorthand_property_get_subproperty (shorthand, 0)), props, state, &v);
  g_value_set_int (&v, border->right);
  _gtk_style_property_assign (GTK_STYLE_PROPERTY (_gtk_css_shorthand_property_get_subproperty (shorthand, 1)), props, state, &v);
  g_value_set_int (&v, border->bottom);
  _gtk_style_property_assign (GTK_STYLE_PROPERTY (_gtk_css_shorthand_property_get_subproperty (shorthand, 2)), props, state, &v);
  g_value_set_int (&v, border->left);
  _gtk_style_property_assign (GTK_STYLE_PROPERTY (_gtk_css_shorthand_property_get_subproperty (shorthand, 3)), props, state, &v);

  g_value_unset (&v);
}

static GtkCssValue *
pack_border (GtkCssShorthandProperty *shorthand,
             GtkStyleQueryFunc        query_func,
             gpointer                 query_data)
{
  GtkCssStyleProperty *prop;
  GtkBorder border;
  GtkCssValue *v;

  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 0);
  v = (* query_func) (_gtk_css_style_property_get_id (prop), query_data);
  if (v)
    border.top = _gtk_css_value_get_int (v);
  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 1);
  v = (* query_func) (_gtk_css_style_property_get_id (prop), query_data);
  if (v)
    border.right = _gtk_css_value_get_int (v);
  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 2);
  v = (* query_func) (_gtk_css_style_property_get_id (prop), query_data);
  if (v)
    border.bottom = _gtk_css_value_get_int (v);
  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 3);
  v = (* query_func) (_gtk_css_style_property_get_id (prop), query_data);
  if (v)
    border.left = _gtk_css_value_get_int (v);

  return _gtk_css_value_new_from_border (&border);
}

static void
unpack_border_radius (GtkCssShorthandProperty *shorthand,
                      GtkStyleProperties      *props,
                      GtkStateFlags            state,
                      const GValue            *value)
{
  GtkCssBorderCornerRadius border;
  GValue v = G_VALUE_INIT;
  guint i;
  
  _gtk_css_number_init (&border.horizontal, g_value_get_int (value), GTK_CSS_PX);
  border.vertical = border.horizontal;
  g_value_init (&v, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  g_value_set_boxed (&v, &border);

  for (i = 0; i < 4; i++)
    _gtk_style_property_assign (GTK_STYLE_PROPERTY (_gtk_css_shorthand_property_get_subproperty (shorthand, i)), props, state, &v);

  g_value_unset (&v);
}

static GtkCssValue *
pack_border_radius (GtkCssShorthandProperty *shorthand,
                    GtkStyleQueryFunc        query_func,
                    gpointer                 query_data)
{
  GtkCssBorderCornerRadius *top_left;
  GtkCssStyleProperty *prop;
  GtkCssValue *v;
  int value = 0;

  prop = GTK_CSS_STYLE_PROPERTY (_gtk_style_property_lookup ("border-top-left-radius"));
  v = (* query_func) (_gtk_css_style_property_get_id (prop), query_data);
  if (v)
    {
      top_left = _gtk_css_value_get_border_corner_radius (v);
      if (top_left)
        value = top_left->horizontal.value;
    }

  return _gtk_css_value_new_from_int (value);
}

static void
unpack_font_description (GtkCssShorthandProperty *shorthand,
                         GtkStyleProperties      *props,
                         GtkStateFlags            state,
                         const GValue            *value)
{
  GtkStyleProperty *prop;
  PangoFontDescription *description;
  PangoFontMask mask;
  GValue v = G_VALUE_INIT;
  
  /* For backwards compat, we only unpack values that are indeed set.
   * For strict CSS conformance we need to unpack all of them.
   * Note that we do set all of them in the parse function, so it
   * will not have effects when parsing CSS files. It will though
   * for custom style providers.
   */

  description = g_value_get_boxed (value);

  if (description)
    mask = pango_font_description_get_set_fields (description);
  else
    mask = 0;

  if (mask & PANGO_FONT_MASK_FAMILY)
    {
      GPtrArray *strv = g_ptr_array_new ();

      g_ptr_array_add (strv, g_strdup (pango_font_description_get_family (description)));
      g_ptr_array_add (strv, NULL);
      g_value_init (&v, G_TYPE_STRV);
      g_value_take_boxed (&v, g_ptr_array_free (strv, FALSE));

      prop = _gtk_style_property_lookup ("font-family");
      _gtk_style_property_assign (prop, props, state, &v);
      g_value_unset (&v);
    }

  if (mask & PANGO_FONT_MASK_STYLE)
    {
      g_value_init (&v, PANGO_TYPE_STYLE);
      g_value_set_enum (&v, pango_font_description_get_style (description));

      prop = _gtk_style_property_lookup ("font-style");
      _gtk_style_property_assign (prop, props, state, &v);
      g_value_unset (&v);
    }

  if (mask & PANGO_FONT_MASK_VARIANT)
    {
      g_value_init (&v, PANGO_TYPE_VARIANT);
      g_value_set_enum (&v, pango_font_description_get_variant (description));

      prop = _gtk_style_property_lookup ("font-variant");
      _gtk_style_property_assign (prop, props, state, &v);
      g_value_unset (&v);
    }

  if (mask & PANGO_FONT_MASK_WEIGHT)
    {
      g_value_init (&v, PANGO_TYPE_WEIGHT);
      g_value_set_enum (&v, pango_font_description_get_weight (description));

      prop = _gtk_style_property_lookup ("font-weight");
      _gtk_style_property_assign (prop, props, state, &v);
      g_value_unset (&v);
    }

  if (mask & PANGO_FONT_MASK_SIZE)
    {
      g_value_init (&v, G_TYPE_DOUBLE);
      g_value_set_double (&v, (double) pango_font_description_get_size (description) / PANGO_SCALE);

      prop = _gtk_style_property_lookup ("font-size");
      _gtk_style_property_assign (prop, props, state, &v);
      g_value_unset (&v);
    }
}

static GtkCssValue *
pack_font_description (GtkCssShorthandProperty *shorthand,
                       GtkStyleQueryFunc        query_func,
                       gpointer                 query_data)
{
  PangoFontDescription *description;
  GtkCssValue *v;

  description = pango_font_description_new ();

  v = (* query_func) (_gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (_gtk_style_property_lookup ("font-family"))), query_data);
  if (v)
    {
      const char **families = _gtk_css_value_get_strv (v);
      /* xxx: Can we set all the families here somehow? */
      if (families)
        pango_font_description_set_family (description, families[0]);
    }

  v = (* query_func) (_gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (_gtk_style_property_lookup ("font-size"))), query_data);
  if (v)
    pango_font_description_set_size (description, round (_gtk_css_value_get_double (v) * PANGO_SCALE));

  v = (* query_func) (_gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (_gtk_style_property_lookup ("font-style"))), query_data);
  if (v)
    pango_font_description_set_style (description, _gtk_css_value_get_pango_style (v));

  v = (* query_func) (_gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (_gtk_style_property_lookup ("font-variant"))), query_data);
  if (v)
    pango_font_description_set_variant (description, _gtk_css_value_get_pango_variant (v));

  v = (* query_func) (_gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (_gtk_style_property_lookup ("font-weight"))), query_data);
  if (v)
    pango_font_description_set_weight (description, _gtk_css_value_get_pango_weight (v));

  return _gtk_css_value_new_take_font_description (description);
}

static void
unpack_to_everything (GtkCssShorthandProperty *shorthand,
                      GtkStyleProperties      *props,
                      GtkStateFlags            state,
                      const GValue            *value)
{
  GtkCssStyleProperty *prop;
  guint i, n;
  
  n = _gtk_css_shorthand_property_get_n_subproperties (shorthand);

  for (i = 0; i < n; i++)
    {
      prop = _gtk_css_shorthand_property_get_subproperty (shorthand, i);
      _gtk_style_property_assign (GTK_STYLE_PROPERTY (prop), props, state, value);
    }
}

static GtkCssValue *
pack_first_element (GtkCssShorthandProperty *shorthand,
                    GtkStyleQueryFunc        query_func,
                    gpointer                 query_data)
{
  GtkCssStyleProperty *prop;
  GtkCssValue *v;
  guint i;

  /* NB: This is a fallback for properties that originally were
   * not used as shorthand. We just pick the first subproperty
   * as a representative.
   * Lesson learned: Don't query the shorthand, query the 
   * real properties instead. */
  for (i = 0; i < _gtk_css_shorthand_property_get_n_subproperties (shorthand); i++)
    {
      prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 0);
      v = (* query_func) (_gtk_css_style_property_get_id (prop), query_data);
      if (v)
        {
          return _gtk_css_value_ref (v);
        }
    }
  return NULL;
}

static void
_gtk_css_shorthand_property_register (const char                        *name,
                                      GType                              value_type,
                                      const char                       **subproperties,
                                      GtkCssShorthandPropertyParseFunc   parse_func,
                                      GtkCssShorthandPropertyAssignFunc  assign_func,
                                      GtkCssShorthandPropertyQueryFunc   query_func)
{
  GtkCssShorthandProperty *node;

  node = g_object_new (GTK_TYPE_CSS_SHORTHAND_PROPERTY,
                       "name", name,
                       "value-type", value_type,
                       "subproperties", subproperties,
                       NULL);

  node->parse = parse_func;
  node->assign = assign_func;
  node->query = query_func;
}

void
_gtk_css_shorthand_property_init_properties (void)
{
  /* The order is important here, be careful when changing it */
  const char *font_subproperties[] = { "font-family", "font-style", "font-variant", "font-weight", "font-size", NULL };
  const char *margin_subproperties[] = { "margin-top", "margin-right", "margin-bottom", "margin-left", NULL };
  const char *padding_subproperties[] = { "padding-top", "padding-right", "padding-bottom", "padding-left", NULL };
  const char *border_width_subproperties[] = { "border-top-width", "border-right-width", "border-bottom-width", "border-left-width", NULL };
  const char *border_radius_subproperties[] = { "border-top-left-radius", "border-top-right-radius",
                                                "border-bottom-right-radius", "border-bottom-left-radius", NULL };
  const char *border_color_subproperties[] = { "border-top-color", "border-right-color", "border-bottom-color", "border-left-color", NULL };
  const char *border_style_subproperties[] = { "border-top-style", "border-right-style", "border-bottom-style", "border-left-style", NULL };
  const char *border_image_subproperties[] = { "border-image-source", "border-image-slice", "border-image-width", "border-image-repeat", NULL };
  const char *border_top_subproperties[] = { "border-top-width", "border-top-style", "border-top-color", NULL };
  const char *border_right_subproperties[] = { "border-right-width", "border-right-style", "border-right-color", NULL };
  const char *border_bottom_subproperties[] = { "border-bottom-width", "border-bottom-style", "border-bottom-color", NULL };
  const char *border_left_subproperties[] = { "border-left-width", "border-left-style", "border-left-color", NULL };
  const char *border_subproperties[] = { "border-top-width", "border-right-width", "border-bottom-width", "border-left-width",
                                         "border-top-style", "border-right-style", "border-bottom-style", "border-left-style",
                                         "border-top-color", "border-right-color", "border-bottom-color", "border-left-color",
                                         "border-image-source", "border-image-slice", "border-image-width", "border-image-repeat", NULL };
  const char *outline_subproperties[] = { "outline-width", "outline-style", "outline-color", NULL };
  const char *background_subproperties[] = { "background-image", "background-repeat", "background-clip", "background-origin",
                                             "background-color", NULL };

  _gtk_css_shorthand_property_register   ("font",
                                          PANGO_TYPE_FONT_DESCRIPTION,
                                          font_subproperties,
                                          parse_font,
                                          unpack_font_description,
                                          pack_font_description);
  _gtk_css_shorthand_property_register   ("margin",
                                          GTK_TYPE_BORDER,
                                          margin_subproperties,
                                          parse_margin,
                                          unpack_border,
                                          pack_border);
  _gtk_css_shorthand_property_register   ("padding",
                                          GTK_TYPE_BORDER,
                                          padding_subproperties,
                                          parse_padding,
                                          unpack_border,
                                          pack_border);
  _gtk_css_shorthand_property_register   ("border-width",
                                          GTK_TYPE_BORDER,
                                          border_width_subproperties,
                                          parse_border_width,
                                          unpack_border,
                                          pack_border);
  _gtk_css_shorthand_property_register   ("border-radius",
                                          G_TYPE_INT,
                                          border_radius_subproperties,
                                          parse_border_radius,
                                          unpack_border_radius,
                                          pack_border_radius);
  _gtk_css_shorthand_property_register   ("border-color",
                                          GDK_TYPE_RGBA,
                                          border_color_subproperties,
                                          parse_border_color,
                                          unpack_to_everything,
                                          pack_first_element);
  _gtk_css_shorthand_property_register   ("border-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          border_style_subproperties,
                                          parse_border_style,
                                          unpack_to_everything,
                                          pack_first_element);
  _gtk_css_shorthand_property_register   ("border-image",
                                          G_TYPE_NONE,
                                          border_image_subproperties,
                                          parse_border_image,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-top",
                                          G_TYPE_NONE,
                                          border_top_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-right",
                                          G_TYPE_NONE,
                                          border_right_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-bottom",
                                          G_TYPE_NONE,
                                          border_bottom_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-left",
                                          G_TYPE_NONE,
                                          border_left_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border",
                                          G_TYPE_NONE,
                                          border_subproperties,
                                          parse_border,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("outline",
                                          G_TYPE_NONE,
                                          outline_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("background",
                                          G_TYPE_NONE,
                                          background_subproperties,
                                          parse_background,
                                          NULL,
                                          NULL);
}
