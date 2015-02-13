/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcsstransientnodeprivate.h"
#include "gtkprivate.h"

G_DEFINE_TYPE (GtkCssTransientNode, gtk_css_transient_node, GTK_TYPE_CSS_NODE)

static GtkWidgetPath *
gtk_css_transient_node_create_widget_path (GtkCssNode *node)
{
  GtkWidgetPath *result;
  GtkCssNode *parent;

  parent = gtk_css_node_get_parent (node);
  if (parent == NULL)
    result = gtk_widget_path_new ();
  else
    result = gtk_css_node_create_widget_path (parent);

  gtk_widget_path_append_type (result, gtk_css_node_get_widget_type (node));
  gtk_css_node_declaration_add_to_widget_path (gtk_css_node_get_declaration (node), result, -1);
  
  return result;
}

static const GtkWidgetPath *
gtk_css_transient_node_get_widget_path (GtkCssNode *node)
{
  GtkCssNode *parent;

  parent = gtk_css_node_get_parent (node);
  if (parent == NULL)
    return NULL;

  return gtk_css_node_get_widget_path (parent);
}

static void
gtk_css_transient_node_class_init (GtkCssTransientNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);

  node_class->create_widget_path = gtk_css_transient_node_create_widget_path;
  node_class->get_widget_path = gtk_css_transient_node_get_widget_path;
}

static void
gtk_css_transient_node_init (GtkCssTransientNode *cssnode)
{
  gtk_css_node_set_visible (GTK_CSS_NODE (cssnode), FALSE);
}

GtkCssNode *
gtk_css_transient_node_new (GtkCssNode *parent)
{
  GtkCssNode *result;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_NODE (parent), NULL);

  result = g_object_new (GTK_TYPE_CSS_TRANSIENT_NODE, NULL);
  gtk_css_node_declaration_unref (result->decl);
  result->decl = gtk_css_node_declaration_ref (parent->decl);

  return result;
}

