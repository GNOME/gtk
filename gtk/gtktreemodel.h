/* gtktreemodel.h
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

#ifndef __GTK_TREE_MODEL_H__
#define __GTK_TREE_MODEL_H__

#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_MODEL            (gtk_tree_model_get_type ())
#define GTK_TREE_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TREE_MODEL, GtkTreeModel))
#define GTK_IS_TREE_MODEL(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TREE_MODEL))
#define GTK_TREE_MODEL_GET_IFACE(obj)  ((GtkTreeModelIface *)g_type_interface_peek (((GTypeInstance *)GTK_TREE_MODEL (obj))->g_class, GTK_TYPE_TREE_MODEL))

typedef struct _GtkTreeIter         GtkTreeIter;
typedef struct _GtkTreePath         GtkTreePath;
typedef struct _GtkTreeRowReference GtkTreeRowReference;
typedef struct _GtkTreeModel        GtkTreeModel; /* Dummy typedef */
typedef struct _GtkTreeModelIface   GtkTreeModelIface;
typedef gboolean (* GtkTreeModelForeachFunc) (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);


typedef enum
{
  GTK_TREE_MODEL_ITERS_PERSIST = 1 << 0,
  GTK_TREE_MODEL_LIST_ONLY = 1 << 1
} GtkTreeModelFlags;

struct _GtkTreeIter
{
  gint stamp;
  gpointer user_data;
  gpointer user_data2;
  gpointer user_data3;
};

struct _GtkTreeModelIface
{
  GTypeInterface g_iface;

  /* Signals */
  void         (* row_changed)           (GtkTreeModel *tree_model,
					  GtkTreePath  *path,
					  GtkTreeIter  *iter);
  void         (* row_inserted)          (GtkTreeModel *tree_model,
					  GtkTreePath  *path,
					  GtkTreeIter  *iter);
  void         (* row_has_child_toggled) (GtkTreeModel *tree_model,
					  GtkTreePath  *path,
					  GtkTreeIter  *iter);
  void         (* row_deleted)           (GtkTreeModel *tree_model,
					  GtkTreePath  *path);
  void         (* rows_reordered)        (GtkTreeModel *tree_model,
					  GtkTreePath  *path,
					  GtkTreeIter  *iter,
					  gint         *new_order);

  /* Virtual Table */
  GtkTreeModelFlags (* get_flags)  (GtkTreeModel *tree_model);   

