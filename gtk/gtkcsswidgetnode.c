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
/* widgets for special casing go here */
#include "gtkbox.h"

/* When these change we do a full restyling. Otherwise we try to figure out
 * if we need to change things. */
#define GTK_CSS_RADICAL_CHANGE (GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_SOURCE | GTK_CSS_CHANGE_PARENT_STYLE)

G_DEFINE_TYPE (GtkCssWidgetNode, gtk_css_widget_node, GTK_TYPE_CSS_NODE)

static void
gtk_css_widget_node_finalize (GObject *object)
{
  GtkCssWidgetNode *node = GTK_CSS_WIDGET_NODE (object);

  _gtk_bitmask_free (node->accumulated_changes);

  G_OBJECT_CLASS (gtk_css_widget_node_parent_class)->finalize (object);
}

static void
gtk_css_widget_node_style_changed (GtkCssNode   *cssnode,
                                   GtkCssStyle  *old_style,
                                   GtkCssStyle  *new_style)
{
  GtkCssWidgetNode *node;
  GtkBitmask *diff;

  node = GTK_CSS_WIDGET_NODE (cssnode);

  GTK_CSS_NODE_CLASS (gtk_css_widget_node_parent_class)->style_changed (cssnode, old_style, new_style);

  diff = gtk_css_style_get_difference (new_style, old_style);
  node->accumulated_changes = _gtk_bitmask_union (node->accumulated_changes, diff);
  _gtk_bitmask_free (diff);
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

static void
gtk_css_widget_node_queue_validate (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (GTK_IS_RESIZE_CONTAINER (widget_node->widget))
    widget_node->validate_cb_id = gtk_widget_add_tick_callback (widget_node->widget,
                                                                gtk_css_widget_node_queue_callback,
                                                                node,
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

static void
gtk_css_widget_node_validate (GtkCssNode *node)
{
  GtkCssWidgetNode *widget_node = GTK_CSS_WIDGET_NODE (node);
  GtkStyleContext *context;

  if (widget_node->widget == NULL)
    return;

  context = _gtk_widget_peek_style_context (widget_node->widget);
  if (context)
    gtk_style_context_validate (context, widget_node->accumulated_changes);
  else
    _gtk_widget_style_context_invalidated (widget_node->widget);
  _gtk_bitmask_free (widget_node->accumulated_changes);
  widget_node->accumulated_changes = _gtk_bitmask_new ();
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
  gboolean animate;

  if (widget_node->widget == NULL)
    return NULL;

  g_object_get (gtk_widget_get_settings (widget_node->widget),
                "gtk-enable-animations", &animate,
                NULL);
  if (animate == FALSE)
    return NULL;

  return gtk_widget_get_frame_clock (widget_node->widget);
}

static void
gtk_css_widget_node_class_init (GtkCssWidgetNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_widget_node_finalize;
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
  node->accumulated_changes = _gtk_bitmask_new ();
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

