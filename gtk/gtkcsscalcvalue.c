/* GTK - The GIMP Toolkit
 * Copyright © 2016 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gtkcsscalcvalueprivate.h"

#include <string.h>

GtkCssValue * gtk_css_calc_value_parse_sum (GtkCssParser             *parser,
                                            GtkCssNumberParseFlags    flags,
                                            GtkCssNumberParseContext *ctx);

static GtkCssValue *
gtk_css_calc_value_parse_value (GtkCssParser             *parser,
                                GtkCssNumberParseFlags    flags,
                                GtkCssNumberParseContext *ctx)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_PARENS))
    {
      GtkCssValue *result;

      gtk_css_parser_start_block (parser);

      result = gtk_css_calc_value_parse_sum (parser, flags, ctx);
      if (result == NULL)
        {
          gtk_css_parser_end_block (parser);
          return NULL;
        }

      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        {
          GtkCssLocation start = *gtk_css_parser_get_start_location (parser);
          gtk_css_parser_skip_until (parser, GTK_CSS_TOKEN_EOF);
          gtk_css_parser_error (parser,
                                GTK_CSS_PARSER_ERROR_SYNTAX,
                                &start,
                                gtk_css_parser_get_start_location (parser),
                                "Expected closing ')' in calc() subterm");
          gtk_css_value_unref (result);
          gtk_css_parser_end_block (parser);
          return NULL;
        }

      gtk_css_parser_end_block (parser);

      return result;
    }

  return gtk_css_number_value_parse_with_context (parser, flags, ctx);
}

static gboolean
is_number (GtkCssValue *value)
{
  return gtk_css_number_value_get_dimension (value) == GTK_CSS_DIMENSION_NUMBER
      && !gtk_css_number_value_has_percent (value);
}

static GtkCssValue *
gtk_css_calc_value_parse_product (GtkCssParser             *parser,
                                  GtkCssNumberParseFlags    flags,
                                  GtkCssNumberParseContext *ctx)
{
  GtkCssValue *result, *value, *temp;
  GtkCssNumberParseFlags actual_flags;
  GtkCssLocation start;

  actual_flags = flags | GTK_CSS_PARSE_NUMBER;
  gtk_css_parser_get_token (parser);
  start = *gtk_css_parser_get_start_location (parser);
  result = gtk_css_calc_value_parse_value (parser, actual_flags, ctx);
  if (result == NULL)
    return NULL;

  while (TRUE)
    {
      if (actual_flags != GTK_CSS_PARSE_NUMBER && !is_number (result))
        actual_flags = GTK_CSS_PARSE_NUMBER;

      if (gtk_css_parser_try_delim (parser, '*'))
        {
          value = gtk_css_calc_value_parse_product (parser, actual_flags, ctx);
          if (value == NULL)
            goto fail;
          if (is_number (value))
            temp = gtk_css_number_value_multiply (result, gtk_css_number_value_get (value, 100));
          else
            temp = gtk_css_number_value_multiply (value, gtk_css_number_value_get (result, 100));
          gtk_css_value_unref (value);
          gtk_css_value_unref (result);
          result = temp;
        }
      else if (gtk_css_parser_try_delim (parser, '/'))
        {
          value = gtk_css_calc_value_parse_product (parser, GTK_CSS_PARSE_NUMBER, ctx);
          if (value == NULL)
            goto fail;
          temp = gtk_css_number_value_multiply (result, 1.0 / gtk_css_number_value_get (value, 100));
          gtk_css_value_unref (value);
          gtk_css_value_unref (result);
          result = temp;
        }
      else
        {
          break;
        }
    }

  if (is_number (result) && !(flags & GTK_CSS_PARSE_NUMBER))
    {
      gtk_css_parser_error (parser,
                            GTK_CSS_PARSER_ERROR_SYNTAX,
                            &start,
                            gtk_css_parser_get_start_location (parser),
                            "calc() product term has no units");
      goto fail;
    }

  return result;

fail:
  gtk_css_value_unref (result);
  return NULL;
}

GtkCssValue *
gtk_css_calc_value_parse_sum (GtkCssParser             *parser,
                              GtkCssNumberParseFlags    flags,
                              GtkCssNumberParseContext *ctx)
{
  GtkCssValue *result;

  result = gtk_css_calc_value_parse_product (parser, flags, ctx);
  if (result == NULL)
    return NULL;

  while (TRUE)
    {
      GtkCssValue *next, *temp;

      if (gtk_css_parser_try_delim (parser, '+'))
        {
          next = gtk_css_calc_value_parse_product (parser, flags, ctx);
          if (next == NULL)
            goto fail;
        }
      else if (gtk_css_parser_try_delim (parser, '-'))
        {
          temp = gtk_css_calc_value_parse_product (parser, flags, ctx);
          if (temp == NULL)
            goto fail;
          next = gtk_css_number_value_multiply (temp, -1);
          gtk_css_value_unref (temp);
        }
      else
        {
          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
              gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
              gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
              gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_DIMENSION))
            {
              gtk_css_parser_error_syntax (parser, "Unexpected signed number, did you forget a space between sign and number?");
              gtk_css_parser_consume_token (parser);
            }
          break;
        }

      temp = gtk_css_number_value_add (result, next);
      gtk_css_value_unref (result);
      gtk_css_value_unref (next);
      result = temp;
    }

  return result;

fail:
  gtk_css_value_unref (result);
  return NULL;
}

typedef struct
{
  GtkCssNumberParseFlags flags;
  GtkCssNumberParseContext *ctx;
  GtkCssValue *value;
} ParseCalcData;

static guint
gtk_css_calc_value_parse_arg (GtkCssParser *parser,
                              guint         arg,
                              gpointer      data_)
{
  ParseCalcData *data = data_;

  data->value = gtk_css_calc_value_parse_sum (parser, data->flags, data->ctx);
  if (data->value == NULL)
    return 0;

  return 1;
}

GtkCssValue *
gtk_css_calc_value_parse (GtkCssParser             *parser,
                          GtkCssNumberParseFlags    flags,
                          GtkCssNumberParseContext *ctx)
{
  ParseCalcData data;

  /* This can only be handled at compute time, we allow '-' after all */
  data.flags = flags & ~GTK_CSS_POSITIVE_ONLY;
  data.value = NULL;
  data.ctx = ctx;

  if (!gtk_css_parser_has_function (parser, "calc"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'calc('");
      return NULL;
    }

  if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_calc_value_parse_arg, &data))
    return NULL;

  return data.value;
}

