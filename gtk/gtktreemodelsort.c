/* gtktreemodelsort.c
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


/* NOTE: There is a potential for confusion in this code as to whether an iter,
 * path or value refers to the GtkTreeModelSort model, or the child model being
 * sorted.  As a convention, variables referencing the child model will have an
 * s_ prefix before them (ie. s_iter, s_value, s_path);
 */

#include "gtktreemodelsort.h"
#include "gtksignal.h"
#include <string.h>

enum {
  CHANGED,
  INSERTED,
  CHILD_TOGGLED,
  DELETED,
  LAST_SIGNAL
};

typedef struct _SortElt SortElt;
struct _SortElt
{
  GtkTreeIter iter;
  SortElt *parent;
  GArray *children;
  gint ref;
  gint offset;
};

typedef struct _SortData SortData;
struct _SortData
{
  GtkTreeModel *model;
  gint sort_col;
  GValueCompareFunc func;
};

static guint tree_model_sort_signals[LAST_SIGNAL] = { 0 };


#define get_array(e,t) ((GArray *)((e)->parent?(e)->parent->children:GTK_TREE_MODEL_SORT(t)->root))


static void         gtk_tree_model_sort_init            (GtkTreeModelSort      *tree_model_sort);
static void         gtk_tree_model_sort_class_init      (GtkTreeModelSortClass *tree_model_sort_class);
static void         gtk_tree_model_sort_tree_model_init (GtkTreeModelIface     *iface);
static void         gtk_tree_model_sort_finalize        (GObject               *object);
static void         gtk_tree_model_sort_changed         (GtkTreeModel          *model,
							 GtkTreePath           *path,
							 GtkTreeIter           *iter,
							 gpointer               data);
static void         gtk_tree_model_sort_inserted        (GtkTreeModel          *model,
							 GtkTreePath           *path,
							 GtkTreeIter           *iter,
							 gpointer               data);
static void         gtk_tree_model_sort_child_toggled   (GtkTreeModel          *model,
							 GtkTreePath           *path,
							 GtkTreeIter           *iter,
							 gpointer               data);
static void         gtk_tree_model_sort_deleted         (GtkTreeModel          *model,
							 GtkTreePath           *path,
							 gpointer               data);
static gint         gtk_tree_model_sort_get_n_columns   (GtkTreeModel          *tree_model);
static GType        gtk_tree_model_sort_get_column_type (GtkTreeModel          *tree_model,
							 gint                   index);
static gboolean     gtk_tree_model_sort_get_iter        (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter,
							 GtkTreePath           *path);
static GtkTreePath *gtk_tree_model_sort_get_path        (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter);
static void         gtk_tree_model_sort_get_value       (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter,
							 gint                   column,
							 GValue                *value);
static gboolean     gtk_tree_model_sort_iter_next       (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter);
static gboolean     gtk_tree_model_sort_iter_children   (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter,
							 GtkTreeIter           *parent);
static gboolean     gtk_tree_model_sort_iter_has_child  (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter);
static gint         gtk_tree_model_sort_iter_n_children (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter);
static gboolean     gtk_tree_model_sort_iter_nth_child  (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter,
							 GtkTreeIter           *parent,
							 gint                   n);
static gboolean     gtk_tree_model_sort_iter_parent     (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter,
							 GtkTreeIter           *child);
static void         gtk_tree_model_sort_ref_iter        (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter);
static void         gtk_tree_model_sort_unref_iter      (GtkTreeModel          *tree_model,
							 GtkTreeIter           *iter);

/* Internal functions */
static gint         gtk_tree_model_sort_array_find_insert (GtkTreeModelSort *tree_model_sort,
							   GArray           *array,
							   GtkTreeIter      *iter,
							   gboolean          skip_sort_elt);
static GtkTreePath *gtk_tree_model_sort_convert_path_real (GtkTreeModelSort *tree_model_sort,
							   GtkTreePath      *child_path,
							   gboolean          build_children);
