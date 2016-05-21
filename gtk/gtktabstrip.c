/* gtktabstrip.c
 *
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtktabstrip.h"
#include "gtktab.h"
#include "gtksimpletab.h"
#include "gtkclosabletab.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkorientable.h"

/*
 * TODO:
 * - custom tabs
 * - scrolling
 * - reordering
 * - dnd
 * - other edges
 */

typedef struct
{
  GtkStack        *stack;
  gboolean         closable;
  gboolean         in_child_changed;
  GdkWindow       *event_window;
} GtkTabStripPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkTabStrip, gtk_tab_strip, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_STACK,
  PROP_CLOSABLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gtk_tab_strip_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  GTK_CONTAINER_CLASS (gtk_tab_strip_parent_class)->add (container, widget);
}

static void
gtk_tab_strip_destroy (GtkWidget *widget)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_tab_strip_set_stack (self, NULL);

  g_clear_object (&priv->stack);

  GTK_WIDGET_CLASS (gtk_tab_strip_parent_class)->destroy (widget);
}

static void
gtk_tab_strip_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkTabStrip *self = GTK_TAB_STRIP (object);

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, gtk_tab_strip_get_stack (self));
      break;

    case PROP_CLOSABLE:
      g_value_set_boolean (value, gtk_tab_strip_get_closable (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_tab_strip_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkTabStrip *self = GTK_TAB_STRIP (object);

  switch (prop_id)
    {
    case PROP_STACK:
      gtk_tab_strip_set_stack (self, g_value_get_object (value));
      break;

    case PROP_CLOSABLE:
      gtk_tab_strip_set_closable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
get_widget_coordinates (GtkWidget *widget,
                        GdkEvent  *event,
                        gdouble   *x,
                        gdouble   *y)
{
  GdkWindow *window = ((GdkEventAny *)event)->window;
  gdouble tx, ty;

  if (!gdk_event_get_coords (event, &tx, &ty))
    return FALSE;

  while (window && window != gtk_widget_get_window (widget))
    {
      gint window_x, window_y;

      gdk_window_get_position (window, &window_x, &window_y);
      tx += window_x;
      ty += window_y;

      window = gdk_window_get_parent (window);
    }

  if (window)
    {
      *x = tx;
      *y = ty;

      return TRUE;
    }
  else
    return FALSE;
}

static GtkTab *
get_tab_at_pos (GtkTabStrip *self,
                gdouble      x,
                gdouble      y)
{
  GtkAllocation allocation;
  GList *children, *l;
  GtkTab *tab;

  children = gtk_container_get_children (GTK_CONTAINER (self));

  tab = NULL;
  for (l = children; l; l = l->next)
    {
      gtk_widget_get_allocation (GTK_WIDGET (l->data), &allocation);
      if ((x >= allocation.x) &&
          (y >= allocation.y) &&
          (x <= (allocation.x + allocation.width)) &&
          (y <= (allocation.y + allocation.height)))
        {
          tab = l->data;
          break;
        }
    }

  g_list_free (children);

  return tab;
}

static gboolean
gtk_tab_strip_button_press (GtkWidget      *widget,
                            GdkEventButton *event)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkTab *tab;
  GtkWidget *child;
  gdouble x, y;

  if (!get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    return FALSE;

  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  tab = get_tab_at_pos (self, x, y);

  if (tab == NULL)
    return FALSE;

  child = gtk_tab_get_widget (tab);
  if (child == NULL)
    return FALSE;

  gtk_stack_set_visible_child (priv->stack, child);

  return TRUE;
}

static void
gtk_tab_strip_map (GtkWidget *widget)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_tab_strip_parent_class)->map (widget);

  gdk_window_show_unraised (priv->event_window);
}

static void
gtk_tab_strip_unmap (GtkWidget *widget)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_tab_strip_parent_class)->unmap (widget);
}

