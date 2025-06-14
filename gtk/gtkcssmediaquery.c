/* GTK - The GIMP Toolkit
 * Copyright (C) 2025 Arjan Molenaar <amolenaarg@gnome.org>
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

#include "gtkcssmediaqueryprivate.h"


void
_gtk_css_media_feature_init (GtkCssDiscreteMediaFeature *media_feature,
                             const char                 *feature_name,
                             const char                 *feature_value)
{
  g_assert (feature_name != NULL);
  g_assert (feature_value != NULL);

  media_feature->name = g_strdup (feature_name);
  media_feature->value = g_strdup (feature_value);
}

void
_gtk_css_media_feature_update (GtkCssDiscreteMediaFeature *media_feature,
                               const char                 *feature_value)
{
  g_assert (media_feature != NULL);
  g_assert (feature_value != NULL);

  g_free (media_feature->value);

  media_feature->value = g_strdup (feature_value);
}

void
_gtk_css_media_feature_clear (GtkCssDiscreteMediaFeature *media_feature)
{
  g_assert (media_feature != NULL);

  g_clear_pointer (&media_feature->name, g_free);
  g_clear_pointer (&media_feature->value, g_free);
}

static gboolean
parse_media_feature (GtkCssParser *parser, GArray *media_features)
{
  const GtkCssToken *token;
  int i;
  GtkCssDiscreteMediaFeature *media_feature = NULL;
  gboolean match;

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_PARENS))
    {
      gtk_css_parser_error_syntax (parser, "Expected '(' after @media query");
      return FALSE;
    }

  gtk_css_parser_start_block (parser);

  token = gtk_css_parser_get_token (parser);

  for (i = 0; i < media_features->len; i++)
    {
      GtkCssDiscreteMediaFeature *mf = &g_array_index (media_features, GtkCssDiscreteMediaFeature, i);
      if (gtk_css_token_is_ident (token, mf->name))
        {
          media_feature = mf;
          break;
        }
    }

  if (media_feature == NULL)
    gtk_css_parser_warn_syntax (parser, "Undefined @media feature '%s'", gtk_css_token_to_string (token));

  gtk_css_parser_consume_token (parser);

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COLON))
    {
      gtk_css_parser_error_syntax (parser, "Expected ':' after @media feature name");
      gtk_css_parser_end_block (parser);
      return FALSE;
    }

  gtk_css_parser_consume_token (parser);

  token = gtk_css_parser_get_token (parser);

  match = (media_feature != NULL) && gtk_css_token_is_ident (token, media_feature->value);

  gtk_css_parser_consume_token (parser);

  gtk_css_parser_end_block (parser);

  return match;
}

static gboolean
parse_media_query (GtkCssParser *parser, GArray *media_features)
{
  const GtkCssToken *token;
  /* TODO: handle `and`, `or, `not` statements. */
  token = gtk_css_parser_get_token (parser);

  if (gtk_css_token_is_ident (token, "not"))
    {
      gtk_css_parser_consume_token (parser);
      return !parse_media_feature (parser, media_features);
    }

  return parse_media_feature (parser, media_features);
}

/*
 * _gtk_css_media_query_parse:
 *
 * Returns: TRUE if the query matches to the provided media features.
 */
gboolean
_gtk_css_media_query_parse (GtkCssParser *parser, GArray *media_features)
{
  gboolean result = parse_media_query (parser, media_features);

  while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA))
    result |= parse_media_query (parser, media_features);

  return result;
}