static void         gtk_tree_model_sort_build_level       (GtkTreeModelSort *tree_model_sort,
							   SortElt          *place);
static void         gtk_tree_model_sort_free_level        (GArray           *array);
static GFunc        gtk_tree_model_sort_get_func          (GtkTreeModelSort *tree_model_sort);
static gint         gtk_tree_model_sort_func              (gconstpointer     a,
							   gconstpointer     b,
							   gpointer          user_data);
static gint         g_value_string_compare_func           (const GValue     *a,
							   const GValue     *b);
static gint         g_value_int_compare_func              (const GValue     *a,
							   const GValue     *b);



GtkType
gtk_tree_model_sort_get_type (void)
{
  static GtkType tree_model_sort_type = 0;

  if (!tree_model_sort_type)
    {
      static const GTypeInfo tree_model_sort_info =
      {
        sizeof (GtkTreeModelSortClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_tree_model_sort_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkTreeModelSort),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_tree_model_sort_init
      };

      static const GInterfaceInfo tree_model_info =
      {
	(GInterfaceInitFunc) gtk_tree_model_sort_tree_model_init,
	NULL,
	NULL
      };

      tree_model_sort_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkTreeModelSort", &tree_model_sort_info, 0);
      g_type_add_interface_static (tree_model_sort_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
    }

  return tree_model_sort_type;
}

static void
gtk_tree_model_sort_class_init (GtkTreeModelSortClass *tree_model_sort_class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) tree_model_sort_class;

  object_class->finalize = gtk_tree_model_sort_finalize;

  tree_model_sort_signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeModelSortClass, changed),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_model_sort_signals[INSERTED] =
    gtk_signal_new ("inserted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeModelSortClass, inserted),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_model_sort_signals[CHILD_TOGGLED] =
    gtk_signal_new ("child_toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeModelSortClass, child_toggled),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_model_sort_signals[DELETED] =
    gtk_signal_new ("deleted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeModelSortClass, deleted),
                    gtk_marshal_VOID__POINTER,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
}

static void
gtk_tree_model_sort_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = gtk_tree_model_sort_get_n_columns;
  iface->get_column_type = gtk_tree_model_sort_get_column_type;
  iface->get_iter = gtk_tree_model_sort_get_iter;
  iface->get_path = gtk_tree_model_sort_get_path;
  iface->get_value = gtk_tree_model_sort_get_value;
  iface->iter_next = gtk_tree_model_sort_iter_next;
  iface->iter_children = gtk_tree_model_sort_iter_children;
  iface->iter_has_child = gtk_tree_model_sort_iter_has_child;
  iface->iter_n_children = gtk_tree_model_sort_iter_n_children;
  iface->iter_nth_child = gtk_tree_model_sort_iter_nth_child;
  iface->iter_parent = gtk_tree_model_sort_iter_parent;
  iface->ref_iter = gtk_tree_model_sort_ref_iter;
  iface->unref_iter = gtk_tree_model_sort_unref_iter;
}

static void
gtk_tree_model_sort_init (GtkTreeModelSort *tree_model_sort)
{
  tree_model_sort->stamp = g_random_int ();
}

GtkTreeModel *
gtk_tree_model_sort_new (void)
{
  return GTK_TREE_MODEL (gtk_type_new (gtk_tree_model_sort_get_type ()));
}

GtkTreeModel *
gtk_tree_model_sort_new_with_model (GtkTreeModel      *child_model,
				    GValueCompareFunc  func,
				    gint               sort_col)
{
  GtkTreeModel *retval;

  retval = gtk_tree_model_sort_new ();
  gtk_tree_model_sort_set_model (GTK_TREE_MODEL_SORT (retval), child_model);

  GTK_TREE_MODEL_SORT (retval)->func = func;
  GTK_TREE_MODEL_SORT (retval)->sort_col = sort_col;
  return retval;
}

/**
 * gtk_tree_model_sort_set_model:
 * @tree_model_sort: The #GtkTreeModelSort.
 * @child_model: A #GtkTreeModel, or NULL.
 * 
 * Sets the model of @tree_model_sort to be @model.  If @model is NULL, then the
 * old model is unset.
 **/
void
gtk_tree_model_sort_set_model (GtkTreeModelSort *tree_model_sort,
			       GtkTreeModel     *child_model)
{
  g_return_if_fail (tree_model_sort != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));

  if (child_model)
    g_object_ref (G_OBJECT (child_model));

  if (tree_model_sort->child_model)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_model_sort->child_model),
				     gtk_tree_model_sort_changed,
				     tree_model_sort);
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_model_sort->child_model),
				     gtk_tree_model_sort_inserted,
				     tree_model_sort);
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_model_sort->child_model),
				     gtk_tree_model_sort_child_toggled,
				     tree_model_sort);
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_model_sort->child_model),
				     gtk_tree_model_sort_deleted,
				     tree_model_sort);

      g_object_unref (G_OBJECT (tree_model_sort->child_model));
    }

  tree_model_sort->child_model = child_model;

  if (child_model)
    {
      gtk_signal_connect (GTK_OBJECT (child_model),
			  "changed",
			  gtk_tree_model_sort_changed,
			  tree_model_sort);
      gtk_signal_connect (GTK_OBJECT (child_model),
			  "inserted",
			  gtk_tree_model_sort_inserted,
			  tree_model_sort);
      gtk_signal_connect (GTK_OBJECT (child_model),
			  "child_toggled",
			  gtk_tree_model_sort_child_toggled,
			  tree_model_sort);
      gtk_signal_connect (GTK_OBJECT (child_model),
			  "deleted",
			  gtk_tree_model_sort_deleted,
			  tree_model_sort);
      tree_model_sort->flags = gtk_tree_model_get_flags (child_model);
    }
}

