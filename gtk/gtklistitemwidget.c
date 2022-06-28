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

#include "gtkbinlayout.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgestureclick.h"
#include "gtkintl.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitemprivate.h"
#include "gtklistbaseprivate.h"
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
  GtkWidget *child, *focus_child;

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

  focus_child = gtk_widget_get_focus_child (widget);
  if (focus_child && gtk_widget_child_focus (focus_child, direction))
    return TRUE;

  for (child = focus_child ? gtk_widget_get_next_sibling (focus_child)
                           : gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (gtk_widget_child_focus (child, direction))
        return TRUE;
    }

  if (focus_child)
    return FALSE;

  if (gtk_widget_is_focus (widget))
    return FALSE;

  return gtk_widget_grab_focus (widget);
}

static gboolean
gtk_list_item_widget_grab_focus (GtkWidget *widget)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (gtk_widget_grab_focus (child))
        return TRUE;
    }

  if (priv->list_item == NULL ||
      !priv->list_item->selectable)
    return FALSE;

  return GTK_WIDGET_CLASS (gtk_list_item_widget_parent_class)->grab_focus (widget);
}

static void
gtk_list_item_widget_setup_func (gpointer object,
                                 gpointer data)
{
  GtkListItemWidget *self = data;
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);
  GtkListItem *list_item = object;

  priv->list_item = list_item;
  list_item->owner = self;

  if (list_item->child)
    gtk_list_item_widget_add_child (self, list_item->child);

  gtk_list_item_widget_set_activatable (self, list_item->activatable);

  gtk_list_item_do_notify (list_item,
                           priv->item != NULL,
                           priv->position != GTK_INVALID_LIST_POSITION,
                           priv->selected);
}

static void
gtk_list_item_widget_setup_factory (GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);
  GtkListItem *list_item;

  list_item = gtk_list_item_new ();

  gtk_list_item_factory_setup (priv->factory,
                               G_OBJECT (list_item),
                               priv->item != NULL,
                               gtk_list_item_widget_setup_func,
                               self);

  g_assert (priv->list_item == list_item);
}

static void
gtk_list_item_widget_teardown_func (gpointer object,
                                    gpointer data)
{
  GtkListItemWidget *self = data;
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);
  GtkListItem *list_item = object;

  g_assert (priv->list_item == list_item);

  priv->list_item = NULL;
  list_item->owner = NULL;

  if (list_item->child)
    gtk_list_item_widget_remove_child (self, list_item->child);

  gtk_list_item_widget_set_activatable (self, FALSE);

  gtk_list_item_do_notify (list_item,
                           priv->item != NULL,
                           priv->position != GTK_INVALID_LIST_POSITION,
                           priv->selected);

  gtk_list_item_set_child (list_item, NULL);
}

static void
gtk_list_item_widget_teardown_factory (GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  gtk_list_item_factory_teardown (priv->factory,
                                  G_OBJECT (priv->list_item),
                                  priv->item != NULL,
                                  gtk_list_item_widget_teardown_func,
                                  self);

  g_assert (priv->list_item == NULL);
}

static void
gtk_list_item_widget_root (GtkWidget *widget)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_list_item_widget_parent_class)->root (widget);

  if (priv->factory)
    gtk_list_item_widget_setup_factory (self);
}

static void
gtk_list_item_widget_unroot (GtkWidget *widget)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (widget);
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_list_item_widget_parent_class)->unroot (widget);

  if (priv->list_item)
    gtk_list_item_widget_teardown_factory (self);
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

  klass->activate_signal = gtk_list_item_widget_activate_signal;

  widget_class->focus = gtk_list_item_widget_focus;
  widget_class->grab_focus = gtk_list_item_widget_grab_focus;
  widget_class->root = gtk_list_item_widget_root;
  widget_class->unroot = gtk_list_item_widget_unroot;

  gobject_class->set_property = gtk_list_item_widget_set_property;
  gobject_class->dispose = gtk_list_item_widget_dispose;

  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SINGLE_CLICK_ACTIVATE] =
    g_param_spec_boolean ("single-click-activate", NULL, NULL,
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

  gtk_widget_class_set_activate_signal (widget_class, signals[ACTIVATE_SIGNAL]);

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

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0,
                                       "activate-keybinding", 0);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter, 0,
                                       "activate-keybinding", 0);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0,
                                       "activate-keybinding", 0);

  /* note that some of these may get overwritten by child widgets,
   * such as GtkTreeExpander */
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, 0,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, GDK_CONTROL_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, GDK_SHIFT_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Space, 0,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Space, GDK_SHIFT_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Space, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
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

  if (!priv->list_item || priv->list_item->activatable)
    {
      if (n_press == 2 && !priv->single_click_activate)
        {
          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
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
gtk_list_item_widget_click_gesture_released (GtkGestureClick   *gesture,
                                             int                n_press,
                                             double             x,
                                             double             y,
                                             GtkListItemWidget *self)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  if (!priv->list_item || priv->list_item->activatable)
    {
      if (n_press == 1 && priv->single_click_activate)
        {
          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
          gtk_widget_activate_action (GTK_WIDGET (self),
                                      "list.activate-item",
                                      "u",
                                      priv->position);
          return;
        }
    }

  if (!priv->list_item || priv->list_item->selectable)
    {
      GdkModifierType state;
      GdkEvent *event;
      gboolean extend, modify;

      event = gtk_gesture_get_last_event (GTK_GESTURE (gesture),
                                          gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture)));
      state = gdk_event_get_modifier_state (event);
      extend = (state & GDK_SHIFT_MASK) != 0;
      modify = (state & GDK_CONTROL_MASK) != 0;

      gtk_widget_activate_action (GTK_WIDGET (self),
                                  "list.select-item",
                                  "(ubb)",
                                  priv->position, modify, extend);
    }

  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);
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

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

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
                          const char         *css_name,
                          GtkAccessibleRole   role)
{
  g_return_val_if_fail (css_name != NULL, NULL);

  return g_object_new (GTK_TYPE_LIST_ITEM_WIDGET,
                       "css-name", css_name,
                       "accessible-role", role,
                       "factory", factory,
                       NULL);
}

