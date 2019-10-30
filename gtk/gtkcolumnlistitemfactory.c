/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkcolumnlistitemfactoryprivate.h"

#include "gtkboxlayout.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitemprivate.h"

struct _GtkColumnListItemFactory
{
  GtkListItemFactory parent_instance;

  GtkColumnView *view; /* no reference, the view references us */
};

struct _GtkColumnListItemFactoryClass
{
  GtkListItemFactoryClass parent_class;
};

G_DEFINE_TYPE (GtkColumnListItemFactory, gtk_column_list_item_factory, GTK_TYPE_LIST_ITEM_FACTORY)

static GtkListItem *
get_nth_child (GtkListItem *parent,
               guint        pos)
{
  GtkWidget *child;
  guint i;

  child = gtk_widget_get_first_child (GTK_WIDGET (parent));
  for (i = 1; i < pos && child; i++)
    child = gtk_widget_get_next_sibling (child);

  return GTK_LIST_ITEM (child);
}

static void
gtk_column_list_item_factory_setup (GtkListItemFactory *factory,
                                    GtkListItem        *list_item)
{
  GtkColumnListItemFactory *self = GTK_COLUMN_LIST_ITEM_FACTORY (factory);
  GListModel *columns;
  guint i;

  gtk_widget_set_layout_manager (GTK_WIDGET (list_item),
                                 gtk_box_layout_new (GTK_ORIENTATION_HORIZONTAL));

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->setup (factory, list_item);

  columns = gtk_column_view_get_columns (self->view);

  for (i = 0; i < g_list_model_get_n_items (columns); i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (columns, i);

      gtk_column_list_item_factory_add_column (self, 
                                               list_item,
                                               column,
                                               FALSE);
    }
}

static void
gtk_column_list_item_factory_teardown (GtkListItemFactory *factory,
                                       GtkListItem        *list_item)
{
  GtkColumnListItemFactory *self = GTK_COLUMN_LIST_ITEM_FACTORY (factory);
  GListModel *columns;
  guint i;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->teardown (factory, list_item);

  columns = gtk_column_view_get_columns (self->view);

  for (i = 0; i < g_list_model_get_n_items (columns); i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (columns, i);
      GtkListItemFactory *column_factory = gtk_column_view_column_get_factory (column);
      GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (list_item));

      if (column_factory)
        gtk_list_item_factory_teardown (column_factory, GTK_LIST_ITEM (child));

      gtk_widget_unparent (child);
    }
}

static void                  
gtk_column_list_item_factory_bind (GtkListItemFactory *factory,
                                   GtkListItem        *list_item,
                                   guint               position,
                                   gpointer            item,
                                   gboolean            selected)
{
  GtkColumnListItemFactory *self = GTK_COLUMN_LIST_ITEM_FACTORY (factory);
  GListModel *columns;
  GtkWidget *child;
  guint i;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->bind (factory, list_item, position, item, selected);

  child = gtk_widget_get_first_child (GTK_WIDGET (list_item));
  columns = gtk_column_view_get_columns (self->view);

  for (i = 0; i < g_list_model_get_n_items (columns); i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (columns, i);
      GtkListItemFactory *column_factory = gtk_column_view_column_get_factory (column);

      if (column_factory)
        gtk_list_item_factory_bind (column_factory, GTK_LIST_ITEM (child), position, item, selected);

      child = gtk_widget_get_next_sibling (child);
    }

  g_assert (child == NULL);
}

static void
gtk_column_list_item_factory_rebind (GtkListItemFactory *factory,
                                     GtkListItem        *list_item,
                                     guint               position,
                                     gpointer            item,
                                     gboolean            selected)
{
  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->bind (factory, list_item, position, item, selected);
  GtkColumnListItemFactory *self = GTK_COLUMN_LIST_ITEM_FACTORY (factory);
  GListModel *columns;
  GtkWidget *child;
  guint i;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->setup (factory, list_item);

  child = gtk_widget_get_first_child (GTK_WIDGET (list_item));
  columns = gtk_column_view_get_columns (self->view);

  for (i = 0; i < g_list_model_get_n_items (columns); i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (columns, i);
      GtkListItemFactory *column_factory = gtk_column_view_column_get_factory (column);

      if (column_factory)
        gtk_list_item_factory_rebind (column_factory, GTK_LIST_ITEM (child), position, item, selected);

      child = gtk_widget_get_next_sibling (child);
    }

  g_assert (child == NULL);
}

