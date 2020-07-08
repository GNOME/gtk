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

#include "gtksor4listmodel.h"

#include "gtkintl.h"
#include "gtkprivate.h"

typedef struct _SortItem SortItem;
struct _SortItem
{
  GObject *item;
  guint position;
};

static void
sort_item_clear (gpointer data)
{
  SortItem *si = data;

  g_clear_object (&si->item);
}

#define GTK_VECTOR_ELEMENT_TYPE SortItem
#define GTK_VECTOR_TYPE_NAME SortArray
#define GTK_VECTOR_NAME sort_array
#define GTK_VECTOR_FREE_FUNC sort_item_clear
#define GTK_VECTOR_BY_VALUE 1
#include "gtkvectorimpl.c"

/**
 * SECTION:gtksor4listmodel
 * @title: GtkSor4ListModel
 * @short_description: A list model that sorts its items
 * @see_also: #GListModel, #GtkSorter
 *
 * #GtkSor4ListModel is a list model that takes a list model and
 * sorts its elements according to a #GtkSorter.
 *
 * #GtkSor4ListModel is a generic model and because of that it
 * cannot take advantage of any external knowledge when sorting.
 * If you run into performance issues with #GtkSor4ListModel, it
 * is strongly recommended that you write your own sorting list
 * model.
 */

enum {
  PROP_0,
  PROP_MODEL,
  PROP_SORTER,
  NUM_PROPERTIES
};

struct _GtkSor4ListModel
{
  GObject parent_instance;

  GListModel *model;
  GtkSorter *sorter;

  SortArray items; /* empty if known unsorted */
};

struct _GtkSor4ListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_sor4_list_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_sor4_list_model_get_n_items (GListModel *list)
{
  GtkSor4ListModel *self = GTK_SOR4_LIST_MODEL (list);

  if (self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_sor4_list_model_get_item (GListModel *list,
                              guint       position)
{
  GtkSor4ListModel *self = GTK_SOR4_LIST_MODEL (list);

  if (self->model == NULL)
    return NULL;

  if (sort_array_is_empty (&self->items))
    return g_list_model_get_item (self->model, position);

  if (position >= sort_array_get_size (&self->items))
    return NULL;

  return g_object_ref (sort_array_get (&self->items, position)->item);
}

static void
gtk_sor4_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_sor4_list_model_get_item_type;
  iface->get_n_items = gtk_sor4_list_model_get_n_items;
  iface->get_item = gtk_sor4_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkSor4ListModel, gtk_sor4_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_sor4_list_model_model_init))

static void
gtk_sor4_list_model_clear_items (GtkSor4ListModel *self)
{
  sort_array_clear (&self->items);
} 

static gboolean
gtk_sor4_list_model_should_sort (GtkSor4ListModel *self)
{
  return self->sorter != NULL &&
         self->model != NULL &&
         gtk_sorter_get_order (self->sorter) != GTK_SORTER_ORDER_NONE;
}

static void
gtk_sor4_list_model_create_items (GtkSor4ListModel *self)
{
  guint i, n_items;

  if (!gtk_sor4_list_model_should_sort (self))
    return;

  n_items = g_list_model_get_n_items (self->model);
  sort_array_reserve (&self->items, n_items);
  for (i = 0; i < n_items; i++)
    {
      sort_array_append (&self->items, &(SortItem) { g_list_model_get_item (self->model, i), i });
    }
}

static int
sort_func (gconstpointer a,
           gconstpointer b,
           gpointer      data)
{
  SortItem *sa = (SortItem *) a;
  SortItem *sb = (SortItem *) b;

  return gtk_sorter_compare (data, sa->item, sb->item);
}

static void
gtk_sor4_list_model_resort (GtkSor4ListModel *self)
{
  g_qsort_with_data (sort_array_get_data (&self->items),
                     sort_array_get_size (&self->items),
                     sizeof (SortItem),
                     sort_func,
                     self->sorter);
}

