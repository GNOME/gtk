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

/*
 * The Media Query parser is based on
 * https://www.w3.org/TR/mediaqueries-5/
 */


static gboolean parse_media_condition (GtkCssParser *parser, GArray *media_features);

/*
 * gtk_css_media_query_parse:
 *
 * Returns: TRUE if the query matches with the provided media features.
 */
gboolean
gtk_css_media_query_parse (GtkCssParser *parser, GArray *media_features)
{
  gboolean result = parse_media_condition (parser, media_features);

  while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA))
    result |= parse_media_condition (parser, media_features);

  return result;
}

/*
 * A feature, without parens: `<feature-name>: <feature-value>`.
 */
static gboolean
parse_media_feature (GtkCssParser *parser, GArray *media_features)
{
  const GtkCssToken *token;
  int i;
  GtkCssDiscreteMediaFeature *media_feature = NULL;

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

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    gtk_css_parser_consume_token (parser);

  if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
    {
      gtk_css_parser_error_syntax (parser, "Expected ':' after @media feature name");
      return FALSE;
    }

  return (media_feature != NULL) && gtk_css_parser_try_ident (parser, media_feature->value);
}

/*
 * ( <media-condition> | <media-feature> )
 */
static gboolean
parse_media_in_parens (GtkCssParser *parser, GArray *media_features)
{
  gboolean result;

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_PARENS))
    {
      gtk_css_parser_error_syntax (parser, "Expected '(' after @media query");
      return FALSE;
    }

  gtk_css_parser_start_block (parser);

  if (gtk_css_parser_has_ident (parser, "not") ||
      gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_PARENS))
    result = parse_media_condition (parser, media_features);
  else
    result = parse_media_feature (parser, media_features);

  gtk_css_parser_end_block (parser);

  return result;
}

/*
 * not <media-in-parens> | <media-in-parens> [ <and <media-in-parens>>* | <or <media-in-parens>>* ]
 */
static gboolean
parse_media_condition (GtkCssParser *parser, GArray *media_features)
{
  gboolean result;
  gboolean is_and;

  if (gtk_css_parser_try_ident (parser, "not"))
    result = !parse_media_in_parens (parser, media_features);
  else
    result = parse_media_in_parens (parser, media_features);

  while ((is_and = gtk_css_parser_has_ident (parser, "and")) ||
         gtk_css_parser_has_ident (parser, "or"))
    {
      gtk_css_parser_consume_token (parser);

      if (is_and)
        result &= parse_media_in_parens (parser, media_features);
      else
        result |= parse_media_in_parens (parser, media_features);
    }

  return result;
}