/**
 * gtk_tree_model_sort_convert_path:
 * @tree_model_sort: The #GtkTreeModelSort.
 * @path: A #GtkTreePath, relative to the child model.
 * 
 * Converts the @path to a new path, relative to the sorted position.  In other
 * words, the value found in the @tree_model_sort ->child_model at the @path, is
 * identical to that found in the @tree_model_sort and the return value.
 * 
 * Return value: A new path, or NULL if @path does not exist in @tree_model_sort
 * ->child_model.
 **/
GtkTreePath *
gtk_tree_model_sort_convert_path (GtkTreeModelSort *tree_model_sort,
				  GtkTreePath      *child_path)
{
  g_return_val_if_fail (tree_model_sort != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort), NULL);
  g_return_val_if_fail (child_path != NULL, NULL);

  return gtk_tree_model_sort_convert_path_real (tree_model_sort, child_path, TRUE);
}

static void
gtk_tree_model_sort_finalize (GObject *object)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) object;

  if (tree_model_sort->root)
    gtk_tree_model_sort_free_level (tree_model_sort->root);

  g_object_unref (G_OBJECT (tree_model_sort->child_model));
}

static void
gtk_tree_model_sort_changed (GtkTreeModel *s_model,
			     GtkTreePath  *s_path,
			     GtkTreeIter  *s_iter,
			     gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  SortElt *elt;
  GArray *array;
  gboolean free_s_path = FALSE;
  gint index;

  g_return_if_fail (s_path != NULL || s_iter != NULL);

  if (s_path == NULL)
    {
      free_s_path = TRUE;
      s_path = gtk_tree_model_get_path (s_model, s_iter);
    }

  path = gtk_tree_model_sort_convert_path_real (tree_model_sort, s_path, FALSE);
  if (path == NULL)
    {
      if (free_s_path)
	gtk_tree_path_free (s_path);
      return;
    }

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  elt = (SortElt *) iter.tree_node;
  array = get_array (elt, tree_model_sort);

  /* FIXME: as an optimization for when the column other then the one we're
   * sorting is changed, we can check the prev and next element to see if
   * they're different.
   */
  
  /* Now we need to resort things. */
  index = gtk_tree_model_sort_array_find_insert (tree_model_sort,
						 array,
						 (GtkTreeIter *) elt,
						 TRUE);
  g_print ("index is %d\n", index);
  gtk_signal_emit_by_name (GTK_OBJECT (data), "changed", path, &iter);

  gtk_tree_path_free (path);
  if (free_s_path)
    gtk_tree_path_free (s_path);
}