static void
gtk_sor4_list_model_remove_items (GtkSor4ListModel *self,
                                  guint             position,
                                  guint             removed,
                                  guint             added,
                                  guint            *unmodified_start,
                                  guint            *unmodified_end)
{
  guint i, n_items, valid;
  guint start, end;

  n_items = sort_array_get_size (&self->items);
  start = n_items;
  end = n_items;
  
  valid = 0;
  for (i = 0; i < n_items; i++)
    {
      SortItem *si = sort_array_index (&self->items, i);

      if (si->position >= position + removed)
        si->position = si->position - removed + added;
      else if (si->position >= position)
        { 
          start = MIN (start, valid);
          end = n_items - i - 1;
          sort_item_clear (si);
          continue;
        }
      if (valid < i)
        *sort_array_index (&self->items, valid) = *sort_array_index (&self->items, i);
      valid++;
    }

  g_assert (valid == n_items - removed);
  memset (sort_array_index (&self->items, valid), 0, sizeof (SortItem) * removed); 
  sort_array_set_size (&self->items, valid);

  *unmodified_start = start;
  *unmodified_end = end;
}

static void
gtk_sor4_list_model_items_changed_cb (GListModel       *model,
                                      guint             position,
                                      guint             removed,
                                      guint             added,
                                      GtkSor4ListModel *self)
{
  guint i, n_items, start, end;

  if (removed == 0 && added == 0)
    return;

  if (!gtk_sor4_list_model_should_sort (self))
    {
      g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
      return;
    }

  gtk_sor4_list_model_remove_items (self, position, removed, added, &start, &end);
  if (added > 0)
    {
      sort_array_reserve (&self->items, sort_array_get_size (&self->items) + added);
      for (i = position; i < position + added; i++)
        {
          sort_array_append (&self->items, &(SortItem) { g_list_model_get_item (self->model, i), i });
        }
      gtk_sor4_list_model_resort (self);

      for (i = 0; i < start; i++)
        {
          SortItem *si = sort_array_get (&self->items, i);
          if (si->position >= position && si->position < position + added)
            {
              start = i;
              break;
            }
        }
      n_items = sort_array_get_size (&self->items);
      for (i = 0; i < end; i++)
        {
          SortItem *si = sort_array_get (&self->items, n_items - i - 1);
          if (si->position >= position && si->position < position + added)
            {
              end = i;
              break;
            }
        }
    }

  n_items = sort_array_get_size (&self->items) - start - end;
  g_list_model_items_changed (G_LIST_MODEL (self), start, n_items - added + removed, n_items);
}

static void
gtk_sor4_list_model_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkSor4ListModel *self = GTK_SOR4_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_sor4_list_model_set_model (self, g_value_get_object (value));
      break;

    case PROP_SORTER:
      gtk_sor4_list_model_set_sorter (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_sor4_list_model_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  GtkSor4ListModel *self = GTK_SOR4_LIST_MODEL (object);

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
gtk_sor4_list_model_sorter_changed_cb (GtkSorter        *sorter,
                                       int               change,
                                       GtkSor4ListModel *self)
{
  guint n_items;

  if (gtk_sorter_get_order (sorter) == GTK_SORTER_ORDER_NONE)
    gtk_sor4_list_model_clear_items (self);
  else if (sort_array_is_empty (&self->items))
    gtk_sor4_list_model_create_items (self);

  gtk_sor4_list_model_resort (self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  if (n_items > 1)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);
}

static void
gtk_sor4_list_model_clear_model (GtkSor4ListModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_sor4_list_model_items_changed_cb, self);
  g_clear_object (&self->model);
  gtk_sor4_list_model_clear_items (self);
}

static void
gtk_sor4_list_model_clear_sorter (GtkSor4ListModel *self)
{
  if (self->sorter == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->sorter, gtk_sor4_list_model_sorter_changed_cb, self);
  g_clear_object (&self->sorter);
  gtk_sor4_list_model_clear_items (self);
}

