/*
 * Copyright © 2016 Red Hat Inc.
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

#include "gtkcsskeyframeruleprivate.h"

#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssKeyframeRulePrivate GtkCssKeyframeRulePrivate;
struct _GtkCssKeyframeRulePrivate {
  double *offsets;
  gsize n_offsets;
  GtkCssStyleDeclaration *style;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssKeyframeRule, gtk_css_keyframe_rule, GTK_TYPE_CSS_RULE)

static void
gtk_css_keyframe_rule_finalize (GObject *object)
{
  GtkCssKeyframeRule *keyframe_rule = GTK_CSS_KEYFRAME_RULE (object);
  GtkCssKeyframeRulePrivate *priv = gtk_css_keyframe_rule_get_instance_private (keyframe_rule);

  g_free (priv->offsets);
  g_object_unref (priv->style);

  G_OBJECT_CLASS (gtk_css_keyframe_rule_parent_class)->finalize (object);
}

static void
gtk_css_keyframe_rule_class_init (GtkCssKeyframeRuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_keyframe_rule_finalize;
}

static void
gtk_css_keyframe_rule_init (GtkCssKeyframeRule *keyframe_rule)
{
  GtkCssKeyframeRulePrivate *priv = gtk_css_keyframe_rule_get_instance_private (keyframe_rule);

  priv->style = gtk_css_style_declaration_new (GTK_CSS_RULE (keyframe_rule));
}

static GtkCssRule *
gtk_css_keyframe_rule_new (GtkCssRule       *parent_rule,
                           GtkCssStyleSheet *parent_style_sheet)
{
  return g_object_new (GTK_TYPE_CSS_KEYFRAME_RULE,
                       "parent-rule", parent_rule,
                       "parent-stylesheet", parent_style_sheet,
                       NULL);
}

GtkCssRule *
gtk_css_keyframe_rule_new_parse (GtkCssTokenSource *source,
                                 GtkCssRule        *parent_rule,
                                 GtkCssStyleSheet  *parent_style_sheet)
{
  GtkCssKeyframeRulePrivate *priv;
  GtkCssTokenSource *style_source;
  const GtkCssToken *token;
  GtkCssRule *rule;
  GArray *offsets;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (parent_rule == NULL || GTK_IS_CSS_RULE (parent_rule), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (parent_style_sheet), NULL);

  rule = gtk_css_keyframe_rule_new (parent_rule, parent_style_sheet);
  priv = gtk_css_keyframe_rule_get_instance_private (GTK_CSS_KEYFRAME_RULE (rule));
  gtk_css_token_source_set_consumer (source, G_OBJECT (rule));

  offsets = g_array_new (FALSE, FALSE, sizeof (double));
  while (TRUE)
    {
      double offset;

      token = gtk_css_token_source_get_token (source);
      if (gtk_css_token_is_ident (token, "from"))
        offset = 0;
      else if (gtk_css_token_is_ident (token, "to"))
        offset = 100;
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
        offset = token->number.number;
      else 
        {
          gtk_css_token_source_error (source, "Expected percentage");
          gtk_css_token_source_consume_all (source);
          g_array_free (offsets, TRUE);
          g_object_unref (rule);
          return NULL;
        }
      g_array_append_val (offsets, offset);
      gtk_css_token_source_consume_token (source);
      token = gtk_css_token_source_get_token (source);
      if (!gtk_css_token_is (token, GTK_CSS_TOKEN_COMMA))
        break;
      gtk_css_token_source_consume_token (source);
    }

  priv->n_offsets = offsets->len;
  priv->offsets = (double *) g_array_free (offsets, FALSE);

  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      gtk_css_token_source_error (source, "Expected percentage");
      gtk_css_token_source_consume_all (source);
      g_object_unref (rule);
      return NULL;
    }
  gtk_css_token_source_consume_token (source);

  style_source = gtk_css_token_source_new_for_part (source, GTK_CSS_TOKEN_CLOSE_CURLY);
  gtk_css_style_declaration_parse (priv->style, style_source);
  gtk_css_token_source_unref (style_source);
  gtk_css_token_source_consume_token (source);

  return rule;
}

gsize
gtk_css_keyframe_rule_get_n_offsets (GtkCssKeyframeRule *rule)
{
  GtkCssKeyframeRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_KEYFRAME_RULE (rule), 0);

  priv = gtk_css_keyframe_rule_get_instance_private (rule);

  return priv->n_offsets;
}

double
gtk_css_keyframe_rule_get_offset (GtkCssKeyframeRule *rule,
                                  gsize               id)
{
  GtkCssKeyframeRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_KEYFRAME_RULE (rule), 0.0);

  priv = gtk_css_keyframe_rule_get_instance_private (rule);
  g_return_val_if_fail (id < priv->n_offsets, 0.0);

  return priv->offsets[id];
}

GtkCssStyleDeclaration *
gtk_css_keyframe_rule_get_style (GtkCssKeyframeRule *rule)
{
  GtkCssKeyframeRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_KEYFRAME_RULE (rule), 0);

  priv = gtk_css_keyframe_rule_get_instance_private (rule);

  return priv->style;
}
