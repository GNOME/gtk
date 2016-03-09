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

#include "gtkcssstyledeclarationprivate.h"

#include "gtkcssselectorprivate.h"
#include "gtkcssstylesheetprivate.h"

typedef struct _GtkCssStyleDeclarationPrivate GtkCssStyleDeclarationPrivate;
struct _GtkCssStyleDeclarationPrivate {
  GPtrArray *declarations;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCssStyleDeclaration, gtk_css_style_declaration, G_TYPE_OBJECT)

static void
gtk_css_style_declaration_finalize (GObject *object)
{
  GtkCssStyleDeclaration *style_declaration = GTK_CSS_STYLE_DECLARATION (object);
  GtkCssStyleDeclarationPrivate *priv = gtk_css_style_declaration_get_instance_private (style_declaration);

  g_ptr_array_unref (priv->declarations);

  G_OBJECT_CLASS (gtk_css_style_declaration_parent_class)->finalize (object);
}

static void
gtk_css_style_declaration_class_init (GtkCssStyleDeclarationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_style_declaration_finalize;
}

static void
gtk_css_style_declaration_init (GtkCssStyleDeclaration *style_declaration)
{
  GtkCssStyleDeclarationPrivate *priv = gtk_css_style_declaration_get_instance_private (style_declaration);

  priv->declarations = g_ptr_array_new_with_free_func (g_object_unref);
}

GtkCssStyleDeclaration *
gtk_css_style_declaration_new (GtkCssRule *parent_rule)
{
  return g_object_new (GTK_TYPE_CSS_STYLE_DECLARATION, NULL);
}

