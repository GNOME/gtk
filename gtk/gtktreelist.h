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

#ifndef __GTK_TREE_LIST_H__
#define __GTK_TREE_LIST_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gio/gio.h>
#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

#define GTK_TYPE_TREE_LIST (gtk_tree_list_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkTreeList, gtk_tree_list, GTK, TREE_LIST, GObject)

typedef GListModel * (* GtkTreeListCreateModelFunc) (gpointer item, gpointer data);

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_tree_list_new                       (GListModel             *root,
                                                         GtkTreeListCreateModelFunc create_func,
                                                         gpointer                data,
                                                         GDestroyNotify          data_destroy);

GDK_AVAILABLE_IN_ALL
guint           gtk_tree_list_get_depth                 (GtkTreeList            *self,
                                                         guint                   position);
GDK_AVAILABLE_IN_ALL
void            gtk_tree_list_set_expanded              (GtkTreeList            *self,
                                                         guint                   position,
                                                         gboolean                expanded);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_tree_list_get_expanded              (GtkTreeList            *self,
                                                         guint                   position);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_tree_list_is_expandable             (GtkTreeList            *self,
                                                         guint                   position);


G_END_DECLS

#endif /* __GTK_TREE_LIST_H__ */
