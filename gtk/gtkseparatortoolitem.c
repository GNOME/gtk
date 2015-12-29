/* gtkseparatortoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
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
#include "gtkseparatormenuitem.h"
#include "gtkseparatortoolitem.h"
#include "gtkintl.h"
#include "gtktoolbarprivate.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcsscustomgadgetprivate.h"

/**
 * SECTION:gtkseparatortoolitem
 * @Short_description: A toolbar item that separates groups of other
 *   toolbar items
 * @Title: GtkSeparatorToolItem
 * @See_also: #GtkToolbar, #GtkRadioToolButton
 *
 * A #GtkSeparatorToolItem is a #GtkToolItem that separates groups of other
 * #GtkToolItems. Depending on the theme, a #GtkSeparatorToolItem will
 * often look like a vertical line on horizontally docked toolbars.
 *
 * If the #GtkToolbar child property “expand” is %TRUE and the property
 * #GtkSeparatorToolItem:draw is %FALSE, a #GtkSeparatorToolItem will act as
 * a “spring” that forces other items to the ends of the toolbar.
 *
 * Use gtk_separator_tool_item_new() to create a new #GtkSeparatorToolItem.
 *
 * # CSS nodes
 *
 * GtkSeparatorToolItem has a single CSS node with name separator.
 */

#define MENU_ID "gtk-separator-tool-item-menu-id"

struct _GtkSeparatorToolItemPrivate
{
  GtkCssGadget *gadget;
  GdkWindow *event_window;
  guint draw : 1;
};

enum {
  PROP_0,
  PROP_DRAW
};

static gboolean gtk_separator_tool_item_create_menu_proxy (GtkToolItem               *item);
static void     gtk_separator_tool_item_set_property      (GObject                   *object,
                                                           guint                      prop_id,
                                                           const GValue              *value,
                                                           GParamSpec                *pspec);
static void     gtk_separator_tool_item_get_property      (GObject                   *object,
                                                           guint                      prop_id,
                                                           GValue                    *value,
                                                           GParamSpec                *pspec);
static void     gtk_separator_tool_item_get_preferred_width (GtkWidget               *widget,
                                                           gint                      *minimum,
                                                           gint                      *natural);
static void     gtk_separator_tool_item_get_preferred_height (GtkWidget              *widget,
                                                           gint                      *minimum,
                                                           gint                      *natural);
static void     gtk_separator_tool_item_size_allocate     (GtkWidget                 *widget,
                                                           GtkAllocation             *allocation);
static gboolean gtk_separator_tool_item_draw              (GtkWidget                 *widget,
                                                           cairo_t                   *cr);
static void     gtk_separator_tool_item_add               (GtkContainer              *container,
                                                           GtkWidget                 *child);
static void     gtk_separator_tool_item_realize           (GtkWidget                 *widget);
static void     gtk_separator_tool_item_unrealize         (GtkWidget                 *widget);
static void     gtk_separator_tool_item_map               (GtkWidget                 *widget);
static void     gtk_separator_tool_item_unmap             (GtkWidget                 *widget);
static gboolean gtk_separator_tool_item_button_event      (GtkWidget                 *widget,
                                                           GdkEventButton            *event);
static gboolean gtk_separator_tool_item_motion_event      (GtkWidget                 *widget,
                                                           GdkEventMotion            *event);

G_DEFINE_TYPE_WITH_PRIVATE (GtkSeparatorToolItem, gtk_separator_tool_item, GTK_TYPE_TOOL_ITEM)

static void
gtk_separator_tool_item_finalize (GObject *object)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);

  g_clear_object (&item->priv->gadget);

  G_OBJECT_CLASS (gtk_separator_tool_item_parent_class)->finalize (object);
}

