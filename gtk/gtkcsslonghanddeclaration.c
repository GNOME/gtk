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

#include "gtkcsslonghanddeclarationprivate.h"

#include "gtkcssstylepropertyprivate.h"

typedef struct _GtkCssLonghandDeclarationPrivate GtkCssLonghandDeclarationPrivate;
struct _GtkCssLonghandDeclarationPrivate {
  GtkCssStyleProperty *prop;
  GtkCssValue *value;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssLonghandDeclaration, gtk_css_longhand_declaration, GTK_TYPE_CSS_DECLARATION)

static void
gtk_css_longhand_declaration_finalize (GObject *object)
{
  GtkCssLonghandDeclaration *longhand_declaration = GTK_CSS_LONGHAND_DECLARATION (object);
  GtkCssLonghandDeclarationPrivate *priv = gtk_css_longhand_declaration_get_instance_private (longhand_declaration);

  if (priv->value)
    _gtk_css_value_unref (priv->value);

  G_OBJECT_CLASS (gtk_css_longhand_declaration_parent_class)->finalize (object);
}

static const char *
gtk_css_longhand_declaration_get_name (GtkCssDeclaration *decl)
{
  GtkCssLonghandDeclaration *longhand_declaration = GTK_CSS_LONGHAND_DECLARATION (decl);
  GtkCssLonghandDeclarationPrivate *priv = gtk_css_longhand_declaration_get_instance_private (longhand_declaration);

  return _gtk_style_property_get_name (GTK_STYLE_PROPERTY (priv->prop));
}

static void
gtk_css_longhand_declaration_print_value (GtkCssDeclaration *decl,
                                          GString           *string)
{
  GtkCssLonghandDeclaration *longhand_declaration = GTK_CSS_LONGHAND_DECLARATION (decl);
  GtkCssLonghandDeclarationPrivate *priv = gtk_css_longhand_declaration_get_instance_private (longhand_declaration);

  _gtk_css_value_print (priv->value, string);
}

static void
gtk_css_longhand_declaration_class_init (GtkCssLonghandDeclarationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssDeclarationClass *decl_class = GTK_CSS_DECLARATION_CLASS (klass);

  object_class->finalize = gtk_css_longhand_declaration_finalize;

  decl_class->get_name = gtk_css_longhand_declaration_get_name;
  decl_class->print_value = gtk_css_longhand_declaration_print_value;
}

static void
gtk_css_longhand_declaration_init (GtkCssLonghandDeclaration *longhand_declaration)
{
}

GtkCssDeclaration *
gtk_css_longhand_declaration_new_parse (GtkCssStyleDeclaration *style,
                                         GtkCssTokenSource      *source)
{
  GtkCssLonghandDeclarationPrivate *priv;
  const GtkCssToken *token;
  GtkCssLonghandDeclaration *decl;
  char *name;

  decl = g_object_new (GTK_TYPE_CSS_LONGHAND_DECLARATION,
                       NULL);
  priv = gtk_css_longhand_declaration_get_instance_private (decl);

  gtk_css_token_source_set_consumer (source, G_OBJECT (decl));

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_token_source_error (source, "Expected a property name");
      gtk_css_token_source_consume_all (source);
      g_object_unref (decl);
      return NULL;
    }
  name = g_utf8_strdown (token->string.string, -1);
  priv->prop = (GtkCssStyleProperty *) _gtk_style_property_lookup (name);
  if (!GTK_IS_CSS_STYLE_PROPERTY (priv->prop))
    {
      gtk_css_token_source_unknown (source, "Property name '%s' is not a CSS property", token->string.string);
      gtk_css_token_source_consume_all (source);
      g_object_unref (decl);
      g_free (name);
      return NULL;
    }
  else if (!g_str_equal (name, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (priv->prop))))
    gtk_css_token_source_deprecated (source,
                                     "The '%s' property has been renamed to '%s'",
                                     name, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (priv->prop)));
  gtk_css_token_source_consume_token (source);
  g_free (name);

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_COLON))
    {
      gtk_css_token_source_error (source, "No colon following property name");
      gtk_css_token_source_consume_all (source);
      g_object_unref (decl);
      return NULL;
    }
  gtk_css_token_source_consume_token (source);
   
  priv->value = gtk_style_property_token_parse (GTK_STYLE_PROPERTY (priv->prop), source);
  if (priv->value == NULL)
    {
      g_object_unref (decl);
      return NULL;
    }

  return GTK_CSS_DECLARATION (decl);
}

guint
gtk_css_longhand_declaration_get_id (GtkCssLonghandDeclaration *decl)
{
  GtkCssLonghandDeclarationPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_LONGHAND_DECLARATION (decl), 0);

  priv = gtk_css_longhand_declaration_get_instance_private (decl);

  return _gtk_css_style_property_get_id (priv->prop);
}

GtkCssStyleProperty *
gtk_css_longhand_declaration_get_property (GtkCssLonghandDeclaration *decl)
{
  GtkCssLonghandDeclarationPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_LONGHAND_DECLARATION (decl), 0);

  priv = gtk_css_longhand_declaration_get_instance_private (decl);

  return priv->prop;
}

GtkCssValue *
gtk_css_longhand_declaration_get_value (GtkCssLonghandDeclaration *decl)
{
  GtkCssLonghandDeclarationPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_LONGHAND_DECLARATION (decl), NULL);

  priv = gtk_css_longhand_declaration_get_instance_private (decl);

  return priv->value;
}

