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
#include "gtkcssstylepropertyprivate.h"
#include "gtkprivate.h"
#include "gtkstylecontextprivate.h"

G_DEFINE_TYPE (GtkCssPathNode, gtk_css_path_node, GTK_TYPE_CSS_NODE)

static void
gtk_css_path_node_finalize (GObject *object)
{
  GtkCssPathNode *node = GTK_CSS_PATH_NODE (object);

  if (node->path)
    gtk_widget_path_unref (node->path);

  G_OBJECT_CLASS (gtk_css_path_node_parent_class)->finalize (object);
}

static void
gtk_css_path_node_invalidate (GtkCssNode *node)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);

  if (path_node->context)
    gtk_style_context_validate (path_node->context, NULL);
}

static gboolean
gtk_css_path_node_real_init_matcher (GtkCssNode     *node,
                                     GtkCssMatcher  *matcher)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);

  if (path_node->path == NULL ||
      gtk_widget_path_length (path_node->path) == 0)
    return FALSE;

  return _gtk_css_matcher_init (matcher,
                                path_node->path,
                                gtk_css_node_get_declaration (node));
}

static GtkCssStyle *
gtk_css_path_node_update_style (GtkCssNode   *cssnode,
                                GtkCssChange  change,
                                gint64        timestamp,
                                GtkCssStyle  *style)
{
  /* This should get rid of animations */
  return GTK_CSS_NODE_CLASS (gtk_css_path_node_parent_class)->update_style (cssnode, change, 0, style);
}

static GtkStyleProvider *
gtk_css_path_node_get_style_provider (GtkCssNode *node)
{
  GtkCssPathNode *path_node = GTK_CSS_PATH_NODE (node);

  if (path_node->context == NULL)
    return NULL;

  return gtk_style_context_get_style_provider (path_node->context);
}

static void
gtk_css_path_node_class_init (GtkCssPathNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_path_node_finalize;

  node_class->invalidate = gtk_css_path_node_invalidate;
  node_class->update_style = gtk_css_path_node_update_style;
  node_class->init_matcher = gtk_css_path_node_real_init_matcher;
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
gtk_css_path_node_unset_context (GtkCssPathNode *node)
{
  gtk_internal_return_if_fail (GTK_IS_CSS_PATH_NODE (node));
  gtk_internal_return_if_fail (node->context != NULL);

  node->context = NULL;

  gtk_css_node_invalidate_style_provider (GTK_CSS_NODE (node));
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

