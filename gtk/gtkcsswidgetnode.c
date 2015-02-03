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
#include "gtkcssanimatedstyleprivate.h"
#include "gtkprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

/* When these change we do a full restyling. Otherwise we try to figure out
 * if we need to change things. */
#define GTK_CSS_RADICAL_CHANGE (GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_SOURCE)

G_DEFINE_TYPE (GtkCssWidgetNode, gtk_css_widget_node, GTK_TYPE_CSS_NODE)

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

static gboolean
gtk_css_style_needs_full_revalidate (GtkCssStyle  *style,
                                     GtkCssChange  change)
{
  /* Try to avoid invalidating if we can */
  if (change & GTK_CSS_RADICAL_CHANGE)
    return TRUE;

  if (GTK_IS_CSS_ANIMATED_STYLE (style))
    style = GTK_CSS_ANIMATED_STYLE (style)->style;

  if (gtk_css_static_style_get_change (GTK_CSS_STATIC_STYLE (style)) & change)
    return TRUE;
  else
    return FALSE;
}

static GtkBitmask *
gtk_css_widget_node_validate (GtkCssNode       *node,
                              gint64            timestamp,
                              GtkCssChange      change,
                              const GtkBitmask *parent_changes)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);
  GtkStyleContext *context;
  GtkBitmask *changes;
  GtkCssStyle *style;

  if (widget_node->widget == NULL)
    return _gtk_bitmask_new ();

  context = gtk_widget_get_style_context (widget_node->widget);
  style = gtk_css_node_get_style (node);
  if (style == NULL)
    style = gtk_css_static_style_get_default ();
  g_object_ref (style);

  /* Try to avoid invalidating if we can */
  if (gtk_css_style_needs_full_revalidate (style, change))
    {
      GtkCssStyle *new_style, *static_style;
      GtkCssNode *parent;

      parent = gtk_css_node_get_parent (node);

      static_style = gtk_css_node_create_style (node);
      new_style = gtk_css_animated_style_new (static_style,
                                              parent ? gtk_css_node_get_style (parent) : NULL,
                                              timestamp,
                                              gtk_css_node_get_style_provider (node),
                                              gtk_style_context_should_create_transitions (context, style) ? style : NULL);
      
      gtk_css_node_set_style (node, new_style);

      g_object_unref (static_style);
      g_object_unref (new_style);
    }
  else
    {
      if (!_gtk_bitmask_is_empty (parent_changes))
        {
          GtkCssStyle *new_values;

          if (GTK_IS_CSS_ANIMATED_STYLE (style))
            {
	      GtkCssStyle *new_base;
              
              new_base = gtk_css_node_update_style (node,
                                                    GTK_CSS_ANIMATED_STYLE (style)->style,
                                                    parent_changes);
              new_values = gtk_css_animated_style_new_advance (GTK_CSS_ANIMATED_STYLE (style),
                                                               new_base,
                                                               timestamp);
              g_object_unref (new_base);
            }
          else
            {
	      new_values = gtk_css_node_update_style (node,
                                                      style,
                                                      parent_changes);
            }

          gtk_css_node_set_style (node, new_values);
          g_object_unref (new_values);
        }
      else if ((change & GTK_CSS_CHANGE_ANIMATE) &&
               GTK_IS_CSS_ANIMATED_STYLE (style))
        {
          GtkCssStyle *new_values;

          new_values = gtk_css_animated_style_new_advance (GTK_CSS_ANIMATED_STYLE (style),
                                                           GTK_CSS_ANIMATED_STYLE (style)->style,
                                                           timestamp);
          gtk_css_node_set_style (node, new_values);
          g_object_unref (new_values);
        }
    }

  changes = gtk_css_style_get_difference (gtk_css_node_get_style (node), style);

  g_object_unref (style);

  gtk_style_context_validate (context, changes);

  return changes;
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

static GtkStyleProviderPrivate *
gtk_css_widget_node_get_style_provider (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  if (widget_node->widget == NULL)
    return GTK_CSS_NODE_CLASS (gtk_css_widget_node_parent_class)->get_style_provider (node);

  return gtk_style_context_get_style_provider (gtk_widget_get_style_context (widget_node->widget));
}

static void
gtk_css_widget_node_class_init (GtkCssWidgetNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);

  node_class->validate = gtk_css_widget_node_validate;
  node_class->set_invalid = gtk_css_widget_node_set_invalid;
  node_class->create_widget_path = gtk_css_widget_node_create_widget_path;
  node_class->get_widget_path = gtk_css_widget_node_get_widget_path;
  node_class->get_style_provider = gtk_css_widget_node_get_style_provider;
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