/* FALSE if the value was inserted, TRUE otherwise */
static gboolean
gtk_tree_model_sort_insert_value (GtkTreeModelSort *sort,
				  GtkTreePath      *s_path,
				  GtkTreeIter      *s_iter)
{
  GtkTreePath *tmp_path;
  GArray *array;
  gint index;
  GtkTreeIter iter;
  SortElt elt;
  gint offset;
  gint j;
  SortElt *tmp_elt;
  offset = gtk_tree_path_get_indices (s_path)[gtk_tree_path_get_depth (s_path) - 1];

  elt.iter = *s_iter;
  elt.ref = 0;
  elt.children = NULL;
  elt.offset = offset;

  tmp_path = gtk_tree_path_copy (s_path);

  if (gtk_tree_path_up (tmp_path))
    {
      GtkTreePath *parent_path;

      parent_path = gtk_tree_model_sort_convert_path_real (sort, tmp_path, FALSE);
      if (parent_path == NULL)
	{
	  gtk_tree_path_free (tmp_path);
	  return FALSE;
	}
      gtk_tree_model_get_iter (GTK_TREE_MODEL (sort), &iter, parent_path);
      elt.parent = ((SortElt *) iter.tree_node);
      array = ((SortElt *) iter.tree_node)->children;
      gtk_tree_path_free (parent_path);
      if (array == NULL)
	{
	  gtk_tree_path_free (tmp_path);
	  return FALSE;
	}
    }
  else
    {
      if (sort->root == NULL)
	sort->root = g_array_sized_new (FALSE, FALSE, sizeof (SortElt), 1);
      array = sort->root;
      elt.parent = NULL;
    }
  gtk_tree_path_free (tmp_path);

  index = gtk_tree_model_sort_array_find_insert (sort, array, (GtkTreeIter *) &elt, FALSE);

  g_array_insert_vals (array, index, &elt, 1);

  /* update all the larger offsets */
  tmp_elt = (SortElt *) array->data;
  for (j = 0; j < array->len; j++, tmp_elt++)
	{
	  if ((tmp_elt->offset >= offset) &&
	      j != index)
	    tmp_elt->offset ++;
	}

  return TRUE;
}

static void
gtk_tree_model_sort_inserted (GtkTreeModel *s_model,
			      GtkTreePath  *s_path,
			      GtkTreeIter  *s_iter,
			      gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (s_path != NULL || s_iter != NULL);

  if (s_path == NULL)
    s_path = gtk_tree_model_get_path (s_model, s_iter);

  if (!(tree_model_sort->flags & GTK_TREE_MODEL_ITERS_PERSIST))
    {
      gtk_tree_model_sort_free_level ((GArray *) tree_model_sort->root);
      tree_model_sort->root = NULL;
    }
  else
    {
      GtkTreeIter real_s_iter;

      if (s_iter == NULL)
	gtk_tree_model_get_iter (s_model, &real_s_iter, s_path);
      else
	real_s_iter = (* s_iter);

      if (!gtk_tree_model_sort_insert_value (tree_model_sort, s_path, &real_s_iter))
	return;
    }

  path = gtk_tree_model_sort_convert_path_real (tree_model_sort, s_path, FALSE);
  if (path == NULL)
    return;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_signal_emit_by_name (GTK_OBJECT (data), "inserted", path, &iter);
  gtk_tree_path_free (path);
}