static void
gtk_tab_strip_realize (GtkWidget *widget)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK |
                            GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                       &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_tab_strip_unrealize (GtkWidget *widget)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_widget_unregister_window (widget, priv->event_window);
  gdk_window_destroy (priv->event_window);
  priv->event_window = NULL;

  GTK_WIDGET_CLASS (gtk_tab_strip_parent_class)->unrealize (widget);
}

static void
gtk_tab_strip_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_tab_strip_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->event_window,
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);
      if (gtk_widget_get_mapped (widget))
        gdk_window_show_unraised (priv->event_window);
    }
}

static void
gtk_tab_strip_class_init (GtkTabStripClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_tab_strip_get_property;
  object_class->set_property = gtk_tab_strip_set_property;

  widget_class->destroy = gtk_tab_strip_destroy;
  widget_class->map = gtk_tab_strip_map;
  widget_class->unmap = gtk_tab_strip_unmap;
  widget_class->realize = gtk_tab_strip_realize;
  widget_class->unrealize = gtk_tab_strip_unrealize;
  widget_class->size_allocate = gtk_tab_strip_size_allocate;
  widget_class->button_press_event = gtk_tab_strip_button_press;

  container_class->add = gtk_tab_strip_add;

  properties[PROP_STACK] =
    g_param_spec_object ("stack", P_("Stack"), P_("The stack of items to manage"),
                         GTK_TYPE_STACK,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_CLOSABLE] =
    g_param_spec_boolean ("closable", P_("Closable"), P_("Whether tabs can be closed"),
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "tabs");
}

static void
gtk_tab_strip_init (GtkTabStrip *self)
{
}

static void
gtk_tab_strip_child_position_changed (GtkTabStrip *self,
                                      GParamSpec  *pspec,
                                      GtkWidget   *child)
{
  GtkWidget *parent;
  GtkTab *tab;
  guint position;

  tab = g_object_get_data (G_OBJECT (child), "GTK_TAB");

  if (!tab || !GTK_IS_TAB (tab))
    return;

  parent = gtk_widget_get_parent (child);

  gtk_container_child_get (GTK_CONTAINER (parent), child,
                           "position", &position,
                           NULL);

  gtk_container_child_set (GTK_CONTAINER (self), GTK_WIDGET (tab),
                           "position", position,
                           NULL);
}

static void
gtk_tab_strip_child_title_changed (GtkTabStrip *self,
                                   GParamSpec  *pspec,
                                   GtkWidget   *child)
{
  g_autofree gchar *title = NULL;
  GtkWidget *parent;
  GtkTab *tab;

  tab = g_object_get_data (G_OBJECT (child), "GTK_TAB");

  if (!GTK_IS_TAB (tab))
    return;

  parent = gtk_widget_get_parent (child);

  gtk_container_child_get (GTK_CONTAINER (parent), child,
                           "title", &title,
                           NULL);

  gtk_tab_set_title (tab, title);
}

static void
update_visible_child (GtkWidget *tab,
                      gpointer   user_data)
{
  GtkWidget *visible_child = user_data;

  if (GTK_IS_TAB (tab))
    {
      if (gtk_tab_get_widget (GTK_TAB (tab)) == visible_child)
        gtk_widget_set_state_flags (tab, GTK_STATE_FLAG_CHECKED, FALSE);
      else
        gtk_widget_unset_state_flags (tab, GTK_STATE_FLAG_CHECKED);
    }
}

static void
gtk_tab_strip_stack_notify_visible_child (GtkTabStrip *self,
                                          GParamSpec  *pspec,
                                          GtkStack    *stack)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkWidget *visible_child;

  visible_child = gtk_stack_get_visible_child (stack);

  priv->in_child_changed = TRUE;
  gtk_container_foreach (GTK_CONTAINER (self), update_visible_child, visible_child);
  priv->in_child_changed = FALSE;
}

static void
tab_activated (GtkTab      *tab,
               GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkWidget *widget;

  widget = gtk_tab_get_widget (tab);
  if (widget)
    gtk_stack_set_visible_child (priv->stack, widget);
}

