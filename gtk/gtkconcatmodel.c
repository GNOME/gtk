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
 * SECTION:gtkconcatmodel
 * @Short_description: concatenation list model
 * @Title: GtkConcatModel
 * @See_also: #GListModel
 *
 * #GtkConcatModel is a #GListModel implementation that takes a list of list models
 * and presents them as one concatenated list.
 *
 * Node that all the types of the passed in list models must match the concat model's
 * type. If they are not, you must use a common ancestor type for the #GtkConcatModel,
 * %G_TYPE_OBJECT being the ultimate option.
 **/

#include "config.h"

#include "gtkconcatmodelprivate.h"

struct _GtkConcatModel
{
  GObject parent_instance;

  GType item_type;
  guint n_items;

  GList *models;
};

struct _GtkConcatModelClass
{
  GObjectClass parent_class;
};

static GType
gtk_concat_model_list_model_get_item_type (GListModel *list)
{
  GtkConcatModel *self = GTK_CONCAT_MODEL (list);

  return self->item_type;
}

static guint
gtk_concat_model_list_model_get_n_items (GListModel *list)
{
  GtkConcatModel *self = GTK_CONCAT_MODEL (list);

  return self->n_items;
}

static gpointer
gtk_concat_model_list_model_get_item (GListModel *list,
                                      guint       position)
{
  GtkConcatModel *self = GTK_CONCAT_MODEL (list);
  GList *l;

  /* FIXME: Use an RBTree to make this O(log N) */
  for (l = self->models; l; l = l->next)
    {
      guint n = g_list_model_get_n_items (l->data);

      if (position < n)
        return g_list_model_get_item (l->data, position);

      position -= n;
    }

  return NULL;
}

static void
gtk_concat_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_concat_model_list_model_get_item_type;
  iface->get_n_items = gtk_concat_model_list_model_get_n_items;
  iface->get_item = gtk_concat_model_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkConcatModel, gtk_concat_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_concat_model_list_model_init))

static void
gtk_concat_model_items_changed (GListModel     *model,
                                guint           position,
                                guint           removed,
                                guint           added,
                                GtkConcatModel *self)
{
  GList *l;

  for (l = self->models; l; l = l->next)
    {
      if (l->data == model)
        break;

      position += g_list_model_get_n_items (l->data);
    }

  self->n_items -= removed;
  self->n_items += added;

  g_list_model_items_changed (G_LIST_MODEL (self),
                              position,
                              removed,
                              added);
}

static void
gtk_concat_model_remove_internal (GtkConcatModel *self,
                                  GListModel     *model,
                                  gboolean        emit_signals)
{
  guint n_items, position;
  GList *l;

  position = 0;
  for (l = self->models; l; l = l->next)
    {
      if (l->data == model)
        break;

      position += g_list_model_get_n_items (l->data);
    }

  g_return_if_fail (l != NULL);

  self->models = g_list_delete_link (self->models, l);
  n_items = g_list_model_get_n_items (model);
  self->n_items -= n_items;
  g_signal_handlers_disconnect_by_func (model, gtk_concat_model_items_changed, self);
  g_object_unref (model);
  
  if (n_items && emit_signals)
    g_list_model_items_changed (G_LIST_MODEL (self),
                                position,
                                n_items,
                                0);
}

static void
gtk_concat_model_dispose (GObject *object)
{
  GtkConcatModel *self = GTK_CONCAT_MODEL (object);

  /* FIXME: Make this work without signal emissions */
  while (self->models)
    gtk_concat_model_remove_internal (self, self->models->data, FALSE);

  G_OBJECT_CLASS (gtk_concat_model_parent_class)->dispose (object);
}

static void
gtk_concat_model_class_init (GtkConcatModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_concat_model_dispose;
}

static void
gtk_concat_model_init (GtkConcatModel *self)
{
}

GtkConcatModel *
gtk_concat_model_new (GType item_type)
{
  GtkConcatModel *self;

  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);

  self = g_object_new (GTK_TYPE_CONCAT_MODEL, NULL);

  self->item_type = item_type;

  return self;
}

void
gtk_concat_model_append (GtkConcatModel *self,
                         GListModel     *model)
{
  guint n_items;

  g_return_if_fail (GTK_IS_CONCAT_MODEL (self));
  g_return_if_fail (G_IS_LIST_MODEL (model));
  g_return_if_fail (g_type_is_a (g_list_model_get_item_type (model), self->item_type));

  g_object_ref (model);
  g_signal_connect (model, "items-changed", G_CALLBACK (gtk_concat_model_items_changed), self);
  self->models = g_list_append (self->models, model);
  n_items = g_list_model_get_n_items (model);
  self->n_items += n_items;
  
  if (n_items)
    g_list_model_items_changed (G_LIST_MODEL (self),
                                self->n_items - n_items,
                                0,
                                n_items);
}

void
gtk_concat_model_remove (GtkConcatModel *self,
                         GListModel     *model)
{
  g_return_if_fail (GTK_IS_CONCAT_MODEL (self));
  g_return_if_fail (G_IS_LIST_MODEL (model));

  gtk_concat_model_remove_internal (self, model, TRUE);
}

GListModel *
gtk_concat_model_get_model_for_item (GtkConcatModel *self,
                                     guint           position)
{
  GList *l;

  g_return_val_if_fail (GTK_IS_CONCAT_MODEL (self), NULL);

  /* FIXME: Use an RBTree to make this O(log N) */
  for (l = self->models; l; l = l->next)
    {
      guint n = g_list_model_get_n_items (l->data);

      if (position < n)
        return l->data;

      position -= n;
    }

  return NULL;
}

