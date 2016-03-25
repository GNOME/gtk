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

#include "gtkintl.h"
#include "gtkprivate.h"

enum {
  PROP_0,
  PROP_CSS_RULES,
  PROP_FILE,
  PROP_OWNER_RULE,
  PROP_PARENT_STYLESHEET,
  NUM_PROPERTIES
};

typedef struct _GtkCssStyleSheetPrivate GtkCssStyleSheetPrivate;
struct _GtkCssStyleSheetPrivate {
  GtkCssRule *owner_rule;
  GtkCssRuleList *css_rules;

  GFile *file;
};

static GParamSpec *style_sheet_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssStyleSheet, gtk_css_style_sheet, G_TYPE_OBJECT)

static void
gtk_css_style_sheet_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkCssStyleSheet *style_sheet = GTK_CSS_STYLE_SHEET (gobject);
  GtkCssStyleSheetPrivate *priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  switch (prop_id)
    {
      case PROP_FILE:
        priv->file = g_value_dup_object (value);
        break;

      case PROP_OWNER_RULE:
        priv->owner_rule = g_value_dup_object (value);
        break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_css_style_sheet_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkCssStyleSheet *style_sheet = GTK_CSS_STYLE_SHEET (gobject);
  GtkCssStyleSheetPrivate *priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  switch (prop_id)
    {
      case PROP_CSS_RULES:
        g_value_set_object (value, priv->css_rules);
        break;

      case PROP_FILE:
        g_value_set_object (value, priv->file);
        break;

      case PROP_OWNER_RULE:
        g_value_set_object (value, priv->owner_rule);
        break;

      case PROP_PARENT_STYLESHEET:
        if (priv->owner_rule)
          g_value_set_object (value, gtk_css_rule_get_parent_style_sheet (priv->owner_rule));
        else
          g_value_set_object (value, NULL);
        break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_css_style_sheet_finalize (GObject *object)
{
  GtkCssStyleSheet *style_sheet = GTK_CSS_STYLE_SHEET (object);
  GtkCssStyleSheetPrivate *priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  g_object_unref (priv->css_rules);
  if (priv->owner_rule)
    g_object_unref (priv->owner_rule);
  if (priv->file)
    g_object_unref (priv->file);

  G_OBJECT_CLASS (gtk_css_style_sheet_parent_class)->finalize (object);
}

static void
gtk_css_style_sheet_class_init (GtkCssStyleSheetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_style_sheet_finalize;
  object_class->get_property = gtk_css_style_sheet_get_property;
  object_class->set_property = gtk_css_style_sheet_set_property;

  style_sheet_props[PROP_CSS_RULES] =
      g_param_spec_object ("css-rules",
                           P_("css rules"),
                           P_("The CSS rules of the style sheet"),
                           GTK_TYPE_CSS_RULE_LIST,
                           GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);
  style_sheet_props[PROP_FILE] =
      g_param_spec_object ("file",
                           P_("file"),
                           P_("The file the style sheet was loaded from or NULL if inline"),
                           G_TYPE_FILE,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);
  style_sheet_props[PROP_OWNER_RULE] =
      g_param_spec_object ("owner-rule",
                           P_("owner rule"),
                           P_("The CSS rule that loaded this style sheet or NULL"),
                           GTK_TYPE_CSS_RULE,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);
  style_sheet_props[PROP_PARENT_STYLESHEET] =
      g_param_spec_object ("parent-stylesheet",
                           P_("parent style sheet"),
                           P_("The parent style sheet that loaded this style sheet or NULL"),
                           GTK_TYPE_CSS_STYLE_SHEET,
                           GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, style_sheet_props);
}

static void
gtk_css_style_sheet_init (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  priv->css_rules = gtk_css_rule_list_new ();
}

static void
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
gtk_css_style_sheet_new (GtkCssTokenSource *source)
{
  GtkCssStyleSheet *sheet;

  g_return_val_if_fail (source != NULL, NULL);

  sheet = g_object_new (GTK_TYPE_CSS_STYLE_SHEET,
                        "file", gtk_css_token_source_get_location (source),
                        NULL);

  gtk_css_style_sheet_parse (sheet, source);

  return sheet;
}

GtkCssStyleSheet *
gtk_css_style_sheet_new_import (GtkCssTokenSource *source,
                                GtkCssRule        *owner_rule)
{
  GtkCssStyleSheet *sheet;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CSS_RULE (owner_rule), NULL);

  sheet = g_object_new (GTK_TYPE_CSS_STYLE_SHEET,
                        "file", gtk_css_token_source_get_location (source),
                        "owner-rule", owner_rule,
                        NULL);

  gtk_css_style_sheet_parse (sheet, source);

  return sheet;
}

GFile *
gtk_css_style_sheet_get_file (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (style_sheet), NULL);

  priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  return priv->file;
}

GtkCssStyleSheet *
gtk_css_style_sheet_get_parent_style_sheet (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (style_sheet), NULL);

  priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  if (priv->owner_rule == NULL)
    return NULL;

  return gtk_css_rule_get_parent_style_sheet (priv->owner_rule);
}

GtkCssRule *
gtk_css_style_sheet_get_owner_rule (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (style_sheet), NULL);

  priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  return priv->owner_rule;
}

GtkCssRuleList *
gtk_css_style_sheet_get_css_rules (GtkCssStyleSheet *style_sheet)
{
  GtkCssStyleSheetPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_STYLE_SHEET (style_sheet), NULL);

  priv = gtk_css_style_sheet_get_instance_private (style_sheet);

  return priv->css_rules;
}

