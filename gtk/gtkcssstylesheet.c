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

#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssStyleSheetPrivate GtkCssStyleSheetPrivate;
struct _GtkCssStyleSheetPrivate {
  GtkCssRule *parent_rule;
  GtkCssRuleList *css_rules;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssStyleSheet, gtk_css_style_sheet, G_TYPE_OBJECT)

static void
gtk_css_style_sheet_finalize (GObject *object)
{
  GtkCssStyleSheet *style_sheet = GTK_CSS_STYLE_SHEET (object);
  GtkCssStyleSheetPrivate *priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  g_object_unref (priv->css_rules);

  G_OBJECT_CLASS (gtk_css_style_sheet_parent_class)->finalize (object);
}

static void
gtk_css_style_sheet_class_init (GtkCssStyleSheetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_style_sheet_finalize;
}

static void
gtk_css_style_sheet_init (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  priv->css_rules = gtk_css_rule_list_new ();
}

GtkCssStyleSheet *
gtk_css_style_sheet_new (void)
{
  return g_object_new (GTK_TYPE_CSS_STYLE_SHEET, NULL);
}

void
gtk_css_style_sheet_parse (GtkCssStyleSheet  *style_sheet,
                           GtkCssTokenSource *source)
{
  GtkCssStyleSheetPrivate *priv;

  g_return_if_fail (GTK_IS_CSS_STYLE_SHEET (style_sheet));
  g_return_if_fail (source != NULL);

  priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  gtk_css_rule_list_parse (priv->css_rules, source, NULL, style_sheet);
}

GtkCssStyleSheet *
gtk_css_style_sheet_get_parent_style_sheet (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (style_sheet), NULL);

  priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  return gtk_css_rule_get_parent_style_sheet (priv->parent_rule);
}

GtkCssRule *
gtk_css_style_sheet_get_parent_rule (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (style_sheet), NULL);

  priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  return priv->parent_rule;
}

GtkCssRuleList *
gtk_css_style_sheet_get_css_rules (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (style_sheet), NULL);

  priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  return priv->css_rules;
}

