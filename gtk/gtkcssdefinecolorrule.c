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

#include "gtkcssdefinecolorruleprivate.h"

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssDefineColorRulePrivate GtkCssDefineColorRulePrivate;
struct _GtkCssDefineColorRulePrivate {
  char *name;
  GtkCssValue *color;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssDefineColorRule, gtk_css_define_color_rule, GTK_TYPE_CSS_RULE)

static void
gtk_css_define_color_rule_finalize (GObject *object)
{
  GtkCssDefineColorRule *define_color_rule = GTK_CSS_DEFINE_COLOR_RULE (object);
  GtkCssDefineColorRulePrivate *priv = gtk_css_define_color_rule_get_instance_private (define_color_rule);

  g_free (priv->name);
  if (priv->color)
    _gtk_css_value_unref (priv->color);

  G_OBJECT_CLASS (gtk_css_define_color_rule_parent_class)->finalize (object);
}

static void
gtk_css_define_color_rule_class_init (GtkCssDefineColorRuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_define_color_rule_finalize;
}

static void
gtk_css_define_color_rule_init (GtkCssDefineColorRule *define_color_rule)
{
}

static GtkCssRule *
gtk_css_define_color_rule_new (GtkCssRule       *parent_rule,
                               GtkCssStyleSheet *parent_style_sheet,
                               const char       *name,
                               GtkCssValue      *color)
{
  return g_object_new (GTK_TYPE_CSS_DEFINE_COLOR_RULE, NULL);
}

GtkCssRule *
gtk_css_define_color_rule_new_parse (GtkCssTokenSource *source,
                                     GtkCssRule        *parent_rule,
                                     GtkCssStyleSheet  *parent_style_sheet)
{
  const GtkCssToken *token;
  GtkCssRule *result;
  GtkCssValue *color;
  char *name;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (parent_rule == NULL || GTK_IS_CSS_RULE (parent_rule), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (parent_style_sheet), NULL);

  token = gtk_css_token_source_get_token (source);
  if (token->type != GTK_CSS_TOKEN_AT_KEYWORD ||
      g_ascii_strcasecmp (token->string.string, "define-color") != 0)
    {
      gtk_css_token_source_error (source, "Expected '@define-color'");
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
  gtk_css_token_source_consume_token (source);

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_token_source_error (source, "Expected name of color");
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
  name = g_strdup (token->string.string);
  gtk_css_token_source_consume_token (source);

  color = gtk_css_color_value_token_parse (source);
  if (color == NULL)
    {
      g_free (name);
      return NULL;
    }

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_SEMICOLON))
    {
      gtk_css_token_source_error (source, "Expected ';' at end of @define-color");
      gtk_css_token_source_consume_all (source);
      g_free (name);
      _gtk_css_value_unref (color);
      return NULL;
    }
  gtk_css_token_source_consume_token (source);

  result = gtk_css_define_color_rule_new (parent_rule, parent_style_sheet, name, color);
  g_free (name);
  _gtk_css_value_unref (color);
  return result;
}

