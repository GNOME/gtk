/* gtktreestore.h
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

#ifndef __GTK_TREE_STORE_H__
#define __GTK_TREE_STORE_H__

#include <gtk/gtktreemodel.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_STORE			(gtk_tree_store_get_type ())
#define GTK_TREE_STORE(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_STORE, GtkTreeStore))
#define GTK_TREE_STORE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_STORE, GtkTreeStoreClass))
#define GTK_IS_TREE_STORE(obj)			(GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_STORE))
#define GTK_IS_TREE_STORE_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_STORE))

typedef struct _GtkTreeStore       GtkTreeStore;
typedef struct _GtkTreeStoreClass  GtkTreeStoreClass;

struct _GtkTreeStore
{
  GtkObject parent;

  gint stamp;
  gpointer root;
  gpointer last;
  gint n_columns;
  GType *column_headers;
};

struct _GtkTreeStoreClass
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


GtkType       gtk_tree_store_get_type        (void);
GtkTreeStore *gtk_tree_store_new             (void);

GtkTreeStore *gtk_tree_store_new_with_values (gint          n_columns,
					      ...);
void          gtk_tree_store_set_n_columns   (GtkTreeStore *tree_store,
					      gint          n_columns);
void          gtk_tree_store_set_column_type (GtkTreeStore *store,
					      gint          column,
					      GType         type);
void          gtk_tree_store_set_cell        (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      gint          column,
					      GValue       *value);
void          gtk_tree_store_set             (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      ...);
void          gtk_tree_store_setv            (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      va_list       var_args);
void          gtk_tree_store_get             (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      ...);
void          gtk_tree_store_getv            (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      va_list       var_args);
void          gtk_tree_store_remove          (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter);
void          gtk_tree_store_insert          (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      GtkTreeIter  *parent,
					      gint          position);
void          gtk_tree_store_insert_before   (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      GtkTreeIter  *parent,
					      GtkTreeIter  *sibling);
void          gtk_tree_store_insert_after    (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      GtkTreeIter  *parent,
					      GtkTreeIter  *sibling);
void          gtk_tree_store_prepend         (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      GtkTreeIter  *parent);
void          gtk_tree_store_append          (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      GtkTreeIter  *parent);
void          gtk_tree_store_get_root_iter   (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter);
gboolean      gtk_tree_store_is_ancestor     (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter,
					      GtkTreeIter  *descendant);
gint          gtk_tree_store_iter_depth      (GtkTreeStore *tree_store,
					      GtkTreeIter  *iter);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_STORE_H__ */
