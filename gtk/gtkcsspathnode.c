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
#include "gtkstylecontextprivate.h"

G_DEFINE_TYPE (GtkCssPathNode, gtk_css_path_node, GTK_TYPE_CSS_NODE)

static void
gtk_css_path_node_invalidate (GtkCssNode *node)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);

  if (path_node->context)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_style_context_invalidate (path_node->context);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static void
gtk_css_path_node_set_invalid (GtkCssNode *node,
                               gboolean    invalid)
{
  /* path nodes are always valid */
}

static GtkWidgetPath *
gtk_css_path_node_real_create_widget_path (GtkCssNode *node)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);
  GtkWidgetPath *path;
  guint length;

  if (path_node->path == NULL)
    path = gtk_widget_path_new ();
  else
    path = gtk_widget_path_copy (path_node->path);

  length = gtk_widget_path_length (path);
  if (length > 0)
    {
      gtk_css_node_declaration_add_to_widget_path (gtk_css_node_get_declaration (node),
                                                   path,
                                                   length - 1);
    }

  return path;
}

static const GtkWidgetPath *
gtk_css_path_node_real_get_widget_path (GtkCssNode *node)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);

  return path_node->path;
}

static GtkStyleProviderPrivate *
gtk_css_path_node_get_style_provider (GtkCssNode *node)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);

  if (path_node->context == NULL)
    return GTK_CSS_NODE_CLASS (gtk_css_path_node_parent_class)->get_style_provider (node);

  return gtk_style_context_get_style_provider (path_node->context);
}

static void
gtk_css_path_node_class_init (GtkCssPathNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);

  node_class->invalidate = gtk_css_path_node_invalidate;
  node_class->set_invalid = gtk_css_path_node_set_invalid;
  node_class->create_widget_path = gtk_css_path_node_real_create_widget_path;
  node_class->get_widget_path = gtk_css_path_node_real_get_widget_path;
  node_class->get_style_provider = gtk_css_path_node_get_style_provider;
}

static void
gtk_css_path_node_init (GtkCssPathNode *cssnode)
{
}

GtkCssNode *
gtk_css_path_node_new (GtkStyleContext *context)
{
  GtkCssPathNode *node;
  
  g_return_val_if_fail (context == NULL || GTK_IS_STYLE_CONTEXT (context), NULL);

  node = g_object_new (GTK_TYPE_CSS_PATH_NODE, NULL);
  node->context = context;

  return GTK_CSS_NODE (node);
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

  gtk_css_node_invalidate (GTK_CSS_NODE (node), GTK_CSS_CHANGE_ANY);
}

GtkWidgetPath *
gtk_css_path_node_get_widget_path (GtkCssPathNode *node)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_PATH_NODE (node), NULL);

  return node->path;
}

