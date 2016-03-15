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

#include "gtkstylepropertyprivate.h"

typedef struct _GtkCssDeclarationPrivate GtkCssDeclarationPrivate;
struct _GtkCssDeclarationPrivate {
  GtkCssStyleDeclaration *style;
  GtkStyleProperty *prop;
  GtkCssValue *value;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssDeclaration, gtk_css_declaration, G_TYPE_OBJECT)

static void
gtk_css_declaration_finalize (GObject *object)
{
  GtkCssDeclaration *declaration = GTK_CSS_DECLARATION (object);
  GtkCssDeclarationPrivate *priv = gtk_css_declaration_get_instance_private (declaration);

  if (priv->value)
    _gtk_css_value_unref (priv->value);

  G_OBJECT_CLASS (gtk_css_declaration_parent_class)->finalize (object);
}

static void
gtk_css_declaration_class_init (GtkCssDeclarationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_declaration_finalize;
}

static void
gtk_css_declaration_init (GtkCssDeclaration *declaration)
{
}

GtkCssDeclaration *
gtk_css_declaration_new_parse (GtkCssStyleDeclaration *style,
                               GtkCssTokenSource      *source)
{
  GtkCssDeclarationPrivate *priv;
  const GtkCssToken *token;
  GtkCssDeclaration *decl;
  char *name;

  decl = g_object_new (GTK_TYPE_CSS_DECLARATION, NULL);
  priv = gtk_css_declaration_get_instance_private (decl);

  gtk_css_token_source_set_consumer (source, G_OBJECT (decl));

  gtk_css_token_source_consume_whitespace (source);
  priv->style = style;
  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_token_source_error (source, "Expected a property name");
      gtk_css_token_source_consume_all (source);
      g_object_unref (decl);
      return NULL;
    }
  name = g_utf8_strdown (token->string.string, -1);
  priv->prop = _gtk_style_property_lookup (name);
  if (priv->prop == NULL)
    {
      gtk_css_token_source_unknown (source, "Unknown property name '%s'", token->string.string);
      gtk_css_token_source_consume_all (source);
      g_object_unref (decl);
      g_free (name);
      return NULL;
    }
  else if (!g_str_equal (name, _gtk_style_property_get_name (priv->prop)))
    gtk_css_token_source_deprecated (source,
                                     "The '%s' property has been renamed to '%s'",
                                     name, _gtk_style_property_get_name (priv->prop));
  gtk_css_token_source_consume_token (source);
  gtk_css_token_source_consume_whitespace (source);
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
   
  priv->value = gtk_style_property_token_parse (priv->prop, source);
  if (priv->value == NULL)
    {
      g_object_unref (decl);
      return NULL;
    }

  return decl;
}
