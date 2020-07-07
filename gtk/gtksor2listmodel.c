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

#include "gtksor2listmodel.h"

#include "gtkintl.h"
#include "gtkprivate.h"

#define GDK_ARRAY_ELEMENT_TYPE GObject *
#define GDK_ARRAY_TYPE_NAME SortArray
#define GDK_ARRAY_NAME sort_array
#define GDK_ARRAY_FREE_FUNC g_object_unref
#include "gdk/gdkarrayimpl.c"

/**
 * SECTION:gtksor2listmodel
 * @title: GtkSor2ListModel
 * @short_description: A list model that sorts its items
 * @see_also: #GListModel, #GtkSorter
 *
 * #GtkSor2ListModel is a list model that takes a list model and
 * sorts its elements according to a #GtkSorter.
 *
 * #GtkSor2ListModel is a generic model and because of that it
 * cannot take advantage of any external knowledge when sorting.
 * If you run into performance issues with #GtkSor2ListModel, it
 * is strongly recommended that you write your own sorting list
 * model.
 */

enum {
  PROP_0,
  PROP_MODEL,
  PROP_SORTER,
  NUM_PROPERTIES
};

struct _GtkSor2ListModel
{
  GObject parent_instance;

  GListModel *model;
  GtkSorter *sorter;

  SortArray items; /* empty if known unsorted */
};

struct _GtkSor2ListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_sor2_list_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_sor2_list_model_get_n_items (GListModel *list)
{
  GtkSor2ListModel *self = GTK_SOR2_LIST_MODEL (list);

  if (self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_sor2_list_model_get_item (GListModel *list,
                              guint       position)
{
  GtkSor2ListModel *self = GTK_SOR2_LIST_MODEL (list);

  if (self->model == NULL)
    return NULL;

  if (sort_array_is_empty (&self->items))
    return g_list_model_get_item (self->model, position);

  if (position >= sort_array_get_size (&self->items))
    return NULL;

  return g_object_ref (sort_array_get (&self->items, position));
}

static void
gtk_sor2_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_sor2_list_model_get_item_type;
  iface->get_n_items = gtk_sor2_list_model_get_n_items;
  iface->get_item = gtk_sor2_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkSor2ListModel, gtk_sor2_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_sor2_list_model_model_init))

static void
gtk_sor2_list_model_clear_items (GtkSor2ListModel *self)
{
  sort_array_clear (&self->items);
} 

static void
gtk_sor2_list_model_create_items (GtkSor2ListModel *self)
{
  guint i, n_items;

  if (self->sorter == NULL ||
      self->model == NULL ||
      gtk_sorter_get_order (self->sorter) == GTK_SORTER_ORDER_NONE)
    return;

  n_items = g_list_model_get_n_items (self->model);
  sort_array_reserve (&self->items, n_items);
  for (i = 0; i < n_items; i++)
    {
      sort_array_append (&self->items, g_list_model_get_item (self->model, i));
    }
}

static int
sort_func (gconstpointer a,
           gconstpointer b,
           gpointer      data)
{
  return gtk_sorter_compare (data, *(GObject **) a, *(GObject **) b);
}

static void
gtk_sor2_list_model_resort (GtkSor2ListModel *self)
{
  g_qsort_with_data (sort_array_get_data (&self->items),
                     sort_array_get_size (&self->items),
                     sizeof (GObject *),
                     sort_func,
                     self->sorter);
}

static void
gtk_sor2_list_model_items_changed_cb (GListModel       *model,
                                      guint             position,
                                      guint             removed,
                                      guint             added,
                                      GtkSor2ListModel *self)
{
  guint n_items;

  /* doesn't free() the array */
  sort_array_set_size (&self->items, 0);
  gtk_sor2_list_model_create_items (self);
  gtk_sor2_list_model_resort (self);

  n_items = g_list_model_get_n_items (model);
  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items - added + removed, n_items);
}

static void
gtk_sor2_list_model_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkSor2ListModel *self = GTK_SOR2_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_sor2_list_model_set_model (self, g_value_get_object (value));
      break;

    case PROP_SORTER:
      gtk_sor2_list_model_set_sorter (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_sor2_list_model_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  GtkSor2ListModel *self = GTK_SOR2_LIST_MODEL (object);

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
gtk_sor2_list_model_sorter_changed_cb (GtkSorter        *sorter,
                                       int               change,
                                       GtkSor2ListModel *self)
{
  guint n_items;

  if (gtk_sorter_get_order (sorter) == GTK_SORTER_ORDER_NONE)
    gtk_sor2_list_model_clear_items (self);
  else if (sort_array_is_empty (&self->items))
    gtk_sor2_list_model_create_items (self);

  gtk_sor2_list_model_resort (self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  if (n_items > 1)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);
}

static void
gtk_sor2_list_model_clear_model (GtkSor2ListModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_sor2_list_model_items_changed_cb, self);
  g_clear_object (&self->model);
  gtk_sor2_list_model_clear_items (self);
}