static void
gtk_column_list_item_factory_update (GtkListItemFactory *factory,
                                     GtkListItem        *list_item,
                                     guint               position,
                                     gboolean            selected)
{
  GtkColumnListItemFactory *self = GTK_COLUMN_LIST_ITEM_FACTORY (factory);
  GListModel *columns;
  GtkWidget *child;
  guint i;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->update (factory, list_item, position, selected);

  child = gtk_widget_get_first_child (GTK_WIDGET (list_item));
  columns = gtk_column_view_get_columns (self->view);

  for (i = 0; i < g_list_model_get_n_items (columns); i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (columns, i);
      GtkListItemFactory *column_factory = gtk_column_view_column_get_factory (column);

      if (column_factory)
        gtk_list_item_factory_update (column_factory, GTK_LIST_ITEM (child), position, selected);

      child = gtk_widget_get_next_sibling (child);
    }

  g_assert (child == NULL);
}

static void
gtk_column_list_item_factory_unbind (GtkListItemFactory *factory,
                                     GtkListItem        *list_item)
{
  GtkColumnListItemFactory *self = GTK_COLUMN_LIST_ITEM_FACTORY (factory);
  GListModel *columns;
  GtkWidget *child;
  guint i;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->unbind (factory, list_item);

  child = gtk_widget_get_first_child (GTK_WIDGET (list_item));
  columns = gtk_column_view_get_columns (self->view);

  for (i = 0; i < g_list_model_get_n_items (columns); i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (columns, i);
      GtkListItemFactory *column_factory = gtk_column_view_column_get_factory (column);

      if (column_factory)
        gtk_list_item_factory_unbind (column_factory, GTK_LIST_ITEM (child));

      child = gtk_widget_get_next_sibling (child);
    }

  g_assert (child == NULL);
}

static void
gtk_column_list_item_factory_class_init (GtkColumnListItemFactoryClass *klass)
{
  GtkListItemFactoryClass *factory_class = GTK_LIST_ITEM_FACTORY_CLASS (klass);

  factory_class->setup = gtk_column_list_item_factory_setup;
  factory_class->teardown = gtk_column_list_item_factory_teardown;
  factory_class->bind = gtk_column_list_item_factory_bind;
  factory_class->rebind = gtk_column_list_item_factory_rebind;
  factory_class->update = gtk_column_list_item_factory_update;
  factory_class->unbind = gtk_column_list_item_factory_unbind;
}

static void
gtk_column_list_item_factory_init (GtkColumnListItemFactory *self)
{
}

GtkColumnListItemFactory *
gtk_column_list_item_factory_new (GtkColumnView *view)
{
  GtkColumnListItemFactory *result;

  result = g_object_new (GTK_TYPE_COLUMN_LIST_ITEM_FACTORY, NULL);

  result->view = view;

  return result;
}

void
gtk_column_list_item_factory_add_column (GtkColumnListItemFactory *factory,
                                         GtkListItem              *list_item,
                                         GtkColumnViewColumn      *column,
                                         gboolean                  check_bind)
{
  GtkListItemFactory *column_factory;
  GtkListItem *child;

  column_factory = gtk_column_view_column_get_factory (column);

  child = gtk_list_item_new ("cell");
  gtk_widget_set_parent (GTK_WIDGET (child), GTK_WIDGET (list_item));
  if (column_factory)
    {
      gpointer item;

      gtk_list_item_factory_setup (column_factory, child);
      if (check_bind &&
          (item = gtk_list_item_get_item (list_item)))
        {
          gtk_list_item_factory_bind (column_factory,
                                      child,
                                      gtk_list_item_get_position (list_item),
                                      item,
                                      gtk_list_item_get_selected (list_item));
        }
    }
}

void
gtk_column_list_item_factory_remove_column (GtkColumnListItemFactory *factory,
                                            GtkListItem              *list_item,
                                            guint                     col_pos,
                                            GtkColumnViewColumn      *column)
{
  GtkListItemFactory *column_factory;
  GtkListItem *child;

  column_factory = gtk_column_view_column_get_factory (column);
  child = get_nth_child (list_item, col_pos);

  if (gtk_list_item_get_item (GTK_LIST_ITEM (child)))
    gtk_list_item_factory_unbind (column_factory, child);
  
  gtk_list_item_factory_teardown (column_factory, child);
  gtk_widget_unparent (GTK_WIDGET (child));
}
