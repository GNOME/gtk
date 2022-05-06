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

#include "gtkintl.h"

/**
 * GtkListItem:
 *
 * `GtkListItem` is used by list widgets to represent items in a `GListModel`.
 *
 * The `GtkListItem`s are managed by the list widget (with its factory)
 * and cannot be created by applications, but they need to be populated
 * by application code. This is done by calling [method@Gtk.ListItem.set_child].
 *
 * `GtkListItem`s exist in 2 stages:
 *
 * 1. The unbound stage where the listitem is not currently connected to
 *    an item in the list. In that case, the [property@Gtk.ListItem:item]
 *    property is set to %NULL.
 *
 * 2. The bound stage where the listitem references an item from the list.
 *    The [property@Gtk.ListItem:item] property is not %NULL.
 */

struct _GtkListItemClass
{
  GObjectClass parent_class;
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

G_DEFINE_TYPE (GtkListItem, gtk_list_item, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_list_item_dispose (GObject *object)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  g_assert (self->owner == NULL); /* would hold a reference */
  g_clear_object (&self->child);

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
      if (self->owner)
        g_value_set_object (value, gtk_list_item_widget_get_item (self->owner));
      break;

    case PROP_POSITION:
      if (self->owner)
        g_value_set_uint (value, gtk_list_item_widget_get_position (self->owner));
      else
        g_value_set_uint (value, GTK_INVALID_LIST_POSITION);
      break;

    case PROP_SELECTABLE:
      g_value_set_boolean (value, self->selectable);
      break;

    case PROP_SELECTED:
      if (self->owner)
        g_value_set_boolean (value, gtk_list_item_widget_get_selected (self->owner));
      else
        g_value_set_boolean (value, FALSE);
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
gtk_list_item_class_init (GtkListItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_list_item_dispose;
  gobject_class->get_property = gtk_list_item_get_property;
  gobject_class->set_property = gtk_list_item_set_property;

  /**
   * GtkListItem:activatable: (attributes org.gtk.Property.get=gtk_list_item_get_activatable org.gtk.Property.set=gtk_list_item_set_activatable)
   *
   * If the item can be activated by the user.
   */
  properties[PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable",
                          P_("Activatable"),
                          P_("If the item can be activated by the user"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:child: (attributes org.gtk.Property.get=gtk_list_item_get_child org.gtk.Property.set=gtk_list_item_set_child)
   *
   * Widget used for display.
   */
  properties[PROP_CHILD] =
    g_param_spec_object ("child",
                         P_("Child"),
                         P_("Widget used for display"),
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:item: (attributes org.gtk.Property.get=gtk_list_item_get_item)
   *
   * Displayed item.
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item",
                         P_("Item"),
                         P_("Displayed item"),
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:position: (attributes org.gtk.Property.get=gtk_list_item_get_position)
   *
   * Position of the item.
   */
  properties[PROP_POSITION] =
    g_param_spec_uint ("position",
                       P_("Position"),
                       P_("Position of the item"),
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:selectable: (attributes org.gtk.Property.get=gtk_list_item_get_selectable org.gtk.Property.set=gtk_list_item_set_selectable)
   *
   * If the item can be selected by the user.
   */
  properties[PROP_SELECTABLE] =
    g_param_spec_boolean ("selectable",
                          P_("Selectable"),
                          P_("If the item can be selected by the user"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:selected: (attributes org.gtk.Property.get=gtk_list_item_get_selected)
   *
   * If the item is currently selected.
   */
  properties[PROP_SELECTED] =
    g_param_spec_boolean ("selected",
                          P_("Selected"),
                          P_("If the item is currently selected"),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_list_item_init (GtkListItem *self)
{
  self->selectable = TRUE;
  self->activatable = TRUE;
}

GtkListItem *
gtk_list_item_new (void)
{
  return g_object_new (GTK_TYPE_LIST_ITEM, NULL);
}

void
gtk_list_item_do_notify (GtkListItem *list_item,
                         gboolean notify_item,
                         gboolean notify_position,
                         gboolean notify_selected)
{
  GObject *object = G_OBJECT (list_item);

  if (notify_item)
    g_object_notify_by_pspec (object, properties[PROP_ITEM]);
  if (notify_position)
    g_object_notify_by_pspec (object, properties[PROP_POSITION]);
  if (notify_selected)
    g_object_notify_by_pspec (object, properties[PROP_SELECTED]);
}

/**
 * gtk_list_item_get_item: (attributes org.gtk.Method.get_property=item)
 * @self: a `GtkListItem`
 *
 * Gets the model item that associated with @self.
 *
 * If @self is unbound, this function returns %NULL.
 *
 * Returns: (nullable) (transfer none) (type GObject): The item displayed
 **/
gpointer
gtk_list_item_get_item (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  if (self->owner == NULL)
    return NULL;

  return gtk_list_item_widget_get_item (self->owner);
}

/**
 * gtk_list_item_get_child: (attributes org.gtk.Method.get_property=child)
 * @self: a `GtkListItem`
 *
 * Gets the child previously set via gtk_list_item_set_child() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The child
 */
GtkWidget *
gtk_list_item_get_child (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  return self->child;
}

/**
 * gtk_list_item_set_child: (attributes org.gtk.Method.set_property=child)
 * @self: a `GtkListItem`
 * @child: (nullable): The list item's child or %NULL to unset
 *
 * Sets the child to be used for this listitem.
 *
 * This function is typically called by applications when
 * setting up a listitem so that the widget can be reused when
 * binding it multiple times.
 */
void
gtk_list_item_set_child (GtkListItem *self,
                         GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  if (self->child && self->owner)
    gtk_list_item_widget_remove_child (self->owner, self->child);

  g_clear_object (&self->child);

  if (child)
    {
      g_object_ref_sink (child);
      self->child = child;

      if (self->owner)
        gtk_list_item_widget_add_child (self->owner, child);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

/**
 * gtk_list_item_get_position: (attributes org.gtk.Method.get_property=position)
 * @self: a `GtkListItem`
 *
 * Gets the position in the model that @self currently displays.
 *
 * If @self is unbound, %GTK_INVALID_LIST_POSITION is returned.
 *
 * Returns: The position of this item
 */
guint
gtk_list_item_get_position (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), GTK_INVALID_LIST_POSITION);

  if (self->owner == NULL)
    return GTK_INVALID_LIST_POSITION;

  return gtk_list_item_widget_get_position (self->owner);
}

/**
 * gtk_list_item_get_selected: (attributes org.gtk.Method.get_property=selected)
 * @self: a `GtkListItem`
 *
 * Checks if the item is displayed as selected.
 *
 * The selected state is maintained by the liste widget and its model
 * and cannot be set otherwise.
 *
 * Returns: %TRUE if the item is selected.
 */
gboolean
gtk_list_item_get_selected (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  if (self->owner == NULL)
    return FALSE;

  return gtk_list_item_widget_get_selected (self->owner);
}

/**
 * gtk_list_item_get_selectable: (attributes org.gtk.Method.get_property=selectable)
 * @self: a `GtkListItem`
 *
 * Checks if a list item has been set to be selectable via
 * gtk_list_item_set_selectable().
 *
 * Do not confuse this function with [method@Gtk.ListItem.get_selected].
 *
 * Returns: %TRUE if the item is selectable
 */
gboolean
gtk_list_item_get_selectable (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  return self->selectable;
}

/**
 * gtk_list_item_set_selectable: (attributes org.gtk.Method.set_property=selectable)
 * @self: a `GtkListItem`
 * @selectable: if the item should be selectable
 *
 * Sets @self to be selectable.
 *
 * If an item is selectable, clicking on the item or using the keyboard
 * will try to select or unselect the item. If this succeeds is up to
 * the model to determine, as it is managing the selected state.
 *
 * Note that this means that making an item non-selectable has no
 * influence on the selected state at all. A non-selectable item
 * may still be selected.
 *
 * By default, list items are selectable. When rebinding them to
 * a new item, they will also be reset to be selectable by GTK.
 */
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
 * gtk_list_item_get_activatable: (attributes org.gtk.Method.get_property=activatable)
 * @self: a `GtkListItem`
 *
 * Checks if a list item has been set to be activatable via
 * gtk_list_item_set_activatable().
 *
 * Returns: %TRUE if the item is activatable
 */
gboolean
gtk_list_item_get_activatable (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  return self->activatable;
}

/**
 * gtk_list_item_set_activatable: (attributes org.gtk.Method.set_property=activatable)
 * @self: a `GtkListItem`
 * @activatable: if the item should be activatable
 *
 * Sets @self to be activatable.
 *
 * If an item is activatable, double-clicking on the item, using
 * the Return key or calling gtk_widget_activate() will activate
 * the item. Activating instructs the containing view to handle
 * activation. `GtkListView` for example will be emitting the
 * [signal@Gtk.ListView::activate] signal.
 *
 * By default, list items are activatable.
 */
void
gtk_list_item_set_activatable (GtkListItem *self,
                               gboolean     activatable)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (self->activatable == activatable)
    return;

  self->activatable = activatable;

  if (self->owner)
    gtk_list_item_widget_set_activatable (self->owner, activatable);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVATABLE]);
}