typedef struct {
  GtkListItemWidget *widget;
  guint position;
  gpointer item;
  gboolean selected;
} GtkListItemWidgetUpdate;

static void
gtk_list_item_widget_update_func (gpointer object,
                                  gpointer data)
{
  GtkListItemWidgetUpdate *update = data;
  GtkListItem *list_item = object;
  /* Track notify manually instead of freeze/thaw_notify for performance reasons. */
  gboolean notify_item = FALSE, notify_position = FALSE, notify_selected = FALSE;
  GtkListItemWidget *self = update->widget;
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);

  /* FIXME: It's kinda evil to notify external objects from here... */

  if (g_set_object (&priv->item, update->item))
    notify_item = TRUE;

  if (priv->position != update->position)
    {
      priv->position = update->position;
      notify_position = TRUE;
    }

  if (priv->selected != update->selected)
    {
      priv->selected = update->selected;
      notify_selected = TRUE;
    }

  if (list_item)
    gtk_list_item_do_notify (list_item, notify_item, notify_position, notify_selected);
}

void
gtk_list_item_widget_update (GtkListItemWidget *self,
                             guint              position,
                             gpointer           item,
                             gboolean           selected)
{
  GtkListItemWidgetPrivate *priv = gtk_list_item_widget_get_instance_private (self);
  GtkListItemWidgetUpdate update = { self, position, item, selected };
  gboolean was_selected;

  was_selected = priv->selected;

  if (priv->list_item)
    {
      gtk_list_item_factory_update (priv->factory,
                                    G_OBJECT (priv->list_item),
                                    priv->item != NULL,
                                    item != NULL,
                                    gtk_list_item_widget_update_func,
                                    &update);
    }
  else
    {
      gtk_list_item_widget_update_func (NULL, &update);
    }

  /* don't look at selected variable, it's not reentrancy safe */
  if (was_selected != priv->selected)
    {
      if (priv->selected)
        gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED, FALSE);
      else
        gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED);

      gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                   GTK_ACCESSIBLE_STATE_SELECTED, priv->selected,
                                   -1);
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
        gtk_list_item_widget_teardown_factory (self);
      g_clear_object (&priv->factory);
    }

  if (factory)
    {
      priv->factory = g_object_ref (factory);

      if (gtk_widget_get_root (GTK_WIDGET (self)))
        gtk_list_item_widget_setup_factory (self);
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
gtk_list_item_widget_set_activatable (GtkListItemWidget *self,
                                      gboolean           activatable)
{
  if (activatable)
    gtk_widget_add_css_class (GTK_WIDGET (self), "activatable");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "activatable");
}

void
gtk_list_item_widget_add_child (GtkListItemWidget *self,
                                GtkWidget         *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}

void
gtk_list_item_widget_reorder_child (GtkListItemWidget *self,
                                    GtkWidget         *child,
                                    guint              position)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *sibling = NULL;

  if (position > 0)
    {
      GtkWidget *c;
      guint i;

      for (c = gtk_widget_get_first_child (widget), i = 0;
           c;
           c = gtk_widget_get_next_sibling (c), i++)
        {
          if (i + 1 == position)
            {
              sibling = c;
              break;
            }
        }
    }

  if (child != sibling)
    gtk_widget_insert_after (child, widget, sibling);
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