static void
gtk_sor4_list_model_dispose (GObject *object)
{
  GtkSor4ListModel *self = GTK_SOR4_LIST_MODEL (object);

  gtk_sor4_list_model_clear_model (self);
  gtk_sor4_list_model_clear_sorter (self);

  G_OBJECT_CLASS (gtk_sor4_list_model_parent_class)->dispose (object);
};

static void
gtk_sor4_list_model_class_init (GtkSor4ListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_sor4_list_model_set_property;
  gobject_class->get_property = gtk_sor4_list_model_get_property;
  gobject_class->dispose = gtk_sor4_list_model_dispose;

  /**
   * GtkSor4ListModel:sorter:
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
   * GtkSor4ListModel:model:
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
gtk_sor4_list_model_init (GtkSor4ListModel *self)
{
}

/**
 * gtk_sor4_list_model_new:
 * @model: (allow-none): the model to sort
 * @sorter: (allow-none): the #GtkSorter to sort @model with
 *
 * Creates a new sort list model that uses the @sorter to sort @model.
 *
 * Returns: a new #GtkSor4ListModel
 **/
GtkSor4ListModel *
gtk_sor4_list_model_new (GListModel *model,
                         GtkSorter  *sorter)
{
  GtkSor4ListModel *result;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (sorter == NULL || GTK_IS_SORTER (sorter), NULL);

  result = g_object_new (GTK_TYPE_SOR4_LIST_MODEL,
                         "model", model,
                         "sorter", sorter,
                         NULL);

  return result;
}

/**
 * gtk_sor4_list_model_set_model:
 * @self: a #GtkSor4ListModel
 * @model: (allow-none): The model to be sorted
 *
 * Sets the model to be sorted. The @model's item type must conform to
 * the item type of @self.
 **/
void
gtk_sor4_list_model_set_model (GtkSor4ListModel *self,
                               GListModel       *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_SOR4_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_sor4_list_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_sor4_list_model_items_changed_cb), self);
      added = g_list_model_get_n_items (model);

      gtk_sor4_list_model_create_items (self);
      gtk_sor4_list_model_resort (self);
    }
  else
    added = 0;
  
  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_sor4_list_model_get_model:
 * @self: a #GtkSor4ListModel
 *
 * Gets the model currently sorted or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model that gets sorted
 **/
GListModel *
gtk_sor4_list_model_get_model (GtkSor4ListModel *self)
{
  g_return_val_if_fail (GTK_IS_SOR4_LIST_MODEL (self), NULL);

  return self->model;
}

/**
 * gtk_sor4_list_model_set_sorter:
 * @self: a #GtkSor4ListModel
 * @sorter: (allow-none): the #GtkSorter to sort @model with
 *
 * Sets a new sorter on @self.
 */
void
gtk_sor4_list_model_set_sorter (GtkSor4ListModel *self,
                                GtkSorter        *sorter)
{
  g_return_if_fail (GTK_IS_SOR4_LIST_MODEL (self));
  g_return_if_fail (sorter == NULL || GTK_IS_SORTER (sorter));

  gtk_sor4_list_model_clear_sorter (self);

  if (sorter)
    {
      self->sorter = g_object_ref (sorter);
      g_signal_connect (sorter, "changed", G_CALLBACK (gtk_sor4_list_model_sorter_changed_cb), self);
      gtk_sor4_list_model_sorter_changed_cb (sorter, GTK_SORTER_CHANGE_DIFFERENT, self);
    }
  else
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
      if (n_items > 1)
        g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORTER]);
}

/**
 * gtk_sor4_list_model_get_sorter:
 * @self: a #GtkSor4LisTModel
 *
 * Gets the sorter that is used to sort @self.
 *
 * Returns: (nullable) (transfer none): the sorter of #self
 */
GtkSorter *
gtk_sor4_list_model_get_sorter (GtkSor4ListModel *self)
{
  g_return_val_if_fail (GTK_IS_SOR4_LIST_MODEL (self), NULL);

  return self->sorter;
}
