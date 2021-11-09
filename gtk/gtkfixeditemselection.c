/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkfixeditemselection.h"

#include "gtkbitset.h"
#include "gtkintl.h"
#include "gtkselectionmodel.h"

/**
 * GtkFixedItemSelection:
 *
 * `GtkFixedItemSelection` is a `GtkSelectionModel` that displays a single
 * fixed item as selected. The fixed item can be any item or `%NULL` and does
 * not need to be part of the list. In that case, no item will be displayed
 * as selected.
 *
 * The item can only be cahnged via application code, for example with
 * `gtk_fixed_item_selection_set_selected_item()`. It can not be changed via the
 * `GtkSelectionModel` APIs.
 *
 * This model was primarily designed for use in sidebars that allow selecting
 * a single item for display in the main view, but can be modified by filtering
 * the displayed list or expanding/collapsing certain parts without changing the
 * main view.
 *
 * Since: 4.6
 */
struct _GtkFixedItemSelection
{
  GObject parent_instance;

  GListModel *model;
  GObject *item;
  guint item_position;
};

struct _GtkFixedItemSelectionClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_SELECTED_ITEM,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_fixed_item_selection_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_fixed_item_selection_get_n_items (GListModel *list)
{
  GtkFixedItemSelection *self = GTK_FIXED_ITEM_SELECTION (list);

  if (self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_fixed_item_selection_get_item (GListModel *list,
                                   guint       position)
{
  GtkFixedItemSelection *self = GTK_FIXED_ITEM_SELECTION (list);

  if (self->model == NULL)
    return NULL;

  return g_list_model_get_item (self->model, position);
}

static void
gtk_fixed_item_selection_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_fixed_item_selection_get_item_type;
  iface->get_n_items = gtk_fixed_item_selection_get_n_items;
  iface->get_item = gtk_fixed_item_selection_get_item;
}

static gboolean
gtk_fixed_item_selection_is_selected (GtkSelectionModel *model,
                                      guint              position)
{
  GtkFixedItemSelection *self = GTK_FIXED_ITEM_SELECTION (model);
  
  if (self->item_position == GTK_INVALID_LIST_POSITION)
    return FALSE;

  return position == self->item_position;
}

static GtkBitset *
gtk_fixed_item_selection_get_selection_in_range (GtkSelectionModel *model,
                                                 guint              pos,
                                                 guint              n_items)
{
  GtkFixedItemSelection *self = GTK_FIXED_ITEM_SELECTION (model);
  GtkBitset *result;

  result = gtk_bitset_new_empty ();
  if (self->item_position != GTK_INVALID_LIST_POSITION)
    gtk_bitset_add (result, self->item_position);

  return result;
}

static void
gtk_fixed_item_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_fixed_item_selection_is_selected;
  iface->get_selection_in_range = gtk_fixed_item_selection_get_selection_in_range;
}

G_DEFINE_TYPE_EXTENDED (GtkFixedItemSelection, gtk_fixed_item_selection, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_fixed_item_selection_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_fixed_item_selection_selection_model_init))

static guint
gtk_fixed_item_selection_find_item_position (GtkFixedItemSelection *self,
                                             GObject *              item,
                                             guint                  start_pos,
                                             guint                  end_pos)
{
  guint pos;

  if (self->model == NULL)
    return GTK_INVALID_LIST_POSITION;

  for (pos = start_pos; pos < end_pos; pos++)
    {
      GObject *check = g_list_model_get_item (self->model, pos);

      g_object_unref (check);

      if (check == item)
        return pos;
    }

  return GTK_INVALID_LIST_POSITION;
}