static void
gtk_tree_model_sort_child_toggled (GtkTreeModel *s_model,
				   GtkTreePath  *s_path,
				   GtkTreeIter  *s_iter,
				   gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean free_s_path = FALSE;

  g_return_if_fail (s_path != NULL || s_iter != NULL);

  if (!(tree_model_sort->flags & GTK_TREE_MODEL_ITERS_PERSIST))
    {
      gtk_tree_model_sort_free_level ((GArray *)tree_model_sort->root);
      tree_model_sort->root = NULL;
    }

  if (s_path == NULL)
    {
      s_path = gtk_tree_model_get_path (s_model, s_iter);
      free_s_path = TRUE;
    }

  path = gtk_tree_model_sort_convert_path_real (tree_model_sort, s_path, FALSE);
  if (path == NULL)
    return;
  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_signal_emit_by_name (GTK_OBJECT (data),
			   "child_toggled",
			   path, &iter);
  gtk_tree_path_free (path);
  if (free_s_path)
    gtk_tree_path_free (s_path);
}

static void
gtk_tree_model_sort_deleted (GtkTreeModel *s_model,
			     GtkTreePath  *s_path,
			     gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;

  g_return_if_fail (s_path != NULL);
  path = gtk_tree_model_sort_convert_path (tree_model_sort, s_path);
  g_return_if_fail (path != NULL);

  if (!(tree_model_sort->flags & GTK_TREE_MODEL_ITERS_PERSIST))
    {
      gtk_tree_model_sort_free_level ((GArray *)tree_model_sort->root);
      tree_model_sort->root = NULL;
    }
  else
    {
      GArray *array;
      GtkTreeIter iter;
      SortElt *elt;
      gint offset;
      gint i;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort), &iter, path);
      elt = (SortElt *) iter.tree_node;
      offset = elt->offset;
      array = get_array (elt, tree_model_sort);
      if (array->len == 1)
	{
	  if (((SortElt *)array->data)->parent == NULL)
	    tree_model_sort->root = NULL;
	  else
	    (((SortElt *)array->data)->parent)->children = NULL;
	  gtk_tree_model_sort_free_level (array);
	}
      else
	{
	  g_array_remove_index (array, elt - ((SortElt *) array->data));

	  for (i = 0; i < array->len; i++)
	    {
	      elt = & (g_array_index (array, SortElt, i));
	      if (elt->offset > offset)
		elt->offset--;
	    }
	}
    }

  tree_model_sort->stamp++;
  gtk_signal_emit_by_name (GTK_OBJECT (data), "deleted", path);
  gtk_tree_path_free (path);
}

static gint
gtk_tree_model_sort_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, 0);

  return gtk_tree_model_get_n_columns (GTK_TREE_MODEL_SORT (tree_model)->child_model);
}

static GType
gtk_tree_model_sort_get_column_type (GtkTreeModel *tree_model,
				     gint          index)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), G_TYPE_INVALID);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, G_TYPE_INVALID);

  return gtk_tree_model_get_column_type (GTK_TREE_MODEL_SORT (tree_model)->child_model, index);
}

static gboolean
gtk_tree_model_sort_get_iter_helper (GtkTreeModelSort *tree_model_sort,
				     GArray       *array,
				     GtkTreeIter  *iter,
				     gint          depth,
				     GtkTreePath  *path)
{
  SortElt *elt;

  if (array == NULL)
    return FALSE;

  if (gtk_tree_path_get_indices (path)[depth] > array->len)
    return FALSE;

  elt = & (g_array_index (array, SortElt, gtk_tree_path_get_indices (path)[depth]));

  if (depth == gtk_tree_path_get_depth (path) - 1)
    {
      iter->stamp = tree_model_sort->stamp;
      iter->tree_node = elt;
      return TRUE;
    }

  if (elt->children != NULL)
    return gtk_tree_model_sort_get_iter_helper (tree_model_sort,
						elt->children,
						iter,
						depth + 1,
						path);

  if (gtk_tree_model_iter_has_child (tree_model_sort->child_model,
				     &(elt->iter)))

    gtk_tree_model_sort_build_level (tree_model_sort, elt);

  return gtk_tree_model_sort_get_iter_helper (tree_model_sort,
					      elt->children,
					      iter,
					      depth + 1,
					      path);
}

