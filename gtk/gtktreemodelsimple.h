/* gtktreemodelsimple.h
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

#ifndef __GTK_TREE_MODEL_SIMPLE_H__
#define __GTK_TREE_MODEL_SIMPLE_H__

#include <gtk/gtktreemodel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_TREE_MODEL_SIMPLE			(gtk_tree_model_simple_get_type ())
#define GTK_TREE_MODEL_SIMPLE(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_MODEL_SIMPLE, GtkTreeModelSimple))
#define GTK_TREE_MODEL_SIMPLE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MODEL_SIMPLE, GtkTreeModelSimpleClass))
#define GTK_IS_TREE_MODEL_SIMPLE(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_MODEL_SIMPLE))
#define GTK_IS_TREE_MODEL_SIMPLE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_MODEL_SIMPLE))

typedef struct _GtkTreeModelSimple       GtkTreeModelSimple;
typedef struct _GtkTreeModelSimpleClass  GtkTreeModelSimpleClass;

struct _GtkTreeModelSimple
{
  GtkObject parent;

  gint stamp;
};

struct _GtkTreeModelSimpleClass
{
  GtkObjectClass parent_class;

  /* signals */
  guint        (* get_flags)       (GtkTreeModel *tree_model);   
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
  void         (* ref_iter)        (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  void         (* unref_iter)      (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);

  /* These will be moved into the GtkTreeModelIface eventually */
  void         (* changed)         (GtkTreeModel *tree_model,
				    GtkTreePath  *path,
				    GtkTreeIter  *iter);
  void         (* inserted)        (GtkTreeModel *tree_model,
				    GtkTreePath  *path,
				    GtkTreeIter  *iter);
  void         (* child_toggled)   (GtkTreeModel *tree_model,
				    GtkTreePath  *path,
				    GtkTreeIter  *iter);
  void         (* deleted)         (GtkTreeModel *tree_model,
				    GtkTreePath  *path);
};

GtkType             gtk_tree_model_simple_get_type      (void);
GtkTreeModelSimple *gtk_tree_model_simple_new           (void);
void                gtk_tree_model_simple_changed       (GtkTreeModelSimple *simple,
                                                         GtkTreePath        *path,
                                                         GtkTreeIter        *iter);
void                gtk_tree_model_simple_inserted      (GtkTreeModelSimple *simple,
                                                         GtkTreePath        *path,
                                                         GtkTreeIter        *iter);
void                gtk_tree_model_simple_child_toggled (GtkTreeModelSimple *simple,
                                                         GtkTreePath        *path,
                                                         GtkTreeIter        *iter);
void                gtk_tree_model_simple_deleted       (GtkTreeModelSimple *simple,
                                                         GtkTreePath        *path,
                                                         GtkTreeIter        *iter);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TREE_MODEL_SIMPLE_H__ */
