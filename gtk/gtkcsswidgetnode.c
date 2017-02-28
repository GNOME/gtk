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
#include "gtksettingsprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

G_DEFINE_TYPE (GtkCssWidgetNode, gtk_css_widget_node, GTK_TYPE_CSS_NODE)

static void
gtk_css_widget_node_finalize (GObject *object)
{
  GtkCssWidgetNode *node = GTK_CSS_WIDGET_NODE (object);

  g_object_unref (node->last_updated_style);

  G_OBJECT_CLASS (gtk_css_widget_node_parent_class)->finalize (object);
}

static void
gtk_css_widget_node_style_changed (GtkCssNode        *cssnode,
                                   GtkCssStyleChange *change)
{
  GtkCssWidgetNode *node;

  node = GTK_CSS_WIDGET_NODE (cssnode);

  if (node->widget)
    gtk_widget_clear_path (node->widget);

  GTK_CSS_NODE_CLASS (gtk_css_widget_node_parent_class)->style_changed (cssnode, change);
}

static gboolean
gtk_css_widget_node_queue_callback (GtkWidget     *widget,
                                    GdkFrameClock *frame_clock,
                                    gpointer       user_data)
{
  GtkCssNode *node = user_data;

  gtk_css_node_invalidate_frame_clock (node, TRUE);
  _gtk_container_queue_restyle (GTK_CONTAINER (widget));

  return G_SOURCE_CONTINUE;
}

static GtkCssStyle *
gtk_css_widget_node_update_style (GtkCssNode   *cssnode,
                                  GtkCssChange  change,
                                  gint64        timestamp,
                                  GtkCssStyle  *style)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (cssnode);

  if (widget_node->widget != NULL)
    {
      GtkStyleContext *context = _gtk_widget_peek_style_context (widget_node->widget);
      if (context)
        gtk_style_context_clear_property_cache (context);
    }

  return GTK_CSS_NODE_CLASS (gtk_css_widget_node_parent_class)->update_style (cssnode, change, timestamp, style);
}

static void
gtk_css_widget_node_queue_validate (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  if (widget_node->widget && _gtk_widget_is_toplevel (widget_node->widget) &&
      GTK_IS_CONTAINER (widget_node->widget))
    widget_node->validate_cb_id = gtk_widget_add_tick_callback (widget_node->widget,
                                                                gtk_css_widget_node_queue_callback,
                                                                node,
                                                                NULL);
}

static void
gtk_css_widget_node_dequeue_validate (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  if (widget_node->widget && _gtk_widget_is_toplevel (widget_node->widget) &&
      GTK_IS_CONTAINER (widget_node->widget))
    gtk_widget_remove_tick_callback (widget_node->widget,
                                     widget_node->validate_cb_id);
}

static void
gtk_css_widget_node_validate (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);
  GtkCssStyleChange change;
  GtkCssStyle *style;

  if (widget_node->widget == NULL)
    return;

  style = gtk_css_node_get_style (node);

  gtk_css_style_change_init (&change, widget_node->last_updated_style, style);
  if (gtk_css_style_change_has_change (&change))
    {
      GtkStyleContext *context;

      context = _gtk_widget_peek_style_context (widget_node->widget);
      if (context)
        gtk_style_context_validate (context, &change);
      else
        _gtk_widget_style_context_invalidated (widget_node->widget);
      g_set_object (&widget_node->last_updated_style, style);
    }
  gtk_css_style_change_finish (&change);
}

static gboolean
gtk_css_widget_node_init_matcher (GtkCssNode     *node,
                                  GtkCssMatcher  *matcher)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  if (widget_node->widget == NULL)
    return FALSE;

  return GTK_CSS_NODE_CLASS (gtk_css_widget_node_parent_class)->init_matcher (node, matcher);
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
  GtkStyleContext *context;
  GtkStyleCascade *cascade;

  if (widget_node->widget == NULL)
    return NULL;

  context = _gtk_widget_peek_style_context (widget_node->widget);
  if (context)
    return gtk_style_context_get_style_provider (context);

  cascade = _gtk_settings_get_style_cascade (gtk_widget_get_settings (widget_node->widget),
                                             gtk_widget_get_scale_factor (widget_node->widget));
  return GTK_STYLE_PROVIDER_PRIVATE (cascade);
}

static GdkFrameClock *
gtk_css_widget_node_get_frame_clock (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  if (widget_node->widget == NULL)
    return NULL;

  if (!gtk_settings_get_enable_animations (gtk_widget_get_settings (widget_node->widget)))
    return NULL;

  return gtk_widget_get_frame_clock (widget_node->widget);
}

static void
gtk_css_widget_node_class_init (GtkCssWidgetNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_widget_node_finalize;
  node_class->update_style = gtk_css_widget_node_update_style;
  node_class->validate = gtk_css_widget_node_validate;
  node_class->queue_validate = gtk_css_widget_node_queue_validate;
  node_class->dequeue_validate = gtk_css_widget_node_dequeue_validate;
  node_class->init_matcher = gtk_css_widget_node_init_matcher;
  node_class->create_widget_path = gtk_css_widget_node_create_widget_path;
  node_class->get_widget_path = gtk_css_widget_node_get_widget_path;
  node_class->get_style_provider = gtk_css_widget_node_get_style_provider;
  node_class->get_frame_clock = gtk_css_widget_node_get_frame_clock;
  node_class->style_changed = gtk_css_widget_node_style_changed;
}

static void
gtk_css_widget_node_init (GtkCssWidgetNode *node)
{
  node->last_updated_style = g_object_ref (gtk_css_static_style_get_default ());
}

GtkCssNode *
gtk_css_widget_node_new (GtkWidget *widget)
{
  GtkCssWidgetNode *result;

  gtk_internal_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  result = g_object_new (GTK_TYPE_CSS_WIDGET_NODE, NULL);
  result->widget = widget;
  gtk_css_node_set_visible (GTK_CSS_NODE (result),
                            _gtk_widget_get_visible (widget));

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

