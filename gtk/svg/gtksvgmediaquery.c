/*
 * Copyright © 2026 Red Hat, Inc
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

#include "gtksvgmediaqueryprivate.h"
#include "gtkcssmediaqueryprivate.h"
#include "gtksvgprivate.h"

void
svg_css_media_condition_free (SvgCssMediaCondition *condition)
{
  switch (condition->type)
    {
    case SVG_CSS_MEDIA_NOT:
      svg_css_media_condition_free (condition->not.condition);
      break;
    case SVG_CSS_MEDIA_AND:
    case SVG_CSS_MEDIA_OR:
      for (unsigned int i = 0; i < condition->and_or.length; i++)
        svg_css_media_condition_free (condition->and_or.condition[i]);
      g_free (condition->and_or.condition);
      break;
    case SVG_CSS_MEDIA_FEATURE:
    default:
      /* nothing to do */ ;
    }

  g_free (condition);
}

gboolean
svg_css_media_condition_evaluate (SvgCssMediaCondition *condition,
                                  GArray               *features)
{
  switch (condition->type)
    {
    case SVG_CSS_MEDIA_NOT:
      return !svg_css_media_condition_evaluate (condition->not.condition, features);
    case SVG_CSS_MEDIA_AND:
      for (unsigned int i = 0; i < condition->and_or.length; i++)
        if (!svg_css_media_condition_evaluate (condition->and_or.condition[i], features))
          return FALSE;
      return TRUE;
    case SVG_CSS_MEDIA_OR:
      for (unsigned int i = 0; i < condition->and_or.length; i++)
        if (svg_css_media_condition_evaluate (condition->and_or.condition[i], features))
          return TRUE;
      return FALSE;
    case SVG_CSS_MEDIA_FEATURE:
      break;
    default:
      g_assert_not_reached ();
    }

  for (unsigned int i = 0; i < features->len; i++)
    {
      GtkCssDiscreteMediaFeature *feature = &g_array_index (features, GtkCssDiscreteMediaFeature, i);
      if (strcmp (feature->name, condition->feature.name) == 0)
        return strcmp (feature->value, condition->feature.value) == 0;
    }

  g_warning ("Undefined @media feature '%s'", condition->feature.value);

  return FALSE;
}

void
svg_css_media_block_free (SvgCssMediaBlock *media)
{
  svg_css_media_condition_free (media->condition);
  g_free (media);
}

void
svg_css_media_block_evaluate (SvgCssMediaBlock *media,
                              GArray           *features)
{
  if (media->next && !media->next->value)
    media->value = FALSE;
  else if (media->condition)
    media->value = svg_css_media_condition_evaluate (media->condition, features);
  else
    media->value = TRUE;
}

static SvgCssMediaCondition *
svg_css_media_condition_not (SvgCssMediaCondition *condition)
{
  SvgCssMediaCondition *c;

  if (condition == NULL)
    return NULL;

  c = g_new0 (SvgCssMediaCondition, 1);
  c->type = SVG_CSS_MEDIA_NOT;
  c->not.condition = condition;

  return c;
}

static SvgCssMediaCondition *
svg_css_media_condition_compound (SvgCssMediaType        type,
                                  size_t                 length,
                                  SvgCssMediaCondition **condition)
{
  SvgCssMediaCondition *c;

  c = g_new0 (SvgCssMediaCondition, 1);
  c->type = type;
  c->and_or.length = length;
  c->and_or.condition = condition;

  return c;
}

static SvgCssMediaCondition *
svg_css_media_condition_feature (const char *name,
                                 const char *value)
{
  SvgCssMediaCondition *c;

  c = g_new (SvgCssMediaCondition, 1);
  c->type = SVG_CSS_MEDIA_FEATURE;
  c->feature.name = name;
  c->feature.value = value;

  return c;
}

static SvgCssMediaCondition *
parse_media_feature (GtkCssParser *parser)
{
  const char *media_features[] = { "prefers-color-scheme", "prefers-contrast", "prefers-reduced-motion" };
  const char *color_scheme_vals[] = { "light", "dark" };
  const char *contrast_vals[] = { "no-preference", "less", "more", "custom" };
  const char *reduced_motion_vals[] = { "no-preference", "reduce" };
  unsigned int feature;

  if (parser_try_enum (parser, media_features, G_N_ELEMENTS (media_features), &feature))
    {
      const char **vals;
      size_t n_vals;
      unsigned int val;

      if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (parser, "Expected ':' after @media feature name");
          return NULL;
        }

      if (feature == 0)
        {
          vals = color_scheme_vals;
          n_vals = G_N_ELEMENTS (color_scheme_vals);
        }
      else if (feature == 1)
        {
          vals = contrast_vals;
          n_vals = G_N_ELEMENTS (contrast_vals);
        }
      else if (feature == 2)
        {
          vals = reduced_motion_vals;
          n_vals = G_N_ELEMENTS (reduced_motion_vals);
        }
      else
        g_assert_not_reached ();

      if (parser_try_enum (parser, vals, n_vals, &val))
        return svg_css_media_condition_feature (media_features[feature], vals[val]);
      else
        {
          gtk_css_parser_warn_syntax (parser, "Undefined %s value '%s'",
                                      media_features[feature],
                                      gtk_css_token_to_string (gtk_css_parser_get_token (parser)));
          gtk_css_parser_consume_token (parser);
        }
    }
  else
    {
      gtk_css_parser_warn_syntax (parser, "Undefined @media feature '%s'",
                                  gtk_css_token_to_string (gtk_css_parser_get_token (parser)));
      gtk_css_parser_consume_token (parser);
    }

  return NULL;
}

