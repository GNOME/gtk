/* gtkcellviewmenuitem.h
 * Copyright (C) 2003  Kristian Rietveld <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_CELL_VIEW_MENU_ITEM_H__
#define __GTK_CELL_VIEW_MENU_ITEM_H__

#include <gtk/gtkmenuitem.h>
#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_VIEW_MENU_ITEM              (gtk_cell_view_menu_item_get_type ())
#define GTK_CELL_VIEW_MENU_ITEM(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_VIEW_MENU_ITEM, GtkCellViewMenuItem))
#define GTK_CELL_VIEW_MENU_ITEM_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_VIEW_MENU_ITEM, GtkCellViewMenuItemClass))
#define GTK_IS_CELL_VIEW_MENU_ITEM(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_VIEW_MENU_ITEM))
#define GTK_IS_CELL_VIEW_MENU_ITEM_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_VIEW_MENU_ITEM))
#define GTK_CELL_VIEW_MENU_ITEM_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_VIEW_MENU_ITEM, GtkCellViewMenuItemClass))


typedef struct _GtkCellViewMenuItem        GtkCellViewMenuItem;
typedef struct _GtkCellViewMenuItemClass   GtkCellViewMenuItemClass;
typedef struct _GtkCellViewMenuItemPrivate GtkCellViewMenuItemPrivate;

struct _GtkCellViewMenuItem
{
  GtkMenuItem parent_instance;

  /*< private >*/
  GtkCellViewMenuItemPrivate *priv;
};

struct _GtkCellViewMenuItemClass
{
  GtkMenuItemClass parent_class;
};


GType      gtk_cell_view_menu_item_get_type        (void);
GtkWidget *gtk_cell_view_menu_item_new             (void);

GtkWidget *gtk_cell_view_menu_item_new_with_pixbuf (GdkPixbuf   *pixbuf);
GtkWidget *gtk_cell_view_menu_item_new_with_text   (const gchar *text);
GtkWidget *gtk_cell_view_menu_item_new_with_markup (const gchar *markup);

GtkWidget *gtk_cell_view_menu_item_new_from_model  (GtkTreeModel *model,
                                                    GtkTreePath  *path);


G_END_DECLS

#endif /* __GTK_CELL_VIEW_MENU_ITEM_H__ */
