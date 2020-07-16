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
#include "gtkcolumnviewlayoutprivate.h"
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

static void
gtk_column_list_item_factory_setup (GtkListItemFactory *factory,
                                    GtkListItemWidget  *widget,
                                    GtkListItem        *list_item)
{
  GtkColumnListItemFactory *self = GTK_COLUMN_LIST_ITEM_FACTORY (factory);
  GListModel *columns;
  guint i;

  /* FIXME: evil */
  gtk_widget_set_layout_manager (GTK_WIDGET (widget),
                                 gtk_column_view_layout_new (self->view));

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->setup (factory, widget, list_item);

  columns = gtk_column_view_get_columns (self->view);

  for (i = 0; i < g_list_model_get_n_items (columns); i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (columns, i);

      gtk_column_list_item_factory_add_column (self,
                                               list_item->owner,
                                               column,
                                               FALSE);

      g_object_unref (column);
    }
}

static void
gtk_column_list_item_factory_teardown (GtkListItemFactory *factory,
                                       GtkListItemWidget  *widget,
                                       GtkListItem        *list_item)
{
  GtkWidget *child;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->teardown (factory, widget, list_item);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (widget))))
    {
      gtk_list_item_widget_remove_child (GTK_LIST_ITEM_WIDGET (widget), child);
    }
}

static void
gtk_column_list_item_factory_update (GtkListItemFactory *factory,
                                     GtkListItemWidget  *widget,
                                     GtkListItem        *list_item,
                                     guint               position,
                                     gpointer            item,
                                     gboolean            selected)
{
  GtkWidget *child;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_column_list_item_factory_parent_class)->update (factory, widget, list_item, position, item, selected);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (widget));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_list_item_widget_update (GTK_LIST_ITEM_WIDGET (child), position, item, selected);
    }
}

static void
gtk_column_list_item_factory_class_init (GtkColumnListItemFactoryClass *klass)
{
  GtkListItemFactoryClass *factory_class = GTK_LIST_ITEM_FACTORY_CLASS (klass);

  factory_class->setup = gtk_column_list_item_factory_setup;
  factory_class->teardown = gtk_column_list_item_factory_teardown;
  factory_class->update = gtk_column_list_item_factory_update;
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
                                         GtkListItemWidget        *list_item,
                                         GtkColumnViewColumn      *column,
                                         gboolean                  check_bind)
{
  GtkWidget *cell;

  cell = gtk_column_view_cell_new (column);
  gtk_list_item_widget_add_child (GTK_LIST_ITEM_WIDGET (list_item), GTK_WIDGET (cell));
  gtk_list_item_widget_update (GTK_LIST_ITEM_WIDGET (cell),
                               gtk_list_item_widget_get_position (list_item),
                               gtk_list_item_widget_get_item (list_item),
                               gtk_list_item_widget_get_selected (list_item));
}
