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

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssbgsizevalueprivate.h"
#include "gtkcssbordervalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcsseasevalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssimageprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssrepeatvalueprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtktypebuiltins.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

/*** PARSING ***/

static gboolean
value_is_done_parsing (GtkCssParser *parser)
{
  return gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF) ||
         gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA) ||
         gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SEMICOLON) ||
         gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_CLOSE_CURLY);
}

static gboolean
parse_four_numbers (GtkCssShorthandProperty  *shorthand,
                    GtkCssValue             **values,
                    GtkCssParser             *parser,
                    GtkCssNumberParseFlags    flags)
{
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (!gtk_css_number_value_can_parse (parser))
        break;

      values[i] = _gtk_css_number_value_parse (parser, flags);
      if (values[i] == NULL)
        return FALSE;
    }

  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a length");
      return FALSE;
    }

  for (; i < 4; i++)
    {
      values[i] = _gtk_css_value_ref (values[(i - 1) >> 1]);
    }

  return TRUE;
}

static gboolean
parse_margin (GtkCssShorthandProperty  *shorthand,
              GtkCssValue             **values,
              GtkCssParser             *parser)
{
  return parse_four_numbers (shorthand,
                             values,
                             parser,
                             GTK_CSS_PARSE_LENGTH);
}

static gboolean
parse_padding (GtkCssShorthandProperty  *shorthand,
               GtkCssValue             **values,
               GtkCssParser             *parser)
{
  return parse_four_numbers (shorthand,
                             values,
                             parser,
                             GTK_CSS_POSITIVE_ONLY
                             | GTK_CSS_PARSE_LENGTH);
}

static gboolean
parse_border_width (GtkCssShorthandProperty  *shorthand,
                    GtkCssValue             **values,
                    GtkCssParser             *parser)
{
  return parse_four_numbers (shorthand,
                             values,
                             parser,
                             GTK_CSS_POSITIVE_ONLY
                             | GTK_CSS_PARSE_LENGTH);
}

static gboolean 
parse_border_radius (GtkCssShorthandProperty  *shorthand,
                     GtkCssValue             **values,
                     GtkCssParser             *parser)
{
  GtkCssValue *x[4] = { NULL, }, *y[4] = { NULL, };
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (!gtk_css_number_value_can_parse (parser))
        break;
      x[i] = _gtk_css_number_value_parse (parser,
                                          GTK_CSS_POSITIVE_ONLY
                                          | GTK_CSS_PARSE_PERCENT
                                          | GTK_CSS_PARSE_LENGTH);
      if (x[i] == NULL)
        goto fail;
    }

  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a number");
      goto fail;
    }

  /* The magic (i - 1) >> 1 below makes it take the correct value
   * according to spec. Feel free to check the 4 cases
   */
  for (; i < 4; i++)
    x[i] = _gtk_css_value_ref (x[(i - 1) >> 1]);

  if (gtk_css_parser_try_delim (parser, '/'))
    {
      for (i = 0; i < 4; i++)
        {
          if (!gtk_css_number_value_can_parse (parser))
            break;
          y[i] = _gtk_css_number_value_parse (parser,
                                              GTK_CSS_POSITIVE_ONLY
                                              | GTK_CSS_PARSE_PERCENT
                                              | GTK_CSS_PARSE_LENGTH);
          if (y[i] == NULL)
            goto fail;
        }

      if (i == 0)
        {
          gtk_css_parser_error_syntax (parser, "Expected a number");
          goto fail;
        }

      for (; i < 4; i++)
        y[i] = _gtk_css_value_ref (y[(i - 1) >> 1]);
    }
  else
    {
      for (i = 0; i < 4; i++)
        y[i] = _gtk_css_value_ref (x[i]);
    }

  for (i = 0; i < 4; i++)
    {
      values[i] = _gtk_css_corner_value_new (x[i], y[i]);
    }

  return TRUE;

fail:
  for (i = 0; i < 4; i++)
    {
      if (x[i])
        _gtk_css_value_unref (x[i]);
      if (y[i])
        _gtk_css_value_unref (y[i]);
    }
  return FALSE;
}

static gboolean 
parse_border_color (GtkCssShorthandProperty  *shorthand,
                    GtkCssValue             **values,
                    GtkCssParser             *parser)
{
  guint i;

  for (i = 0; i < 4; i++)
    {
      values[i] = _gtk_css_color_value_parse (parser);
      if (values[i] == NULL)
        return FALSE;

      if (value_is_done_parsing (parser))
        break;
    }

  for (i++; i < 4; i++)
    {
      values[i] = _gtk_css_value_ref (values[(i - 1) >> 1]);
    }

  return TRUE;
}

