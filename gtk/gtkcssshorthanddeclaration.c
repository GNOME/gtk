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

#include "gtkcssshorthanddeclarationprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"

typedef struct _GtkCssShorthandDeclarationPrivate GtkCssShorthandDeclarationPrivate;
struct _GtkCssShorthandDeclarationPrivate {
  GtkCssShorthandProperty *prop;
  GtkCssValue **values;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssShorthandDeclaration, gtk_css_shorthand_declaration, GTK_TYPE_CSS_DECLARATION)

static void
gtk_css_shorthand_declaration_finalize (GObject *object)
{
  GtkCssShorthandDeclaration *shorthand = GTK_CSS_SHORTHAND_DECLARATION (object);
  GtkCssShorthandDeclarationPrivate *priv = gtk_css_shorthand_declaration_get_instance_private (shorthand);
  guint i;

  if (priv->values)
    {
      for (i = 0; i < gtk_css_shorthand_declaration_get_length (shorthand); i++)
        {
          _gtk_css_value_unref (priv->values[i]);
        }
      g_free (priv->values);
    }

  G_OBJECT_CLASS (gtk_css_shorthand_declaration_parent_class)->finalize (object);
}

static const char *
gtk_css_shorthand_declaration_get_name (GtkCssDeclaration *decl)
{
  GtkCssShorthandDeclaration *shorthand_declaration = GTK_CSS_SHORTHAND_DECLARATION (decl);
  GtkCssShorthandDeclarationPrivate *priv = gtk_css_shorthand_declaration_get_instance_private (shorthand_declaration);

  return _gtk_style_property_get_name (GTK_STYLE_PROPERTY (priv->prop));
}

static void
gtk_css_shorthand_declaration_print_value (GtkCssDeclaration *decl,
                                           GString           *string)
{
  GtkCssShorthandDeclaration *shorthand = GTK_CSS_SHORTHAND_DECLARATION (decl);
  GtkCssShorthandDeclarationPrivate *priv = gtk_css_shorthand_declaration_get_instance_private (shorthand);
  guint i;

  /* XXX */
  for (i = 0; i < gtk_css_shorthand_declaration_get_length (shorthand); i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      _gtk_css_value_print (priv->values[i], string);
    }
}

static void
gtk_css_shorthand_declaration_class_init (GtkCssShorthandDeclarationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssDeclarationClass *decl_class = GTK_CSS_DECLARATION_CLASS (klass);

  object_class->finalize = gtk_css_shorthand_declaration_finalize;

  decl_class->get_name = gtk_css_shorthand_declaration_get_name;
  decl_class->print_value = gtk_css_shorthand_declaration_print_value;
}

static void
gtk_css_shorthand_declaration_init (GtkCssShorthandDeclaration *shorthand_declaration)
{
}

GtkCssDeclaration *
gtk_css_shorthand_declaration_new_parse (GtkCssStyleDeclaration *style,
                                         GtkCssTokenSource      *source)
{
  GtkCssShorthandDeclarationPrivate *priv;
  const GtkCssToken *token;
  GtkCssShorthandDeclaration *decl;
  GtkCssValue *array;
  guint i;
  char *name;

  decl = g_object_new (GTK_TYPE_CSS_SHORTHAND_DECLARATION, NULL);
  priv = gtk_css_shorthand_declaration_get_instance_private (decl);

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
  priv->prop = (GtkCssShorthandProperty *) _gtk_style_property_lookup (name);
  if (!GTK_IS_CSS_SHORTHAND_PROPERTY (priv->prop))
    {
      gtk_css_token_source_unknown (source, "Property name '%s' is not a shorthand property", token->string.string);
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
   
  array = gtk_style_property_token_parse (GTK_STYLE_PROPERTY (priv->prop), source);
  if (array == NULL)
    {
      g_object_unref (decl);
      return NULL;
    }
  priv->values = g_new (GtkCssValue *, gtk_css_shorthand_declaration_get_length (decl));
  for (i = 0; i < gtk_css_shorthand_declaration_get_length (decl); i++)
    {
      priv->values[i] = _gtk_css_value_ref (_gtk_css_array_value_get_nth (array, i));
    }
  _gtk_css_value_unref (array);

  return GTK_CSS_DECLARATION (decl);
}

guint
gtk_css_shorthand_declaration_get_length (GtkCssShorthandDeclaration *decl)
{
  GtkCssShorthandDeclarationPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_DECLARATION (decl), 0);

  priv = gtk_css_shorthand_declaration_get_instance_private (decl);

  return _gtk_css_shorthand_property_get_n_subproperties (priv->prop);
}

GtkCssStyleProperty *
gtk_css_shorthand_declaration_get_subproperty (GtkCssShorthandDeclaration *decl,
                                               guint                       id)
{
  GtkCssShorthandDeclarationPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_DECLARATION (decl), NULL);
  g_return_val_if_fail (id < gtk_css_shorthand_declaration_get_length (decl), NULL);

  priv = gtk_css_shorthand_declaration_get_instance_private (decl);

  return _gtk_css_shorthand_property_get_subproperty (priv->prop, id);
}

GtkCssValue *
gtk_css_shorthand_declaration_get_value (GtkCssShorthandDeclaration *decl,
                                         guint                       id)
{
  GtkCssShorthandDeclarationPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_DECLARATION (decl), NULL);
  g_return_val_if_fail (id < gtk_css_shorthand_declaration_get_length (decl), NULL);

  priv = gtk_css_shorthand_declaration_get_instance_private (decl);

  return priv->values[id];
}

