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

#include "gtkcssdeclarationprivate.h"

#include "gtkcsslonghanddeclarationprivate.h"
#include "gtkcssshorthanddeclarationprivate.h"
#include "gtkcsswidgetstyledeclarationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

enum {
  PROP_0,
  PROP_NAME,
  PROP_PARENT_STYLE,
  NUM_PROPERTIES
};

typedef struct _GtkCssDeclarationPrivate GtkCssDeclarationPrivate;
struct _GtkCssDeclarationPrivate {
  GtkCssStyleDeclaration *parent_style;
};

static GParamSpec *declaration_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkCssDeclaration, gtk_css_declaration, G_TYPE_OBJECT)

static void
gtk_css_declaration_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkCssDeclaration *declaration = GTK_CSS_DECLARATION (gobject);
  GtkCssDeclarationPrivate *priv = gtk_css_declaration_get_instance_private (declaration);

  switch (prop_id)
    {
    case PROP_PARENT_STYLE:
      priv->parent_style = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_css_declaration_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkCssDeclaration *declaration = GTK_CSS_DECLARATION (gobject);
  GtkCssDeclarationPrivate *priv = gtk_css_declaration_get_instance_private (declaration);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, gtk_css_declaration_get_name (declaration));
      break;

    case PROP_PARENT_STYLE:
      g_value_set_object (value, priv->parent_style);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_css_declaration_class_init (GtkCssDeclarationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_css_declaration_set_property;
  object_class->get_property = gtk_css_declaration_get_property;

  declaration_props[PROP_NAME] =
      g_param_spec_string ("name",
                           P_("name"),
                           P_("Name this declaration defines a value for"),
                           NULL,
                           GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);
  declaration_props[PROP_PARENT_STYLE] =
      g_param_spec_object ("parent-style",
                           P_("parent style"),
                           P_("The parent style"),
                           GTK_TYPE_CSS_STYLE_DECLARATION,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, declaration_props);
}

static void
gtk_css_declaration_init (GtkCssDeclaration *declaration)
{
}

GtkCssDeclaration *
gtk_css_declaration_new_parse (GtkCssStyleDeclaration *style,
                               GtkCssTokenSource      *source)
{
  const GtkCssToken *token;
  GtkStyleProperty *prop;
  char *name;

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_token_source_error (source, "Expected a property name");
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
  name = g_utf8_strdown (token->string.string, -1);
  prop = _gtk_style_property_lookup (name);
  g_free (name);
  if (GTK_IS_CSS_STYLE_PROPERTY (prop))
    return gtk_css_longhand_declaration_new_parse (style, source);
  else if (GTK_IS_CSS_SHORTHAND_PROPERTY (prop))
    return gtk_css_shorthand_declaration_new_parse (style, source);
  else if (gtk_css_widget_style_declaration_accepts_name (token->string.string))
    return gtk_css_widget_style_declaration_new_parse (style, source);
  else
    {
      gtk_css_token_source_unknown (source,
                                    "Property name \"%s\" does not define a valid property",
                                    token->string.string);
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
}

GtkCssStyleDeclaration *
gtk_css_declaration_get_parent_style (GtkCssDeclaration *decl)
{
  GtkCssDeclarationPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_DECLARATION (decl), NULL);

  priv = gtk_css_declaration_get_instance_private (decl);

  return priv->parent_style;
}

const char *
gtk_css_declaration_get_name (GtkCssDeclaration *decl)
{
  g_return_val_if_fail (GTK_IS_CSS_DECLARATION (decl), NULL);

  return GTK_CSS_DECLARATION_GET_CLASS (decl)->get_name (decl);
}

void
gtk_css_declaration_print_value (GtkCssDeclaration *decl,
                                 GString           *string)
{
  g_return_if_fail (GTK_IS_CSS_DECLARATION (decl));
  g_return_if_fail (string != NULL);

  return GTK_CSS_DECLARATION_GET_CLASS (decl)->print_value (decl, string);
}

char *
gtk_css_declaration_get_value_string (GtkCssDeclaration *decl)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_CSS_DECLARATION (decl), NULL);

  string = g_string_new (NULL);
  gtk_css_declaration_print_value (decl, string);

  return g_string_free (string, FALSE);
}