static gboolean
parse_border_style (GtkCssShorthandProperty  *shorthand,
                    GtkCssValue             **values,
                    GtkCssParser             *parser)
{
  guint i;

  for (i = 0; i < 4; i++)
    {
      values[i] = _gtk_css_border_style_value_try_parse (parser);
      if (values[i] == NULL)
        break;
    }

  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a border style");
      return FALSE;
    }

  for (; i < 4; i++)
    values[i] = _gtk_css_value_ref (values[(i - 1) >> 1]);

  return TRUE;
}

static gboolean
parse_border_image (GtkCssShorthandProperty  *shorthand,
                    GtkCssValue             **values,
                    GtkCssParser             *parser)
{
  do
    {
      if (values[0] == NULL &&
          (gtk_css_parser_has_ident (parser, "none") ||
           _gtk_css_image_can_parse (parser)))
        {
          GtkCssImage *image;

          if (gtk_css_parser_try_ident (parser, "none"))
            image = NULL;
          else
            {
              image = _gtk_css_image_new_parse (parser);
              if (image == NULL)
                return FALSE;
            }

          values[0] = _gtk_css_image_value_new (image);
        }
      else if (values[3] == NULL &&
               (values[3] = _gtk_css_border_repeat_value_try_parse (parser)))
        {
          /* please move along */
        }
      else if (values[1] == NULL)
        {
          values[1] = _gtk_css_border_value_parse (parser,
                                                   GTK_CSS_PARSE_PERCENT
                                                   | GTK_CSS_PARSE_NUMBER
                                                   | GTK_CSS_POSITIVE_ONLY,
                                                   FALSE,
                                                   TRUE);
          if (values[1] == NULL)
            return FALSE;

          if (gtk_css_parser_try_delim (parser, '/'))
            {
              values[2] = _gtk_css_border_value_parse (parser,
                                                       GTK_CSS_PARSE_PERCENT
                                                       | GTK_CSS_PARSE_LENGTH
                                                       | GTK_CSS_PARSE_NUMBER
                                                       | GTK_CSS_POSITIVE_ONLY,
                                                       TRUE,
                                                       FALSE);
              if (values[2] == NULL)
                return FALSE;
            }
        }
      else
        {
          /* We parsed everything and there's still stuff left?
           * Pretend we didn't notice and let the normal code produce
           * a 'junk at end of value' error
           */
          break;
        }
    }
  while (!value_is_done_parsing (parser));

  return TRUE;
}

static gboolean
parse_border_side (GtkCssShorthandProperty  *shorthand,
                   GtkCssValue             **values,
                   GtkCssParser             *parser)
{
  do
  {
    if (values[0] == NULL &&
        gtk_css_number_value_can_parse (parser))
      {
        values[0] = _gtk_css_number_value_parse (parser,
                                                 GTK_CSS_POSITIVE_ONLY
                                                 | GTK_CSS_PARSE_LENGTH);
        if (values[0] == NULL)
          return FALSE;
      }
    else if (values[1] == NULL &&
             (values[1] = _gtk_css_border_style_value_try_parse (parser)))
      {
        /* Nothing to do */
      }
    else if (values[2] == NULL)
      {
        values[2] = _gtk_css_color_value_parse (parser);
        if (values[2] == NULL)
          return FALSE;
      }
    else
      {
        /* We parsed and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error
         */
        break;
      }
  }
  while (!value_is_done_parsing (parser));

  return TRUE;
}

static gboolean
parse_border (GtkCssShorthandProperty  *shorthand,
              GtkCssValue             **values,
              GtkCssParser             *parser)
{
  do
  {
    if (values[0] == NULL &&
        gtk_css_number_value_can_parse (parser))
      {
        values[0] = _gtk_css_number_value_parse (parser,
                                                 GTK_CSS_POSITIVE_ONLY
                                                 | GTK_CSS_PARSE_LENGTH);
        if (values[0] == NULL)
          return FALSE;
        values[1] = _gtk_css_value_ref (values[0]);
        values[2] = _gtk_css_value_ref (values[0]);
        values[3] = _gtk_css_value_ref (values[0]);
      }
    else if (values[4] == NULL &&
             (values[4] = _gtk_css_border_style_value_try_parse (parser)))
      {
        values[5] = _gtk_css_value_ref (values[4]);
        values[6] = _gtk_css_value_ref (values[4]);
        values[7] = _gtk_css_value_ref (values[4]);
      }
    else if (!G_IS_VALUE (&values[8]))
      {
        values[8] = _gtk_css_color_value_parse (parser);
        if (values[8] == NULL)
          return FALSE;

        values[9] = _gtk_css_value_ref (values[8]);
        values[10] = _gtk_css_value_ref (values[8]);
        values[11] = _gtk_css_value_ref (values[8]);
      }
    else
      {
        /* We parsed everything and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error
         */
        break;
      }
  }
  while (!value_is_done_parsing (parser));

  /* Note that border-image values are not set: according to the spec
   * they just need to be reset when using the border shorthand
   */

  return TRUE;
}

