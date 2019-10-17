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

#include "gtksortlistmodel.h"

#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtksortlistmodel
 * @title: GtkSortListModel
 * @short_description: A list model that sorts its items
 * @see_also: #GListModel
 *
 * #GtkSortListModel is a list model that takes a list model and
 * sorts its elements according to a compare function.
 *
 * #GtkSortListModel is a generic model and because of that it
 * cannot take advantage of any external knowledge when sorting.
 * If you run into performance issues with #GtkSortListModel, it
 * is strongly recommended that you write your own sorting list
 * model.
 */

enum {
  PROP_0,
  PROP_HAS_SORT,
  PROP_ITEM_TYPE,
  PROP_MODEL,
  NUM_PROPERTIES
};

struct _GtkSortListModel
{
  GObject parent_instance;

  GType item_type;
  GListModel *model;
  GCompareDataFunc sort_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  GSequence *sorted; /* NULL if sort_func == NULL */
  GSequence *unsorted; /* NULL if sort_func == NULL */
};

struct _GtkSortListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_sort_list_model_get_item_type (GListModel *list)
{
  GtkSortListModel *self = GTK_SORT_LIST_MODEL (list);

  return self->item_type;
}

static guint
gtk_sort_list_model_get_n_items (GListModel *list)
{
  GtkSortListModel *self = GTK_SORT_LIST_MODEL (list);

  if (self->model == NULL)
    return 0;

  if (self->sorted)
    return g_sequence_get_length (self->sorted);

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_sort_list_model_get_item (GListModel *list,
                              guint       position)
{
  GtkSortListModel *self = GTK_SORT_LIST_MODEL (list);
  GSequenceIter *iter;

  if (self->model == NULL)
    return NULL;

  if (self->unsorted == NULL)
    return g_list_model_get_item (self->model, position);

  iter = g_sequence_get_iter_at_pos (self->sorted, position);
  if (g_sequence_iter_is_end (iter))
      return NULL;

  return g_object_ref (g_sequence_get (iter));
}

static void
gtk_sort_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_sort_list_model_get_item_type;
  iface->get_n_items = gtk_sort_list_model_get_n_items;
  iface->get_item = gtk_sort_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkSortListModel, gtk_sort_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_sort_list_model_model_init))

static void
gtk_sort_list_model_remove_items (GtkSortListModel *self,
                                  guint             position,
                                  guint             n_items,
                                  guint            *unmodified_start,
                                  guint            *unmodified_end)
{
  GSequenceIter *unsorted_iter;
  guint i, pos, start, end, length_before;

  start = end = length_before = g_sequence_get_length (self->sorted);
  unsorted_iter = g_sequence_get_iter_at_pos (self->unsorted, position);

  for (i = 0; i < n_items ; i++)
    {
      GSequenceIter *sorted_iter;
      GSequenceIter *next;

      next = g_sequence_iter_next (unsorted_iter);

      sorted_iter = g_sequence_get (unsorted_iter);
      pos = g_sequence_iter_get_position (sorted_iter);
      start = MIN (start, pos);
      end = MIN (end, length_before - i - 1 - pos);

      g_sequence_remove (sorted_iter);
      g_sequence_remove (unsorted_iter);

      unsorted_iter = next;
    }

  *unmodified_start = start;
  *unmodified_end = end;
}

static void
gtk_sort_list_model_add_items (GtkSortListModel *self,
                               guint             position,
                               guint             n_items,
                               guint            *unmodified_start,
                               guint            *unmodified_end)
{
  GSequenceIter *unsorted_iter, *sorted_iter;
  guint i, pos, start, end, length_before;

  unsorted_iter = g_sequence_get_iter_at_pos (self->unsorted, position);
  start = end = length_before = g_sequence_get_length (self->sorted);

  for (i = 0; i < n_items; i++)
    {
      gpointer item = g_list_model_get_item (self->model, position + i);
      sorted_iter = g_sequence_insert_sorted (self->sorted, item, self->sort_func, self->user_data);
      g_sequence_insert_before (unsorted_iter, sorted_iter);
      if (unmodified_start != NULL || unmodified_end != NULL)
        {
          pos = g_sequence_iter_get_position (sorted_iter);
          start = MIN (start, pos);
          end = MIN (end, length_before + i - pos);
        }
    }

  if (unmodified_start)
    *unmodified_start = start;
  if (unmodified_end)
    *unmodified_end = end;
}

