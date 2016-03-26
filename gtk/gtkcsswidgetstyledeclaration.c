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

#include "gtkcsswidgetstyledeclarationprivate.h"

#include "gtkwidget.h"

#include <string.h>

typedef struct _GtkCssWidgetStyleDeclarationPrivate GtkCssWidgetStyleDeclarationPrivate;
struct _GtkCssWidgetStyleDeclarationPrivate {
  char *name;
  char *value;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssWidgetStyleDeclaration, gtk_css_widget_style_declaration, GTK_TYPE_CSS_DECLARATION)

static void
gtk_css_widget_style_declaration_finalize (GObject *object)
{
  GtkCssWidgetStyleDeclaration *widget_style = GTK_CSS_WIDGET_STYLE_DECLARATION (object);
  GtkCssWidgetStyleDeclarationPrivate *priv = gtk_css_widget_style_declaration_get_instance_private (widget_style);

  g_free (priv->name);
  g_free (priv->value);

  G_OBJECT_CLASS (gtk_css_widget_style_declaration_parent_class)->finalize (object);
}

static const char *
gtk_css_widget_style_declaration_get_name (GtkCssDeclaration *decl)
{
  GtkCssWidgetStyleDeclaration *widget_style_declaration = GTK_CSS_WIDGET_STYLE_DECLARATION (decl);
  GtkCssWidgetStyleDeclarationPrivate *priv = gtk_css_widget_style_declaration_get_instance_private (widget_style_declaration);

  return priv->name;
}

static void
gtk_css_widget_style_declaration_print_value (GtkCssDeclaration *decl,
                                              GString           *string)
{
  GtkCssWidgetStyleDeclaration *widget_style = GTK_CSS_WIDGET_STYLE_DECLARATION (decl);
  GtkCssWidgetStyleDeclarationPrivate *priv = gtk_css_widget_style_declaration_get_instance_private (widget_style);

  g_string_append (string, priv->value);
}

static void
gtk_css_widget_style_declaration_class_init (GtkCssWidgetStyleDeclarationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssDeclarationClass *decl_class = GTK_CSS_DECLARATION_CLASS (klass);

  object_class->finalize = gtk_css_widget_style_declaration_finalize;

  decl_class->get_name = gtk_css_widget_style_declaration_get_name;
  decl_class->print_value = gtk_css_widget_style_declaration_print_value;
}

static void
gtk_css_widget_style_declaration_init (GtkCssWidgetStyleDeclaration *widget_style_declaration)
{
}

gboolean
gtk_css_widget_style_declaration_accepts_name (const char *name)
{
  g_return_val_if_fail (name != NULL, FALSE);

  if (name[0] != '-')
    return FALSE;

  if (g_str_has_prefix (name, "-gtk-"))
    return FALSE;

  return TRUE;
}

static void
warn_if_deprecated (GtkCssTokenSource *source,
                    const gchar       *name)
{
  gchar *n = NULL;
  gchar *p;
  const gchar *type_name;
  const gchar *property_name;
  GType type;
  GTypeClass *class = NULL;
  GParamSpec *pspec;

  n = g_strdup (name);

  /* skip initial - */
  type_name = n + 1;

  p = strchr (type_name, '-');
  if (!p)
    goto out;

  p[0] = '\0';
  property_name = p + 1;

  type = g_type_from_name (type_name);
  if (type == G_TYPE_INVALID ||
      !g_type_is_a (type, GTK_TYPE_WIDGET))
    goto out;

  class = g_type_class_ref (type);
  pspec = gtk_widget_class_find_style_property (GTK_WIDGET_CLASS (class), property_name);
  if (!pspec)
    goto out;

  if (!(pspec->flags & G_PARAM_DEPRECATED))
    goto out;

  gtk_css_token_source_deprecated (source,
                                   "The style property %s:%s is deprecated and shouldn't be "
                                   "used anymore. It will be removed in a future version",
                                   g_type_name (pspec->owner_type), pspec->name);

out:
  g_free (n);
  if (class)
    g_type_class_unref (class);
}

GtkCssDeclaration *
gtk_css_widget_style_declaration_new_parse (GtkCssStyleDeclaration *style,
                                            GtkCssTokenSource      *source)
{
  GtkCssWidgetStyleDeclarationPrivate *priv;
  const GtkCssToken *token;
  GtkCssWidgetStyleDeclaration *decl;

  decl = g_object_new (GTK_TYPE_CSS_WIDGET_STYLE_DECLARATION,
                       "parent-style", style,
                       NULL);
  priv = gtk_css_widget_style_declaration_get_instance_private (decl);

  gtk_css_token_source_set_consumer (source, G_OBJECT (decl));

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_token_source_error (source, "Expected a property name");
      gtk_css_token_source_consume_all (source);
      g_object_unref (decl);
      return NULL;
    }
  if (!gtk_css_widget_style_declaration_accepts_name (token->string.string))
    {
      gtk_css_token_source_unknown (source, "Property name '%s' is not valid for a widget style property", token->string.string);
      gtk_css_token_source_consume_all (source);
      g_object_unref (decl);
      return NULL;
    }
  
  priv->name = g_strdup (token->string.string);
  warn_if_deprecated (source, priv->name);
  gtk_css_token_source_consume_token (source);

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_COLON))
    {
      gtk_css_token_source_error (source, "No colon following property name");
      gtk_css_token_source_consume_all (source);
      g_object_unref (decl);
      return NULL;
    }
  gtk_css_token_source_consume_token (source);
   
  /* skip whitespace */
  gtk_css_token_source_get_token (source);
  priv->value = gtk_css_token_source_consume_to_string (source);

  return GTK_CSS_DECLARATION (decl);
}