static GtkCssValue *
_gtk_css_font_variant_value_try_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "normal"))
    return _gtk_css_ident_value_new ("normal");
  else if (gtk_css_parser_try_ident (parser, "small-caps"))
    return _gtk_css_ident_value_new ("small-caps");
  return NULL;
}

static gboolean
parse_font (GtkCssShorthandProperty  *shorthand,
            GtkCssValue             **values,
            GtkCssParser             *parser)
{
  gboolean parsed_one;

  do
    {
      parsed_one = FALSE;

      if (values[1] == NULL)
        {
          values[1] = _gtk_css_font_style_value_try_parse (parser);
          parsed_one = parsed_one || values[1] != NULL;
        }

      if (values[2] == NULL)
        {
          values[2] = _gtk_css_font_variant_value_try_parse (parser);
          parsed_one = parsed_one || values[2] != NULL;
        }

      if (values[3] == NULL)
        {
          values[3] = gtk_css_font_weight_value_try_parse (parser);
          if (values[3] == NULL && gtk_css_number_value_can_parse (parser))
            {
              /* This needs to check for font-size, too */
              GtkCssValue *num = _gtk_css_number_value_parse (parser,
                                                              GTK_CSS_PARSE_NUMBER |
                                                              GTK_CSS_PARSE_LENGTH |
                                                              GTK_CSS_PARSE_PERCENT |
                                                              GTK_CSS_POSITIVE_ONLY);
              if (num == NULL)
                return FALSE;

              if (gtk_css_number_value_get_dimension (num) != GTK_CSS_DIMENSION_NUMBER)
                {
                  values[5] = num;
                  goto have_font_size;
                }

              values[3] = num;
              if (_gtk_css_number_value_get (values[3], 100) < 1 || 
                  _gtk_css_number_value_get (values[3], 100) > 1000)
                {
                  gtk_css_parser_error_value (parser, "Font weight values must be between 1 and 1000");
                  g_clear_pointer (&values[3], gtk_css_value_unref);
                  return FALSE;
                }
            }
          parsed_one = parsed_one || values[3] != NULL;
        }

      if (values[4] == NULL)
        {
          values[4] = _gtk_css_font_stretch_value_try_parse (parser);
          parsed_one = parsed_one || values[4] != NULL;
        }
    }
  while (parsed_one && !value_is_done_parsing (parser));

  values[5] = gtk_css_font_size_value_parse (parser);
  if (values[5] == NULL)
    return FALSE;

have_font_size:
  values[0] = gtk_css_font_family_value_parse (parser);
  if (values[0] == NULL)
    return FALSE;

  return TRUE;
}

static gboolean
parse_one_background (GtkCssShorthandProperty  *shorthand,
                      GtkCssValue             **values,
                      GtkCssParser             *parser)
{
  GtkCssValue *value = NULL;

  do
    {
      /* the image part */
      if (values[0] == NULL &&
          (gtk_css_parser_has_ident (parser, "none") ||
           _gtk_css_image_can_parse (parser)))
        {
          GtkCssImage *image;

          if (gtk_css_parser_try_ident (parser, "none"))
            image = NULL;
          else
            {
              image = _gtk_css_image_new_parse (parser);
              if (image == NULL)
                return FALSE;
            }

          values[0] = _gtk_css_image_value_new (image);
        }
      else if (values[1] == NULL &&
               (value = _gtk_css_position_value_try_parse (parser)))
        {
          values[1] = value;
          value = NULL;

          if (gtk_css_parser_try_delim (parser, '/') &&
              (value = _gtk_css_bg_size_value_parse (parser)))
            {
              values[2] = value;
              value = NULL;
            }
        }
      else if (values[3] == NULL &&
               (value = _gtk_css_background_repeat_value_try_parse (parser)))
        {
          values[3] = value;
          value = NULL;
        }
      else if ((values[4] == NULL || values[5] == NULL) &&
               (value = _gtk_css_area_value_try_parse (parser)))
        {
          values[4] = value;

          if (values[5] == NULL)
            {
              values[5] = values[4];
              values[4] = NULL;
            }
          value = NULL;
        }
      else if (values[6] == NULL)
        {
          value = _gtk_css_color_value_parse (parser);
          if (value == NULL)
            values[6] = _gtk_css_value_ref (_gtk_css_style_property_get_initial_value 
                                            (_gtk_css_shorthand_property_get_subproperty (shorthand, 6)));
          else
            values[6] = value;

          value = NULL;
        }
      else
        {
          /* We parsed everything and there's still stuff left?
           * Pretend we didn't notice and let the normal code produce
           * a 'junk at end of value' error
           */
          break;
        }
    }
  while (!value_is_done_parsing (parser));

  if (values[5] != NULL && values[4] == NULL)
    values[4] = _gtk_css_value_ref (values[5]);

  return TRUE;
}

