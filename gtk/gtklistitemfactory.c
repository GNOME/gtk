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

#include "gtklistitemfactoryprivate.h"

#include "gtklistitemprivate.h"

struct _GtkListItemFactory
{
  GObject parent_instance;

  GtkListCreateWidgetFunc create_func;
  GtkListBindWidgetFunc bind_func;
  gpointer user_data;
  GDestroyNotify user_destroy;
};

struct _GtkListItemFactoryClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkListItemFactory, gtk_list_item_factory, G_TYPE_OBJECT)

static void
gtk_list_item_factory_finalize (GObject *object)
{
  GtkListItemFactory *self = GTK_LIST_ITEM_FACTORY (object);

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  G_OBJECT_CLASS (gtk_list_item_factory_parent_class)->finalize (object);
}

static void
gtk_list_item_factory_class_init (GtkListItemFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_list_item_factory_finalize;
}

static void
gtk_list_item_factory_init (GtkListItemFactory *self)
{
}

GtkListItemFactory *
gtk_list_item_factory_new (GtkListCreateWidgetFunc create_func,
                           GtkListBindWidgetFunc   bind_func,
                           gpointer                user_data,
                           GDestroyNotify          user_destroy)
{
  GtkListItemFactory *self;

  g_return_val_if_fail (create_func, NULL);
  g_return_val_if_fail (bind_func, NULL);
  g_return_val_if_fail (user_data != NULL || user_destroy == NULL, NULL);

  self = g_object_new (GTK_TYPE_LIST_ITEM_FACTORY, NULL);

  self->create_func = create_func;
  self->bind_func = bind_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  return self;
}

GtkListItem *
gtk_list_item_factory_create (GtkListItemFactory *self)
{
  GtkWidget *widget, *result;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_FACTORY (self), NULL);

  widget = self->create_func (self->user_data);

  result = gtk_list_item_new ("row");

  gtk_list_item_set_child (GTK_LIST_ITEM (result), widget);

  return GTK_LIST_ITEM (result);
}

void
gtk_list_item_factory_bind (GtkListItemFactory *self,
                            GtkListItem        *list_item,
                            gpointer            item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (list_item));

  gtk_list_item_bind (list_item, item);

  self->bind_func (gtk_bin_get_child (GTK_BIN (list_item)), item, self->user_data);
}

void
gtk_list_item_factory_unbind (GtkListItemFactory *self,
                              GtkListItem        *list_item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (list_item));

  gtk_list_item_unbind (list_item);
}