typedef struct
{
  GtkCssNumberParseFlags flags;
  GtkCssNumberParseContext *ctx;
  GPtrArray *values;
} ParseArgnData;

static guint
gtk_css_argn_value_parse_arg (GtkCssParser *parser,
                              guint         arg,
                              gpointer      data_)
{
  ParseArgnData *data = data_;
  GtkCssValue *value;

  value = gtk_css_calc_value_parse_sum (parser, data->flags, data->ctx);
  if (value == NULL)
    return 0;

  g_ptr_array_add (data->values, value);

  return 1;
}

typedef struct
{
  GtkCssNumberParseFlags flags;
  GtkCssNumberParseContext *ctx;
  GtkCssValue *values[3];
} ParseClampData;

static guint
gtk_css_clamp_value_parse_arg (GtkCssParser *parser,
                               guint         arg,
                               gpointer      data_)
{
  ParseClampData *data = data_;

  if ((arg == 0 || arg == 2))
    {
      if (gtk_css_parser_try_ident (parser, "none"))
        {
          data->values[arg] = NULL;
          return 1;
        }
    }

  data->values[arg] = gtk_css_calc_value_parse_sum (parser, data->flags, data->ctx);
  if (data->values[arg] == NULL)
    return 0;

  return 1;
}

GtkCssValue *
gtk_css_clamp_value_parse (GtkCssParser             *parser,
                           GtkCssNumberParseFlags    flags,
                           GtkCssNumberParseContext *ctx,
                           guint                     type)
{
  ParseClampData data;
  GtkCssValue *result = NULL;

  if (!gtk_css_parser_has_function (parser, "clamp"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'clamp('");
      return NULL;
    }

  /* This can only be handled at compute time, we allow '-' after all */
  data.flags = flags & ~GTK_CSS_POSITIVE_ONLY;
  data.ctx = ctx;
  data.values[0] = NULL;
  data.values[1] = NULL;
  data.values[2] = NULL;

  if (gtk_css_parser_consume_function (parser, 3, 3, gtk_css_clamp_value_parse_arg, &data))
    {
      GtkCssDimension dim = gtk_css_number_value_get_dimension (data.values[1]);
      if ((data.values[0] && gtk_css_number_value_get_dimension (data.values[0]) != dim) ||
          (data.values[2] && gtk_css_number_value_get_dimension (data.values[2]) != dim))
        gtk_css_parser_error_syntax (parser, "Inconsistent types in 'clamp('");
      else
        result = gtk_css_math_value_new (type, 0, data.values, 3);
    }

  if (result == NULL)
    {
      g_clear_pointer (&data.values[0], gtk_css_value_unref);
      g_clear_pointer (&data.values[1], gtk_css_value_unref);
      g_clear_pointer (&data.values[2], gtk_css_value_unref);
    }

  return result;
}

typedef struct {
  GtkCssNumberParseFlags flags;
  GtkCssNumberParseContext *ctx;
  guint mode;
  gboolean has_mode;
  GtkCssValue *values[2];
} ParseRoundData;

static guint
gtk_css_round_value_parse_arg (GtkCssParser *parser,
                               guint         arg,
                               gpointer      data_)
{
  ParseRoundData *data = data_;

  if (arg == 0)
    {
      const char *modes[] = { "nearest", "up", "down", "to-zero" };

      for (guint i = 0; i < G_N_ELEMENTS (modes); i++)
        {
          if (gtk_css_parser_try_ident (parser, modes[i]))
            {
              data->mode = i;
              data->has_mode = TRUE;
              return 1;
            }
        }

      data->values[0] = gtk_css_calc_value_parse_sum (parser, data->flags, data->ctx);
      if (data->values[0] == NULL)
        return 0;
    }
  else if (arg == 1)
    {
      GtkCssValue *value = gtk_css_calc_value_parse_sum (parser, data->flags, data->ctx);

      if (value == NULL)
        return 0;

      if (data->has_mode)
        data->values[0] = value;
      else
        data->values[1] = value;
    }
  else
    {
      if (!data->has_mode)
        {
          gtk_css_parser_error_syntax (parser, "Too many argument for 'round'");
          return 0;
        }

      data->values[1] = gtk_css_calc_value_parse_sum (parser, data->flags, data->ctx);

      if (data->values[1] == NULL)
        return 0;
    }

  return 1;
}

GtkCssValue *
gtk_css_round_value_parse (GtkCssParser             *parser,
                           GtkCssNumberParseFlags    flags,
                           GtkCssNumberParseContext *ctx,
                           guint                     type)
{
  ParseRoundData data;
  GtkCssValue *result = NULL;

  if (!gtk_css_parser_has_function (parser, "round"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'round('");
      return NULL;
    }

  data.flags = flags & ~GTK_CSS_POSITIVE_ONLY;
  data.ctx = ctx;
  data.mode = ROUND_NEAREST;
  data.has_mode = FALSE;
  data.values[0] = NULL;
  data.values[1] = NULL;

  if (gtk_css_parser_consume_function (parser, 1, 3, gtk_css_round_value_parse_arg, &data) &&
      data.values[0] != NULL)
    {
      if (data.values[1] != NULL &&
          gtk_css_number_value_get_dimension (data.values[0]) !=
          gtk_css_number_value_get_dimension (data.values[1]))
        gtk_css_parser_error_syntax (parser, "Inconsistent types in 'round('");
      else if (data.values[1] == NULL &&
               gtk_css_number_value_get_dimension (data.values[0]) != GTK_CSS_DIMENSION_NUMBER)
        gtk_css_parser_error_syntax (parser, "Can't omit second argument to 'round(' here");
      else
        result = gtk_css_math_value_new (type, data.mode, data.values, data.values[1] != NULL ? 2 : 1);
    }

  if (result == NULL)
    {
      g_clear_pointer (&data.values[0], gtk_css_value_unref);
      g_clear_pointer (&data.values[1], gtk_css_value_unref);
    }

  return result;
}

typedef struct {
  GtkCssNumberParseFlags flags;
  GtkCssNumberParseContext *ctx;
  GtkCssValue *values[2];
} ParseArg2Data;

static guint
gtk_css_arg2_value_parse_arg (GtkCssParser *parser,
                              guint         arg,
                              gpointer      data_)
{
  ParseArg2Data *data = data_;

  data->values[arg] = gtk_css_calc_value_parse_sum (parser, data->flags, data->ctx);
  if (data->values[arg] == NULL)
    return 0;

  return 1;
}

GtkCssValue *
gtk_css_arg2_value_parse (GtkCssParser             *parser,
                          GtkCssNumberParseFlags    flags,
                          GtkCssNumberParseContext *ctx,
                          guint                     min_args,
                          guint                     max_args,
                          const char               *function,
                          guint                     type)
{
  ParseArg2Data data;
  GtkCssValue *result = NULL;

  g_assert (1 <= min_args && min_args <= max_args && max_args <= 2);

  if (!gtk_css_parser_has_function (parser, function))
    {
      gtk_css_parser_error_syntax (parser, "Expected '%s('", function);
      return NULL;
    }

  data.flags = flags & ~GTK_CSS_POSITIVE_ONLY;
  data.ctx = ctx;
  data.values[0] = NULL;
  data.values[1] = NULL;

  if (gtk_css_parser_consume_function (parser, min_args, max_args, gtk_css_arg2_value_parse_arg, &data))
    {
      if (data.values[1] != NULL &&
          gtk_css_number_value_get_dimension (data.values[0]) !=
          gtk_css_number_value_get_dimension (data.values[1]))
        gtk_css_parser_error_syntax (parser, "Inconsistent types in '%s('", function);
      else
        result = gtk_css_math_value_new (type, 0, data.values, data.values[1] != NULL ? 2 : 1);
    }

  if (result == NULL)
    {
      g_clear_pointer (&data.values[0], gtk_css_value_unref);
      g_clear_pointer (&data.values[1], gtk_css_value_unref);
    }

  return result;
}

GtkCssValue *
gtk_css_argn_value_parse (GtkCssParser             *parser,
                          GtkCssNumberParseFlags    flags,
                          GtkCssNumberParseContext *ctx,
                          const char               *function,
                          guint                     type)
{
  ParseArgnData data;
  GtkCssValue *result = NULL;

  if (!gtk_css_parser_has_function (parser, function))
    {
      gtk_css_parser_error_syntax (parser, "Expected '%s('", function);
      return NULL;
    }

  /* This can only be handled at compute time, we allow '-' after all */
  data.flags = flags & ~GTK_CSS_POSITIVE_ONLY;
  data.values = g_ptr_array_new ();
  data.ctx = ctx;

  if (gtk_css_parser_consume_function (parser, 1, G_MAXUINT, gtk_css_argn_value_parse_arg, &data))
    {
      GtkCssValue *val = (GtkCssValue *) g_ptr_array_index (data.values, 0);
      GtkCssDimension dim = gtk_css_number_value_get_dimension (val);
      guint i;
      for (i = 1; i < data.values->len; i++)
        {
          val = (GtkCssValue *) g_ptr_array_index (data.values, i);
          if (gtk_css_number_value_get_dimension (val) != dim)
            break;
        }
      if (i < data.values->len)
        gtk_css_parser_error_syntax (parser, "Inconsistent types in '%s('", function);
      else
        result = gtk_css_math_value_new (type, 0, (GtkCssValue **)data.values->pdata, data.values->len);
    }

  if (result == NULL)
    {
      for (guint i = 0; i < data.values->len; i++)
        gtk_css_value_unref ((GtkCssValue *)g_ptr_array_index (data.values, i));
    }

  g_ptr_array_unref (data.values);

  return result;
}