static gboolean
parse_background (GtkCssShorthandProperty  *shorthand,
                  GtkCssValue             **values,
                  GtkCssParser             *parser)
{
  GtkCssValue *step_values[7];
  GPtrArray *arrays[6];
  guint i;

  for (i = 0; i < 6; i++)
    {
      arrays[i] = g_ptr_array_new ();
      step_values[i] = NULL;
    }
  
  step_values[6] = NULL;

  do {
    if (!parse_one_background (shorthand, step_values, parser))
      {
        for (i = 0; i < 6; i++)
          {
            g_ptr_array_set_free_func (arrays[i], (GDestroyNotify) _gtk_css_value_unref);
            g_ptr_array_unref (arrays[i]);
          }
        return FALSE;
      }

      for (i = 0; i < 6; i++)
        {
          if (step_values[i] == NULL)
            {
              GtkCssValue *initial = _gtk_css_style_property_get_initial_value (
                                         _gtk_css_shorthand_property_get_subproperty (shorthand, i));
              step_values[i] = _gtk_css_value_ref (_gtk_css_array_value_get_nth (initial, 0));
            }

          g_ptr_array_add (arrays[i], step_values[i]);
          step_values[i] = NULL;
        }
  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  for (i = 0; i < 6; i++)
    {
      values[i] = _gtk_css_array_value_new_from_array ((GtkCssValue **) arrays[i]->pdata, arrays[i]->len);
      g_ptr_array_unref (arrays[i]);
    }

  values[6] = step_values[6];

  return TRUE;
}

static gboolean
has_transition_property (GtkCssParser *parser,
                         gpointer      option_data,
                         gpointer      user_data)
{
  return gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT);
}

static gboolean
parse_transition_property (GtkCssParser *parser,
                           gpointer      option_data,
                           gpointer      user_data)
{
  GtkCssValue **value = option_data;

  *value = _gtk_css_ident_value_try_parse (parser);
  g_assert (*value);

  return TRUE;
}

static gboolean
parse_transition_time (GtkCssParser *parser,
                       gpointer      option_data,
                       gpointer      user_data)
{
  GtkCssValue **value = option_data;

  *value = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_TIME);

  return *value != NULL;
}

static gboolean
parse_transition_timing_function (GtkCssParser *parser,
                                  gpointer      option_data,
                                  gpointer      user_data)
{
  GtkCssValue **value = option_data;

  *value = _gtk_css_ease_value_parse (parser);

  return *value != NULL;
}

static gboolean
parse_one_transition (GtkCssShorthandProperty  *shorthand,
                      GtkCssValue             **values,
                      GtkCssParser             *parser)
{
  const GtkCssParseOption options[] = {
    { (void *) _gtk_css_ease_value_can_parse, parse_transition_timing_function, &values[3] },
    { (void *) gtk_css_number_value_can_parse, parse_transition_time, &values[1] },
    { (void *) gtk_css_number_value_can_parse, parse_transition_time, &values[2] },
    { (void *) has_transition_property, parse_transition_property, &values[0] },
  };

  return gtk_css_parser_consume_any (parser, options, G_N_ELEMENTS (options), NULL);
}

