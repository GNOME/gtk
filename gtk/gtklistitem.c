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

#include "gtklistitemprivate.h"

#include "gtkbindings.h"
#include "gtkbinlayout.h"
#include "gtkcssnodeprivate.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkgestureclick.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtklistitem
 * @title: GtkListItem
 * @short_description: Object used to represent items of a ListModel
 * @see_also: #GtkListView, #GListModel
 *
 * #GtkListItem is the object that list-handling containers such
 * as #GtkListView use to represent items in a #GListModel. They are
 * managed by the container and cannot be created by application code.
 *
 * #GtkListItems need to be populated by application code. This is done by
 * calling gtk_list_item_set_child().
 *
 * #GtkListItems exist in 2 stages:
 *
 * 1. The unbound stage where the listitem is not currently connected to
 *    an item in the list. In that case, the #GtkListItem:item property is
 *    set to %NULL.
 *
 * 2. The bound stage where the listitem references an item from the list.
 *    The #GtkListItem:item property is not %NULL.
 */

struct _GtkListItem
{
  GtkWidget parent_instance;

  GObject *item;
  GtkWidget *child;
  guint position;

  guint activatable : 1;
  guint selectable : 1;
  guint selected : 1;
};

struct _GtkListItemClass
{
  GtkWidgetClass parent_class;

  void          (* activate_signal)                             (GtkListItem            *self);
};

enum
{
  PROP_0,
  PROP_ACTIVATABLE,
  PROP_CHILD,
  PROP_ITEM,
  PROP_POSITION,
  PROP_SELECTABLE,
  PROP_SELECTED,

  N_PROPS
};

enum
{
  ACTIVATE_SIGNAL,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkListItem, gtk_list_item, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_list_item_activate_signal (GtkListItem *self)
{
  if (!self->activatable)
    return;

  gtk_widget_activate_action (GTK_WIDGET (self),
                              "list.activate-item",
                              "u",
                              self->position);
}

static gboolean
gtk_list_item_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GtkListItem *self = GTK_LIST_ITEM (widget);

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
  if (self->child)
    {
      if (gtk_widget_get_focus_child (widget))
        return FALSE;
      if (gtk_widget_child_focus (self->child, direction))
        return TRUE;
    }

  if (gtk_widget_is_focus (widget))
    return FALSE;

  if (!gtk_widget_get_can_focus (widget) ||
      !self->selectable)
    return FALSE;

  return gtk_widget_grab_focus (widget);
}

static gboolean
gtk_list_item_grab_focus (GtkWidget *widget)
{
  GtkListItem *self = GTK_LIST_ITEM (widget);

  if (self->child && gtk_widget_grab_focus (self->child))
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_list_item_parent_class)->grab_focus (widget);
}

static void
gtk_list_item_dispose (GObject *object)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  g_assert (self->item == NULL);
  g_clear_pointer (&self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_list_item_parent_class)->dispose (object);
}

