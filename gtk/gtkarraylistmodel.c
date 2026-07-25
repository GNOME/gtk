/*
 * Copyright © 2026 RedHat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkarraylistmodelprivate.h"

struct _GtkArrayListModel
{
  GObject parent_instance;

  GPtrArray *array;
  gpointer data;
  GDestroyNotify notify;

  unsigned int n_items;
};

struct _GtkArrayListModelClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_N_ITEMS,

  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_array_list_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_array_list_model_get_n_items (GListModel *list)
{
  GtkArrayListModel *self = GTK_ARRAY_LIST_MODEL (list);

  return self->array->len;
}

static gpointer
gtk_array_list_model_get_item (GListModel   *list,
                               unsigned int  position)
{
  GtkArrayListModel *self = GTK_ARRAY_LIST_MODEL (list);

  if (position < self->array->len)
    return g_object_ref (g_ptr_array_index (self->array, position));

  return NULL;
}

static void
gtk_array_list_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_array_list_model_get_item_type;
  iface->get_n_items = gtk_array_list_model_get_n_items;
  iface->get_item = gtk_array_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkArrayListModel, gtk_array_list_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_array_list_model_list_model_init))

static void
gtk_array_list_model_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkArrayListModel *self = GTK_ARRAY_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, G_TYPE_OBJECT);
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, self->array->len);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_array_list_model_dispose (GObject *object)
{
  GtkArrayListModel *self = GTK_ARRAY_LIST_MODEL (object);

  if (self->notify)
    self->notify (self->data);

  self->notify = NULL;

  g_ptr_array_unref (self->array);

  G_OBJECT_CLASS (gtk_array_list_model_parent_class)->dispose (object);
}

static void
gtk_array_list_model_class_init (GtkArrayListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_array_list_model_get_property;
  object_class->dispose = gtk_array_list_model_dispose;

  properties[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        G_TYPE_OBJECT,
                        G_PARAM_READABLE | G_PARAM_STATIC_NAME);

  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_array_list_model_init (GtkArrayListModel *self)
{
}

GtkArrayListModel *
gtk_array_list_model_new (GPtrArray     *array,
                         gpointer        data,
                         GDestroyNotify  notify)
{
  GtkArrayListModel *result;

  result = g_object_new (GTK_TYPE_ARRAY_LIST_MODEL, NULL);

  result->array = g_ptr_array_ref (array);
  result->data = data;
  result->notify = notify;

  result->n_items = array->len;

  return result;
}

void
gtk_array_list_model_item_inserted (GtkArrayListModel *self,
                                    unsigned int       position)
{
  g_return_if_fail (GTK_IS_ARRAY_LIST_MODEL (self));
  g_return_if_fail (position < self->array->len);

  self->n_items = self->array->len;

  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

void
gtk_array_list_model_item_added (GtkArrayListModel *self)
{
  gtk_array_list_model_item_inserted (self, self->array->len - 1);
}

void
gtk_array_list_model_item_removed (GtkArrayListModel *self,
                                   unsigned int       position)
{
  g_return_if_fail (GTK_IS_ARRAY_LIST_MODEL (self));
  g_return_if_fail (position <= self->array->len);

  self->n_items = self->array->len;

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

void
gtk_array_list_model_item_moved (GtkArrayListModel *self,
                                 unsigned int       previous_position,
                                 unsigned int       position)
{
  unsigned int min, max;

  g_return_if_fail (GTK_IS_ARRAY_LIST_MODEL (self));

  if (position == previous_position)
    return;

  min = MIN (previous_position, position);
  max = MAX (previous_position, position) + 1;

  g_list_model_items_changed (G_LIST_MODEL (self), min, max - min, max - min);
}

void
gtk_array_list_model_sorted (GtkArrayListModel *self)
{
  g_return_if_fail (GTK_IS_ARRAY_LIST_MODEL (self));

  g_list_model_items_changed (G_LIST_MODEL (self), 0, self->n_items, self->n_items);
}

void
gtk_array_list_model_clear (GtkArrayListModel *self)
{
  g_return_if_fail (GTK_IS_ARRAY_LIST_MODEL (self));

  if (self->n_items > 0)
    {
      unsigned int n_items = self->n_items;

      self->n_items = 0;

      g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
    }

  if (self->notify)
    {
      self->notify (self->data);
      self->notify = NULL;
    }
}
