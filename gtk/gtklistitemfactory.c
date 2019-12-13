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

G_DEFINE_TYPE (GtkListItemFactory, gtk_list_item_factory, G_TYPE_OBJECT)

static void
gtk_list_item_factory_default_setup (GtkListItemFactory *self,
                                     GtkListItem        *list_item)
{
}

static void
gtk_list_item_factory_default_teardown (GtkListItemFactory *self,
                                        GtkListItem        *list_item)
{
  gtk_list_item_set_child (list_item, NULL);

  gtk_list_item_set_selectable (list_item, TRUE);

}

static void                  
gtk_list_item_factory_default_update (GtkListItemFactory *self,
                                      GtkListItem        *list_item,
                                      guint               position,
                                      gpointer            item,
                                      gboolean            selected)
{
  gtk_list_item_set_item (list_item, item);
  gtk_list_item_set_position (list_item, position);
  gtk_list_item_set_selected (list_item, selected);
}

static void
gtk_list_item_factory_class_init (GtkListItemFactoryClass *klass)
{
  klass->setup = gtk_list_item_factory_default_setup;
  klass->teardown = gtk_list_item_factory_default_teardown;
  klass->update = gtk_list_item_factory_default_update;
}

static void
gtk_list_item_factory_init (GtkListItemFactory *self)
{
}

void
gtk_list_item_factory_setup (GtkListItemFactory *self,
                             GtkListItem        *list_item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));

  GTK_LIST_ITEM_FACTORY_GET_CLASS (self)->setup (self, list_item);
}

void
gtk_list_item_factory_teardown (GtkListItemFactory *self,
                                GtkListItem        *list_item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));

  GTK_LIST_ITEM_FACTORY_GET_CLASS (self)->teardown (self, list_item);
}

void
gtk_list_item_factory_update (GtkListItemFactory *self,
                              GtkListItem        *list_item,
                              guint               position,
                              gpointer            item,
                              gboolean            selected)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (list_item));

  g_object_freeze_notify (G_OBJECT (list_item));

  GTK_LIST_ITEM_FACTORY_GET_CLASS (self)->update (self, list_item, position, item, selected);

  g_object_thaw_notify (G_OBJECT (list_item));
}

