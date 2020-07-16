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

#include "gtkgseqsortmodel.h"

#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtksortlistmodel
 * @title: GtkGSeqSortModel
 * @short_description: A list model that sorts its items
 * @see_also: #GListModel, #GtkSorter
 *
 * #GtkGSeqSortModel is a list model that takes a list model and
 * sorts its elements according to a #GtkSorter.
 *
 * #GtkGSeqSortModel is a generic model and because of that it
 * cannot take advantage of any external knowledge when sorting.
 * If you run into performance issues with #GtkGSeqSortModel, it
 * is strongly recommended that you write your own sorting list
 * model.
 */

enum {
  PROP_0,
  PROP_MODEL,
  PROP_SORTER,
  NUM_PROPERTIES
};

typedef struct _GtkGSeqSortEntry GtkGSeqSortEntry;

struct _GtkGSeqSortModel
{
  GObject parent_instance;

  GListModel *model;
  GtkSorter *sorter;

  GSequence *sorted; /* NULL if known unsorted */
  GSequence *unsorted; /* NULL if known unsorted */
};

struct _GtkGSeqSortModelClass
{
  GObjectClass parent_class;
};

struct _GtkGSeqSortEntry
{
  GSequenceIter *sorted_iter;
  GSequenceIter *unsorted_iter;
  gpointer item; /* holds ref */
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void
gtk_gseq_sort_entry_free (gpointer data)
{
  GtkGSeqSortEntry *entry = data;

  g_object_unref (entry->item);

  g_slice_free (GtkGSeqSortEntry, entry);
}

static GType
gtk_gseq_sort_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_gseq_sort_model_get_n_items (GListModel *list)
{
  GtkGSeqSortModel *self = GTK_GSEQ_SORT_MODEL (list);

  if (self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_gseq_sort_model_get_item (GListModel *list,
                              guint       position)
{
  GtkGSeqSortModel *self = GTK_GSEQ_SORT_MODEL (list);
  GSequenceIter *iter;
  GtkGSeqSortEntry *entry;

  if (self->model == NULL)
    return NULL;

  if (self->unsorted == NULL)
    return g_list_model_get_item (self->model, position);

  iter = g_sequence_get_iter_at_pos (self->sorted, position);
  if (g_sequence_iter_is_end (iter))
    return NULL;

  entry = g_sequence_get (iter);

  return g_object_ref (entry->item);
}

static void
gtk_gseq_sort_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_gseq_sort_model_get_item_type;
  iface->get_n_items = gtk_gseq_sort_model_get_n_items;
  iface->get_item = gtk_gseq_sort_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkGSeqSortModel, gtk_gseq_sort_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_gseq_sort_model_model_init))

static void
gtk_gseq_sort_model_remove_items (GtkGSeqSortModel *self,
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
      GtkGSeqSortEntry *entry;
      GSequenceIter *next;

      next = g_sequence_iter_next (unsorted_iter);

      entry = g_sequence_get (unsorted_iter);
      pos = g_sequence_iter_get_position (entry->sorted_iter);
      start = MIN (start, pos);
      end = MIN (end, length_before - i - 1 - pos);

      g_sequence_remove (entry->unsorted_iter);
      g_sequence_remove (entry->sorted_iter);

      unsorted_iter = next;
    }

  *unmodified_start = start;
  *unmodified_end = end;
}

static int
_sort_func (gconstpointer item1,
            gconstpointer item2,
            gpointer      data)
{
  GtkGSeqSortEntry *entry1 = (GtkGSeqSortEntry *) item1;
  GtkGSeqSortEntry *entry2 = (GtkGSeqSortEntry *) item2;
  GtkOrdering result;

  result = gtk_sorter_compare (GTK_SORTER (data), entry1->item, entry2->item);

  if (result == GTK_ORDERING_EQUAL)
    result = g_sequence_iter_compare (entry1->unsorted_iter, entry2->unsorted_iter);

  return result;
}