static void
gtk_list_item_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  switch (property_id)
    {
    case PROP_ACTIVATABLE:
      g_value_set_boolean (value, self->activatable);
      break;

    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;

    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_POSITION:
      g_value_set_uint (value, self->position);
      break;

    case PROP_SELECTABLE:
      g_value_set_boolean (value, self->selectable);
      break;

    case PROP_SELECTED:
      g_value_set_boolean (value, self->selected);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_item_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  switch (property_id)
    {
    case PROP_ACTIVATABLE:
      gtk_list_item_set_activatable (self, g_value_get_boolean (value));
      break;

    case PROP_CHILD:
      gtk_list_item_set_child (self, g_value_get_object (value));
      break;

    case PROP_SELECTABLE:
      gtk_list_item_set_selectable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_item_select_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
  GtkListItem *self = GTK_LIST_ITEM (widget);
  gboolean modify, extend;

  if (!self->selectable)
    return;

  g_variant_get (parameter, "(bb)", &modify, &extend);

  gtk_widget_activate_action (GTK_WIDGET (self),
                              "list.select-item",
                              "(ubb)",
                              self->position, modify, extend);
}

static void
gtk_list_item_class_init (GtkListItemClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  klass->activate_signal = gtk_list_item_activate_signal;

  widget_class->focus = gtk_list_item_focus;
  widget_class->grab_focus = gtk_list_item_grab_focus;

  gobject_class->dispose = gtk_list_item_dispose;
  gobject_class->get_property = gtk_list_item_get_property;
  gobject_class->set_property = gtk_list_item_set_property;

  /**
   * GtkListItem:activatable:
   *
   * If the item can be activated by the user
   */
  properties[PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable",
                          P_("Activatable"),
                          P_("If the item can be activated by the user"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:child:
   *
   * Widget used for display
   */
  properties[PROP_CHILD] =
    g_param_spec_object ("child",
                         P_("Child"),
                         P_("Widget used for display"),
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:item:
   *
   * Displayed item
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item",
                         P_("Item"),
                         P_("Displayed item"),
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:position:
   *
   * Position of the item
   */
  properties[PROP_POSITION] =
    g_param_spec_uint ("position",
                       P_("Position"),
                       P_("Position of the item"),
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:selectable:
   *
   * If the item can be selected by the user
   */
  properties[PROP_SELECTABLE] =
    g_param_spec_boolean ("selectable",
                          P_("Selectable"),
                          P_("If the item can be selected by the user"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:selected:
   *
   * If the item is currently selected
   */
  properties[PROP_SELECTED] =
    g_param_spec_boolean ("selected",
                          P_("Selected"),
                          P_("If the item is currently selected"),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkListItem::activate-signal:
   *
   * This is a keybinding signal, which will cause this row to be activated.
   *
   * Do not use it, it is an implementation detail.
   *
   * If you want to be notified when the user activates a listitem (by key or not),
   * look at the list widget this item is contained in.
   */
  signals[ACTIVATE_SIGNAL] =
    g_signal_new (I_("activate-keybinding"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListItemClass, activate_signal),
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
                                   gtk_list_item_select_action);

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

  /* This gets overwritten by gtk_list_item_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, I_("row"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_list_item_click_gesture_pressed (GtkGestureClick *gesture,
                                     int              n_press,
                                     double           x,
                                     double           y,
                                     GtkListItem     *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  if (!self->selectable && !self->activatable)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (self->selectable)
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
                                  self->position, modify, extend);
    }

  if (self->activatable)
    {
      if (n_press == 2)
        {
          gtk_widget_activate_action (GTK_WIDGET (self),
                                      "list.activate-item",
                                      "u",
                                      self->position);
        }
    }

  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_ACTIVE, FALSE);

  if (gtk_widget_get_focus_on_click (widget))
    gtk_widget_grab_focus (widget);
}

static void
gtk_list_item_enter_cb (GtkEventControllerFocus *controller,
                        GtkListItem             *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_activate_action (widget,
                              "list.scroll-to-item",
                              "u",
                              self->position);
}

static void
gtk_list_item_click_gesture_released (GtkGestureClick *gesture,
                                      int              n_press,
                                      double           x,
                                      double           y,
                                      GtkListItem     *self)
{
  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);
}

static void
gtk_list_item_click_gesture_canceled (GtkGestureClick  *gesture,
                                      GdkEventSequence *sequence,
                                      GtkListItem      *self)
{
  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);
}

static void
gtk_list_item_init (GtkListItem *self)
{
  GtkEventController *controller;
  GtkGesture *gesture;

  self->selectable = TRUE;
  self->activatable = TRUE;
  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);

  gesture = gtk_gesture_click_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture),
                                     FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
                                 GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_list_item_click_gesture_pressed), self);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_list_item_click_gesture_released), self);
  g_signal_connect (gesture, "cancel",
                    G_CALLBACK (gtk_list_item_click_gesture_canceled), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_focus_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_list_item_enter_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

GtkListItem *
gtk_list_item_new (const char *css_name)
{
  GtkListItem *result;

  g_return_val_if_fail (css_name != NULL, NULL);

  result = g_object_new (GTK_TYPE_LIST_ITEM,
                         "css-name", css_name,
                         NULL);

  return result;
}

/**
 * gtk_list_item_get_item:
 * @self: a #GtkListItem
 *
 * Gets the item that is currently displayed in model that @self is
 * currently bound to or %NULL if @self is unbound.
 *
 * Returns: (nullable) (transfer none) (type GObject): The item displayed
 **/
gpointer
gtk_list_item_get_item (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  return self->item;
}

/**
 * gtk_list_item_get_child:
 * @self: a #GtkListItem
 *
 * Gets the child previously set via gtk_list_item_set_child() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The child
 **/
GtkWidget *
gtk_list_item_get_child (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  return self->child;
}

/**
 * gtk_list_item_set_child:
 * @self: a #GtkListItem
 * @child: (nullable): The list item's child or %NULL to unset
 *
 * Sets the child to be used for this listitem.
 *
 * This function is typically called by applications when
 * setting up a listitem so that the widget can be reused when
 * binding it multiple times.
 **/
void
gtk_list_item_set_child (GtkListItem *self,
                         GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child)
    {
      gtk_widget_insert_after (child, GTK_WIDGET (self), NULL);
      self->child = child;
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

void
gtk_list_item_set_item (GtkListItem *self,
                        gpointer     item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));
  g_return_if_fail (item == NULL || G_IS_OBJECT (item));

  if (self->item == item)
    return;

  g_clear_object (&self->item);
  if (item)
    self->item = g_object_ref (item);

  gtk_css_node_invalidate (gtk_widget_get_css_node (GTK_WIDGET (self)), GTK_CSS_CHANGE_ANIMATIONS);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

/**
 * gtk_list_item_get_position:
 * @self: a #GtkListItem
 *
 * Gets the position in the model that @self currently displays.
 * If @self is unbound, 0 is returned.
 *
 * Returns: The position of this item
 **/
guint
gtk_list_item_get_position (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), 0);

  return self->position;
}

void
gtk_list_item_set_position (GtkListItem *self,
                            guint        position)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (self->position == position)
    return;

  self->position = position;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POSITION]);
}

/**
 * gtk_list_item_get_selected:
 * @self: a #GtkListItem
 *
 * Checks if the item is displayed as selected. The selected state is
 * maintained by the container and its list model and cannot be set
 * otherwise.
 *
 * Returns: %TRUE if the item is selected.
 **/
gboolean
gtk_list_item_get_selected (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  return self->selected;
}

void
gtk_list_item_set_selected (GtkListItem *self,
                            gboolean     selected)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (self->selected == selected)
    return;

  self->selected = selected;

  if (selected)
    gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
}

/**
 * gtk_list_item_get_selectable:
 * @self: a #GtkListItem
 *
 * Checks if a list item has been set to be selectable via
 * gtk_list_item_set_selectable().
 *
 * Do not confuse this function with gtk_list_item_get_selected().
 *
 * Returns: %TRUE if the item is selectable
 **/
gboolean
gtk_list_item_get_selectable (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  return self->selectable;
}

/**
 * gtk_list_item_set_selectable:
 * @self: a #GtkListItem
 * @selectable: if the item should be selectable
 *
 * Sets @self to be selectable. If an item is selectable, clicking
 * on the item or using the keyboard will try to select or unselect
 * the item. If this succeeds is up to the model to determine, as
 * it is managing the selected state.
 *
 * Note that this means that making an item non-selectable has no
 * influence on the selected state at all. A non-selectable item 
 * may still be selected.
 *
 * By default, list items are selectable. When rebinding them to
 * a new item, they will also be reset to be selectable by GTK.
 **/
void
gtk_list_item_set_selectable (GtkListItem *self,
                              gboolean     selectable)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (self->selectable == selectable)
    return;

  self->selectable = selectable;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTABLE]);
}

/**
 * gtk_list_item_get_activatable:
 * @self: a #GtkListItem
 *
 * Checks if a list item has been set to be activatable via
 * gtk_list_item_set_activatable().
 *
 * Returns: %TRUE if the item is activatable
 **/
gboolean
gtk_list_item_get_activatable (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  return self->activatable;
}

/**
 * gtk_list_item_set_activatable:
 * @self: a #GtkListItem
 * @activatable: if the item should be activatable
 *
 * Sets @self to be activatable.
 *
 * If an item is activatable, double-clicking on the item, using
 * the <Return> key or calling gtk_widget_activate() will activate
 * the item. Activating instructs the containing view to handle
 * activation. #GtkListView for example will be emitting the
 * GtkListView::activate signal.
 *
 * By default, list items are activatable
 **/
void
gtk_list_item_set_activatable (GtkListItem *self,
                               gboolean     activatable)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (self->activatable == activatable)
    return;

  self->activatable = activatable;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVATABLE]);
}
