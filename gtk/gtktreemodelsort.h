/* gtktreemodelsort.h
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

#ifndef __GTK_TREE_MODEL_SORT_H__
#define __GTK_TREE_MODEL_SORT_H__

#include <gtk/gtktreemodel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_MODEL_SORT			(gtk_tree_model_sort_get_type ())
#define GTK_TREE_MODEL_SORT(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_MODEL_SORT, GtkTreeModelSort))
#define GTK_TREE_MODEL_SORT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_MODEL_SORT, GtkTreeModelSortClass))
#define GTK_IS_TREE_MODEL_SORT(obj)			(GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_MODEL_SORT))
#define GTK_IS_TREE_MODEL_SORT_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_MODEL_SORT))

typedef struct _GtkTreeModelSort       GtkTreeModelSort;
typedef struct _GtkTreeModelSortClass  GtkTreeModelSortClass;

typedef gint (* GValueCompareFunc) (const GValue *a,
				    const GValue *b);

struct _GtkTreeModelSort
{
  GtkObject parent;

  /* < private > */
  gpointer root;
  gint stamp;
  guint flags;
  GtkTreeModel *child_model;
  gint sort_col;
  GValueCompareFunc func;
};

struct _GtkTreeModelSortClass
{
  GtkObjectClass parent_class;

  /* signals */
  /* Will be moved into the GtkTreeModelIface eventually */
  void (* changed)       (GtkTreeModel *tree_model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter);
  void (* inserted)      (GtkTreeModel *tree_model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter);
  void (* child_toggled) (GtkTreeModel *tree_model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter);
  void (* deleted)       (GtkTreeModel *tree_model,
			  GtkTreePath  *path);
};


GtkType       gtk_tree_model_sort_get_type       (void);
GtkTreeModel *gtk_tree_model_sort_new            (void);
GtkTreeModel *gtk_tree_model_sort_new_with_model (GtkTreeModel      *child_model,
						  GValueCompareFunc  func,
						  gint               sort_col);
void          gtk_tree_model_sort_set_model      (GtkTreeModelSort  *tree_model_sort,
						  GtkTreeModel      *model);
GtkTreePath  *gtk_tree_model_sort_convert_path   (GtkTreeModelSort  *tree_model_sort,
						  GtkTreePath       *child_path);

/* not implemented */
void          gtk_tree_model_sort_set_sort_column (GtkTreeModelSort  *tree_model_sort,
                                                   gint               sort_col);
void          gtk_tree_model_sort_set_compare     (GtkTreeModelSort  *tree_model_sort,
                                                   GValueCompareFunc  func);
void          gtk_tree_model_sort_convert_iter    (GtkTreeModelSort  *tree_model_sort,
                                                   GtkTreeIter       *sort_iter,
                                                   GtkTreeIter       *child_iter);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_MODEL_SORT_H__ */
