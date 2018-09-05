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


/*
 * SECTION:gtklistlistmodel
 * @Short_description: a List model for lists
 * @Title: GtkListListModel
 * @See_also: #GListModel, #GList
 *
 * #GtkListListModel is a #GListModel implementation that takes a list API and provides
 * it as a GListModel.
 **/

#include "config.h"

#include "gtklistlistmodelprivate.h"

struct _GtkListListModel
{
  GObject parent_instance;

  GType item_type;
  guint n_items;
  gpointer (* get_first) (gpointer);
  gpointer (* get_next) (gpointer, gpointer);
  gpointer (* get_previous) (gpointer, gpointer);
  gpointer (* get_last) (gpointer);
  gpointer (* get_item) (gpointer, gpointer);
  gpointer data;
  GDestroyNotify notify;
};

struct _GtkListListModelClass
{
  GObjectClass parent_class;
};

static GType
gtk_list_list_model_get_item_type (GListModel *list)
{
  GtkListListModel *self = GTK_LIST_LIST_MODEL (list);

  return self->item_type;
}

static guint
gtk_list_list_model_get_n_items (GListModel *list)
{
  GtkListListModel *self = GTK_LIST_LIST_MODEL (list);

  return self->n_items;
}

static gpointer
gtk_list_list_model_get_item (GListModel *list,
                              guint       position)
{
  GtkListListModel *self = GTK_LIST_LIST_MODEL (list);
  gpointer result;
  guint i;

  if (position >= self->n_items)
    {
      return NULL;
    }
  else if (self->get_last &&
           position >= self->n_items / 2)
    {
      result = self->get_last (self->data);

      for (i = self->n_items - 1; i > position; i--)
        {
          result = self->get_previous (result, self->data);
        }
    }
  else
    {
      result = self->get_first (self->data);

      for (i = 0; i < position; i++)
        {
          result = self->get_next (result, self->data);
        }
    }

  return self->get_item (result, self->data);
}

static void
gtk_list_list_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_list_list_model_get_item_type;
  iface->get_n_items = gtk_list_list_model_get_n_items;
  iface->get_item = gtk_list_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkListListModel, gtk_list_list_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_list_list_model_list_model_init))

static void
gtk_list_list_model_dispose (GObject *object)
{
  GtkListListModel *self = GTK_LIST_LIST_MODEL (object);

  if (self->notify)
    self->notify (self->data);

  self->n_items = 0;
  self->notify = NULL;

  G_OBJECT_CLASS (gtk_list_list_model_parent_class)->dispose (object);
}

static void
gtk_list_list_model_class_init (GtkListListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_list_list_model_dispose;
}

static void
gtk_list_list_model_init (GtkListListModel *self)
{
}

GtkListListModel *
gtk_list_list_model_new (GType          item_type,
                         gpointer       (* get_first) (gpointer),
                         gpointer       (* get_next) (gpointer, gpointer),
                         gpointer       (* get_previous) (gpointer, gpointer),
                         gpointer       (* get_last) (gpointer),
                         gpointer       (* get_item) (gpointer, gpointer),
                         gpointer       data,
                         GDestroyNotify notify)
{
  guint n_items;
  gpointer item;

  n_items = 0;
  for (item = get_first (data);
       item != NULL;
       item = get_next (item, data))
    n_items++;

  return gtk_list_list_model_new_with_size (item_type,
                                            n_items,
                                            get_first,
                                            get_next,
                                            get_previous,
                                            get_last,
                                            get_item,
                                            data,
                                            notify);
}

GtkListListModel *
gtk_list_list_model_new_with_size (GType          item_type,
                                   guint          n_items,
                                   gpointer       (* get_first) (gpointer),
                                   gpointer       (* get_next) (gpointer, gpointer),
                                   gpointer       (* get_previous) (gpointer, gpointer),
                                   gpointer       (* get_last) (gpointer),
                                   gpointer       (* get_item) (gpointer, gpointer),
                                   gpointer       data,
                                   GDestroyNotify notify)
{
  GtkListListModel *result;

  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (get_first != NULL, NULL);
  g_return_val_if_fail (get_next != NULL, NULL);
  g_return_val_if_fail (get_previous != NULL, NULL);
  g_return_val_if_fail (get_item != NULL, NULL);

  result = g_object_new (GTK_TYPE_LIST_LIST_MODEL, NULL);

  result->item_type = item_type;
  result->n_items = n_items;
  result->get_first = get_first;
  result->get_next = get_next;
  result->get_previous = get_previous;
  result->get_last = get_last;
  result->get_item = get_item;
  result->data = data;
  result->notify = notify;

  return result;
}

void
gtk_list_list_model_item_added (GtkListListModel *self,
                                gpointer          item)
{
  gpointer x;
  guint position;

  g_return_if_fail (GTK_IS_LIST_LIST_MODEL (self));
  g_return_if_fail (item != NULL);

  position = 0;
  for (x = self->get_first (self->data);
       x != item;
       x = self->get_next (x, self->data))
    position++;

  gtk_list_list_model_item_added_at (self, position);
}

void
gtk_list_list_model_item_added_at (GtkListListModel *self,
                                   guint             position)
{
  g_return_if_fail (GTK_IS_LIST_LIST_MODEL (self));
  g_return_if_fail (position <= self->n_items);

  self->n_items += 1;

  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

void
gtk_list_list_model_item_removed (GtkListListModel *self,
                                  gpointer          previous)
{
  gpointer x;
  guint position;

  g_return_if_fail (GTK_IS_LIST_LIST_MODEL (self));

  if (previous == NULL)
    {
      position = 0;
    }
  else
    {
      position = 1;

      for (x = self->get_first (self->data);
           x != previous;
           x = self->get_next (x, self->data))
        position++;
    }

  gtk_list_list_model_item_removed_at (self, position);
}

void
gtk_list_list_model_item_removed_at (GtkListListModel *self,
                                     guint             position)
{
  g_return_if_fail (GTK_IS_LIST_LIST_MODEL (self));
  g_return_if_fail (position < self->n_items);

  self->n_items -= 1;

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

void
gtk_list_list_model_clear (GtkListListModel *self)
{
  guint n_items;

  g_return_if_fail (GTK_IS_LIST_LIST_MODEL (self));

  n_items = self->n_items;
  
  if (self->notify)
    self->notify (self->data);

  self->n_items = 0;
  self->notify = NULL;

  if (n_items > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);
}