static void
gtk_sort_list_model_items_changed_cb (GListModel       *model,
                                      guint             position,
                                      guint             removed,
                                      guint             added,
                                      GtkSortListModel *self)
{
  guint n_items, start, end, start2, end2;

  if (removed == 0 && added == 0)
    return;

  if (self->sorted == NULL)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
      return;
    }

  gtk_sort_list_model_remove_items (self, position, removed, &start, &end);
  gtk_sort_list_model_add_items (self, position, added, &start2, &end2);
  start = MIN (start, start2);
  end = MIN (end, end2);

  n_items = g_sequence_get_length (self->sorted) - start - end;
  g_list_model_items_changed (G_LIST_MODEL (self), start, n_items - added + removed, n_items);
}

static void
gtk_sort_list_model_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkSortListModel *self = GTK_SORT_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      self->item_type = g_value_get_gtype (value);
      break;

    case PROP_MODEL:
      gtk_sort_list_model_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_sort_list_model_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  GtkSortListModel *self = GTK_SORT_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_HAS_SORT:
      g_value_set_boolean (value, self->sort_func != NULL);
      break;

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
gtk_sort_list_model_clear_model (GtkSortListModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_sort_list_model_items_changed_cb, self);
  g_clear_object (&self->model);
  g_clear_pointer (&self->sorted, g_sequence_free);
  g_clear_pointer (&self->unsorted, g_sequence_free);
}

static void
gtk_sort_list_model_dispose (GObject *object)
{
  GtkSortListModel *self = GTK_SORT_LIST_MODEL (object);

  gtk_sort_list_model_clear_model (self);
  if (self->user_destroy)
    self->user_destroy (self->user_data);
  self->sort_func = NULL;
  self->user_data = NULL;
  self->user_destroy = NULL;

  G_OBJECT_CLASS (gtk_sort_list_model_parent_class)->dispose (object);
};