static void
gtk_fixed_item_selection_items_changed_cb (GListModel            *model,
                                           guint                  position,
                                           guint                  removed,
                                           guint                  added,
                                           GtkFixedItemSelection *self)
{
  if (self->item == NULL)
    {
      /* no item to update */
    }
  else if (self->item_position == GTK_INVALID_LIST_POSITION)
    {
      /* maybe the item got newly added */
      self->item_position = gtk_fixed_item_selection_find_item_position (self, self->item, position, position + added);
    }
  else if (self->item_position < position)
    {
      /* nothing to do, position stays the same */
    }
  else if (self->item_position < position + removed)
    {
      self->item_position = gtk_fixed_item_selection_find_item_position (self, self->item, position, position + added);
    }
  else
    {
      self->item_position += added - removed;
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
gtk_fixed_item_selection_clear_model (GtkFixedItemSelection *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, 
                                        gtk_fixed_item_selection_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_fixed_item_selection_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)

{
  GtkFixedItemSelection *self = GTK_FIXED_ITEM_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_fixed_item_selection_set_model (self, g_value_get_object (value));
      break;

    case PROP_SELECTED_ITEM:
      gtk_fixed_item_selection_set_selected_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_fixed_item_selection_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkFixedItemSelection *self = GTK_FIXED_ITEM_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_SELECTED_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_fixed_item_selection_dispose (GObject *object)
{
  GtkFixedItemSelection *self = GTK_FIXED_ITEM_SELECTION (object);

  gtk_fixed_item_selection_clear_model (self);

  g_clear_object (&self->item);

  G_OBJECT_CLASS (gtk_fixed_item_selection_parent_class)->dispose (object);
}

static void
gtk_fixed_item_selection_class_init (GtkFixedItemSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_fixed_item_selection_get_property;
  gobject_class->set_property = gtk_fixed_item_selection_set_property;
  gobject_class->dispose = gtk_fixed_item_selection_dispose;

  /**
   * GtkFixedItemSelection:selected-item: (attributes org.gtk.property.get=gtk_fixed_item_selection_get_selected_item org.gtk.Property.set=gtk_fixed_item_selection_set_selected_item)
   *
   * The selected item.
   *
   * Since: 4.6
   */
  properties[PROP_SELECTED_ITEM] =
    g_param_spec_object ("selected-item",
                       P_("The selected item"),
                       P_("The selected item"),
                       G_TYPE_OBJECT,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkFixedItemSelection:model: (attributes org.gtk.property.get=gtk_fixed_item_selection_get_model org.gtk.Property.set=gtk_fixed_item_selection_set_model)
   *
   * The model being managed.
   *
   * Since: 4.6
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                       P_("The model"),
                       P_("The model being managed"),
                       G_TYPE_LIST_MODEL,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_fixed_item_selection_init (GtkFixedItemSelection *self)
{
  self->item_position = GTK_INVALID_LIST_POSITION;
}

/**
 * gtk_fixed_item_selection_new:
 * @model: (nullable) (transfer full): the `GListModel` to manage
 *
 * Creates a new selection to handle @model.
 *
 * Returns: (transfer full) (type GtkFixedItemSelection): a new `GtkFixedItemSelection`
 *
 * Since: 4.6
 */
GtkFixedItemSelection *
gtk_fixed_item_selection_new (GListModel *model)
{
  GtkFixedItemSelection *self;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);

  self = g_object_new (GTK_TYPE_FIXED_ITEM_SELECTION,
                       "model", model,
                       NULL);

  /* consume the reference */
  g_clear_object (&model);

  return self;
}

/**
 * gtk_fixed_item_selection_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkFixedItemSelection`
 *
 * Gets the model that @self is wrapping.
 *
 * Returns: (transfer none): The model being wrapped
 *
 * Since: 4.6
 */
GListModel *
gtk_fixed_item_selection_get_model (GtkFixedItemSelection *self)
{
  g_return_val_if_fail (GTK_IS_FIXED_ITEM_SELECTION (self), NULL);

  return self->model;
}

/**
 * gtk_fixed_item_selection_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkFixedItemSelection`
 * @model: (nullable): A `GListModel` to wrap
 *
 * Sets the model that @self should wrap.
 *
 * If @model is %NULL, this model will be empty.
 *
 * Since: 4.6
 */
void
gtk_fixed_item_selection_set_model (GtkFixedItemSelection *self,
                                    GListModel            *model)
{
  guint n_items_before;

  g_return_if_fail (GTK_IS_FIXED_ITEM_SELECTION (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  n_items_before = self->model ? g_list_model_get_n_items (self->model) : 0;
  gtk_fixed_item_selection_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (self->model, "items-changed",
                        G_CALLBACK (gtk_fixed_item_selection_items_changed_cb),
                        self);
    }

  g_list_model_items_changed (G_LIST_MODEL (self),
                              0,
                              n_items_before,
                              model ? g_list_model_get_n_items (self->model) : 0);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_fixed_item_selection_get_selected_item: (attributes org.gtk.Method.get_property=selected-item)
 * @self: a `GtkFixedItemSelection`
 *
 * Gets the item that is selected
 *
 * Returns: (transfer none) (type GObject) (nullable): The selected item
 *
 * Since: 4.6
 */
gpointer
gtk_fixed_item_selection_get_selected_item (GtkFixedItemSelection *self)
{
  g_return_val_if_fail (GTK_IS_FIXED_ITEM_SELECTION (self), NULL);

  return self->item;
}

static void
gtk_fixed_item_selection_set_selected_item_internal (GtkFixedItemSelection *self,
                                                     GObject               *item,
                                                     guint                  position)
{
  guint old_position;

  if (!g_set_object (&self->item, item))
    return;

  old_position = self->item_position;
  self->item_position = position;

  if (old_position == GTK_INVALID_LIST_POSITION)
    gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), position, 1);
  else if (position == GTK_INVALID_LIST_POSITION)
    gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), old_position, 1);
  else if (position < old_position)
    gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), position, old_position - position + 1);
  else
    gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), old_position, position - old_position + 1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
}