static gboolean
parse_transition (GtkCssShorthandProperty  *shorthand,
                  GtkCssValue             **values,
                  GtkCssParser             *parser)
{
  GtkCssValue *step_values[4];
  GPtrArray *arrays[4];
  guint i;

  for (i = 0; i < 4; i++)
    {
      arrays[i] = g_ptr_array_new ();
      step_values[i] = NULL;
    }

  do {
    if (!parse_one_transition (shorthand, step_values, parser))
      {
        for (i = 0; i < 4; i++)
          {
            g_ptr_array_set_free_func (arrays[i], (GDestroyNotify) _gtk_css_value_unref);
            g_ptr_array_unref (arrays[i]);
          }
        return FALSE;
      }

      for (i = 0; i < 4; i++)
        {
          if (step_values[i] == NULL)
            {
              GtkCssValue *initial = _gtk_css_style_property_get_initial_value (
                                         _gtk_css_shorthand_property_get_subproperty (shorthand, i));
              step_values[i] = _gtk_css_value_ref (_gtk_css_array_value_get_nth (initial, 0));
            }

          g_ptr_array_add (arrays[i], step_values[i]);
          step_values[i] = NULL;
        }
  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  for (i = 0; i < 4; i++)
    {
      values[i] = _gtk_css_array_value_new_from_array ((GtkCssValue **) arrays[i]->pdata, arrays[i]->len);
      g_ptr_array_unref (arrays[i]);
    }

  return TRUE;
}

static gboolean
parse_one_animation (GtkCssShorthandProperty  *shorthand,
                     GtkCssValue             **values,
                     GtkCssParser             *parser)
{
  do
    {
      if (values[1] == NULL && gtk_css_parser_try_ident (parser, "infinite"))
        {
          values[1] = _gtk_css_number_value_new (HUGE_VAL, GTK_CSS_NUMBER);
        }
      else if ((values[1] == NULL || values[3] == NULL) &&
               gtk_css_number_value_can_parse (parser))
        {
          GtkCssValue *value;
          
          value = _gtk_css_number_value_parse (parser,
                                               GTK_CSS_POSITIVE_ONLY
                                               | (values[1] == NULL ? GTK_CSS_PARSE_NUMBER : 0)
                                               | (values[3] == NULL ? GTK_CSS_PARSE_TIME : 0));
          if (value == NULL)
            return FALSE;

          if (gtk_css_number_value_get_dimension (value) == GTK_CSS_DIMENSION_NUMBER)
            values[1] = value;
          else if (values[2] == NULL)
            values[2] = value;
          else
            values[3] = value;
        }
      else if (values[4] == NULL &&
               _gtk_css_ease_value_can_parse (parser))
        {
          values[4] = _gtk_css_ease_value_parse (parser);

          if (values[4] == NULL)
            return FALSE;
        }
      else if (values[5] == NULL &&
               (values[5] = _gtk_css_direction_value_try_parse (parser)))
        {
          /* nothing to do */
        }
      else if (values[6] == NULL &&
               (values[6] = _gtk_css_fill_mode_value_try_parse (parser)))
        {
          /* nothing to do */
        }
      else if (values[0] == NULL &&
               (values[0] = _gtk_css_ident_value_try_parse (parser)))
        {
          /* nothing to do */
          /* keep in mind though that this needs to come last as fill modes, directions
           * etc are valid idents */
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
parse_animation (GtkCssShorthandProperty  *shorthand,
                 GtkCssValue             **values,
                 GtkCssParser             *parser)
{
  GtkCssValue *step_values[7];
  GPtrArray *arrays[7];
  guint i;

  for (i = 0; i < 7; i++)
    {
      arrays[i] = g_ptr_array_new ();
      step_values[i] = NULL;
    }
  
  do {
    if (!parse_one_animation (shorthand, step_values, parser))
      {
        for (i = 0; i < 7; i++)
          {
            g_ptr_array_set_free_func (arrays[i], (GDestroyNotify) _gtk_css_value_unref);
            g_ptr_array_unref (arrays[i]);
          }
        return FALSE;
      }

      for (i = 0; i < 7; i++)
        {
          if (step_values[i] == NULL)
            {
              GtkCssValue *initial = _gtk_css_style_property_get_initial_value (
                                         _gtk_css_shorthand_property_get_subproperty (shorthand, i));
              step_values[i] = _gtk_css_value_ref (_gtk_css_array_value_get_nth (initial, 0));
            }

          g_ptr_array_add (arrays[i], step_values[i]);
          step_values[i] = NULL;
        }
  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  for (i = 0; i < 7; i++)
    {
      values[i] = _gtk_css_array_value_new_from_array ((GtkCssValue **) arrays[i]->pdata, arrays[i]->len);
      g_ptr_array_unref (arrays[i]);
    }

  return TRUE;
}

static gboolean
parse_text_decoration (GtkCssShorthandProperty  *shorthand,
                       GtkCssValue             **values,
                       GtkCssParser             *parser)
{
  do
  {
    if (values[0] == NULL &&
        (values[0] = _gtk_css_text_decoration_line_value_try_parse (parser)))
      {
        if (values[0] == NULL)
          return FALSE;
      }
    else if (values[1] == NULL &&
        (values[1] = _gtk_css_text_decoration_style_value_try_parse (parser)))
      {
        if (values[1] == NULL)
          return FALSE;
      }
    else if (values[2] == NULL)
      {
        values[2] = _gtk_css_color_value_parse (parser);
        if (values[2] == NULL)
          return FALSE;
      }
    else
      {
        /* We parsed and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error */
        break;
      }
  }
  while (!value_is_done_parsing (parser));

  return TRUE;
}

static gboolean
parse_font_variant (GtkCssShorthandProperty  *shorthand,
                    GtkCssValue             **values,
                    GtkCssParser             *parser)
{
  if (gtk_css_parser_try_ident (parser, "normal"))
    {
      /* all initial values */
    }
  else if (gtk_css_parser_try_ident (parser, "none"))
    {
      /* all initial values, except for font-variant-ligatures */
      values[0] = _gtk_css_font_variant_ligature_value_new (GTK_CSS_FONT_VARIANT_LIGATURE_NONE);
    }
  else
    {
      GtkCssFontVariantLigature ligatures;
      GtkCssFontVariantNumeric numeric;
      GtkCssFontVariantEastAsian east_asian;

      ligatures = 0;
      numeric = 0;
      east_asian = 0;
      do {
        GtkCssFontVariantLigature parsed_ligature;
        GtkCssFontVariantNumeric parsed_numeric;
        GtkCssFontVariantEastAsian parsed_east_asian;

        parsed_ligature = _gtk_css_font_variant_ligature_try_parse_one (parser, ligatures);
        if (parsed_ligature == 0 && ligatures != 0)
          {
            gtk_css_parser_error_value (parser, "Invalid combination of ligature values");
            return FALSE;
          }
        if (parsed_ligature == GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL ||
            parsed_ligature == GTK_CSS_FONT_VARIANT_LIGATURE_NONE)
          {
            gtk_css_parser_error_value (parser, "Unexpected ligature value");
            return FALSE;
          }
        if (parsed_ligature != ligatures)
          {
            ligatures = parsed_ligature;
            goto found;
          }

        parsed_numeric = _gtk_css_font_variant_numeric_try_parse_one (parser, numeric);
        if (parsed_numeric == 0 && numeric != 0)
          {
            gtk_css_parser_error_value (parser, "Invalid combination of numeric values");
            return FALSE;
          }
        if (parsed_numeric == GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL)
          {
            gtk_css_parser_error_value (parser, "Unexpected numeric value");
            return FALSE;
          }
        if (parsed_numeric != numeric)
          {
            numeric = parsed_numeric;
            goto found;
          }

        parsed_east_asian = _gtk_css_font_variant_east_asian_try_parse_one (parser, east_asian);
        if (parsed_east_asian == 0 && east_asian != 0)
          {
            gtk_css_parser_error_value (parser, "Invalid combination of east asian values");
            return FALSE;
          }
        if (parsed_east_asian == GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL)
          {
            gtk_css_parser_error_value (parser, "Unexpected east asian value");
            return FALSE;
          }
        if (parsed_east_asian != east_asian)
          {
            east_asian = parsed_east_asian;
            goto found;
          }

        if (values[1] == NULL)
          {
            values[1] = _gtk_css_font_variant_position_value_try_parse (parser);
            if (values[1])
              {
                if (_gtk_css_font_variant_position_value_get (values[1]) == GTK_CSS_FONT_VARIANT_POSITION_NORMAL)
                  {
                    gtk_css_parser_error_value (parser, "Unexpected position value");
                    return FALSE;
                  }
                goto found;
              }
          }
        if (values[2] == NULL)
          {
            values[2] = _gtk_css_font_variant_caps_value_try_parse (parser);
            if (values[2])
              {
                if (_gtk_css_font_variant_caps_value_get (values[2]) == GTK_CSS_FONT_VARIANT_CAPS_NORMAL)
                  {
                    gtk_css_parser_error_value (parser, "Unexpected caps value");
                    return FALSE;
                  }
                goto found;
              }
          }

        if (values[4] == NULL)
          {
            values[4] = _gtk_css_font_variant_alternate_value_try_parse (parser);
            if (values[4])
              {
                if (_gtk_css_font_variant_alternate_value_get (values[4]) == GTK_CSS_FONT_VARIANT_ALTERNATE_NORMAL)
                  {
                    gtk_css_parser_error_value (parser, "Unexpected alternate value");
                    return FALSE;
                  }
                goto found;
              }
          }

        gtk_css_parser_error_value (parser, "Unknown value for property");
        return FALSE;

found:
        if (value_is_done_parsing (parser))
          break;

      } while (1);

      if (ligatures != 0)
        {
          values[0] = _gtk_css_font_variant_ligature_value_new (ligatures);
          if (values[0] == NULL)
            {
              gtk_css_parser_error_value (parser, "Invalid combination of ligature values");
              return FALSE;
            }
        }

      if (numeric != 0)
        {
          values[3] = _gtk_css_font_variant_numeric_value_new (numeric);
          if (values[3] == NULL)
            {
              gtk_css_parser_error_value (parser, "Invalid combination of numeric values");
              return FALSE;
            }
        }

      if (east_asian != 0)
        {
          values[5] = _gtk_css_font_variant_east_asian_value_new (east_asian);
          if (values[5] == NULL)
            {
              gtk_css_parser_error_value (parser, "Invalid combination of east asian values");
              return FALSE;
            }
        }
    }

  return TRUE;
}

static gboolean
parse_all (GtkCssShorthandProperty  *shorthand,
           GtkCssValue             **values,
           GtkCssParser             *parser)
{
  gtk_css_parser_error_syntax (parser, "The 'all' property can only be set to 'initial', 'inherit' or 'unset'");
  return FALSE;
}

/*** PACKING ***/

static void
pack_border (GtkCssShorthandProperty *shorthand,
             GValue                  *value,
             GtkStyleQueryFunc        query_func,
             gpointer                 query_data)
{
  GtkCssStyleProperty *prop;
  GtkBorder border;
  GValue v;

  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 0);
  _gtk_style_property_query (GTK_STYLE_PROPERTY (prop), &v, query_func, query_data);
  border.top = g_value_get_int (&v);
  g_value_unset (&v);

  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 1);
  _gtk_style_property_query (GTK_STYLE_PROPERTY (prop), &v, query_func, query_data);
  border.right = g_value_get_int (&v);
  g_value_unset (&v);

  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 2);
  _gtk_style_property_query (GTK_STYLE_PROPERTY (prop), &v, query_func, query_data);
  border.bottom = g_value_get_int (&v);
  g_value_unset (&v);

  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 3);
  _gtk_style_property_query (GTK_STYLE_PROPERTY (prop), &v, query_func, query_data);
  border.left = g_value_get_int (&v);
  g_value_unset (&v);

  g_value_init (value, GTK_TYPE_BORDER);
  g_value_set_boxed (value, &border);
}

static void
pack_border_radius (GtkCssShorthandProperty *shorthand,
                    GValue                  *value,
                    GtkStyleQueryFunc        query_func,
                    gpointer                 query_data)
{
  GtkCssValue *v;
  int i = 0;

  v = (* query_func) (GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS, query_data);
  if (v)
    i = _gtk_css_corner_value_get_x (v, 100);

  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, i);
}

