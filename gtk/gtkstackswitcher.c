/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "gtkstackswitcher.h"
#include "gtkradiobutton.h"
#include "gtklabel.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkstackswitcher
 * @Short_description: A controller for GtkStack
 * @Title: GtkStackSwitcher
 * @See_also: #GtkStack
 *
 * The GtkStackSwitcher widget acts as a controller for a
 * #GtkStack; it shows a row of buttons to switch between
 * the various pages of the associated stack widget.
 *
 * All the content for the buttons comes from the child properties
 * of the #GtkStack.
 *
 * It is possible to associate multiple #GtkStackSwitcher widgets
 * with the same stack widget.
 *
 * The GtkStackSwitcher widget was added in 3.10.
 */

typedef struct _GtkStackSwitcherPrivate GtkStackSwitcherPrivate;
struct _GtkStackSwitcherPrivate
{
  GtkStack *stack;
  GHashTable *buttons;
  gboolean in_child_changed;
};

enum {
  PROP_0,
  PROP_STACK
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkStackSwitcher, gtk_stack_switcher, GTK_TYPE_BOX)

static void
gtk_stack_switcher_init (GtkStackSwitcher *switcher)
{
  GtkStyleContext *context;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (switcher);

  priv->stack = NULL;
  priv->buttons = g_hash_table_new (g_direct_hash, g_direct_equal);

  context = gtk_widget_get_style_context (GTK_WIDGET (switcher));
  gtk_style_context_add_class (context, "stack-switcher");
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (switcher), GTK_ORIENTATION_HORIZONTAL);
}

static void
on_button_clicked (GtkWidget        *widget,
                   GtkStackSwitcher *self)
{
  GtkWidget *child;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);

  if (!priv->in_child_changed)
    {
      child = g_object_get_data (G_OBJECT (widget), "stack-child");
      gtk_stack_set_visible_child (priv->stack, child);
    }
}

static void
rebuild_child (GtkWidget   *self,
               const gchar *icon_name,
               const gchar *title)
{
  GtkStyleContext *context;
  GtkWidget *button_child;

  button_child = gtk_bin_get_child (GTK_BIN (self));
  if (button_child != NULL)
    gtk_widget_destroy (button_child);

  button_child = NULL;
  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (icon_name != NULL)
    {
      button_child = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
      if (title != NULL)
        gtk_widget_set_tooltip_text (GTK_WIDGET (self), title);

      gtk_style_context_remove_class (context, "text-button");
      gtk_style_context_add_class (context, "image-button");
    }
  else if (title != NULL)
    {
      button_child = gtk_label_new (title);

      gtk_widget_set_tooltip_text (GTK_WIDGET (self), NULL);

      gtk_style_context_remove_class (context, "image-button");
      gtk_style_context_add_class (context, "text-button");
    }

  if (button_child)
    {
      gtk_widget_set_halign (GTK_WIDGET (button_child), GTK_ALIGN_CENTER);
      gtk_widget_show_all (button_child);
      gtk_container_add (GTK_CONTAINER (self), button_child);
    }
}

static void
update_needs_attention (GtkWidget *widget, GtkWidget *button, gpointer data)
{
  GtkContainer *container;
  gboolean needs_attention;
  GtkStyleContext *context;

  container = GTK_CONTAINER (data);
  gtk_container_child_get (container, widget,
                           "needs-attention", &needs_attention,
                           NULL);

  context = gtk_widget_get_style_context (button);
  if (needs_attention)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);
}

static void
update_button (GtkStackSwitcher *self,
               GtkWidget        *widget,
               GtkWidget        *button)
{
  gchar *title;
  gchar *icon_name;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);

  gtk_container_child_get (GTK_CONTAINER (priv->stack), widget,
                           "title", &title,
                           "icon-name", &icon_name,
                           NULL);

  rebuild_child (button, icon_name, title);

  gtk_widget_set_visible (button, gtk_widget_get_visible (widget) && (title != NULL || icon_name != NULL));

  if (icon_name != NULL)
    gtk_widget_set_size_request (button, -1, -1);
  else
    gtk_widget_set_size_request (button, 100, -1);

  g_free (title);
  g_free (icon_name);

  update_needs_attention (widget, button, priv->stack);
}

static void
on_title_icon_visible_updated (GtkWidget        *widget,
                               GParamSpec       *pspec,
                               GtkStackSwitcher *self)
{
  GtkWidget *button;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);

  button = g_hash_table_lookup (priv->buttons, widget);
  update_button (self, widget, button);
}

