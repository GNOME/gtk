/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "gtkselectionfiltermodel.h"
#include "gtkbitset.h"

#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkselectionfiltermodel
 * @title: GtkSelectionFilterModel
 * @short_description: A list model that turns a selection in a model
 * @see_also: #GtkSelectionModel
 *
 * #GtkSelectionFilterModel is a list model that presents the
 * selected items in a #GtkSelectionModel as its own list model.
 */

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_MODEL,
  NUM_PROPERTIES
};

struct _GtkSelectionFilterModel
{
  GObject parent_instance;

  GType item_type;
  GtkSelectionModel *model;
  GtkBitset *selection;
};

struct _GtkSelectionFilterModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_selection_filter_model_get_item_type (GListModel *list)
{
  GtkSelectionFilterModel *self = GTK_SELECTION_FILTER_MODEL (list);

  return self->item_type;
}

static guint
gtk_selection_filter_model_get_n_items (GListModel *list)
{
  GtkSelectionFilterModel *self = GTK_SELECTION_FILTER_MODEL (list);

  if (self->selection)
    return gtk_bitset_get_size (self->selection);

  return 0;
}

static gpointer
gtk_selection_filter_model_get_item (GListModel *list,
                                     guint       position)
{
  GtkSelectionFilterModel *self = GTK_SELECTION_FILTER_MODEL (list);

  position = gtk_bitset_get_nth (self->selection, position);

  return g_list_model_get_item (G_LIST_MODEL (self->model), position);
}

static void
gtk_selection_filter_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_selection_filter_model_get_item_type;
  iface->get_n_items = gtk_selection_filter_model_get_n_items;
  iface->get_item = gtk_selection_filter_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkSelectionFilterModel, gtk_selection_filter_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_selection_filter_model_list_model_init))

static void
selection_filter_model_items_changed (GtkSelectionFilterModel *self,
                                      guint                    position,
                                      guint                    removed,
                                      guint                    added)
{
  GtkBitset *selection;
  guint sel_position = 0;
  guint sel_removed = 0;
  guint sel_added = 0;

  selection = gtk_selection_model_get_selection (self->model);

  if (position > 0)
    sel_position = gtk_bitset_get_size_in_range (self->selection, 0, position - 1);

  if (removed > 0)
    sel_removed = gtk_bitset_get_size_in_range (self->selection, position, position + removed - 1);

  if (added > 0)
    sel_added = gtk_bitset_get_size_in_range (selection, position, position + added - 1);

  gtk_bitset_unref (self->selection);
  self->selection = gtk_bitset_copy (selection);

  gtk_bitset_unref (selection);

  if (sel_removed > 0 || sel_added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), sel_position, sel_removed, sel_added);
}

static void
gtk_selection_filter_model_items_changed_cb (GListModel              *model,
                                             guint                    position,
                                             guint                    removed,
                                             guint                    added,
                                             GtkSelectionFilterModel *self)
{
  selection_filter_model_items_changed (self, position, removed, added);
}

static void
gtk_selection_filter_model_selection_changed_cb (GListModel              *model,
                                                 guint                    position,
                                                 guint                    n_items,
                                                 GtkSelectionFilterModel *self)
{
  selection_filter_model_items_changed (self, position, n_items, n_items);
}

static void
gtk_selection_filter_model_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GtkSelectionFilterModel *self = GTK_SELECTION_FILTER_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      self->item_type = g_value_get_gtype (value);
      break;

    case PROP_MODEL:
      gtk_selection_filter_model_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_selection_filter_model_get_property (GObject     *object,
                                         guint        prop_id,
                                         GValue      *value,
                                         GParamSpec  *pspec)
{
  GtkSelectionFilterModel *self = GTK_SELECTION_FILTER_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, self->item_type);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_selection_filter_model_clear_model (GtkSelectionFilterModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_selection_filter_model_items_changed_cb, self);
  g_signal_handlers_disconnect_by_func (self->model, gtk_selection_filter_model_selection_changed_cb, self);

  g_clear_object (&self->model);
  g_clear_pointer (&self->selection, gtk_bitset_unref);
}

