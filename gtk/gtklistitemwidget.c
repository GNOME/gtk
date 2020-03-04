/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtklistitemwidgetprivate.h"

#include "gtkbindings.h"
#include "gtkbinlayout.h"
#include "gtkcssnodeprivate.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgestureclick.h"
#include "gtkintl.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitemprivate.h"
#include "gtkmain.h"
#include "gtkselectionmodel.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"

typedef struct _GtkListItemWidgetPrivate GtkListItemWidgetPrivate;
struct _GtkListItemWidgetPrivate
{
  GtkListItemFactory *factory;
  GtkListItem *list_item;

  GObject *item;
  guint position;
  gboolean selected;
  gboolean single_click_activate;
};

enum {
  PROP_0,
  PROP_FACTORY,
  PROP_SINGLE_CLICK_ACTIVATE,

  N_PROPS
};

enum
{
  ACTIVATE_SIGNAL,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkListItemWidget, gtk_list_item_widget, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_list_item_widget_activate_signal (GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  if (priv->list_item && !priv->list_item->activatable)
    return;

  gtk_widget_activate_action (GTK_WIDGET (self),
                              "list.activate-item",
                              "u",
                              priv->position);
}

static gboolean
gtk_list_item_widget_focus (GtkWidget        *widget,
                            GtkDirectionType  direction)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  /* The idea of this function is the following:
   * 1. If any child can take focus, do not ever attempt
   *    to take focus.
   * 2. Otherwise, if this item is selectable or activatable,
   *    allow focusing this widget.
   *
   * This makes sure every item in a list is focusable for
   * activation and selection handling, but no useless widgets
   * get focused and moving focus is as fast as possible.
   */
  if (priv->list_item && priv->list_item->child)
    {
      if (gtk_widget_get_focus_child (widget))
        return FALSE;
      if (gtk_widget_child_focus (priv->list_item->child, direction))
        return TRUE;
    }

  if (gtk_widget_is_focus (widget))
    return FALSE;

  if (!gtk_widget_get_can_focus (widget) ||
      !priv->list_item->selectable)
    return FALSE;

  return gtk_widget_grab_focus (widget);
}

static gboolean
gtk_list_item_widget_grab_focus (GtkWidget *widget)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  if (priv->list_item && priv->list_item->child && gtk_widget_grab_focus (priv->list_item->child))
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_list_item_widget_parent_class)->grab_focus (widget);
}

static void
gtk_list_item_widget_root (GtkWidget *widget)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_list_item_widget_parent_class)->root (widget);

  if (priv->factory)
    gtk_list_item_factory_setup (priv->factory, self);
}

static void
gtk_list_item_widget_unroot (GtkWidget *widget)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_list_item_widget_parent_class)->unroot (widget);

  if (priv->list_item)
      gtk_list_item_factory_teardown (priv->factory, self);
}

static void
gtk_list_item_widget_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_list_item_widget_set_factory (self, g_value_get_object (value));
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      gtk_list_item_widget_set_single_click_activate (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_item_widget_dispose (GObject *object)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (object);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  g_assert (priv->list_item == NULL);

  g_clear_object (&priv->item);
  g_clear_object (&priv->factory);

  G_OBJECT_CLASS (gtk_list_item_widget_parent_class)->dispose (object);
}

static void
gtk_list_item_widget_select_action (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *parameter)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);
  gboolean modify, extend;

  if (priv->list_item && !priv->list_item->selectable)
    return;

  g_variant_get (parameter, "(bb)", &modify, &extend);

  gtk_widget_activate_action (GTK_WIDGET (self),
                              "list.select-item",
                              "(ubb)",
                              priv->position, modify, extend);
}