static void
on_position_updated (GtkWidget        *widget,
                     GParamSpec       *pspec,
                     GtkStackSwitcher *self)
{
  GtkWidget *button;
  gint position;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);

  button = g_hash_table_lookup (priv->buttons, widget);

  gtk_container_child_get (GTK_CONTAINER (priv->stack), widget,
                           "position", &position,
                           NULL);

  gtk_box_reorder_child (GTK_BOX (self), button, position);
}

static void
on_needs_attention_updated (GtkWidget        *widget,
                            GParamSpec       *pspec,
                            GtkStackSwitcher *self)
{
  GtkWidget *button;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);

  button = g_hash_table_lookup (priv->buttons, widget);
  update_button (self, widget, button);
}

static void
add_child (GtkWidget        *widget,
           GtkStackSwitcher *self)
{
  GtkWidget *button;
  GList *group;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);

  button = gtk_radio_button_new (NULL);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

  update_button (self, widget, button);

  group = gtk_container_get_children (GTK_CONTAINER (self));
  if (group != NULL)
    {
      gtk_radio_button_join_group (GTK_RADIO_BUTTON (button), GTK_RADIO_BUTTON (group->data));
      g_list_free (group);
    }

  gtk_container_add (GTK_CONTAINER (self), button);

  g_object_set_data (G_OBJECT (button), "stack-child", widget);
  g_signal_connect (button, "clicked", G_CALLBACK (on_button_clicked), self);
  g_signal_connect (widget, "notify::visible", G_CALLBACK (on_title_icon_visible_updated), self);
  g_signal_connect (widget, "child-notify::title", G_CALLBACK (on_title_icon_visible_updated), self);
  g_signal_connect (widget, "child-notify::icon-name", G_CALLBACK (on_title_icon_visible_updated), self);
  g_signal_connect (widget, "child-notify::position", G_CALLBACK (on_position_updated), self);
  g_signal_connect (widget, "child-notify::needs-attention", G_CALLBACK (on_needs_attention_updated), self);

  g_hash_table_insert (priv->buttons, widget, button);
}

static void
remove_child (GtkWidget        *widget,
              GtkStackSwitcher *self)
{
  GtkWidget *button;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);

  g_signal_handlers_disconnect_by_func (widget, on_title_icon_visible_updated, self);
  g_signal_handlers_disconnect_by_func (widget, on_position_updated, self);
  g_signal_handlers_disconnect_by_func (widget, on_needs_attention_updated, self);

  button = g_hash_table_lookup (priv->buttons, widget);
  gtk_container_remove (GTK_CONTAINER (self), button);
  g_hash_table_remove (priv->buttons, widget);
}

static void
populate_switcher (GtkStackSwitcher *self)
{
  GtkStackSwitcherPrivate *priv;
  GtkWidget *widget, *button;

  priv = gtk_stack_switcher_get_instance_private (self);
  gtk_container_foreach (GTK_CONTAINER (priv->stack), (GtkCallback)add_child, self);

  widget = gtk_stack_get_visible_child (priv->stack);
  if (widget)
    {
      button = g_hash_table_lookup (priv->buttons, widget);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    }
}

static void
clear_switcher (GtkStackSwitcher *self)
{
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);
  gtk_container_foreach (GTK_CONTAINER (priv->stack), (GtkCallback)remove_child, self);
}

static void
on_child_changed (GtkWidget        *widget,
                  GParamSpec       *pspec,
                  GtkStackSwitcher *self)
{
  GtkWidget *child;
  GtkWidget *button;
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (self);

  child = gtk_stack_get_visible_child (GTK_STACK (widget));
  button = g_hash_table_lookup (priv->buttons, child);
  if (button != NULL)
    {
      priv->in_child_changed = TRUE;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      priv->in_child_changed = FALSE;
    }
}

static void
on_stack_child_added (GtkContainer     *container,
                      GtkWidget        *widget,
                      GtkStackSwitcher *self)
{
  add_child (widget, self);
}

static void
on_stack_child_removed (GtkContainer     *container,
                        GtkWidget        *widget,
                        GtkStackSwitcher *self)
{
  remove_child (widget, self);
}