static SvgCssMediaCondition * parse_media_condition (GtkCssParser *parser);

static SvgCssMediaCondition *
parse_media_in_parens (GtkCssParser *parser)
{
  SvgCssMediaCondition *condition;

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_PARENS))
    {
      gtk_css_parser_error_syntax (parser, "Expected '(' after @media query");
      return NULL;
    }

  gtk_css_parser_start_block (parser);

  if (gtk_css_parser_has_ident (parser, "not") ||
      gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_PARENS))
    condition = parse_media_condition (parser);
  else
    condition = parse_media_feature (parser);

  gtk_css_parser_end_block (parser);

  return condition;
}

static SvgCssMediaCondition *
parse_media_condition (GtkCssParser *parser)
{
  SvgCssMediaCondition *condition;
  gboolean is_and;

  if (gtk_css_parser_try_ident (parser, "not"))
    condition = svg_css_media_condition_not (parse_media_in_parens (parser));
  else
    condition = parse_media_in_parens (parser);

  if (condition == NULL)
    return NULL;

  if ((is_and = gtk_css_parser_has_ident (parser, "and")) ||
      gtk_css_parser_has_ident (parser, "or"))
    {
      GPtrArray *a;

      a = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_css_media_condition_free);
      g_ptr_array_add (a, condition);

      while (gtk_css_parser_try_ident (parser, is_and ? "and" : "or"))
        {
          condition = parse_media_in_parens (parser);
          if (condition == NULL)
            {
              g_ptr_array_unref (a);
              return NULL;
            }
          g_ptr_array_add (a, condition);
        }

      condition = svg_css_media_condition_compound (is_and ? SVG_CSS_MEDIA_AND : SVG_CSS_MEDIA_OR,
                                                    a->len, (SvgCssMediaCondition **) a->pdata);
      g_ptr_array_free (a, FALSE);
    }

  return condition;
}

SvgCssMediaCondition *
parse_media_query (GtkCssParser *parser)
{
  SvgCssMediaCondition *condition;

  condition = parse_media_condition (parser);
  if (condition == NULL)
    return NULL;

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
    {
      GPtrArray *a = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_css_media_condition_free);
      g_ptr_array_add (a, condition);

      while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA))
        {
          condition = parse_media_condition (parser);
          if (condition == NULL)
            {
              g_ptr_array_unref (a);
              return NULL;
            }
          g_ptr_array_add (a, condition);
        }

      condition = svg_css_media_condition_compound (SVG_CSS_MEDIA_OR, a->len, (SvgCssMediaCondition **) a->pdata);
      g_ptr_array_free (a, FALSE);
    }

  return condition;
}

void
gtk_svg_update_media (GtkSvg *self)
{
  GArray *features;
  GtkCssDiscreteMediaFeature feature;
  GtkInterfaceColorScheme prefers_color_scheme = GTK_INTERFACE_COLOR_SCHEME_LIGHT;
  GtkInterfaceContrast prefers_contrast = GTK_INTERFACE_CONTRAST_NO_PREFERENCE;
  GtkReducedMotion prefers_reduced_motion = GTK_REDUCED_MOTION_NO_PREFERENCE;

  const char *color_scheme_vals[] = { "light", "light", "dark", "light" };
  const char *contrast_vals[] = { "no-preference", "no-preference", "more", "less" };
  const char *reduced_motion_vals[] = { "no-preference", "reduce" };

  features = g_array_sized_new (FALSE, FALSE, sizeof (GtkCssDiscreteMediaFeature), 3);

  if (self->settings)
    g_object_get (self->settings,
                  "gtk-interface-color-scheme", &prefers_color_scheme,
                  "gtk-interface-contrast", &prefers_contrast,
                  "gtk-interface-reduced-motion", &prefers_reduced_motion,
                  NULL);

  feature.name = "prefers-color-scheme";
  feature.value = color_scheme_vals[prefers_color_scheme];
  g_array_append_vals (features, &feature, 1);

  feature.name = "prefers-contrast";
  feature.value = contrast_vals[prefers_contrast];
  g_array_append_vals (features, &feature, 1);

  feature.name = "prefers-reduced-motion";
  feature.value = reduced_motion_vals[prefers_reduced_motion];
  g_array_append_vals (features, &feature, 1);

  for (GList *l = self->media; l; l = l->next)
    {
      SvgCssMediaBlock *media = l->data;
      svg_css_media_block_evaluate (media, features);
    }

  g_array_unref (features);

}
