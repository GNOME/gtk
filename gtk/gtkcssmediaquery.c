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


GtkCssDiscreteMediaFeature *
_gtk_css_media_feature_new (const char *feature_name,
                            const char *feature_value)
{
  GtkCssDiscreteMediaFeature *media_feature;

  media_feature = g_new0 (GtkCssDiscreteMediaFeature, 1);
  _gtk_css_media_feature_init (media_feature, feature_name, feature_value);

  return media_feature;
}

void
_gtk_css_media_feature_init (GtkCssDiscreteMediaFeature *media_feature,
                             const char *feature_name,
                             const char *feature_value)
{
  g_assert (feature_name != NULL);
  g_assert (feature_value != NULL);

  media_feature->name = g_strdup (feature_name);
  media_feature->value = g_strdup (feature_value);
}

void
_gtk_css_media_feature_free (GtkCssDiscreteMediaFeature *media_feature)
{
  g_assert (media_feature != NULL);

  if (media_feature->name)
    g_free (media_feature->name);

  if (media_feature->value)
    g_free (media_feature->value);

  g_free (media_feature);
}

gboolean
_gtk_css_media_feature_match (GtkCssDiscreteMediaFeature *media_feature,
                              const char *feature_name,
                              const char *feature_value)
{
  // TODO
  return TRUE;
}


static gboolean
parse_media_feature (GtkCssParser *parser)
{
  const GtkCssToken *token;

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_PARENS))
    {
      gtk_css_parser_error_syntax (parser, "Expected '(' after @media query");
      return FALSE;
    }

  gtk_css_parser_start_block (parser);

  token = gtk_css_parser_get_token (parser);

  if (gtk_css_token_is_ident (token, "prefers-color-scheme"))
    {
      gtk_css_parser_consume_token (parser);
    }

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COLON))
    {
      gtk_css_parser_error_syntax (parser, "Expected ':' after @media feature name");
      gtk_css_parser_end_block (parser);
      return FALSE;
    }

  gtk_css_parser_consume_token (parser);

  token = gtk_css_parser_get_token (parser);

  if (gtk_css_token_is_ident (token, "dark"))
    {
      gtk_css_parser_consume_token (parser);
    }
  else if (gtk_css_token_is_ident (token, "light"))
    {
      gtk_css_parser_consume_token (parser);
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Expected 'dark'/'light'");
    }

  gtk_css_parser_end_block (parser);

  return TRUE;
}

static void
parse_media_query (GtkCssParser *parser)
{
  /* TODO: handle `and`, `or, `not` statements. */
  parse_media_feature (parser);
}

gboolean
_gtk_css_media_query_parse (GtkCssParser *parser, GArray *media_features)
{
  do {
      parse_media_query (parser);
    }
  while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  return TRUE;
}