static void
gtk_selection_filter_model_dispose (GObject *object)
{
  GtkSelectionFilterModel *self = GTK_SELECTION_FILTER_MODEL (object);

  gtk_selection_filter_model_clear_model (self);

  G_OBJECT_CLASS (gtk_selection_filter_model_parent_class)->dispose (object);
}

static void
gtk_selection_filter_model_class_init (GtkSelectionFilterModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_selection_filter_model_set_property;
  gobject_class->get_property = gtk_selection_filter_model_get_property;
  gobject_class->dispose = gtk_selection_filter_model_dispose;

  /**
   * GtkSelectionFilterModel:item-type:
   *
   * The #GType for elements of this object
   */
  properties[PROP_ITEM_TYPE] =
      g_param_spec_gtype ("item-type",
                          P_("Item type"),
                          P_("The type of elements of this object"),
                          G_TYPE_OBJECT,
                          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSelectionFilterModel:model:
   *
   * The model being filtered
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("The model being filtered"),
                           GTK_TYPE_SELECTION_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_selection_filter_model_init (GtkSelectionFilterModel *self)
{
}

/**
 * gtk_selection_filter_model_new:
 * @model: the selection model to filter
 *
 * Creates a new #GtkSelectionFilterModel that will include the
 * selected items from the underlying selection model.
 *
 * Returns: a new #GtkSelectionFilterModel
 **/
GtkSelectionFilterModel *
gtk_selection_filter_model_new (GtkSelectionModel *model)
{
  GtkSelectionFilterModel *result;

  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), NULL);

  result = g_object_new (GTK_TYPE_SELECTION_FILTER_MODEL,
                         "item-type", g_list_model_get_item_type (G_LIST_MODEL (model)),
                         "model", model,
                         NULL);

  return result;
}

/**
 * gtk_selection_filter_model_new_for_type:
 * @item_type: the type of the items that will be returned
 *
 * Creates a new empty selection filter model set up to return items
 * of type @item_type. It is up to the application to set a proper
 * selection model to ensure the item type is matched.
 *
 * Returns: a new #GtkSelectionFilterModel
 **/
GtkSelectionFilterModel *
gtk_selection_filter_model_new_for_type (GType item_type)
{
  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);

  return g_object_new (GTK_TYPE_SELECTION_FILTER_MODEL,
                       "item-type", item_type,
                       NULL);
}

/**
 * gtk_selection_filter_model_set_model:
 * @self: a #GtkSelectionFilterModel
 * @model: (allow-none): The model to be filtered
 *
 * Sets the model to be filtered.
 *
 * Note that GTK makes no effort to ensure that @model conforms to
 * the item type of @self. It assumes that the caller knows what they
 * are doing and have set up an appropriate filter to ensure that item
 * types match.
 **/
void
gtk_selection_filter_model_set_model (GtkSelectionFilterModel *self,
                                      GtkSelectionModel       *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_SELECTION_FILTER_MODEL (self));
  g_return_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model));
  g_return_if_fail (model == NULL || g_type_is_a (g_list_model_get_item_type (G_LIST_MODEL (model)),
                                                  self->item_type));

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_selection_filter_model_clear_model (self);

  if (model)
    {
      GtkBitset *selection;

      self->model = g_object_ref (model);

      selection = gtk_selection_model_get_selection (self->model);
      self->selection = gtk_bitset_copy (selection);
      gtk_bitset_unref (selection);

      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_selection_filter_model_items_changed_cb), self);
      g_signal_connect (model, "selection-changed", G_CALLBACK (gtk_selection_filter_model_selection_changed_cb), self);
    }

  added = g_list_model_get_n_items (G_LIST_MODEL (self));

  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_selection_filter_model_get_model:
 * @self: a #GtkSelectionFilterModel
 *
 * Gets the model currently filtered or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model that gets filtered
 **/
GtkSelectionModel *
gtk_selection_filter_model_get_model (GtkSelectionFilterModel *self)
{
  g_return_val_if_fail (GTK_IS_SELECTION_FILTER_MODEL (self), NULL);

  return self->model;
}
