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
#define GTK_TREE_MODEL(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_MODEL, GtkTreeModel))
#define GTK_TREE_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_MODEL, GtkTreeModelClass))
#define GTK_IS_TREE_MODEL(obj)	       (GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_MODEL))
#define GTK_IS_TREE_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_MODEL))
#define GTK_TREE_MODEL_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TREE_MODEL, GtkTreeModelClass))

typedef gpointer                  GtkTreeNode;
typedef struct _GtkTreePath       GtkTreePath;
typedef struct _GtkTreeModel      GtkTreeModel;
typedef struct _GtkTreeModelClass GtkTreeModelClass;

struct _GtkTreeModel
{
  GtkObject parent;
};

struct _GtkTreeModelClass
{
  GtkObjectClass parent_class;

  /* signals */
  void       (* node_changed)         (GtkTreeModel *tree_model,
				       GtkTreePath  *path,
				       GtkTreeNode  *node);
  void       (* node_inserted)        (GtkTreeModel *tree_model,
				       GtkTreePath  *path,
				       GtkTreeNode  *node);
  void       (* node_child_toggled)   (GtkTreeModel *tree_model,
				       GtkTreePath  *path,
				       GtkTreeNode  *node);
  void       (* node_deleted)         (GtkTreeModel *tree_model,
				       GtkTreePath  *path);

  /* VTable - not signals */
  gint         (* get_n_columns)   (GtkTreeModel *tree_model);
  GtkTreeNode  (* get_node)        (GtkTreeModel *tree_model,
				    GtkTreePath  *path);
  GtkTreePath *(* get_path)        (GtkTreeModel *tree_model,
				    GtkTreeNode   node);
  void         (* node_get_value)  (GtkTreeModel *tree_model,
				    GtkTreeNode   node,
				    gint          column,
				    GValue       *value);
  gboolean     (* node_next)       (GtkTreeModel *tree_model,
				    GtkTreeNode  *node);
  GtkTreeNode  (* node_children)   (GtkTreeModel *tree_model,
				    GtkTreeNode   node);
  gboolean     (* node_has_child)  (GtkTreeModel *tree_model,
				    GtkTreeNode   node);
  gint         (* node_n_children) (GtkTreeModel *tree_model,
				    GtkTreeNode   node);
  GtkTreeNode  (* node_nth_child)  (GtkTreeModel *tree_model,
				    GtkTreeNode   node,
				    gint          n);
  GtkTreeNode  (* node_parent)     (GtkTreeModel *tree_model,
				    GtkTreeNode   node);
};


/* Basic tree_model operations */
GtkType        gtk_tree_model_get_type        (void);

/* GtkTreePath Operations */
GtkTreePath   *gtk_tree_path_new              (void);
GtkTreePath   *gtk_tree_path_new_from_string  (gchar        *path);
gchar         *gtk_tree_path_to_string        (GtkTreePath  *path);
GtkTreePath   *gtk_tree_path_new_root         (void);
void           gtk_tree_path_append_index     (GtkTreePath  *path,
					       gint          index);
void           gtk_tree_path_prepend_index    (GtkTreePath  *path,
					       gint          index);
gint           gtk_tree_path_get_depth        (GtkTreePath  *path);
gint          *gtk_tree_path_get_indices      (GtkTreePath  *path);
void           gtk_tree_path_free             (GtkTreePath  *path);
GtkTreePath   *gtk_tree_path_copy             (GtkTreePath  *path);
gint           gtk_tree_path_compare          (GtkTreePath  *a,
					       GtkTreePath  *b);
void           gtk_tree_path_next             (GtkTreePath  *path);
gint           gtk_tree_path_prev             (GtkTreePath  *path);
gint           gtk_tree_path_up               (GtkTreePath  *path);
void           gtk_tree_path_down             (GtkTreePath  *path);

/* Header operations */
gint           gtk_tree_model_get_n_columns   (GtkTreeModel *tree_model);

/* Node operations */
GtkTreeNode    gtk_tree_model_get_node        (GtkTreeModel *tree_model,
					       GtkTreePath  *path);
GtkTreePath   *gtk_tree_model_get_path        (GtkTreeModel *tree_model,
					       GtkTreeNode   node);
void           gtk_tree_model_node_get_value  (GtkTreeModel *tree_model,
					       GtkTreeNode   node,
					       gint          column,
					       GValue       *value);
gboolean       gtk_tree_model_node_next       (GtkTreeModel *tree_model,
					       GtkTreeNode  *node);
GtkTreeNode    gtk_tree_model_node_children   (GtkTreeModel *tree_model,
					       GtkTreeNode   node);
gboolean       gtk_tree_model_node_has_child  (GtkTreeModel *tree_model,
					       GtkTreeNode   node);
gint           gtk_tree_model_node_n_children (GtkTreeModel *tree_model,
					       GtkTreeNode   node);
GtkTreeNode    gtk_tree_model_node_nth_child  (GtkTreeModel *tree_model,
					       GtkTreeNode   node,
					       gint          n);
GtkTreeNode    gtk_tree_model_node_parent     (GtkTreeModel *tree_model,
					       GtkTreeNode   node);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_MODEL_H__ */
