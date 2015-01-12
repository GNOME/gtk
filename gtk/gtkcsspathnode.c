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

#include "gtkcsspathnodeprivate.h"
#include "gtkprivate.h"

G_DEFINE_TYPE (GtkCssPathNode, gtk_css_path_node, GTK_TYPE_CSS_NODE)

static GtkWidgetPath *
gtk_css_path_node_real_create_widget_path (GtkCssNode *node)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);

  if (path_node->path == NULL)
    return gtk_widget_path_new ();

  return gtk_widget_path_copy (path_node->path);
}

static const GtkWidgetPath *
gtk_css_path_node_real_get_widget_path (GtkCssNode *node)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);

  return path_node->path;
}

static void
gtk_css_path_node_class_init (GtkCssPathNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);

  node_class->create_widget_path = gtk_css_path_node_real_create_widget_path;
  node_class->get_widget_path = gtk_css_path_node_real_get_widget_path;
}

static void
gtk_css_path_node_init (GtkCssPathNode *cssnode)
{
}

GtkCssNode *
gtk_css_path_node_new (void)
{
  return g_object_new (GTK_TYPE_CSS_PATH_NODE, NULL);
}

void
gtk_css_path_node_set_widget_path (GtkCssPathNode *node,
                                   GtkWidgetPath  *path)
{
  gtk_internal_return_if_fail (GTK_IS_CSS_PATH_NODE (node));

  if (node->path == path)
    return;

  if (node->path)
    gtk_widget_path_unref (node->path);

  if (path)
    gtk_widget_path_ref (path);

  node->path = path;
}

GtkWidgetPath *
gtk_css_path_node_get_widget_path (GtkCssPathNode *node)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_PATH_NODE (node), NULL);

  return node->path;
}

