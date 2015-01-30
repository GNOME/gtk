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

#include "gtkcsswidgetnodeprivate.h"

#include "gtkcontainerprivate.h"
#include "gtkprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

G_DEFINE_TYPE (GtkCssWidgetNode, gtk_css_widget_node, GTK_TYPE_CSS_NODE)

static void
gtk_css_widget_node_invalidate (GtkCssNode   *node,
                                GtkCssChange  change)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  widget_node->pending_changes |= change;
}

static void
gtk_css_widget_node_set_invalid (GtkCssNode *node,
                                 gboolean    invalid)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  GTK_CSS_NODE_CLASS (gtk_css_widget_node_parent_class)->set_invalid (node, invalid);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (invalid && 
      gtk_css_node_get_parent (node) == NULL &&
      GTK_IS_RESIZE_CONTAINER (widget_node->widget))
    {
      _gtk_container_queue_restyle (GTK_CONTAINER (widget_node->widget));
    }
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static GtkBitmask *
gtk_css_widget_node_validate (GtkCssNode       *node,
                              gint64            timestamp,
                              GtkCssChange      change,
                              const GtkBitmask *parent_changes)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  change |= widget_node->pending_changes;
  widget_node->pending_changes = 0;

  if (widget_node->widget == NULL)
    return _gtk_bitmask_new ();

  return _gtk_style_context_validate (gtk_widget_get_style_context (widget_node->widget),
                                      node,
                                      timestamp,
                                      change,
                                      parent_changes);
}

static GtkWidgetPath *
gtk_css_widget_node_create_widget_path (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);
  GtkWidgetPath *path;
  guint length;

  if (widget_node->widget == NULL)
    path = gtk_widget_path_new ();
  else
    path = _gtk_widget_create_path (widget_node->widget);
  
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
gtk_css_widget_node_get_widget_path (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  if (widget_node->widget == NULL)
    return NULL;

  return gtk_widget_get_path (widget_node->widget);
}

static void
gtk_css_widget_node_class_init (GtkCssWidgetNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);

  node_class->invalidate = gtk_css_widget_node_invalidate;
  node_class->validate = gtk_css_widget_node_validate;
  node_class->set_invalid = gtk_css_widget_node_set_invalid;
  node_class->create_widget_path = gtk_css_widget_node_create_widget_path;
  node_class->get_widget_path = gtk_css_widget_node_get_widget_path;
}

static void
gtk_css_widget_node_init (GtkCssWidgetNode *cssnode)
{
}

GtkCssNode *
gtk_css_widget_node_new (GtkWidget *widget)
{
  GtkCssWidgetNode *result;

  gtk_internal_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  result = g_object_new (GTK_TYPE_CSS_WIDGET_NODE, NULL);
  result->widget = widget;

  return GTK_CSS_NODE (result);
}

void
gtk_css_widget_node_widget_destroyed (GtkCssWidgetNode *node)
{
  gtk_internal_return_if_fail (GTK_IS_CSS_WIDGET_NODE (node));
  gtk_internal_return_if_fail (node->widget != NULL);

  node->widget = NULL;
  /* Contents of this node are now undefined.
   * So we don't clear the style or anything.
   */
}

GtkWidget *
gtk_css_widget_node_get_widget (GtkCssWidgetNode *node)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_WIDGET_NODE (node), NULL);

  return node->widget;
}