static void
gtk_tab_strip_stack_add (GtkTabStrip *self,
                         GtkWidget   *widget,
                         GtkStack    *stack)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkTab *tab;
  gint position = 0;

  gtk_container_child_get (GTK_CONTAINER (stack), widget,
                           "position", &position,
                           NULL);

  tab = g_object_new (priv->closable ? GTK_TYPE_CLOSABLE_TAB : GTK_TYPE_SIMPLE_TAB,
                      "widget", widget,
                      NULL);

  g_object_set_data (G_OBJECT (widget), "GTK_TAB", tab);

  g_signal_connect (tab, "activate",
                    G_CALLBACK (tab_activated), self);

  g_signal_connect_object (widget, "child-notify::position",
                           G_CALLBACK (gtk_tab_strip_child_position_changed), self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (widget, "child-notify::title",
                           G_CALLBACK (gtk_tab_strip_child_title_changed), self,
                           G_CONNECT_SWAPPED);

  gtk_box_pack_start (GTK_BOX (self), GTK_WIDGET (tab), TRUE, TRUE, 0);

  g_object_bind_property (widget, "visible", tab, "visible", G_BINDING_SYNC_CREATE);

  gtk_tab_strip_child_title_changed (self, NULL, widget);
  gtk_tab_strip_stack_notify_visible_child (self, NULL, stack);
}

static void
gtk_tab_strip_stack_remove (GtkTabStrip *self,
                            GtkWidget   *widget,
                            GtkStack    *stack)
{
  GtkTab *tab;

  tab = g_object_get_data (G_OBJECT (widget), "GTK_TAB");

  if (GTK_IS_TAB (tab))
    gtk_container_remove (GTK_CONTAINER (self), GTK_WIDGET (tab));
}

GtkWidget *
gtk_tab_strip_new (void)
{
  return g_object_new (GTK_TYPE_TAB_STRIP, NULL);
}

GtkStack *
gtk_tab_strip_get_stack (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TAB_STRIP (self), NULL);

  return priv->stack;
}

static void
gtk_tab_strip_cold_plug (GtkWidget *widget,
                         gpointer   user_data)
{
  GtkTabStrip *self = user_data;
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_tab_strip_stack_add (self, widget, priv->stack);
}

void
gtk_tab_strip_set_stack (GtkTabStrip *self,
                         GtkStack    *stack)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_if_fail (GTK_IS_TAB_STRIP (self));
  g_return_if_fail (!stack || GTK_IS_STACK (stack));

  if (priv->stack == stack)
    return;

  if (priv->stack != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->stack,
                                            G_CALLBACK (gtk_tab_strip_stack_notify_visible_child),
                                            self);

      g_signal_handlers_disconnect_by_func (priv->stack,
                                            G_CALLBACK (gtk_tab_strip_stack_add),
                                            self);

      g_signal_handlers_disconnect_by_func (priv->stack,
                                            G_CALLBACK (gtk_tab_strip_stack_remove),
                                            self);

      gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback)gtk_widget_destroy, NULL);

      g_clear_object (&priv->stack);
    }

  if (stack != NULL)
    {
      priv->stack = g_object_ref (stack);

      g_signal_connect_object (priv->stack,
                               "notify::visible-child",
                               G_CALLBACK (gtk_tab_strip_stack_notify_visible_child),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (priv->stack,
                               "add",
                               G_CALLBACK (gtk_tab_strip_stack_add),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (priv->stack,
                               "remove",
                               G_CALLBACK (gtk_tab_strip_stack_remove),
                               self,
                               G_CONNECT_SWAPPED);

      gtk_container_foreach (GTK_CONTAINER (priv->stack),
                             gtk_tab_strip_cold_plug,
                             self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STACK]);
}

void
gtk_tab_strip_set_closable (GtkTabStrip *self,
                            gboolean     closable)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_if_fail (GTK_IS_TAB_STRIP (self));

  if (priv->closable == closable)
    return;

  priv->closable = closable;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLOSABLE]);
}

gboolean
gtk_tab_strip_get_closable (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TAB_STRIP (self), FALSE);

  return priv->closable;
}