static gboolean
gtk_tree_model_sort_get_iter (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreePath  *path)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);

  if (GTK_TREE_MODEL_SORT (tree_model)->root == NULL)
    gtk_tree_model_sort_build_level (GTK_TREE_MODEL_SORT (tree_model), NULL);

  return gtk_tree_model_sort_get_iter_helper (GTK_TREE_MODEL_SORT (tree_model),
					      GTK_TREE_MODEL_SORT (tree_model)->root,
					      iter, 0, path);
}

static GtkTreePath *
gtk_tree_model_sort_get_path (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, NULL);

  return gtk_tree_model_get_path (GTK_TREE_MODEL_SORT (tree_model)->child_model, iter);
}

static void
gtk_tree_model_sort_get_value (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       gint          column,
			       GValue        *value)
{
  SortElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp);

  elt = iter->tree_node;

  gtk_tree_model_get_value (GTK_TREE_MODEL_SORT (tree_model)->child_model, (GtkTreeIter *)elt, column, value);
}

static gboolean
gtk_tree_model_sort_iter_next (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  GArray *array;
  SortElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, FALSE);

  elt = iter->tree_node;
  array = get_array (elt, tree_model);

  if (elt - ((SortElt*) array->data) >=  array->len - 1)
    {
      iter->stamp = 0;
      return FALSE;
    }
  iter->tree_node = elt + 1;
  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_children (GtkTreeModel *tree_model,
				   GtkTreeIter  *iter,
				   GtkTreeIter  *parent)
{
  SortElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);

  if (parent)
    g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == parent->stamp, FALSE);

  if (parent)
    elt = parent->tree_node;
  else
    elt = (SortElt *) ((GArray *)GTK_TREE_MODEL_SORT (tree_model)->root)->data;

  if (elt == NULL)
    return FALSE;

  if (elt->children == NULL &&
      gtk_tree_model_iter_has_child (GTK_TREE_MODEL_SORT (tree_model)->child_model, (GtkTreeIter *)elt))
    gtk_tree_model_sort_build_level (GTK_TREE_MODEL_SORT (tree_model), elt);

  if (elt->children == NULL)
    return FALSE;

  iter->stamp = GTK_TREE_MODEL_SORT (tree_model)->stamp;
  iter->tree_node = elt->children->data;

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_has_child (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter)
{
  SortElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, FALSE);

  elt = iter->tree_node;
  if (elt->children)
    return TRUE;

  return gtk_tree_model_iter_has_child (GTK_TREE_MODEL_SORT (tree_model)->child_model, (GtkTreeIter *) elt);
}

static gint
gtk_tree_model_sort_iter_n_children (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter)
{
  SortElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, 0);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, 0);

  elt = iter->tree_node;
  if (elt->children)
    return elt->children->len;

  return gtk_tree_model_iter_n_children (GTK_TREE_MODEL_SORT (tree_model)->child_model, (GtkTreeIter *) elt);
}


static gboolean
gtk_tree_model_sort_iter_nth_child (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *parent,
				    gint          n)
{
  SortElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  if (parent)
    g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == parent->stamp, FALSE);

  elt = iter->tree_node;


  if (elt->children == NULL)
    {
      if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL_SORT (tree_model)->child_model, (GtkTreeIter *)elt))
	gtk_tree_model_sort_build_level (GTK_TREE_MODEL_SORT (tree_model), elt);
      else
	return FALSE;
    }

  if (elt->children == NULL)
    return FALSE;

  if (n >= elt->children->len)
    return FALSE;

  iter->stamp = GTK_TREE_MODEL_SORT (tree_model)->stamp;
  iter->tree_node = &g_array_index (elt->children, SortElt, n);

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_parent (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 GtkTreeIter  *child)
{
  SortElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == child->stamp, FALSE);

  elt = iter->tree_node;
  if (elt->parent)
    {
      iter->stamp = GTK_TREE_MODEL_SORT (tree_model)->stamp;
      iter->tree_node = elt->parent;

      return TRUE;
    }
  return FALSE;
}

