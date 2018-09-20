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

#include "gtklistitemmanagerprivate.h"

struct _GtkListItemManager
{
  GObject parent_instance;

  GtkWidget *widget;
  GListModel *model;
  GtkListItemFactory *factory;
};

struct _GtkListItemManagerClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkListItemManager, gtk_list_item_manager, G_TYPE_OBJECT)

static void
gtk_list_item_manager_dispose (GObject *object)
{
  GtkListItemManager *self = GTK_LIST_ITEM_MANAGER (object);

  g_clear_object (&self->model);
  g_clear_object (&self->factory);

  G_OBJECT_CLASS (gtk_list_item_manager_parent_class)->dispose (object);
}

static void
gtk_list_item_manager_class_init (GtkListItemManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_list_item_manager_dispose;
}

static void
gtk_list_item_manager_init (GtkListItemManager *self)
{
}

GtkListItemManager *
gtk_list_item_manager_new (GtkWidget *widget)
{
  GtkListItemManager *self;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  self = g_object_new (GTK_TYPE_LIST_ITEM_MANAGER, NULL);

  self->widget = widget;

  return self;
}

void
gtk_list_item_manager_set_factory (GtkListItemManager *self,
                                   GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (factory));

  if (self->factory == factory)
    return;

  g_clear_object (&self->factory);

  self->factory = g_object_ref (factory);
}

GtkListItemFactory *
gtk_list_item_manager_get_factory (GtkListItemManager *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  return self->factory;
}

void
gtk_list_item_manager_set_model (GtkListItemManager *self,
                                 GListModel         *model)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  g_clear_object (&self->model);

  if (model)
    self->model = g_object_ref (model);
}

GListModel *
gtk_list_item_manager_get_model (GtkListItemManager *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  return self->model;
}

/*
 * gtk_list_item_manager_model_changed:
 * @self: a #GtkListItemManager
 * @position: the position at which the model changed
 * @removed: the number of items removed
 * @added: the number of items added
 *
 * This function must be called by the owning @widget at the
 * appropriate time.  
 * The manager does not connect to GListModel::items-changed itself
 * but relies on its widget calling this function.
 *
 * This function should be called after @widget has released all
 * #GListItems it intends to delete in response to the @removed rows
 * but before it starts creating new ones for the @added rows.
 **/
void
gtk_list_item_manager_model_changed (GtkListItemManager *self,
                                     guint               position,
                                     guint               removed,
                                     guint               added)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (self->model != NULL);
}

/*
 * gtk_list_item_manager_create_list_item:
 * @self: a #GtkListItemManager
 * @position: the row in the model to create a list item for
 * @next_sibling: the widget this widget should be inserted before or %NULL
 *     if none
 *
 * Creates a new list item widget to use for @position. No widget may
 * yet exist that is used for @position.
 *
 * Returns: a properly setup widget to use in @position
 **/
GtkWidget *
gtk_list_item_manager_create_list_item (GtkListItemManager *self,
                                        guint               position,
                                        GtkWidget          *next_sibling)
{
  GtkWidget *result;
  gpointer item;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (next_sibling == NULL || GTK_IS_WIDGET (next_sibling), NULL);

  result = gtk_list_item_factory_create (self->factory);
  item = g_list_model_get_item (self->model, position);
  gtk_list_item_factory_bind (self->factory, result, item);
  g_object_unref (item);
  gtk_widget_insert_before (result, self->widget, next_sibling);

  return result;
}
