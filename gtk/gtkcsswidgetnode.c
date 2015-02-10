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
/* widgets for special casing go here */
#include "gtkbox.h"

/* When these change we do a full restyling. Otherwise we try to figure out
 * if we need to change things. */
#define GTK_CSS_RADICAL_CHANGE (GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_SOURCE)

G_DEFINE_TYPE (GtkCssWidgetNode, gtk_css_widget_node, GTK_TYPE_CSS_NODE)

static GtkCssStyle *
gtk_css_widget_node_update_style (GtkCssNode   *cssnode,
                                  GtkCssChange  pending_change,
                                  GtkCssStyle  *old_style)
{
  return NULL;
}

static void
gtk_css_widget_node_queue_validate (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (GTK_IS_RESIZE_CONTAINER (widget_node->widget))
    widget_node->validate_cb_id = gtk_widget_add_tick_callback (widget_node->widget,
                                                                (GtkTickCallback) _gtk_container_queue_restyle,
                                                                NULL,
                                                                NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_css_widget_node_dequeue_validate (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (GTK_IS_RESIZE_CONTAINER (widget_node->widget))
    gtk_widget_remove_tick_callback (widget_node->widget,
                                     widget_node->validate_cb_id);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static gboolean
gtk_css_static_style_needs_revalidate (GtkCssStaticStyle  *style,
                                       GtkCssChange        change)
{
  /* Try to avoid invalidating if we can */
  if (change & GTK_CSS_RADICAL_CHANGE)
    return TRUE;

  if (gtk_css_static_style_get_change (style) & change)
    return TRUE;
  else
    return FALSE;
}

static GtkCssStyle *
validate_static_style (GtkCssNode       *node,
                       GtkCssStyle      *style,
                       GtkCssChange      change,
                       const GtkBitmask *parent_changes)
{
  if (gtk_css_static_style_needs_revalidate (GTK_CSS_STATIC_STYLE (style), change))
    {
      return gtk_css_node_create_style (node);
    }
  else if (!_gtk_bitmask_is_empty (parent_changes))
    {
      return gtk_css_node_update_style (node,
                                        style,
                                        parent_changes);
    }
  else
    {
      return g_object_ref (style);
    }
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
  GtkCssStyle *style, *static_style;
  GtkCssStyle *new_style, *new_static_style;

  if (widget_node->widget == NULL)
    return _gtk_bitmask_new ();

  context = gtk_widget_get_style_context (widget_node->widget);
  style = gtk_css_node_get_style (node);
  if (style == NULL)
    style = gtk_css_static_style_get_default ();

  if (GTK_IS_CSS_ANIMATED_STYLE (style))
    {
      static_style = GTK_CSS_ANIMATED_STYLE (style)->style;
    }
  else
    {
      static_style = style;
    }

  new_static_style = validate_static_style (node, static_style, change, parent_changes);

  if (new_static_style != static_style)
    {
      GtkCssNode *parent = gtk_css_node_get_parent (node);
      new_style = gtk_css_animated_style_new (new_static_style,
                                              parent ? gtk_css_node_get_style (parent) : NULL,
                                              timestamp,
                                              gtk_css_node_get_style_provider (node),
                                              gtk_style_context_should_create_transitions (context, style) ? style : NULL);
      
      g_object_unref (new_static_style);
    }
  else if (GTK_IS_CSS_ANIMATED_STYLE (style))
    {
      new_style = gtk_css_animated_style_new_advance (GTK_CSS_ANIMATED_STYLE (style),
                                                      static_style,
                                                      timestamp);
    }
  else
    {
      new_style = g_object_ref (style);
    }

  changes = gtk_css_style_get_difference (new_style, style);

  if (GTK_IS_CSS_ANIMATED_STYLE (new_style) &&
      !gtk_css_animated_style_is_static (GTK_CSS_ANIMATED_STYLE (new_style)))
    gtk_css_node_set_invalid (node, TRUE);

  gtk_css_node_set_style (node, new_style);
  g_object_unref (new_style);

  gtk_style_context_validate (context, changes);

  return changes;
}

typedef GtkWidgetPath * (* GetPathForChildFunc) (GtkContainer *, GtkWidget *);

static gboolean
widget_needs_widget_path (GtkWidget *widget)
{
  static GetPathForChildFunc funcs[2];
  GtkWidget *parent;
  GetPathForChildFunc parent_func;
  guint i;

  if (G_UNLIKELY (funcs[0] == NULL))
    {
      i = 0;
      funcs[i++] = GTK_CONTAINER_CLASS (g_type_class_ref (GTK_TYPE_CONTAINER))->get_path_for_child;
      funcs[i++] = GTK_CONTAINER_CLASS (g_type_class_ref (GTK_TYPE_BOX))->get_path_for_child;

      g_assert (i == G_N_ELEMENTS (funcs));
    }

  parent = gtk_widget_get_parent (widget);
  if (parent == NULL)
    return FALSE;

  parent_func = GTK_CONTAINER_GET_CLASS (GTK_CONTAINER (parent))->get_path_for_child;
  for (i = 0; i < G_N_ELEMENTS (funcs); i++)
    {
      if (funcs[i] == parent_func)
        return FALSE;
    }

  return TRUE;
}

gboolean
gtk_css_widget_node_init_matcher (GtkCssNode     *node,
                                  GtkCssMatcher  *matcher)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  if (widget_node->widget == NULL)
    return FALSE;

  if (!widget_needs_widget_path (widget_node->widget))
    return GTK_CSS_NODE_CLASS (gtk_css_widget_node_parent_class)->init_matcher (node, matcher);

  return _gtk_css_matcher_init (matcher,
                                gtk_widget_get_path (widget_node->widget),
                                gtk_css_node_get_declaration (node));
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

  node_class->update_style = gtk_css_widget_node_update_style;
  node_class->validate = gtk_css_widget_node_validate;
  node_class->queue_validate = gtk_css_widget_node_queue_validate;
  node_class->dequeue_validate = gtk_css_widget_node_dequeue_validate;
  node_class->init_matcher = gtk_css_widget_node_init_matcher;
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
  gtk_css_node_set_visible (GTK_CSS_NODE (result),
                            gtk_widget_get_visible (widget));

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