static void
gtk_tree_model_sort_ref_iter (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter)
{
}

static void
gtk_tree_model_sort_unref_iter (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{

}

/* Internal functions */

static gint
gtk_tree_model_sort_array_find_insert (GtkTreeModelSort *tree_model_sort,
				       GArray           *array,
				       GtkTreeIter      *iter,
				       gboolean          skip_sort_elt)
{
  gint middle;
  gint cmp;
  GValueCompareFunc func;
  GValue value = {0, };
  GValue tmp_value = {0, };
  SortElt *tmp_elt;

  func = (GValueCompareFunc) gtk_tree_model_sort_get_func (tree_model_sort);

  g_return_val_if_fail (func != NULL, 0);

  gtk_tree_model_get_value (tree_model_sort->child_model, iter, tree_model_sort->sort_col, &value);

  for (middle = 0; middle < array->len; middle++)
    {
      tmp_elt = &(g_array_index (array, SortElt, middle));
      if (!skip_sort_elt &&
	  (SortElt *) iter == tmp_elt)
	  continue;
      gtk_tree_model_get_value (tree_model_sort->child_model,
				(GtkTreeIter *) tmp_elt,
				tree_model_sort->sort_col,
				&tmp_value);

      cmp = ((func) (&value, &tmp_value));
      g_value_unset (&tmp_value);

      if (cmp >= 0)
	break;
    }
  return middle;
}


static GtkTreePath *
gtk_tree_model_sort_convert_path_real (GtkTreeModelSort *tree_model_sort,
				       GtkTreePath      *child_path,
				       gboolean          build_children)
{
  GtkTreePath *retval;
  GArray *array;
  gint *indices;
  gint i = 0;

  if (tree_model_sort->root == NULL)
    {
      if (build_children)
	gtk_tree_model_sort_build_level (tree_model_sort, NULL);
      else
	return FALSE;
    }

  retval = gtk_tree_path_new ();
  array = (GArray *) tree_model_sort->root;
  indices = gtk_tree_path_get_indices (child_path);

  do
    {
      SortElt *elt;
      gboolean found = FALSE;
      gint j;

      if ((array->len < indices[i]) || (array == NULL))
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}

      elt = (SortElt *) array->data;
      for (j = 0; j < array->len; j++, elt++)
	{
	  if (elt->offset == indices[i])
	    {
	      found = TRUE;
	      break;
	    }
	}
      if (! found)
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}

      gtk_tree_path_prepend_index (retval, j);

      i++;

      if (i == gtk_tree_path_get_depth (child_path))
	break;

      if (elt->children == NULL)
	{
	  if (build_children)
	    {
	      gtk_tree_path_prepend_index (retval, j);
	      gtk_tree_model_sort_build_level (tree_model_sort, elt);
	    }
	  else
	    {
	      gtk_tree_path_free (retval);
	      return NULL;
	    }
	}
    }
  while (TRUE);

  return retval;
}
  
static void
gtk_tree_model_sort_build_level (GtkTreeModelSort *tree_model_sort,
				 SortElt          *place)
{
  gint n, i = 0;
  GArray *children;
  GtkTreeIter *parent_iter = NULL;
  GtkTreeIter iter;
  SortElt elt;
  SortData sort_data;

  if (place)
    parent_iter = & (place->iter);

      
  n = gtk_tree_model_iter_n_children (tree_model_sort->child_model, parent_iter);

  if (n == 0)
    return;

  children = g_array_sized_new (FALSE, FALSE, sizeof (SortElt), n);

  if (place)
    place->children = children;
  else
    tree_model_sort->root = children;

  gtk_tree_model_iter_children (tree_model_sort->child_model,
				&iter,
				parent_iter);

  do
    {
      elt.iter = iter;
      elt.parent = place;
      elt.children = NULL;
      elt.ref = 0;
      elt.offset = i;

      g_array_append_vals (children, &elt, 1);
      i++;
    }
  while (gtk_tree_model_iter_next (tree_model_sort->child_model, &iter));

  sort_data.func = (GValueCompareFunc) gtk_tree_model_sort_get_func (tree_model_sort);
  sort_data.model = tree_model_sort->child_model;
  sort_data.sort_col = tree_model_sort->sort_col;

  g_array_sort_with_data (children, gtk_tree_model_sort_func, &sort_data);
}

