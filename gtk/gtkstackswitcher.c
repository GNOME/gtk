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
#include "gtkdnd.h"
#include "gtkdragdest.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkwidgetprivate.h"
#include "gtktypebuiltins.h"
#include "gtkimage.h"
#include "gtkselectionmodel.h"

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
 * of the #GtkStack; the button visibility in a #GtkStackSwitcher
 * widget is controlled by the visibility of the child in the
 * #GtkStack.
 *
 * It is possible to associate multiple #GtkStackSwitcher widgets
 * with the same #GtkStack widget.
 *
 * The GtkStackSwitcher widget was added in 3.10.
 *
 * # CSS nodes
 *
 * GtkStackSwitcher has a single CSS node named stackswitcher and
 * style class .stack-switcher.
 *
 * When circumstances require it, GtkStackSwitcher adds the
 * .needs-attention style class to the widgets representing the
 * stack pages.
 */

#define TIMEOUT_EXPAND 500

typedef struct _GtkStackSwitcherPrivate GtkStackSwitcherPrivate;
struct _GtkStackSwitcherPrivate
{
  GtkSelectionModel *pages;
  GtkStackSwitcherDataFunc data_func;
  gpointer user_data;
  GDestroyNotify destroy_notify;
  GHashTable *buttons;
  GtkWidget *switch_button;
  guint switch_timer;
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

  gtk_widget_set_has_surface (GTK_WIDGET (switcher), FALSE);

  priv = gtk_stack_switcher_get_instance_private (switcher);

  priv->buttons = g_hash_table_new (g_direct_hash, g_direct_equal);

  context = gtk_widget_get_style_context (GTK_WIDGET (switcher));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (switcher), GTK_ORIENTATION_HORIZONTAL);

  gtk_drag_dest_set (GTK_WIDGET (switcher), 0, NULL, 0);
  gtk_drag_dest_set_track_motion (GTK_WIDGET (switcher), TRUE);
}

static void
on_button_toggled (GtkWidget        *button,
                   GParamSpec       *pspec,
                   GtkStackSwitcher *self)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);
  gboolean active;
  guint index;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  index = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "child-index"));

  if (active)
    {
      gtk_selection_model_select_item (priv->pages, index, TRUE);
    }
  else
    {
      gboolean selected = gtk_selection_model_is_selected (priv->pages, index);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), selected);
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
      button_child = gtk_image_new_from_icon_name (icon_name);
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
      gtk_container_add (GTK_CONTAINER (self), button_child);
    }
}

static void
update_button (GtkStackSwitcher *self,
               GObject          *item,
               GtkWidget        *button)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);
  gboolean visible;
  gchar *title;
  gchar *icon_name;
  gboolean needs_attention;
  GtkStyleContext *context;

  priv->data_func (item, priv->user_data, &visible, &title, &icon_name, &needs_attention);

g_print ("update button: visible %d title %s\n", visible, title);
  rebuild_child (button, icon_name, title);

  gtk_widget_set_visible (button, visible && (title != NULL || icon_name != NULL));

  context = gtk_widget_get_style_context (button);
  if (needs_attention)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);

  g_free (title);
  g_free (icon_name);
}

static void
on_item_changed (GObject          *item,
                 GParamSpec       *pspec,
                 GtkStackSwitcher *self)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);
  GtkWidget *button;

  button = g_hash_table_lookup (priv->buttons, item);
  update_button (self, item, button);
}

static void
remove_switch_timer (GtkStackSwitcher *self)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);

  if (priv->switch_timer)
    {
      g_source_remove (priv->switch_timer);
      priv->switch_timer = 0;
    }
}

static gboolean
gtk_stack_switcher_switch_timeout (gpointer data)
{
  GtkStackSwitcher *self = data;
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);
  GtkWidget *button;

  priv->switch_timer = 0;

  button = priv->switch_button;
  priv->switch_button = NULL;

  if (button)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

  return G_SOURCE_REMOVE;
}