static void
gtk_gseq_sort_model_add_items (GtkGSeqSortModel *self,
                               guint             position,
                               guint             n_items,
                               guint            *unmodified_start,
                               guint            *unmodified_end)
{
  GSequenceIter *unsorted_end;
  guint i, pos, start, end, length_before;

  unsorted_end = g_sequence_get_iter_at_pos (self->unsorted, position);
  start = end = length_before = g_sequence_get_length (self->sorted);

  for (i = 0; i < n_items; i++)
    {
      GtkGSeqSortEntry *entry = g_slice_new0 (GtkGSeqSortEntry);

      entry->item = g_list_model_get_item (self->model, position + i);
      entry->unsorted_iter = g_sequence_insert_before (unsorted_end, entry);
      entry->sorted_iter = g_sequence_insert_sorted (self->sorted, entry, _sort_func, self->sorter);
      if (unmodified_start != NULL || unmodified_end != NULL)
        {
          pos = g_sequence_iter_get_position (entry->sorted_iter);
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
gtk_gseq_sort_model_items_changed_cb (GListModel       *model,
                                      guint             position,
                                      guint             removed,
                                      guint             added,
                                      GtkGSeqSortModel *self)
{
  guint n_items, start, end, start2, end2;

  if (removed == 0 && added == 0)
    return;

  if (self->sorted == NULL)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
      return;
    }

  gtk_gseq_sort_model_remove_items (self, position, removed, &start, &end);
  gtk_gseq_sort_model_add_items (self, position, added, &start2, &end2);
  start = MIN (start, start2);
  end = MIN (end, end2);

  n_items = g_sequence_get_length (self->sorted) - start - end;
  g_list_model_items_changed (G_LIST_MODEL (self), start, n_items - added + removed, n_items);
}

static void
gtk_gseq_sort_model_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkGSeqSortModel *self = GTK_GSEQ_SORT_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_gseq_sort_model_set_model (self, g_value_get_object (value));
      break;

    case PROP_SORTER:
      gtk_gseq_sort_model_set_sorter (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_gseq_sort_model_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  GtkGSeqSortModel *self = GTK_GSEQ_SORT_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_SORTER:
      g_value_set_object (value, self->sorter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_gseq_sort_model_clear_sequences (GtkGSeqSortModel *self)
{
  g_clear_pointer (&self->unsorted, g_sequence_free);
  g_clear_pointer (&self->sorted, g_sequence_free);
} 

static void
gtk_gseq_sort_model_create_sequences (GtkGSeqSortModel *self)
{
  if (self->sorted)
    return;

  if (self->sorter == NULL ||
      self->model == NULL ||
      gtk_sorter_get_order (self->sorter) == GTK_SORTER_ORDER_NONE)
    return;

  self->sorted = g_sequence_new (gtk_gseq_sort_entry_free);
  self->unsorted = g_sequence_new (NULL);

  gtk_gseq_sort_model_add_items (self, 0, g_list_model_get_n_items (self->model), NULL, NULL);
}

static void gtk_gseq_sort_model_resort (GtkGSeqSortModel *self);

static void
gtk_gseq_sort_model_sorter_changed_cb (GtkSorter        *sorter,
                                       int               change,
                                       GtkGSeqSortModel *self)
{
  if (gtk_sorter_get_order (sorter) == GTK_SORTER_ORDER_NONE)
    gtk_gseq_sort_model_clear_sequences (self);
  else if (self->sorted == NULL)
    {
      guint n_items = g_list_model_get_n_items (self->model);
      gtk_gseq_sort_model_create_sequences (self);
      g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);
    }
  else
    gtk_gseq_sort_model_resort (self);
}

static void
gtk_gseq_sort_model_clear_model (GtkGSeqSortModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_gseq_sort_model_items_changed_cb, self);
  g_clear_object (&self->model);
  gtk_gseq_sort_model_clear_sequences (self);
}

static void
gtk_gseq_sort_model_clear_sorter (GtkGSeqSortModel *self)
{
  if (self->sorter == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->sorter, gtk_gseq_sort_model_sorter_changed_cb, self);
  g_clear_object (&self->sorter);
  gtk_gseq_sort_model_clear_sequences (self);
}

static void
gtk_gseq_sort_model_dispose (GObject *object)
{
  GtkGSeqSortModel *self = GTK_GSEQ_SORT_MODEL (object);

  gtk_gseq_sort_model_clear_model (self);
  gtk_gseq_sort_model_clear_sorter (self);

  G_OBJECT_CLASS (gtk_gseq_sort_model_parent_class)->dispose (object);
};

static void
gtk_gseq_sort_model_class_init (GtkGSeqSortModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_gseq_sort_model_set_property;
  gobject_class->get_property = gtk_gseq_sort_model_get_property;
  gobject_class->dispose = gtk_gseq_sort_model_dispose;

  /**
   * GtkGSeqSortModel:sorter:
   *
   * The sorter for this model
   */
  properties[PROP_SORTER] =
      g_param_spec_object ("sorter",
                            P_("Sorter"),
                            P_("The sorter for this model"),
                            GTK_TYPE_SORTER,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGSeqSortModel:model:
   *
   * The model being sorted
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("The model being sorted"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_gseq_sort_model_init (GtkGSeqSortModel *self)
{
}

/**
 * gtk_gseq_sort_model_new:
 * @model: (allow-none): the model to sort
 * @sorter: (allow-none): the #GtkSorter to sort @model with
 *
 * Creates a new sort list model that uses the @sorter to sort @model.
 *
 * Returns: a new #GtkGSeqSortModel
 **/
GtkGSeqSortModel *
gtk_gseq_sort_model_new (GListModel *model,
                         GtkSorter  *sorter)
{
  GtkGSeqSortModel *result;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (sorter == NULL || GTK_IS_SORTER (sorter), NULL);

  result = g_object_new (GTK_TYPE_GSEQ_SORT_MODEL,
                         "model", model,
                         "sorter", sorter,
                         NULL);

  return result;
}

/**
 * gtk_gseq_sort_model_set_model:
 * @self: a #GtkGSeqSortModel
 * @model: (allow-none): The model to be sorted
 *
 * Sets the model to be sorted. The @model's item type must conform to
 * the item type of @self.
 **/
void
gtk_gseq_sort_model_set_model (GtkGSeqSortModel *self,
                               GListModel       *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_GSEQ_SORT_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_gseq_sort_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_gseq_sort_model_items_changed_cb), self);
      added = g_list_model_get_n_items (model);

      gtk_gseq_sort_model_create_sequences (self);
    }
  else
    added = 0;
  
  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_gseq_sort_model_get_model:
 * @self: a #GtkGSeqSortModel
 *
 * Gets the model currently sorted or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model that gets sorted
 **/
GListModel *
gtk_gseq_sort_model_get_model (GtkGSeqSortModel *self)
{
  g_return_val_if_fail (GTK_IS_GSEQ_SORT_MODEL (self), NULL);

  return self->model;
}

static void
gtk_gseq_sort_model_resort (GtkGSeqSortModel *self)
{
  guint n_items;

  g_return_if_fail (GTK_IS_GSEQ_SORT_MODEL (self));
  
  if (self->sorted == NULL)
    return;

  n_items = g_list_model_get_n_items (self->model);
  if (n_items <= 1)
    return;

  g_sequence_sort (self->sorted, _sort_func, self->sorter);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);
}

/**
 * gtk_gseq_sort_model_set_sorter:
 * @self: a #GtkGSeqSortModel
 * @sorter: (allow-none): the #GtkSorter to sort @model with
 *
 * Sets a new sorter on @self.
 */
void
gtk_gseq_sort_model_set_sorter (GtkGSeqSortModel *self,
                                GtkSorter        *sorter)
{
  guint n_items;

  g_return_if_fail (GTK_IS_GSEQ_SORT_MODEL (self));
  g_return_if_fail (sorter == NULL || GTK_IS_SORTER (sorter));

  gtk_gseq_sort_model_clear_sorter (self);

  if (sorter)
    {
      self->sorter = g_object_ref (sorter);
      g_signal_connect (sorter, "changed", G_CALLBACK (gtk_gseq_sort_model_sorter_changed_cb), self);
    }

  gtk_gseq_sort_model_create_sequences (self);
    
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  if (n_items > 1)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORTER]);
}

/**
 * gtk_gseq_sort_model_get_sorter:
 * @self: a #GtkSortLisTModel
 *
 * Gets the sorter that is used to sort @self.
 *
 * Returns: (nullable) (transfer none): the sorter of #self
 */
GtkSorter *
gtk_gseq_sort_model_get_sorter (GtkGSeqSortModel *self)
{
  g_return_val_if_fail (GTK_IS_GSEQ_SORT_MODEL (self), NULL);

  return self->sorter;
}
