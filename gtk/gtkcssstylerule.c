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

#include "gtkcssstyleruleprivate.h"

#include "gtkcssselectorprivate.h"
#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssStyleRulePrivate GtkCssStyleRulePrivate;
struct _GtkCssStyleRulePrivate {
  GPtrArray *selectors;
  GtkCssStyleDeclaration *style;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssStyleRule, gtk_css_style_rule, GTK_TYPE_CSS_RULE)

static void
gtk_css_style_rule_finalize (GObject *object)
{
  GtkCssStyleRule *style_rule = GTK_CSS_STYLE_RULE (object);
  GtkCssStyleRulePrivate *priv = gtk_css_style_rule_get_instance_private (style_rule);

  g_ptr_array_unref (priv->selectors);
  g_object_unref (priv->style);

  G_OBJECT_CLASS (gtk_css_style_rule_parent_class)->finalize (object);
}

static void
gtk_css_style_rule_class_init (GtkCssStyleRuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_style_rule_finalize;
}

static void
gtk_css_style_rule_init (GtkCssStyleRule *style_rule)
{
  GtkCssStyleRulePrivate *priv = gtk_css_style_rule_get_instance_private (style_rule);

  priv->selectors = g_ptr_array_new_with_free_func ((GDestroyNotify) _gtk_css_selector_free);
  priv->style = gtk_css_style_declaration_new (GTK_CSS_RULE (style_rule));
}

static GtkCssRule *
gtk_css_style_rule_new (GtkCssRule       *parent_rule,
                        GtkCssStyleSheet *parent_style_sheet)
{
  return g_object_new (GTK_TYPE_CSS_STYLE_RULE, NULL);
}

static GtkCssStyleRule *
gtk_css_style_rule_parse_selectors (GtkCssStyleRule   *rule,
                                    GtkCssTokenSource *source)
{
  GtkCssStyleRulePrivate *priv = gtk_css_style_rule_get_instance_private (rule);
  GtkCssTokenSource *child_source;
  GtkCssSelector *selector;
  const GtkCssToken *token;

  for (token = gtk_css_token_source_get_token (source);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_token_source_get_token (source))
    {
      gtk_css_token_source_consume_whitespace (source);
      child_source = gtk_css_token_source_new_for_part (source, GTK_CSS_TOKEN_COMMA);
      selector = gtk_css_selector_token_parse (child_source);
      gtk_css_token_source_unref (child_source);
      if (selector == NULL)
        {
          g_object_unref (rule);
          return NULL;
        }
      g_ptr_array_add (priv->selectors, selector);
      gtk_css_token_source_consume_token (source);
    }

  return rule;
}

GtkCssRule *
gtk_css_style_rule_new_parse (GtkCssTokenSource *source,
                              GtkCssRule        *parent_rule,
                              GtkCssStyleSheet  *parent_style_sheet)
{
  GtkCssStyleRulePrivate *priv;
  GtkCssTokenSource *child_source;
  GtkCssStyleRule *rule;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (parent_rule == NULL || GTK_IS_CSS_RULE (parent_rule), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (parent_style_sheet), NULL);

  rule = GTK_CSS_STYLE_RULE (gtk_css_style_rule_new (parent_rule, parent_style_sheet));
  priv = gtk_css_style_rule_get_instance_private (rule);

  child_source = gtk_css_token_source_new_for_part (source, GTK_CSS_TOKEN_OPEN_CURLY);
  rule = gtk_css_style_rule_parse_selectors (rule, child_source);
  gtk_css_token_source_unref (child_source);

  gtk_css_token_source_consume_token (source);
  child_source = gtk_css_token_source_new_for_part (source, GTK_CSS_TOKEN_CLOSE_CURLY);
  if (rule == NULL)
    gtk_css_token_source_consume_all (child_source);
  else
    gtk_css_style_declaration_parse (priv->style, child_source);
  gtk_css_token_source_unref (child_source);
  gtk_css_token_source_consume_token (source);

  return GTK_CSS_RULE (rule);
}

GtkCssStyleDeclaration *
gtk_css_style_rule_get_style (GtkCssStyleRule *rule)
{
  GtkCssStyleRulePrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_STYLE_RULE (rule), NULL);

  priv = gtk_css_style_rule_get_instance_private (rule);

  return priv->style;
}