  gint         (* get_n_columns)   (GtkTreeModel *tree_model);
  GType        (* get_column_type) (GtkTreeModel *tree_model,
				    gint          index);
  gboolean     (* get_iter)        (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreePath  *path);
  GtkTreePath *(* get_path)        (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  void         (* get_value)       (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    gint          column,
				    GValue       *value);
  gboolean     (* iter_next)       (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  gboolean     (* iter_children)   (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *parent);
  gboolean     (* iter_has_child)  (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  gint         (* iter_n_children) (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  gboolean     (* iter_nth_child)  (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *parent,
				    gint          n);
  gboolean     (* iter_parent)     (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *child);
  void         (* ref_node)        (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  void         (* unref_node)      (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
};


/* GtkTreePath operations */
GtkTreePath *gtk_tree_path_new              (void);
GtkTreePath *gtk_tree_path_new_from_string  (const gchar       *path);
gchar       *gtk_tree_path_to_string        (GtkTreePath       *path);
GtkTreePath *gtk_tree_path_new_root         (void);
void         gtk_tree_path_append_index     (GtkTreePath       *path,
					     gint               index);
void         gtk_tree_path_prepend_index    (GtkTreePath       *path,
					     gint               index);
gint         gtk_tree_path_get_depth        (GtkTreePath       *path);
gint        *gtk_tree_path_get_indices      (GtkTreePath       *path);
void         gtk_tree_path_free             (GtkTreePath       *path);
GtkTreePath *gtk_tree_path_copy             (GtkTreePath       *path);
gint         gtk_tree_path_compare          (const GtkTreePath *a,
					     const GtkTreePath *b);
void         gtk_tree_path_next             (GtkTreePath       *path);
gboolean     gtk_tree_path_prev             (GtkTreePath       *path);
gboolean     gtk_tree_path_up               (GtkTreePath       *path);
void         gtk_tree_path_down             (GtkTreePath       *path);

gboolean     gtk_tree_path_is_ancestor      (GtkTreePath       *path,
                                             GtkTreePath       *descendant);
gboolean     gtk_tree_path_is_descendant    (GtkTreePath       *path,
                                             GtkTreePath       *ancestor);

/* Row reference (an object that tracks model changes so it refers to the same
 * row always; a path refers to a position, not a fixed row).  You almost always
 * want to call gtk_tree_row_reference_new.
 */

GtkTreeRowReference *gtk_tree_row_reference_new       (GtkTreeModel        *model,
						       GtkTreePath         *path);
GtkTreeRowReference *gtk_tree_row_reference_new_proxy (GObject             *proxy,
						       GtkTreeModel        *model,
						       GtkTreePath         *path);
GtkTreePath         *gtk_tree_row_reference_get_path  (GtkTreeRowReference *reference);
gboolean             gtk_tree_row_reference_valid     (GtkTreeRowReference *reference);
void                 gtk_tree_row_reference_free      (GtkTreeRowReference *reference);
/* These two functions are only needed if you created the row reference with a
 * proxy object */
void                 gtk_tree_row_reference_inserted  (GObject     *proxy,
						       GtkTreePath *path);
void                 gtk_tree_row_reference_deleted   (GObject     *proxy,
						       GtkTreePath *path);
void                 gtk_tree_row_reference_reordered (GObject     *proxy,
						       GtkTreePath *path,
						       GtkTreeIter *iter,
						       gint        *new_order);
						       
/* GtkTreeIter operations */
GtkTreeIter *     gtk_tree_iter_copy             (GtkTreeIter  *iter);
void              gtk_tree_iter_free             (GtkTreeIter  *iter);

GtkType           gtk_tree_model_get_type        (void) G_GNUC_CONST;
GtkTreeModelFlags gtk_tree_model_get_flags       (GtkTreeModel *tree_model);
gint              gtk_tree_model_get_n_columns   (GtkTreeModel *tree_model);
GType             gtk_tree_model_get_column_type (GtkTreeModel *tree_model,
						  gint          index);


/* Iterator movement */
gboolean          gtk_tree_model_get_iter        (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter,
						  GtkTreePath  *path);
gboolean          gtk_tree_model_get_iter_from_string (GtkTreeModel *tree_model,
						       GtkTreeIter  *iter,
						       const gchar  *path_string);
gboolean          gtk_tree_model_get_iter_root   (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter);
GtkTreePath *     gtk_tree_model_get_path        (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter);
void              gtk_tree_model_get_value       (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter,
						  gint          column,
						  GValue       *value);
gboolean          gtk_tree_model_iter_next       (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter);
gboolean          gtk_tree_model_iter_children   (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter,
						  GtkTreeIter  *parent);
gboolean          gtk_tree_model_iter_has_child  (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter);
gint              gtk_tree_model_iter_n_children (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter);
gboolean          gtk_tree_model_iter_nth_child  (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter,
						  GtkTreeIter  *parent,
						  gint          n);
gboolean          gtk_tree_model_iter_parent     (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter,
						  GtkTreeIter  *child);
void              gtk_tree_model_ref_node        (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter);
void              gtk_tree_model_unref_node      (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter);
void              gtk_tree_model_get             (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter,
						  ...);
void              gtk_tree_model_get_valist      (GtkTreeModel *tree_model,
						  GtkTreeIter  *iter,
						  va_list       var_args);


void              gtk_tree_model_foreach         (GtkTreeModel            *model,
						  GtkTreeModelForeachFunc  func,
						  gpointer                 user_data);


/* Signals */
void gtk_tree_model_row_changed           (GtkTreeModel *tree_model,
					   GtkTreePath  *path,
					   GtkTreeIter  *iter);
void gtk_tree_model_row_inserted          (GtkTreeModel *tree_model,
					   GtkTreePath  *path,
					   GtkTreeIter  *iter);
void gtk_tree_model_row_has_child_toggled (GtkTreeModel *tree_model,
					   GtkTreePath  *path,
					   GtkTreeIter  *iter);
void gtk_tree_model_row_deleted           (GtkTreeModel *tree_model,
					   GtkTreePath  *path);
void gtk_tree_model_rows_reordered        (GtkTreeModel *tree_model,
					   GtkTreePath  *path,
					   GtkTreeIter  *iter,
					   gint         *new_order);




#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_MODEL_H__ */
