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
#include "gtkstylepropertyprivate.h"

typedef struct _GtkCssDeclarationPrivate GtkCssDeclarationPrivate;
struct _GtkCssDeclarationPrivate {
  GtkCssStyleDeclaration *style;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkCssDeclaration, gtk_css_declaration, G_TYPE_OBJECT)

static void
gtk_css_declaration_class_init (GtkCssDeclarationClass *klass)
{
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
  else
    {
      gtk_css_token_source_error (source, "Property name does not define a valid property");
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
}

GtkCssStyleDeclaration *
gtk_css_declaration_get_style (GtkCssDeclaration *decl)
{
  GtkCssDeclarationPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_DECLARATION (decl), NULL);

  priv = gtk_css_declaration_get_instance_private (decl);

  return priv->style;
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