static gboolean
gtk_stack_switcher_drag_motion (GtkWidget *widget,
                                GdkDrop   *drop,
                                gint       x,
                                gint       y)
{
  GtkStackSwitcher *self = GTK_STACK_SWITCHER (widget);
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);
  GtkWidget *button;
  GHashTableIter iter;
  gpointer value;
  gboolean retval = FALSE;

  button = NULL;
  g_hash_table_iter_init (&iter, priv->buttons);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      if (gtk_widget_contains (GTK_WIDGET (value), x, y))
        {
          button = GTK_WIDGET (value);
          retval = TRUE;
          break;
        }
    }

  if (button != priv->switch_button)
    remove_switch_timer (self);

  priv->switch_button = button;

  if (button && !priv->switch_timer)
    {
      priv->switch_timer = g_timeout_add (TIMEOUT_EXPAND,
                                          gtk_stack_switcher_switch_timeout,
                                          self);
      g_source_set_name_by_id (priv->switch_timer, "[gtk] gtk_stack_switcher_switch_timeout");
    }

  return retval;
}

static void
gtk_stack_switcher_drag_leave (GtkWidget *widget,
                               GdkDrop   *drop)
{
  GtkStackSwitcher *self = GTK_STACK_SWITCHER (widget);

  remove_switch_timer (self);
}

static void
add_child (guint             position,
           GtkStackSwitcher *self)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);
  GtkWidget *button;
  GObject *item;
  gboolean selected;

g_print ("add child\n");
  button = gtk_toggle_button_new ();
  gtk_widget_set_focus_on_click (button, FALSE);

  item = g_list_model_get_item (G_LIST_MODEL (priv->pages), position);
  update_button (self, item, button);

  gtk_container_add (GTK_CONTAINER (self), button);

  g_object_set_data (G_OBJECT (button), "child-index", GUINT_TO_POINTER (position));
  selected = gtk_selection_model_is_selected (priv->pages, position);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), selected);

  g_signal_connect (button, "notify::active", G_CALLBACK (on_button_toggled), self);
  g_signal_connect (item, "notify", G_CALLBACK (on_item_changed), self);

  g_hash_table_insert (priv->buttons, item, button);

  g_object_unref (item);
}

static void
populate_switcher (GtkStackSwitcher *self)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);
  guint i;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (priv->pages)); i++)
    add_child (i, self);
}

static void
clear_switcher (GtkStackSwitcher *self)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (self);
  GHashTableIter iter;
  GtkWidget *button;
  GObject *item;

  g_hash_table_iter_init (&iter, priv->buttons);
  while (g_hash_table_iter_next (&iter, (gpointer *)&item, (gpointer *)&button))
    {
      gtk_container_remove (GTK_CONTAINER (self), button);
      g_hash_table_iter_remove (&iter);
      g_signal_handlers_disconnect_by_func (item, on_item_changed, self);
    }
}

static void
items_changed_cb (GListModel       *model,
                  guint             position,
                  guint             removed,
                  guint             added,
                  GtkStackSwitcher *switcher)
{
  clear_switcher (switcher);
  populate_switcher (switcher);
}

static void
selection_changed_cb (GtkSelectionModel *model,
                      guint              position,
                      guint              n_items,
                      GtkStackSwitcher  *switcher)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (switcher);
  guint i;

  for (i = position; i < position + n_items; i++)
    {
      GObject *item;
      GtkWidget *button;
      gboolean selected;

      item = g_list_model_get_item (G_LIST_MODEL (priv->pages), i);
      button = g_hash_table_lookup (priv->buttons, item);
      selected = gtk_selection_model_is_selected (priv->pages, i);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), selected);
      g_object_unref (item);
    }
}

static void
disconnect_stack_signals (GtkStackSwitcher *switcher)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (switcher);

  g_signal_handlers_disconnect_by_func (priv->pages, items_changed_cb, switcher);
  g_signal_handlers_disconnect_by_func (priv->pages, selection_changed_cb, switcher);
}

static void
connect_stack_signals (GtkStackSwitcher *switcher)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (switcher);

  g_signal_connect (priv->pages, "items-changed", G_CALLBACK (items_changed_cb), switcher);
  g_signal_connect (priv->pages, "selection-changed", G_CALLBACK (selection_changed_cb), switcher);
}

static void
stack_data_func (GObject *item,
                 gpointer data,
                 gboolean *visible,
                 char    **title,
                 char    **icon_name,
                 gboolean *needs_attention)
{
  GtkStack *stack = data;
  GtkWidget *widget = GTK_WIDGET (item);
  GtkStackPage *page = gtk_stack_get_page (stack, widget);

  if (page)
    {
      *visible = gtk_widget_get_visible (widget);
      g_object_get (page,
                    "title", title,
                    "icon-name", icon_name,
                    "needs-attention", needs_attention,
                    NULL);
    }
  else
    {
      *visible = FALSE;
      *title = g_strdup ("*deceased*");
      *icon_name = NULL;
      *needs_attention = FALSE;
    }
}