static void
gtk_sor2_list_model_clear_sorter (GtkSor2ListModel *self)
{
  if (self->sorter == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->sorter, gtk_sor2_list_model_sorter_changed_cb, self);
  g_clear_object (&self->sorter);
  gtk_sor2_list_model_clear_items (self);
}

static void
gtk_sor2_list_model_dispose (GObject *object)
{
  GtkSor2ListModel *self = GTK_SOR2_LIST_MODEL (object);

  gtk_sor2_list_model_clear_model (self);
  gtk_sor2_list_model_clear_sorter (self);

  G_OBJECT_CLASS (gtk_sor2_list_model_parent_class)->dispose (object);
};

static void
gtk_sor2_list_model_class_init (GtkSor2ListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_sor2_list_model_set_property;
  gobject_class->get_property = gtk_sor2_list_model_get_property;
  gobject_class->dispose = gtk_sor2_list_model_dispose;

  /**
   * GtkSor2ListModel:sorter:
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
   * GtkSor2ListModel:model:
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
gtk_sor2_list_model_init (GtkSor2ListModel *self)
{
}

/**
 * gtk_sor2_list_model_new:
 * @model: (allow-none): the model to sort
 * @sorter: (allow-none): the #GtkSorter to sort @model with
 *
 * Creates a new sort list model that uses the @sorter to sort @model.
 *
 * Returns: a new #GtkSor2ListModel
 **/
GtkSor2ListModel *
gtk_sor2_list_model_new (GListModel *model,
                         GtkSorter  *sorter)
{
  GtkSor2ListModel *result;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (sorter == NULL || GTK_IS_SORTER (sorter), NULL);

  result = g_object_new (GTK_TYPE_SOR2_LIST_MODEL,
                         "model", model,
                         "sorter", sorter,
                         NULL);

  return result;
}

/**
 * gtk_sor2_list_model_set_model:
 * @self: a #GtkSor2ListModel
 * @model: (allow-none): The model to be sorted
 *
 * Sets the model to be sorted. The @model's item type must conform to
 * the item type of @self.
 **/
void
gtk_sor2_list_model_set_model (GtkSor2ListModel *self,
                               GListModel       *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_SOR2_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_sor2_list_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_sor2_list_model_items_changed_cb), self);
      added = g_list_model_get_n_items (model);

      gtk_sor2_list_model_create_items (self);
      gtk_sor2_list_model_resort (self);
    }
  else
    added = 0;
  
  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_sor2_list_model_get_model:
 * @self: a #GtkSor2ListModel
 *
 * Gets the model currently sorted or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model that gets sorted
 **/
GListModel *
gtk_sor2_list_model_get_model (GtkSor2ListModel *self)
{
  g_return_val_if_fail (GTK_IS_SOR2_LIST_MODEL (self), NULL);

  return self->model;
}

/**
 * gtk_sor2_list_model_set_sorter:
 * @self: a #GtkSor2ListModel
 * @sorter: (allow-none): the #GtkSorter to sort @model with
 *
 * Sets a new sorter on @self.
 */
void
gtk_sor2_list_model_set_sorter (GtkSor2ListModel *self,
                                GtkSorter        *sorter)
{
  g_return_if_fail (GTK_IS_SOR2_LIST_MODEL (self));
  g_return_if_fail (sorter == NULL || GTK_IS_SORTER (sorter));

  gtk_sor2_list_model_clear_sorter (self);

  if (sorter)
    {
      self->sorter = g_object_ref (sorter);
      g_signal_connect (sorter, "changed", G_CALLBACK (gtk_sor2_list_model_sorter_changed_cb), self);
      gtk_sor2_list_model_sorter_changed_cb (sorter, GTK_SORTER_CHANGE_DIFFERENT, self);
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
 * gtk_sor2_list_model_get_sorter:
 * @self: a #GtkSor2LisTModel
 *
 * Gets the sorter that is used to sort @self.
 *
 * Returns: (nullable) (transfer none): the sorter of #self
 */
GtkSorter *
gtk_sor2_list_model_get_sorter (GtkSor2ListModel *self)
{
  g_return_val_if_fail (GTK_IS_SOR2_LIST_MODEL (self), NULL);

  return self->sorter;
}