static void
pack_font_description (GtkCssShorthandProperty *shorthand,
                       GValue                  *value,
                       GtkStyleQueryFunc        query_func,
                       gpointer                 query_data)
{
  PangoFontDescription *description;
  GtkCssValue *v;

  description = pango_font_description_new ();

  v = (* query_func) (GTK_CSS_PROPERTY_FONT_FAMILY, query_data);
  if (v)
    {
      int i;
      GString *s = g_string_new ("");

      for (i = 0; i < _gtk_css_array_value_get_n_values (v); i++)
        {
          if (i > 0)
            g_string_append (s, ",");
          g_string_append (s, _gtk_css_string_value_get (_gtk_css_array_value_get_nth (v, i)));
        }

      pango_font_description_set_family (description, s->str);
      g_string_free (s, TRUE);
    }

  v = (* query_func) (GTK_CSS_PROPERTY_FONT_SIZE, query_data);
  if (v)
    pango_font_description_set_absolute_size (description, round (_gtk_css_number_value_get (v, 100) * PANGO_SCALE));

  v = (* query_func) (GTK_CSS_PROPERTY_FONT_STYLE, query_data);
  if (v)
    pango_font_description_set_style (description, _gtk_css_font_style_value_get (v));

  v = (* query_func) (GTK_CSS_PROPERTY_FONT_WEIGHT, query_data);
  if (v)
    pango_font_description_set_weight (description, _gtk_css_number_value_get (v, 100));

  v = (* query_func) (GTK_CSS_PROPERTY_FONT_STRETCH, query_data);
  if (v)
    pango_font_description_set_stretch (description, _gtk_css_font_stretch_value_get (v));

  g_value_init (value, PANGO_TYPE_FONT_DESCRIPTION);
  g_value_take_boxed (value, description);
}

