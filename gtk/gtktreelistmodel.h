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

#ifndef __GTK_TREE_LIST_MODEL_H__
#define __GTK_TREE_LIST_MODEL_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gio/gio.h>
#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

#define GTK_TYPE_TREE_LIST_MODEL (gtk_tree_list_model_get_type ())
#define GTK_TYPE_TREE_LIST_ROW (gtk_tree_list_row_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkTreeListModel, gtk_tree_list_model, GTK, TREE_LIST_MODEL, GObject)
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkTreeListRow, gtk_tree_list_row, GTK, TREE_LIST_ROW, GObject)

typedef GListModel * (* GtkTreeListModelCreateModelFunc) (gpointer item, gpointer data);

GDK_AVAILABLE_IN_ALL
GtkTreeListModel *      gtk_tree_list_model_new                 (gboolean                passthrough,
                                                                 GListModel             *root,
                                                                 gboolean                autoexpand,
                                                                 GtkTreeListModelCreateModelFunc create_func,
                                                                 gpointer                data,
                                                                 GDestroyNotify          data_destroy);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_tree_list_model_get_passthrough     (GtkTreeListModel       *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_tree_list_model_set_autoexpand      (GtkTreeListModel       *self,
                                                                 gboolean                autoexpand);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_tree_list_model_get_autoexpand      (GtkTreeListModel       *self);

GDK_AVAILABLE_IN_ALL
GtkTreeListRow *        gtk_tree_list_model_get_row             (GtkTreeListModel       *self,
                                                                 guint                   position);

GDK_AVAILABLE_IN_ALL
gpointer                gtk_tree_list_row_get_item              (GtkTreeListRow         *self);
GDK_AVAILABLE_IN_ALL
guint                   gtk_tree_list_row_get_depth             (GtkTreeListRow         *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_tree_list_row_set_expanded          (GtkTreeListRow         *self,
                                                                 gboolean                expanded);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_tree_list_row_get_expanded          (GtkTreeListRow         *self);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_tree_list_row_is_expandable         (GtkTreeListRow         *self);

GDK_AVAILABLE_IN_ALL
void
gtk_tree_list_model_dump (GtkTreeListModel *self);


G_END_DECLS

#endif /* __GTK_TREE_LIST_MODEL_H__ */
