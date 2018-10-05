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

  GtkListItemSetupFunc setup_func;
  GtkListItemBindFunc bind_func;
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
gtk_list_item_factory_new (GtkListItemSetupFunc setup_func,
                           GtkListItemBindFunc  bind_func,
                           gpointer             user_data,
                           GDestroyNotify       user_destroy)
{
  GtkListItemFactory *self;

  g_return_val_if_fail (setup_func || bind_func, NULL);
  g_return_val_if_fail (user_data != NULL || user_destroy == NULL, NULL);

  self = g_object_new (GTK_TYPE_LIST_ITEM_FACTORY, NULL);

  self->setup_func = setup_func;
  self->bind_func = bind_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  return self;
}

void
gtk_list_item_factory_setup (GtkListItemFactory *self,
                             GtkListItem         *list_item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));

  if (self->setup_func)
    self->setup_func (list_item, self->user_data);
}

void
gtk_list_item_factory_bind (GtkListItemFactory *self,
                            GtkListItem        *list_item,
                            guint               position,
                            gpointer            item,
                            gboolean            selected)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (list_item));

  g_object_freeze_notify (G_OBJECT (list_item));

  gtk_list_item_set_item (list_item, item);
  gtk_list_item_set_position (list_item, position);
  gtk_list_item_set_selected (list_item, selected);

  if (self->bind_func)  
    self->bind_func (list_item, self->user_data);

  g_object_thaw_notify (G_OBJECT (list_item));
}

void
gtk_list_item_factory_update (GtkListItemFactory *self,
                              GtkListItem        *list_item,
                              guint               position,
                              gboolean            selected)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (list_item));

  g_object_freeze_notify (G_OBJECT (list_item));

  gtk_list_item_set_position (list_item, position);
  gtk_list_item_set_selected (list_item, selected);

  g_object_thaw_notify (G_OBJECT (list_item));
}

void
gtk_list_item_factory_unbind (GtkListItemFactory *self,
                              GtkListItem        *list_item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (list_item));

  g_object_freeze_notify (G_OBJECT (list_item));

  gtk_list_item_set_item (list_item, NULL);
  gtk_list_item_set_position (list_item, 0);
  gtk_list_item_set_selected (list_item, FALSE);

  g_object_thaw_notify (G_OBJECT (list_item));
}