static void
gtk_list_item_widget_class_init (GtkListItemWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  klass->activate_signal = gtk_list_item_widget_activate_signal;

  widget_class->focus = gtk_list_item_widget_focus;
  widget_class->grab_focus = gtk_list_item_widget_grab_focus;
  widget_class->root = gtk_list_item_widget_root;
  widget_class->unroot = gtk_list_item_widget_unroot;

  gobject_class->set_property = gtk_list_item_widget_set_property;
  gobject_class->dispose = gtk_list_item_widget_dispose;

  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         "Factory",
                         "Factory managing this list item",
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SINGLE_CLICK_ACTIVATE] =
    g_param_spec_boolean ("single-click-activate",
                          "Single click activate",
                          "Activate on single click",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  signals[ACTIVATE_SIGNAL] =
    g_signal_new (I_("activate-keybinding"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListItemWidgetClass, activate_signal),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  widget_class->activate_signal = signals[ACTIVATE_SIGNAL];

  /**
   * GtkListItem|listitem.select:
   * @modify: %TRUE to toggle the existing selection, %FALSE to select
   * @extend: %TRUE to extend the selection
   *
   * Changes selection if the item is selectable.
   * If the item is not selectable, nothing happens.
   *
   * This function will emit the list.select-item action and the resulting
   * behavior, in particular the interpretation of @modify and @extend
   * depends on the view containing this listitem. See for example
   * GtkListView|list.select-item or GtkGridView|list.select-item.
   */
  gtk_widget_class_install_action (widget_class,
                                   "listitem.select",
                                   "(bb)",
                                   gtk_list_item_widget_select_action);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0,
                                "activate-keybinding", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0,
                                "activate-keybinding", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0,
                                "activate-keybinding", 0);

  /* note that some of these may get overwritten by child widgets,
   * such as GtkTreeExpander */
  gtk_binding_entry_add_action (binding_set, GDK_KEY_space, 0,
                                "listitem.select", "(bb)", TRUE, FALSE);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_space, GDK_CONTROL_MASK,
                                "listitem.select", "(bb)", TRUE, FALSE);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_space, GDK_SHIFT_MASK,
                                "listitem.select", "(bb)", TRUE, FALSE);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_space, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "listitem.select", "(bb)", TRUE, FALSE);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_KP_Space, 0,
                                "listitem.select", "(bb)", TRUE, FALSE);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
                                "listitem.select", "(bb)", TRUE, FALSE);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_KP_Space, GDK_SHIFT_MASK,
                                "listitem.select", "(bb)", TRUE, FALSE);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "listitem.select", "(bb)", TRUE, FALSE);

  /* This gets overwritten by gtk_list_item_widget_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, I_("row"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_list_item_widget_click_gesture_pressed (GtkGestureClick   *gesture,
                                            int                n_press,
                                            double             x,
                                            double             y,
                                            GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);

  if (priv->list_item && !priv->list_item->selectable && !priv->list_item->activatable)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (!priv->list_item || priv->list_item->selectable)
    {
      GdkModifierType state;
      GdkModifierType mask;
      gboolean extend = FALSE, modify = FALSE;

      if (gtk_get_current_event_state (&state))
        {
          mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_MODIFY_SELECTION);
          if ((state & mask) == mask)
            modify = TRUE;
          mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_EXTEND_SELECTION);
          if ((state & mask) == mask)
            extend = TRUE;
        }

      gtk_widget_activate_action (GTK_WIDGET (self),
                                  "list.select-item",
                                  "(ubb)",
                                  priv->position, modify, extend);
    }

  if (!priv->list_item || priv->list_item->activatable)
    {
      if (n_press == 2 || priv->single_click_activate)
        {
          gtk_widget_activate_action (GTK_WIDGET (self),
                                      "list.activate-item",
                                      "u",
                                      priv->position);
        }
    }

  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_ACTIVE, FALSE);

  if (gtk_widget_get_focus_on_click (widget))
    gtk_widget_grab_focus (widget);
}

static void
gtk_list_item_widget_enter_cb (GtkEventControllerFocus *controller,
                               GtkListItemWidget       *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  gtk_widget_activate_action (widget,
                              "list.scroll-to-item",
                              "u",
                              priv->position);
}

static void
gtk_list_item_widget_hover_cb (GtkEventControllerMotion *controller,
                               double                    x,
                               double                    y,
                               GtkListItemWidget        *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  if (!priv->single_click_activate)
    return;

  if (!priv->list_item || priv->list_item->selectable)
    {
      gtk_widget_activate_action (GTK_WIDGET (self),
                                  "list.select-item",
                                  "(ubb)",
                                  priv->position, FALSE, FALSE);
    }
}

static void
gtk_list_item_widget_click_gesture_released (GtkGestureClick   *gesture,
                                             int                n_press,
                                             double             x,
                                             double             y,
                                             GtkListItemWidget *self)
{
  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);
}

static void
gtk_list_item_widget_click_gesture_canceled (GtkGestureClick   *gesture,
                                             GdkEventSequence  *sequence,
                                             GtkListItemWidget *self)
{
  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);
}

static void
gtk_list_item_widget_init (GtkListItemWidget *self)
{
  GtkEventController *controller;
  GtkGesture *gesture;

  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);

  gesture = gtk_gesture_click_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture),
                                     FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
                                 GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_list_item_widget_click_gesture_pressed), self);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_list_item_widget_click_gesture_released), self);
  g_signal_connect (gesture, "cancel",
                    G_CALLBACK (gtk_list_item_widget_click_gesture_canceled), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_focus_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_list_item_widget_enter_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_list_item_widget_hover_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

GtkWidget *
gtk_list_item_widget_new (GtkListItemFactory *factory,
                          const char         *css_name)
{
  g_return_val_if_fail (css_name != NULL, NULL);

  return g_object_new (GTK_TYPE_LIST_ITEM_WIDGET,
                       "css-name", css_name,
                       "factory", factory,
                       NULL);
}

void
gtk_list_item_widget_update (GtkListItemWidget *self,
                             guint              position,
                             gpointer           item,
                             gboolean           selected)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  if (priv->list_item)
    gtk_list_item_factory_update (priv->factory, self, position, item, selected);
  else
    gtk_list_item_widget_default_update (self, NULL, position, item, selected);

  if (selected)
    gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED);
}

void
gtk_list_item_widget_default_setup (GtkListItemWidget *self,
                                    GtkListItem       *list_item)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  priv->list_item = list_item;
  list_item->owner = self;

  if (list_item->child)
    gtk_list_item_widget_add_child (self, list_item->child);

  if (priv->item)
    g_object_notify (G_OBJECT (list_item), "item");
  if (priv->position != GTK_INVALID_LIST_POSITION)
    g_object_notify (G_OBJECT (list_item), "position");
  if (priv->selected)
    g_object_notify (G_OBJECT (list_item), "selected");
}

void
gtk_list_item_widget_default_teardown (GtkListItemWidget *self,
                                       GtkListItem       *list_item)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  g_assert (priv->list_item == list_item);

  priv->list_item = NULL;
  list_item->owner = NULL;

  if (list_item->child)
    gtk_list_item_widget_remove_child (self, list_item->child);

  if (priv->item)
    g_object_notify (G_OBJECT (list_item), "item");
  if (priv->position != GTK_INVALID_LIST_POSITION)
    g_object_notify (G_OBJECT (list_item), "position");
  if (priv->selected)
    g_object_notify (G_OBJECT (list_item), "selected");
}

void
gtk_list_item_widget_default_update (GtkListItemWidget *self,
                                     GtkListItem       *list_item,
                                     guint              position,
                                     gpointer           item,
                                     gboolean           selected)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  /* FIXME: It's kinda evil to notify external objects from here... */
  
  if (g_set_object (&priv->item, item))
    {
      if (list_item)
        g_object_notify (G_OBJECT (list_item), "item");
    }

  if (priv->position != position)
    {
      priv->position = position;
      if (list_item)
        g_object_notify (G_OBJECT (list_item), "position");
    }

  if (priv->selected != selected)
    {
      priv->selected = selected;
      if (list_item)
        g_object_notify (G_OBJECT (list_item), "selected");
    }
}