static void
disconnect_stack_signals (GtkStackSwitcher *switcher)
{
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (switcher);
  g_signal_handlers_disconnect_by_func (priv->stack, on_stack_child_added, switcher);
  g_signal_handlers_disconnect_by_func (priv->stack, on_stack_child_removed, switcher);
  g_signal_handlers_disconnect_by_func (priv->stack, on_child_changed, switcher);
  g_signal_handlers_disconnect_by_func (priv->stack, disconnect_stack_signals, switcher);
}

static void
connect_stack_signals (GtkStackSwitcher *switcher)
{
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (switcher);
  g_signal_connect_after (priv->stack, "add",
                          G_CALLBACK (on_stack_child_added), switcher);
  g_signal_connect_after (priv->stack, "remove",
                          G_CALLBACK (on_stack_child_removed), switcher);
  g_signal_connect (priv->stack, "notify::visible-child",
                    G_CALLBACK (on_child_changed), switcher);
  g_signal_connect_swapped (priv->stack, "destroy",
                            G_CALLBACK (disconnect_stack_signals), switcher);
}

/**
 * gtk_stack_switcher_set_stack:
 * @switcher: a #GtkStackSwitcher
 * @stack: (allow-none): a #GtkStack
 *
 * Sets the stack to control.
 *
 * Since: 3.10
 */
void
gtk_stack_switcher_set_stack (GtkStackSwitcher *switcher,
                              GtkStack         *stack)
{
  GtkStackSwitcherPrivate *priv;

  g_return_if_fail (GTK_IS_STACK_SWITCHER (switcher));
  g_return_if_fail (GTK_IS_STACK (stack) || stack == NULL);

  priv = gtk_stack_switcher_get_instance_private (switcher);

  if (priv->stack == stack)
    return;

  if (priv->stack)
    {
      disconnect_stack_signals (switcher);
      clear_switcher (switcher);
      g_clear_object (&priv->stack);
    }
  if (stack)
    {
      priv->stack = g_object_ref (stack);
      populate_switcher (switcher);
      connect_stack_signals (switcher);
    }

  gtk_widget_queue_resize (GTK_WIDGET (switcher));

  g_object_notify (G_OBJECT (switcher), "stack");
}

/**
 * gtk_stack_switcher_get_stack:
 * @switcher: a #GtkStackSwitcher
 *
 * Retrieves the stack.
 * See gtk_stack_switcher_set_stack().
 *
 * Returns: (transfer none): the stack, or %NULL if
 *    none has been set explicitly.
 *
 * Since: 3.10
 */
GtkStack *
gtk_stack_switcher_get_stack (GtkStackSwitcher *switcher)
{
  GtkStackSwitcherPrivate *priv;
  g_return_val_if_fail (GTK_IS_STACK_SWITCHER (switcher), NULL);

  priv = gtk_stack_switcher_get_instance_private (switcher);
  return priv->stack;
}

static void
gtk_stack_switcher_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (switcher);
  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, priv->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_switcher_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);

  switch (prop_id)
    {
    case PROP_STACK:
      gtk_stack_switcher_set_stack (switcher, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_switcher_dispose (GObject *object)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);

  gtk_stack_switcher_set_stack (switcher, NULL);

  G_OBJECT_CLASS (gtk_stack_switcher_parent_class)->dispose (object);
}

static void
gtk_stack_switcher_finalize (GObject *object)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);
  GtkStackSwitcherPrivate *priv;

  priv = gtk_stack_switcher_get_instance_private (switcher);

  g_hash_table_destroy (priv->buttons);

  G_OBJECT_CLASS (gtk_stack_switcher_parent_class)->finalize (object);
}

static void
gtk_stack_switcher_class_init (GtkStackSwitcherClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_stack_switcher_get_property;
  object_class->set_property = gtk_stack_switcher_set_property;
  object_class->dispose = gtk_stack_switcher_dispose;
  object_class->finalize = gtk_stack_switcher_finalize;

  g_object_class_install_property (object_class,
                                   PROP_STACK,
                                   g_param_spec_object ("stack",
                                                        P_("Stack"),
                                                        P_("Stack"),
                                                        GTK_TYPE_STACK,
                                                        GTK_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
}

/**
 * gtk_stack_switcher_new:
 *
 * Create a new #GtkStackSwitcher.
 *
 * Returns: a new #GtkStackSwitcher.
 *
 * Since: 3.10
 */
GtkWidget *
gtk_stack_switcher_new (void)
{
  return g_object_new (GTK_TYPE_STACK_SWITCHER, NULL);
}
