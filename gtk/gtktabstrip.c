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
#include "gtkscrolledwindow.h"
#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkadjustmentprivate.h"
#include "gtkboxgadgetprivate.h"
#include "gtkwidgetprivate.h"

typedef struct
{
  GtkCssGadget    *gadget;
  GtkStack        *stack;
  gboolean         closable;
  gboolean         reversed;
  GtkWidget       *scrolledwindow;
  GtkWidget       *tabs;
  GtkScrollType    autoscroll_mode;
  guint            autoscroll_id;
  gboolean         autoscroll_goal_set;
  gdouble          autoscroll_goal;
} GtkTabStripPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkTabStrip, gtk_tab_strip, GTK_TYPE_CONTAINER)

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
  g_warning ("Can't add children to %s", G_OBJECT_TYPE_NAME (container));
}

static void
gtk_tab_strip_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
  g_warning ("Can't remove children from %s", G_OBJECT_TYPE_NAME (container));
}

static void
gtk_tab_strip_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GtkTabStrip *self = GTK_TAB_STRIP (container);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  if (include_internals)
    (*callback) (priv->scrolledwindow, callback_data);
}

static GType
gtk_tab_strip_child_type (GtkContainer *container)
{
  return G_TYPE_NONE;
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
gtk_tab_strip_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum_size,
                                   gint      *natural_size)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tab_strip_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tab_strip_get_preferred_width_for_height (GtkWidget *widget,
                                              gint       height,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     height,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tab_strip_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       width,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tab_strip_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static gboolean
gtk_tab_strip_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_draw (priv->gadget, cr);

  return FALSE;
}

static void
update_node_ordering (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  gboolean reverse;

  reverse = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  if ((reverse && !priv->reversed) ||
      (!reverse && priv->reversed))
    {
      gtk_box_gadget_reverse_children (GTK_BOX_GADGET (priv->gadget));
      priv->reversed = reverse;
    }
}

static void
gtk_tab_strip_direction_changed (GtkWidget        *widget,
                                 GtkTextDirection  previous_direction)
{
  update_node_ordering (GTK_TAB_STRIP (widget));

  GTK_WIDGET_CLASS (gtk_tab_strip_parent_class)->direction_changed (widget, previous_direction);
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

static void
gtk_tab_strip_finalize (GObject *object)
{
  GtkTabStrip *self = GTK_TAB_STRIP (object);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_tab_strip_parent_class)->finalize (object);
}

static void
gtk_tab_strip_class_init (GtkTabStripClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_tab_strip_get_property;
  object_class->set_property = gtk_tab_strip_set_property;
  object_class->finalize = gtk_tab_strip_finalize;

  widget_class->destroy = gtk_tab_strip_destroy;
  widget_class->get_preferred_width = gtk_tab_strip_get_preferred_width;
  widget_class->get_preferred_height = gtk_tab_strip_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_tab_strip_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_tab_strip_get_preferred_height_for_width;
  widget_class->size_allocate = gtk_tab_strip_size_allocate;
  widget_class->draw = gtk_tab_strip_draw;
  widget_class->direction_changed = gtk_tab_strip_direction_changed;

  container_class->add = gtk_tab_strip_add;
  container_class->remove = gtk_tab_strip_remove;
  container_class->forall = gtk_tab_strip_forall;
  container_class->child_type = gtk_tab_strip_child_type;

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
update_scrolling (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkPolicyType hscroll, vscroll;

  hscroll = GTK_POLICY_EXTERNAL;
  vscroll = GTK_POLICY_NEVER;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolledwindow),
                                  hscroll, vscroll);
}

static void
gtk_tab_strip_init (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkCssNode *widget_node;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv->closable = FALSE;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  priv->gadget = gtk_box_gadget_new_for_node (widget_node, GTK_WIDGET (self));
  gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_HORIZONTAL);

  priv->scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (priv->scrolledwindow);
  gtk_widget_set_parent (priv->scrolledwindow, GTK_WIDGET (self));
  gtk_box_gadget_insert_widget (GTK_BOX_GADGET (priv->gadget), 1, priv->scrolledwindow);
  gtk_box_gadget_set_gadget_expand (GTK_BOX_GADGET (priv->gadget), G_OBJECT (priv->scrolledwindow), TRUE);
  update_scrolling (self);

  priv->tabs = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (priv->tabs);
  gtk_container_add (GTK_CONTAINER (priv->scrolledwindow), priv->tabs);
}

static void
gtk_tab_strip_child_position_changed (GtkTabStrip *self,
                                      GParamSpec  *pspec,
                                      GtkWidget   *child)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
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

  gtk_container_child_set (GTK_CONTAINER (priv->tabs), GTK_WIDGET (tab),
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

  gtk_container_foreach (GTK_CONTAINER (priv->tabs), update_visible_child, visible_child);
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

  gtk_box_pack_start (GTK_BOX (priv->tabs), GTK_WIDGET (tab), TRUE, TRUE, 0);

  g_object_bind_property (widget, "visible", tab, "visible", G_BINDING_SYNC_CREATE);

  gtk_tab_strip_child_title_changed (self, NULL, widget);
  gtk_tab_strip_stack_notify_visible_child (self, NULL, stack);
}

static void
gtk_tab_strip_stack_remove (GtkTabStrip *self,
                            GtkWidget   *widget,
                            GtkStack    *stack)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkTab *tab;

  tab = g_object_get_data (G_OBJECT (widget), "GTK_TAB");

  if (GTK_IS_TAB (tab))
    gtk_container_remove (GTK_CONTAINER (priv->tabs), GTK_WIDGET (tab));
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