void
gtk_list_item_widget_set_factory (GtkListItemWidget  *self,
                                  GtkListItemFactory *factory)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  if (priv->factory == factory)
    return;

  if (priv->factory)
    {
      if (priv->list_item)
        gtk_list_item_factory_teardown (factory, self);
      g_clear_object (&priv->factory);
    }

  if (factory)
    {
      priv->factory = g_object_ref (factory);

      if (gtk_widget_get_root (GTK_WIDGET (self)))
        gtk_list_item_factory_setup (factory, self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

void
gtk_list_item_widget_set_single_click_activate (GtkListItemWidget *self,
                                                gboolean           single_click_activate)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  if (priv->single_click_activate == single_click_activate)
    return;

  priv->single_click_activate = single_click_activate;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SINGLE_CLICK_ACTIVATE]);
}

void
gtk_list_item_widget_add_child (GtkListItemWidget *self,
                                GtkWidget         *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}

void
gtk_list_item_widget_remove_child (GtkListItemWidget *self,
                                   GtkWidget         *child)
{
  gtk_widget_unparent (child);
}

GtkListItem *
gtk_list_item_widget_get_list_item (GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  return priv->list_item;
}

guint
gtk_list_item_widget_get_position (GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  return priv->position;
}

gpointer
gtk_list_item_widget_get_item (GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  return priv->item;
}

gboolean
gtk_list_item_widget_get_selected (GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  return priv->selected;
}