static void
set_model (GtkStackSwitcher *switcher,
           GtkSelectionModel *model,
           GtkStackSwitcherDataFunc data_func,
           gpointer                 data,
           GDestroyNotify           destroy_notify)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (switcher);

  if (model)
    {
      priv->pages = g_object_ref (model),
      priv->data_func = data_func;
      priv->user_data = data;
      priv->destroy_notify = destroy_notify;
      populate_switcher (switcher);
      connect_stack_signals (switcher);
    }
}

static void
unset_model (GtkStackSwitcher *switcher)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (switcher);

  if (priv->pages)
    {
      disconnect_stack_signals (switcher);
      clear_switcher (switcher);
      priv->data_func = NULL;
      if (priv->destroy_notify)
        priv->destroy_notify (priv->user_data);
      priv->destroy_notify = NULL;
      priv->user_data = NULL;
      g_clear_object (&priv->pages);
    }
}

void
gtk_stack_switcher_set_model (GtkStackSwitcher         *switcher,
                              GtkSelectionModel        *model,
                              GtkStackSwitcherDataFunc  data_func,
                              gpointer                  data,
                              GDestroyNotify            destroy)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (switcher);

  if (priv->pages == model)
    return;

  unset_model (switcher);
  set_model (switcher, model, data_func, data, destroy);

  gtk_widget_queue_resize (GTK_WIDGET (switcher));
}

/**
 * gtk_stack_switcher_set_stack:
 * @switcher: a #GtkStackSwitcher
 * @stack: (allow-none): a #GtkStack
 *
 * Sets the stack to control.
 */
void
gtk_stack_switcher_set_stack (GtkStackSwitcher *switcher,
                              GtkStack         *stack)
{
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (switcher);
  GtkSelectionModel *model;

  g_return_if_fail (GTK_IS_STACK_SWITCHER (switcher));
  g_return_if_fail (GTK_IS_STACK (stack) || stack == NULL);

  if (priv->user_data == (gpointer) stack)
    return;

  unset_model (switcher);

  model = gtk_stack_get_pages (stack);
  set_model (switcher, model, stack_data_func, g_object_ref (stack), g_object_unref);
  g_object_unref (model);

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
 * Returns: (nullable) (transfer none): the stack, or %NULL if
 *    none has been set explicitly.
 */
GtkStack *
gtk_stack_switcher_get_stack (GtkStackSwitcher *switcher)
{
  GtkStackSwitcherPrivate *priv;
  g_return_val_if_fail (GTK_IS_STACK_SWITCHER (switcher), NULL);

  priv = gtk_stack_switcher_get_instance_private (switcher);
  if (priv->data_func == stack_data_func)
    return GTK_STACK (priv->user_data);

  return NULL;
}

static void
gtk_stack_switcher_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, gtk_stack_switcher_get_stack (switcher));
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

  remove_switch_timer (switcher);
  unset_model (switcher);

  G_OBJECT_CLASS (gtk_stack_switcher_parent_class)->dispose (object);
}

static void
gtk_stack_switcher_finalize (GObject *object)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);
  GtkStackSwitcherPrivate *priv = gtk_stack_switcher_get_instance_private (switcher);

  g_hash_table_destroy (priv->buttons);

  G_OBJECT_CLASS (gtk_stack_switcher_parent_class)->finalize (object);
}

static void
gtk_stack_switcher_class_init (GtkStackSwitcherClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_stack_switcher_get_property;
  object_class->set_property = gtk_stack_switcher_set_property;
  object_class->dispose = gtk_stack_switcher_dispose;
  object_class->finalize = gtk_stack_switcher_finalize;

  widget_class->drag_motion = gtk_stack_switcher_drag_motion;
  widget_class->drag_leave = gtk_stack_switcher_drag_leave;

  g_object_class_install_property (object_class,
                                   PROP_STACK,
                                   g_param_spec_object ("stack",
                                                        P_("Stack"),
                                                        P_("Stack"),
                                                        GTK_TYPE_STACK,
                                                        GTK_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));

  gtk_widget_class_set_css_name (widget_class, I_("stackswitcher"));
}

/**
 * gtk_stack_switcher_new:
 *
 * Create a new #GtkStackSwitcher.
 *
 * Returns: a new #GtkStackSwitcher.
 */
GtkWidget *
gtk_stack_switcher_new (void)
{
  return g_object_new (GTK_TYPE_STACK_SWITCHER, NULL);
}