/**
 * gtk_fixed_item_selection_set_selected_item: (attributes org.gtk.Method.set_property=selected-item)
 * @self: a `GtkFixedItemSelection`
 * @item: (type GObject) (nullable): The item to display as selected
 *
 * Sets the item that @self should display as selected.
 *
 * Consider using gtk_fixed_item_selection_set_selected_position() instead, so that
 * the item's position is known in advance.
 *
 * If @item is %NULL, no item will be selected.
 *
 * Since: 4.6
 */
void
gtk_fixed_item_selection_set_selected_item (GtkFixedItemSelection *self,
                                            gpointer               item)
{
  guint position;

  g_return_if_fail (GTK_IS_FIXED_ITEM_SELECTION (self));
  g_return_if_fail (item == NULL || G_IS_OBJECT (item));

  if (self->item == item)
    return;

  if (item)
    position = gtk_fixed_item_selection_find_item_position (self, item, 0, g_list_model_get_n_items (G_LIST_MODEL (self)));
  else
    position = GTK_INVALID_LIST_POSITION;

  gtk_fixed_item_selection_set_selected_item_internal (self, item, position);
}

/**
 * gtk_fixed_item_selection_set_selected_position:
 * @self: a `GtkFixedItemSelection`
 * @position: the current position of the item to select
 *
 * Selects the item at the given position. When the list gets modified, the
 * position of the item might change.
 *
 * If the positon is larger than the number of items in the list, this function
 * selects no item.
 *
 * Since: 4.6
 **/
void
gtk_fixed_item_selection_set_selected_position (GtkFixedItemSelection  *self,
                                                guint                   position)
{
  GObject *item;

  g_return_if_fail (GTK_IS_FIXED_ITEM_SELECTION (self));

  item = g_list_model_get_item (G_LIST_MODEL (self), position);
  gtk_fixed_item_selection_set_selected_item_internal (self, item, position);
  g_clear_object (&item);
}
