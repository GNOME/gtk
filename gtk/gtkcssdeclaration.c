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

typedef struct _GtkCssDeclarationPrivate GtkCssDeclarationPrivate;
struct _GtkCssDeclarationPrivate {
  GtkCssStyleDeclaration *style;
  char *name;
  char *value;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssDeclaration, gtk_css_declaration, G_TYPE_OBJECT)

static void
gtk_css_declaration_finalize (GObject *object)
{
  GtkCssDeclaration *declaration = GTK_CSS_DECLARATION (object);
  GtkCssDeclarationPrivate *priv = gtk_css_declaration_get_instance_private (declaration);

  g_free (priv->name);
  g_free (priv->value);

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
gtk_css_declaration_new (GtkCssStyleDeclaration *style,
                         const char             *name,
                         const char             *value)
{
  GtkCssDeclarationPrivate *priv;
  GtkCssDeclaration *result;

  result = g_object_new (GTK_TYPE_CSS_DECLARATION, NULL);
  priv = gtk_css_declaration_get_instance_private (result);

  priv->style = style;
  priv->name = g_strdup (name);
  priv->value = g_strdup (value);

  return result;
}

GtkCssDeclaration *
gtk_css_declaration_new_parse (GtkCssStyleDeclaration *style,
                               GtkCssTokenSource      *source)
{
  const GtkCssToken *token;
  GtkCssDeclaration *decl;
  char *name, *value;

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
  name = g_strdup (token->string.string);
  gtk_css_token_source_consume_token (source);
  gtk_css_token_source_consume_whitespace (source);
  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_COLON))
    {
      g_free (name);
      gtk_css_token_source_consume_all (source);
      return NULL;
    }

  value = gtk_css_token_source_consume_to_string (source);

  decl = gtk_css_declaration_new (style, name, value);
  g_free (name);
  g_free (value);

  return decl;
}
