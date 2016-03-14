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

#include "gtkcssrulelistprivate.h"
#include "gtkcssstyleruleprivate.h"
#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssRuleListPrivate GtkCssRuleListPrivate;
struct _GtkCssRuleListPrivate {
  GPtrArray *items;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssRuleList, gtk_css_rule_list, G_TYPE_OBJECT)

static void
gtk_css_rule_list_finalize (GObject *object)
{
  GtkCssRuleList *rule_list = GTK_CSS_RULE_LIST (object);
  GtkCssRuleListPrivate *priv = gtk_css_rule_list_get_instance_private (rule_list);

  g_ptr_array_unref (priv->items);

  G_OBJECT_CLASS (gtk_css_rule_list_parent_class)->finalize (object);
}

static void
gtk_css_rule_list_class_init (GtkCssRuleListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_rule_list_finalize;
}

static void
gtk_css_rule_list_init (GtkCssRuleList *rule_list)
{
  GtkCssRuleListPrivate *priv = gtk_css_rule_list_get_instance_private (rule_list);
  
  priv->items = g_ptr_array_new_with_free_func (g_object_unref);
}

GtkCssRuleList *
gtk_css_rule_list_new (void)
{
  return g_object_new (GTK_TYPE_CSS_RULE_LIST, NULL);
}

void
gtk_css_rule_list_parse (GtkCssRuleList    *rule_list,
                         GtkCssTokenSource *source,
                         GtkCssRule        *parent_rule,
                         GtkCssStyleSheet  *parent_style_sheet)
{
  GtkCssRule *rule;
  const GtkCssToken *token;

  g_return_if_fail (GTK_IS_CSS_RULE_LIST (rule_list));
  g_return_if_fail (source != NULL);
  g_return_if_fail (parent_rule == NULL || GTK_IS_CSS_RULE (parent_rule));
  g_return_if_fail (GTK_IS_CSS_STYLE_SHEET (parent_style_sheet));

  gtk_css_token_source_set_consumer (source, G_OBJECT (rule_list));

  for (token = gtk_css_token_source_get_token (source);
       token->type != GTK_CSS_TOKEN_EOF;
       token = gtk_css_token_source_get_token (source))
    {
      switch (token->type)
        {
          case GTK_CSS_TOKEN_WHITESPACE:
            gtk_css_token_source_consume_token (source);
            break;

          case GTK_CSS_TOKEN_AT_KEYWORD:
            rule = gtk_css_rule_new_from_at_rule (source, parent_rule, parent_style_sheet);
            if (rule)
              gtk_css_rule_list_append (rule_list, rule);
            break;

          case GTK_CSS_TOKEN_CDO:
          case GTK_CSS_TOKEN_CDC:
            if (parent_rule == NULL)
              {
                gtk_css_token_source_consume_token (source);
                break;
              }
            /* else fall through */
          default:
            rule = gtk_css_style_rule_new_parse (source, parent_rule, parent_style_sheet);
            if (rule)
              gtk_css_rule_list_append (rule_list, rule);
            break;
        }
    }
}

void
gtk_css_rule_list_insert (GtkCssRuleList *rule_list,
                          gsize           id,
                          GtkCssRule     *rule)
{
  GtkCssRuleListPrivate *priv;

  g_return_if_fail (GTK_IS_CSS_RULE_LIST (rule_list));
  g_return_if_fail (GTK_IS_CSS_RULE (rule));

  priv = gtk_css_rule_list_get_instance_private (rule_list);

  g_ptr_array_insert (priv->items, id, g_object_ref (rule));
}

void
gtk_css_rule_list_append (GtkCssRuleList *rule_list,
                          GtkCssRule     *rule)
{
  GtkCssRuleListPrivate *priv;

  g_return_if_fail (GTK_IS_CSS_RULE_LIST (rule_list));
  g_return_if_fail (GTK_IS_CSS_RULE (rule));

  priv = gtk_css_rule_list_get_instance_private (rule_list);

  g_ptr_array_add (priv->items, g_object_ref (rule));
}

GtkCssRule *
gtk_css_rule_list_get_item (GtkCssRuleList *rule_list,
                            gsize           id)
{
  GtkCssRuleListPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_RULE_LIST (rule_list), NULL);

  priv = gtk_css_rule_list_get_instance_private (rule_list);
  g_return_val_if_fail (id < priv->items->len, NULL);

  return g_ptr_array_index (priv->items, id);
}

gsize
gtk_css_rule_list_get_length (GtkCssRuleList *rule_list)
{
  GtkCssRuleListPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_RULE_LIST (rule_list), 0);

  priv = gtk_css_rule_list_get_instance_private (rule_list);

  return priv->items->len;
}