static void
gtk_tree_model_sort_free_level (GArray *array)
{
  gint i;

  if (array == NULL)
    return;

  for (i = 0; i < array->len; i++)
    {
      SortElt *elt;

      elt = &g_array_index (array, SortElt, i);
      if (elt->children)
	gtk_tree_model_sort_free_level (array);
    }

  g_array_free (array, TRUE);
}

static GFunc
gtk_tree_model_sort_get_func (GtkTreeModelSort *tree_model_sort)
{
  GValueCompareFunc func;
  if (tree_model_sort->func)
    func = tree_model_sort->func;
  else
    {
      switch (gtk_tree_model_get_column_type (tree_model_sort->child_model,
					      tree_model_sort->sort_col))
	{
	case G_TYPE_STRING:
	  func = &g_value_string_compare_func;
	  break;
	case G_TYPE_INT:
	  func = &g_value_int_compare_func;
	  break;
	default:
	  g_warning ("No comparison function for row %d (Type %s)\n",
		     tree_model_sort->sort_col,
		     g_type_name (gtk_tree_model_get_column_type (tree_model_sort->child_model,
								  tree_model_sort->sort_col)));
	  return NULL;
	}

    }
  return (GFunc) func;
}

static gint
gtk_tree_model_sort_func  (gconstpointer a,
			   gconstpointer b,
			   gpointer      user_data)
{
  GValue value_a = {0, };
  GValue value_b = {0, };
  SortData *sort_data = user_data;
  gint retval;

  gtk_tree_model_get_value (sort_data->model, (GtkTreeIter *) a, sort_data->sort_col, &value_a);
  gtk_tree_model_get_value (sort_data->model, (GtkTreeIter *) b, sort_data->sort_col, &value_b);

  retval = (sort_data->func) (&value_a, &value_b);

  g_value_unset (&value_a);
  g_value_unset (&value_b);

  return retval;
}
  

static gint
g_value_string_compare_func (const GValue *a,
			     const GValue *b)
{
  gchar *a_str = g_value_get_string (a);
  gchar *b_str = g_value_get_string (b);

  if (b_str == NULL)
    return a_str == NULL;
  if (a_str == NULL)
    return -1;

  return strcmp (a_str, b_str);
}

static gint
g_value_int_compare_func (const GValue *a,
			  const GValue *b)
{
  return g_value_get_int (a) < g_value_get_int (b);
}


/* DEAD CODE */

#if 0
  /* FIXME: we can, as we are an array, do binary search to find the correct
   * location to insert the element.  However, I'd rather get it working.  The
   * below is quite wrong, but a step in the right direction.
   */
  low = 0;
  high = array->len;
  middle = (low + high)/2;

  /* Insert the value into the array */
  while (low != high)
    {
      gint cmp;
      tmp_elt = &(g_array_index (array, SortElt, middle));
      gtk_tree_model_get_value (sort->child_model,
				(GtkTreeIter *) tmp_elt,
				sort->sort_col,
				&tmp_value);

      cmp = ((func) (&tmp_value, &s_value));
      g_value_unset (&tmp_value);

      if (cmp < 0)
	high = middle;
      else if (cmp > 0)
	low = middle;
      else if (cmp == 0)
	break;
      middle = (low + high)/2;
    }
#endif