static void
pack_first_element (GtkCssShorthandProperty *shorthand,
                    GValue                  *value,
                    GtkStyleQueryFunc        query_func,
                    gpointer                 query_data)
{
  GtkCssStyleProperty *prop;

  /* NB: This is a fallback for properties that originally were
   * not used as shorthand. We just pick the first subproperty
   * as a representative.
   * Lesson learned: Don't query the shorthand, query the 
   * real properties instead. */
  prop = _gtk_css_shorthand_property_get_subproperty (shorthand, 0);
  _gtk_style_property_query (GTK_STYLE_PROPERTY (prop),
                             value,
                             query_func,
                             query_data);
}

static void
_gtk_css_shorthand_property_register (const char                        *name,
                                      GType                              value_type,
                                      const char                       **subproperties,
                                      GtkCssShorthandPropertyParseFunc   parse_func,
                                      GtkCssShorthandPropertyQueryFunc   query_func)
{
  GtkCssShorthandProperty *node;

  node = g_object_new (GTK_TYPE_CSS_SHORTHAND_PROPERTY,
                       "name", name,
                       "value-type", value_type,
                       "subproperties", subproperties,
                       NULL);

  node->parse = parse_func;
  node->query = query_func;
}

/* NB: return value is transfer: container */
static const char **
get_all_subproperties (void)
{
  const char **properties;
  guint i, n;

  n = _gtk_css_style_property_get_n_properties ();
  properties = g_new (const char *, n + 1);
  properties[n] = NULL;

  for (i = 0; i < n; i++)
    {
      properties[i] = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (_gtk_css_style_property_lookup_by_id (i)));
    }

  return properties;
}