static void
gtk_separator_tool_item_class_init (GtkSeparatorToolItemClass *class)
{
  GObjectClass *object_class;
  GtkContainerClass *container_class;
  GtkToolItemClass *toolitem_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GObjectClass *)class;
  container_class = (GtkContainerClass *)class;
  toolitem_class = (GtkToolItemClass *)class;
  widget_class = (GtkWidgetClass *)class;

  object_class->set_property = gtk_separator_tool_item_set_property;
  object_class->get_property = gtk_separator_tool_item_get_property;
  object_class->finalize = gtk_separator_tool_item_finalize;

  widget_class->get_preferred_width = gtk_separator_tool_item_get_preferred_width;
  widget_class->get_preferred_height = gtk_separator_tool_item_get_preferred_height;
  widget_class->size_allocate = gtk_separator_tool_item_size_allocate;
  widget_class->draw = gtk_separator_tool_item_draw;
  widget_class->realize = gtk_separator_tool_item_realize;
  widget_class->unrealize = gtk_separator_tool_item_unrealize;
  widget_class->map = gtk_separator_tool_item_map;
  widget_class->unmap = gtk_separator_tool_item_unmap;
  widget_class->button_press_event = gtk_separator_tool_item_button_event;
  widget_class->button_release_event = gtk_separator_tool_item_button_event;
  widget_class->motion_notify_event = gtk_separator_tool_item_motion_event;

  toolitem_class->create_menu_proxy = gtk_separator_tool_item_create_menu_proxy;
  
  container_class->add = gtk_separator_tool_item_add;
  
  g_object_class_install_property (object_class,
                                   PROP_DRAW,
                                   g_param_spec_boolean ("draw",
                                                         P_("Draw"),
                                                         P_("Whether the separator is drawn, or just blank"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (widget_class, "separator");
}

static void
gtk_separator_tool_item_init (GtkSeparatorToolItem *separator_item)
{
  GtkSeparatorToolItemPrivate *priv;
  GtkWidget *widget;
  GtkCssNode *widget_node;

  widget = GTK_WIDGET (separator_item);
  priv = separator_item->priv = gtk_separator_tool_item_get_instance_private (separator_item);
  priv->draw = TRUE;

  gtk_widget_set_has_window (widget, FALSE);

  widget_node = gtk_widget_get_css_node (widget);
  separator_item->priv->gadget =
    gtk_css_custom_gadget_new_for_node (widget_node,
                                        widget,
                                        NULL, NULL, NULL,
                                        NULL, NULL);
}

static void
gtk_separator_tool_item_add (GtkContainer *container,
                             GtkWidget    *child)
{
  g_warning ("attempt to add a child to an GtkSeparatorToolItem");
}

static gboolean
gtk_separator_tool_item_create_menu_proxy (GtkToolItem *item)
{
  GtkWidget *menu_item = NULL;
  
  menu_item = gtk_separator_menu_item_new ();
  
  gtk_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);
  
  return TRUE;
}

static void
gtk_separator_tool_item_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_DRAW:
      gtk_separator_tool_item_set_draw (item, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_tool_item_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_DRAW:
      g_value_set_boolean (value, gtk_separator_tool_item_get_draw (item));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_tool_item_get_preferred_width (GtkWidget *widget,
                                             gint      *minimum,
                                             gint      *natural)
{
  gtk_css_gadget_get_preferred_size (GTK_SEPARATOR_TOOL_ITEM (widget)->priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_separator_tool_item_get_preferred_height (GtkWidget *widget,
                                              gint      *minimum,
                                              gint      *natural)
{
  gtk_css_gadget_get_preferred_size (GTK_SEPARATOR_TOOL_ITEM (widget)->priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_separator_tool_item_size_allocate (GtkWidget     *widget,
                                       GtkAllocation *allocation)
{
  GtkSeparatorToolItem *separator = GTK_SEPARATOR_TOOL_ITEM (widget);
  GtkSeparatorToolItemPrivate *priv = separator->priv;
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_separator_tool_item_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GtkSeparatorToolItem *separator = GTK_SEPARATOR_TOOL_ITEM (widget);
  GtkSeparatorToolItemPrivate *priv = separator->priv;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget) |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                       &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_separator_tool_item_unrealize (GtkWidget *widget)
{
  GtkSeparatorToolItem *separator = GTK_SEPARATOR_TOOL_ITEM (widget);
  GtkSeparatorToolItemPrivate *priv = separator->priv;

  if (priv->event_window)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_separator_tool_item_parent_class)->unrealize (widget);
}

static void
gtk_separator_tool_item_map (GtkWidget *widget)
{
  GtkSeparatorToolItem *separator = GTK_SEPARATOR_TOOL_ITEM (widget);
  GtkSeparatorToolItemPrivate *priv = separator->priv;

  GTK_WIDGET_CLASS (gtk_separator_tool_item_parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show (priv->event_window);
}

static void
gtk_separator_tool_item_unmap (GtkWidget *widget)
{
  GtkSeparatorToolItem *separator = GTK_SEPARATOR_TOOL_ITEM (widget);
  GtkSeparatorToolItemPrivate *priv = separator->priv;

  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_separator_tool_item_parent_class)->unmap (widget);
}

static gboolean
gtk_separator_tool_item_motion_event (GtkWidget      *widget,
                                      GdkEventMotion *event)
{
  GtkSeparatorToolItem *separator = GTK_SEPARATOR_TOOL_ITEM (widget);
  GtkSeparatorToolItemPrivate *priv = separator->priv;

  /* We want window dragging to work on empty toolbar areas,
   * so we only eat button events on visible separators
   */
  return priv->draw;
}

static gboolean
gtk_separator_tool_item_button_event (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  GtkSeparatorToolItem *separator = GTK_SEPARATOR_TOOL_ITEM (widget);
  GtkSeparatorToolItemPrivate *priv = separator->priv;

  /* We want window dragging to work on empty toolbar areas,
   * so we only eat button events on visible separators
   */
  return priv->draw;
}

static gboolean
gtk_separator_tool_item_draw (GtkWidget *widget,
                              cairo_t   *cr)
{
  if (GTK_SEPARATOR_TOOL_ITEM (widget)->priv->draw)
    gtk_css_gadget_draw (GTK_SEPARATOR_TOOL_ITEM (widget)->priv->gadget, cr);

  return FALSE;
}

/**
 * gtk_separator_tool_item_new:
 * 
 * Create a new #GtkSeparatorToolItem
 * 
 * Returns: the new #GtkSeparatorToolItem
 * 
 * Since: 2.4
 */
GtkToolItem *
gtk_separator_tool_item_new (void)
{
  GtkToolItem *self;
  
  self = g_object_new (GTK_TYPE_SEPARATOR_TOOL_ITEM,
                       NULL);
  
  return self;
}

/**
 * gtk_separator_tool_item_get_draw:
 * @item: a #GtkSeparatorToolItem 
 * 
 * Returns whether @item is drawn as a line, or just blank. 
 * See gtk_separator_tool_item_set_draw().
 * 
 * Returns: %TRUE if @item is drawn as a line, or just blank.
 * 
 * Since: 2.4
 */
gboolean
gtk_separator_tool_item_get_draw (GtkSeparatorToolItem *item)
{
  g_return_val_if_fail (GTK_IS_SEPARATOR_TOOL_ITEM (item), FALSE);
  
  return item->priv->draw;
}

/**
 * gtk_separator_tool_item_set_draw:
 * @item: a #GtkSeparatorToolItem
 * @draw: whether @item is drawn as a vertical line
 * 
 * Whether @item is drawn as a vertical line, or just blank.
 * Setting this to %FALSE along with gtk_tool_item_set_expand() is useful
 * to create an item that forces following items to the end of the toolbar.
 * 
 * Since: 2.4
 */
void
gtk_separator_tool_item_set_draw (GtkSeparatorToolItem *item,
                                  gboolean              draw)
{
  g_return_if_fail (GTK_IS_SEPARATOR_TOOL_ITEM (item));

  draw = draw != FALSE;

  if (draw != item->priv->draw)
    {
      item->priv->draw = draw;
      if (draw)
        gtk_css_gadget_remove_class (item->priv->gadget, "invisible");
      else
        gtk_css_gadget_add_class (item->priv->gadget, "invisible");

      gtk_widget_queue_draw (GTK_WIDGET (item));

      g_object_notify (G_OBJECT (item), "draw");
    }
}
