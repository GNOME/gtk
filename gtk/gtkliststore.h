/* gtkliststore.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#ifndef __GTK_LIST_STORE_H__
#define __GTK_LIST_STORE_H__

#include <gtk/gtktreemodel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_LIST_STORE	       (gtk_list_store_get_type ())
#define GTK_LIST_STORE(obj)	       (GTK_CHECK_CAST ((obj), GTK_TYPE_LIST_STORE, GtkListStore))
#define GTK_LIST_STORE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_LISTSTORE, GtkListStoreClass))
#define GTK_IS_LIST_STORE(obj)	       (GTK_CHECK_TYPE ((obj), GTK_TYPE_LIST_STORE))
#define GTK_IS_LIST_STORE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_LIST_STORE))

typedef struct _GtkListStore       GtkListStore;
typedef struct _GtkListStoreClass  GtkListStoreClass;

struct _GtkListStore
{
  GtkObject parent;

  /*< private >*/
  gint stamp;
  gpointer root;
  gint n_columns;
  GType *column_headers;
};

struct _GtkListStoreClass
{
  GtkObjectClass parent_class;

  /* signals */
  /* Will be moved into the GtkTreeModelIface eventually */
  void       (* changed)         (GtkTreeModel *tree_model,
				  GtkTreePath  *path,
				  GtkTreeIter  *iter);
  void       (* inserted)        (GtkTreeModel *tree_model,
				  GtkTreePath  *path,
				  GtkTreeIter  *iter);
  void       (* child_toggled)   (GtkTreeModel *tree_model,
				  GtkTreePath  *path,
				  GtkTreeIter  *iter);
  void       (* deleted)         (GtkTreeModel *tree_model,
				  GtkTreePath  *path);
};

GtkType      gtk_list_store_get_type           (void);
GtkObject   *gtk_list_store_new                (void);
GtkObject   *gtk_list_store_new_with_types     (gint          n_columns,
						...);
void         gtk_list_store_set_n_columns      (GtkListStore *store,
						gint          n_columns);
void         gtk_list_store_set_column_type    (GtkListStore *store,
						gint          column,
						GType         type);
GtkTreeIter *gtk_list_store_node_new           (void);
void         gtk_list_store_node_set_cell      (GtkListStore *store,
						GtkTreeIter  *iter,
						gint          column,
						GValue       *value);
void         gtk_list_store_node_remove        (GtkListStore *store,
						GtkTreeIter  *iter);
GtkTreeIter *gtk_list_store_node_insert        (GtkListStore *store,
						gint          position,
						GtkTreeIter  *iter);
GtkTreeIter *gtk_list_store_node_insert_before (GtkListStore *store,
						GtkTreeIter   sibling,
						GtkTreeIter  *iter);
GtkTreeIter *gtk_list_store_node_prepend       (GtkListStore *store,
						GtkTreeIter  *iter);
GtkTreeIter *gtk_list_store_node_append        (GtkListStore *store,
						GtkTreeIter  *iter);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_LIST_STORE_H__ */
