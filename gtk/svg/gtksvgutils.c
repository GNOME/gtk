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

#include "gtksvgutilsprivate.h"
#include "gtksvgprivate.h"
#include "gtksvgnumberprivate.h"

/* Break string into tokens that are separated by whitespace */
GStrv
parse_strv (const char *string)
{
  const char *p, *p0;
  GStrvBuilder *builder;

  if (string == NULL)
    return NULL;

  builder = g_strv_builder_new ();
  p = string;
  while (*p)
    {
      while (*p == ' ')
        p++;

      if (!*p)
        break;

      p0 = p;
      while (*p && *p != ' ')
        p++;

      g_strv_builder_take (builder, g_strndup (p0, p - p0));
    }

  return g_strv_builder_unref_to_strv (builder);
}

GtkCssParser *
parser_new_for_string (const char *string)
{
  GBytes *bytes;
  GtkCssParser *parser;

  bytes = g_bytes_new_static (string, strlen (string));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);
  gtk_css_parser_skip_whitespace (parser);
  g_bytes_unref (bytes);

  return parser;
}

gboolean
parse_number_or_named (const char *string,
                       double      min,
                       double      max,
                       const char *name,
                       double      named_value,
                       double     *result)
{
  GtkCssParser *parser = parser_new_for_string (string);
  gboolean ret = FALSE;

  gtk_css_parser_skip_whitespace (parser);
  if (name && gtk_css_parser_try_ident (parser, name))
    {
      *result = named_value;
      ret = TRUE;
    }
  else
    {
      double d;
      SvgUnit u;

      if (svg_number_parse2 (parser, min, max, SVG_PARSE_NUMBER, &d, &u))
        {
          *result = d;
          ret = TRUE;
        }
    }
  gtk_css_parser_skip_whitespace (parser);
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    ret = FALSE;

  gtk_css_parser_unref (parser);

  return ret;
}

gboolean
parse_number (const char *string,
              double      min,
              double      max,
              double     *result)
{
  return parse_number_or_named (string, min, max, NULL, 0, result);
}

gboolean
parser_try_duration (GtkCssParser *parser,
                     int64_t      *result)
{
  double v;
  SvgUnit unit;

  if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_TIME, &v, &unit))
    {
      if (unit == SVG_UNIT_NUMBER)
        *result = (int64_t) round (v * G_TIME_SPAN_SECOND);
      else if (unit == SVG_UNIT_MS)
        *result = (int64_t) round (v * G_TIME_SPAN_MILLISECOND);
      else if (unit == SVG_UNIT_S)
        *result = (int64_t) round (v * G_TIME_SPAN_SECOND);
      else
        return FALSE;

      return TRUE;
    }

  return FALSE;
}

gboolean
parse_duration (const char *string,
                gboolean    allow_indefinite,
                int64_t    *result)
{
  GtkCssParser *parser = parser_new_for_string (string);
  gboolean ret = FALSE;

  gtk_css_parser_skip_whitespace (parser);
  if (allow_indefinite && gtk_css_parser_try_ident (parser, "indefinite"))
    {
      *result = INDEFINITE;
      ret = TRUE;
    }
  else if (parser_try_duration (parser, result))
    {
      ret = TRUE;
    }

  gtk_css_parser_skip_whitespace (parser);
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    ret = FALSE;

  gtk_css_parser_unref (parser);
  return ret;
}

gboolean
parser_try_enum (GtkCssParser  *parser,
                 const char   **values,
                 size_t         n_values,
                 unsigned int  *result)
{
  for (unsigned int i = 0; i < n_values; i++)
    {
      if (gtk_css_parser_try_ident (parser, values[i]))
        {
          *result = i;
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
parse_enum (const char    *string,
            const char   **values,
            size_t         n_values,
            unsigned int  *result)
{
  GtkCssParser *parser = parser_new_for_string (string);
  gboolean ret;

  gtk_css_parser_skip_whitespace (parser);
  ret = parser_try_enum (parser, values, n_values, result);
  gtk_css_parser_skip_whitespace (parser);
  ret = ret && gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF);
  gtk_css_parser_unref (parser);

  return ret;
}

gboolean
parser_parse_list (GtkCssParser  *parser,
                   ItemParseFunc  func,
                   gpointer       data)
{
  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_skip_whitespace (parser);
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_EOF);

      if (!func (parser, data))
        {
          gtk_css_parser_end_block (parser);
          return FALSE;
        }

      gtk_css_parser_skip_whitespace (parser);
      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        {
          gtk_css_parser_error_syntax (parser, "Junk at end of value");
          gtk_css_parser_end_block (parser);
          return FALSE;
        }

      gtk_css_parser_end_block (parser);
    }

  return TRUE;
}

typedef struct
{
  double min, max;
  GArray *array;
} NumData;

static gboolean
parse_one_number (GtkCssParser *parser,
                  gpointer      data)
{
  NumData *d = data;
  double v;

  if (!gtk_css_parser_consume_number (parser, &v))
    return FALSE;

  if (v < d->min || d->max < v)
    return FALSE;

  g_array_append_val (d->array, v);
  return TRUE;
}

GArray *
parse_number_list (const char *string,
                   double      min,
                   double      max)
{
  GtkCssParser *parser = parser_new_for_string (string);
  NumData data;

  data.min = min;
  data.max = max;
  data.array = g_array_new (FALSE, TRUE, sizeof (double));

  if (!parser_parse_list (parser, parse_one_number, &data))
    g_clear_pointer (&data.array, g_array_unref);

  gtk_css_parser_skip_whitespace (parser);
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    g_clear_pointer (&data.array, g_array_unref);

  gtk_css_parser_unref (parser);
  return data.array;
}

void
skip_whitespace_and_optional_comma (GtkCssParser *parser)
{
  gtk_css_parser_skip_whitespace (parser);
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
    gtk_css_parser_skip (parser);
  gtk_css_parser_skip_whitespace (parser);
}

GArray *
array_new_with_clear_func (size_t         elem_size,
                           GDestroyNotify clear_func)
{
  GArray *a = g_array_new (FALSE, FALSE, elem_size);
  g_array_set_clear_func (a, clear_func);
  return a;
}

gboolean
path_in_stroke (GskPath                *path,
                const graphene_point_t *point,
                GskStroke              *stroke)
{
  GskPathPoint p;
  float dist;

  /* FIXME: not quite right */
  return gsk_path_get_closest_point (path, point, gsk_stroke_get_line_width (stroke) / 2, &p, &dist);
}