void
_gtk_css_shorthand_property_init_properties (void)
{
  /* The order is important here, be careful when changing it */
  const char *font_subproperties[] = { "font-family", "font-style", "font-variant-caps", "font-weight", "font-stretch", "font-size", NULL };
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
  const char *outline_radius_subproperties[] = { "-gtk-outline-top-left-radius", "-gtk-outline-top-right-radius",
                                                 "-gtk-outline-bottom-right-radius", "-gtk-outline-bottom-left-radius", NULL };
  const char *background_subproperties[] = { "background-image", "background-position", "background-size", "background-repeat", "background-clip", "background-origin",
                                             "background-color", NULL };
  const char *transition_subproperties[] = { "transition-property", "transition-duration", "transition-delay", "transition-timing-function", NULL };
  const char *animation_subproperties[] = { "animation-name", "animation-iteration-count", "animation-duration", "animation-delay", 
                                            "animation-timing-function", "animation-direction", "animation-fill-mode", NULL };
  const char *text_decoration_subproperties[] = { "text-decoration-line", "text-decoration-style", "text-decoration-color", NULL };
  const char *font_variant_subproperties[] = { "font-variant-ligatures", "font-variant-position", "font-variant-caps", "font-variant-numeric", "font-variant-alternates", "font-variant-east-asian", NULL };

  const char **all_subproperties;

  _gtk_css_shorthand_property_register   ("font",
                                          PANGO_TYPE_FONT_DESCRIPTION,
                                          font_subproperties,
                                          parse_font,
                                          pack_font_description);
  _gtk_css_shorthand_property_register   ("margin",
                                          GTK_TYPE_BORDER,
                                          margin_subproperties,
                                          parse_margin,
                                          pack_border);
  _gtk_css_shorthand_property_register   ("padding",
                                          GTK_TYPE_BORDER,
                                          padding_subproperties,
                                          parse_padding,
                                          pack_border);
  _gtk_css_shorthand_property_register   ("border-width",
                                          GTK_TYPE_BORDER,
                                          border_width_subproperties,
                                          parse_border_width,
                                          pack_border);
  _gtk_css_shorthand_property_register   ("border-radius",
                                          G_TYPE_INT,
                                          border_radius_subproperties,
                                          parse_border_radius,
                                          pack_border_radius);
  _gtk_css_shorthand_property_register   ("border-color",
                                          GDK_TYPE_RGBA,
                                          border_color_subproperties,
                                          parse_border_color,
                                          pack_first_element);
  _gtk_css_shorthand_property_register   ("border-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          border_style_subproperties,
                                          parse_border_style,
                                          pack_first_element);
  _gtk_css_shorthand_property_register   ("border-image",
                                          G_TYPE_NONE,
                                          border_image_subproperties,
                                          parse_border_image,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-top",
                                          G_TYPE_NONE,
                                          border_top_subproperties,
                                          parse_border_side,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-right",
                                          G_TYPE_NONE,
                                          border_right_subproperties,
                                          parse_border_side,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-bottom",
                                          G_TYPE_NONE,
                                          border_bottom_subproperties,
                                          parse_border_side,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-left",
                                          G_TYPE_NONE,
                                          border_left_subproperties,
                                          parse_border_side,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border",
                                          G_TYPE_NONE,
                                          border_subproperties,
                                          parse_border,
                                          NULL);
  _gtk_css_shorthand_property_register   ("-gtk-outline-radius",
                                          G_TYPE_INT,
                                          outline_radius_subproperties,
                                          parse_border_radius,
                                          pack_border_radius);
  _gtk_css_shorthand_property_register   ("outline",
                                          G_TYPE_NONE,
                                          outline_subproperties,
                                          parse_border_side,
                                          NULL);
  _gtk_css_shorthand_property_register   ("background",
                                          G_TYPE_NONE,
                                          background_subproperties,
                                          parse_background,
                                          NULL);
  _gtk_css_shorthand_property_register   ("transition",
                                          G_TYPE_NONE,
                                          transition_subproperties,
                                          parse_transition,
                                          NULL);
  _gtk_css_shorthand_property_register   ("animation",
                                          G_TYPE_NONE,
                                          animation_subproperties,
                                          parse_animation,
                                          NULL);
  _gtk_css_shorthand_property_register   ("text-decoration",
                                          G_TYPE_NONE,
                                          text_decoration_subproperties,
                                          parse_text_decoration,
                                          NULL);
  _gtk_css_shorthand_property_register   ("font-variant",
                                          G_TYPE_NONE,
                                          font_variant_subproperties,
                                          parse_font_variant,
                                          NULL);

  all_subproperties = get_all_subproperties ();
  _gtk_css_shorthand_property_register   ("all",
                                          G_TYPE_NONE,
                                          all_subproperties,
                                          parse_all,
                                          NULL);
  g_free (all_subproperties);
}