static void
gtk_sort_list_model_class_init (GtkSortListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_sort_list_model_set_property;
  gobject_class->get_property = gtk_sort_list_model_get_property;
  gobject_class->dispose = gtk_sort_list_model_dispose;

  /**
   * GtkSortListModel:has-sort:
   *
   * If a sort function is set for this model
   */
  properties[PROP_HAS_SORT] =
      g_param_spec_boolean ("has-sort",
                            P_("has sort"),
                            P_("If a sort function is set for this model"),
                            FALSE,
                            GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSortListModel:item-type:
   *
   * The #GType for items of this model
   */
  properties[PROP_ITEM_TYPE] =
      g_param_spec_gtype ("item-type",
                          P_("Item type"),
                          P_("The type of items of this list"),
                          G_TYPE_OBJECT,
                          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSortListModel:model:
   *
   * The model being sorted
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("The model being sorted"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_sort_list_model_init (GtkSortListModel *self)
{
}


/**
 * gtk_sort_list_model_new:
 * @model: the model to sort
 * @sort_func: (allow-none): sort function or %NULL to not sort items
 * @user_data: user data passed to @sort_func
 * @user_destroy: destroy notifier for @user_data
 *
 * Creates a new sort list model that uses the @sort_func to sort @model.
 *
 * Returns: a new #GtkSortListModel
 **/
GtkSortListModel *
gtk_sort_list_model_new (GListModel       *model,
                         GCompareDataFunc  sort_func,
                         gpointer          user_data,
                         GDestroyNotify    user_destroy)
{
  GtkSortListModel *result;

  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  result = g_object_new (GTK_TYPE_SORT_LIST_MODEL,
                         "item-type", g_list_model_get_item_type (model),
                         "model", model,
                         NULL);

  if (sort_func)
    gtk_sort_list_model_set_sort_func (result, sort_func, user_data, user_destroy);

  return result;
}

/**
 * gtk_sort_list_model_new_for_type:
 * @item_type: the type of the items that will be returned
 *
 * Creates a new empty sort list model set up to return items of type @item_type.
 * It is up to the application to set a proper sort function and model to ensure
 * the item type is matched.
 *
 * Returns: a new #GtkSortListModel
 **/
GtkSortListModel *
gtk_sort_list_model_new_for_type (GType item_type)
{
  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);

  return g_object_new (GTK_TYPE_SORT_LIST_MODEL,
                       "item-type", item_type,
                       NULL);
}

static void
gtk_sort_list_model_create_sequences (GtkSortListModel *self)
{
  if (!self->sort_func || self->model == NULL)
    return;

  self->sorted = g_sequence_new (g_object_unref);
  self->unsorted = g_sequence_new (NULL);

  gtk_sort_list_model_add_items (self, 0, g_list_model_get_n_items (self->model), NULL, NULL);
}

/**
 * gtk_sort_list_model_set_sort_func:
 * @self: a #GtkSortListModel
 * @sort_func: (allow-none): sort function or %NULL to not sort items
 * @user_data: user data passed to @sort_func
 * @user_destroy: destroy notifier for @user_data
 *
 * Sets the function used to sort items. The function will be called for every
 * item and must return an integer less than, equal to, or greater than zero if
 * for two items from the model if the first item is considered to be respectively
 * less than, equal to, or greater than the second.
 **/
void
gtk_sort_list_model_set_sort_func (GtkSortListModel *self,
                                   GCompareDataFunc  sort_func,
                                   gpointer          user_data,
                                   GDestroyNotify    user_destroy)
{
  guint n_items;

  g_return_if_fail (GTK_IS_SORT_LIST_MODEL (self));
  g_return_if_fail (sort_func != NULL || (user_data == NULL && !user_destroy));

  if (!sort_func && !self->sort_func)
    return;

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  g_clear_pointer (&self->unsorted, g_sequence_free);
  g_clear_pointer (&self->sorted, g_sequence_free);
  self->sort_func = sort_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;
  
    gtk_sort_list_model_create_sequences (self);
    
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  if (n_items > 1)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_SORT]);
}

/**
 * gtk_sort_list_model_set_model:
 * @self: a #GtkSortListModel
 * @model: (allow-none): The model to be sorted
 *
 * Sets the model to be sorted. The @model's item type must conform to
 * the item type of @self.
 **/
void
gtk_sort_list_model_set_model (GtkSortListModel *self,
                               GListModel       *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_SORT_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));
  if (model)
    {
      g_return_if_fail (g_type_is_a (g_list_model_get_item_type (model), self->item_type));
    }

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_sort_list_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_sort_list_model_items_changed_cb), self);
      added = g_list_model_get_n_items (model);

      gtk_sort_list_model_create_sequences (self);
    }
  else
    added = 0;
  
  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_sort_list_model_get_model:
 * @self: a #GtkSortListModel
 *
 * Gets the model currently sorted or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model that gets sorted
 **/
GListModel *
gtk_sort_list_model_get_model (GtkSortListModel *self)
{
  g_return_val_if_fail (GTK_IS_SORT_LIST_MODEL (self), NULL);

  return self->model;
}

/**
 * gtk_sort_list_model_has_sort:
 * @self: a #GtkSortListModel
 *
 * Checks if a sort function is currently set on @self
 *
 * Returns: %TRUE if a sort function is set
 **/
gboolean
gtk_sort_list_model_has_sort (GtkSortListModel *self)
{
  g_return_val_if_fail (GTK_IS_SORT_LIST_MODEL (self), FALSE);

  return self->sort_func != NULL;
}

/**
 * gtk_sort_list_model_resort:
 * @self: a #GtkSortListModel
 *
 * Causes @self to resort all items in the model.
 *
 * Calling this function is necessary when data used by the sort
 * function has changed.
 **/
void
gtk_sort_list_model_resort (GtkSortListModel *self)
{
  guint n_items;

  g_return_if_fail (GTK_IS_SORT_LIST_MODEL (self));
  
  if (self->sorted == NULL)
    return;

  n_items = g_list_model_get_n_items (self->model);
  if (n_items <= 1)
    return;

  g_sequence_sort (self->sorted, self->sort_func, self->user_data);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);
}

