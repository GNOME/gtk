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
#include "gtktreesortable.h"
#include "gtktreestore.h"
#include "gtksignal.h"
#include "gtktreedatalist.h"
#include <string.h>


typedef struct _SortElt SortElt;
struct _SortElt
{
  GtkTreeIter           iter;
  SortElt              *parent;
  GArray               *children;
  gint                  offset;
  gint                  ref;
};

typedef struct _SortData SortData;
struct _SortData
{
  GtkTreeModelSort *tree_model_sort;
  GtkTreePath *parent_a;
  GtkTreePath *parent_b;
};

typedef struct _SortTuple SortTuple;
struct _SortTuple
{
  SortElt   *elt;
  GArray    *children;
  gint       offset;  
};

#define get_array(e,t) ((GArray *)((e)->parent?(e)->parent->children:GTK_TREE_MODEL_SORT(t)->root))

/* object init and signal handlers */
static void gtk_tree_model_sort_init                  (GtkTreeModelSort      *tree_model_sort);
static void gtk_tree_model_sort_class_init            (GtkTreeModelSortClass *tree_model_sort_class);
static void gtk_tree_model_sort_tree_model_init       (GtkTreeModelIface     *iface);
static void gtk_tree_model_sort_tree_sortable_init    (GtkTreeSortableIface  *iface);
static void gtk_tree_model_sort_finalize              (GObject               *object);
static void gtk_tree_model_sort_row_changed           (GtkTreeModel          *model,
						       GtkTreePath           *start_path,
						       GtkTreeIter           *start_iter,
						       gpointer               data);
static void gtk_tree_model_sort_row_inserted          (GtkTreeModel          *model,
						       GtkTreePath           *path,
						       GtkTreeIter           *iter,
						       gpointer               data);
static void gtk_tree_model_sort_row_has_child_toggled (GtkTreeModel          *model,
						       GtkTreePath           *path,
						       GtkTreeIter           *iter,
						       gpointer               data);
static void gtk_tree_model_sort_row_deleted           (GtkTreeModel          *model,
						       GtkTreePath           *path,
						       gpointer               data);
static void gtk_tree_model_sort_rows_reordered        (GtkTreeModel          *s_model,
						       GtkTreePath           *s_path,
						       GtkTreeIter           *s_iter,
						       gint                  *new_order,
						       gpointer               data);


/* TreeModel interface */
static gint         gtk_tree_model_sort_get_n_columns      (GtkTreeModel          *tree_model);
static GType        gtk_tree_model_sort_get_column_type    (GtkTreeModel          *tree_model,
                                                            gint                   index);
static gboolean     gtk_tree_model_sort_get_iter           (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            GtkTreePath           *path);
static GtkTreePath *gtk_tree_model_sort_get_path           (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static void         gtk_tree_model_sort_get_value          (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            gint                   column,
                                                            GValue                *value);
static gboolean     gtk_tree_model_sort_iter_next          (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static gboolean     gtk_tree_model_sort_iter_children      (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            GtkTreeIter           *parent);
static gboolean     gtk_tree_model_sort_iter_has_child     (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static gint         gtk_tree_model_sort_iter_n_children    (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static gboolean     gtk_tree_model_sort_iter_nth_child     (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            GtkTreeIter           *parent,
                                                            gint                   n);
static gboolean     gtk_tree_model_sort_iter_parent        (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            GtkTreeIter           *child);


static void         gtk_tree_model_sort_ref_node           (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static void         gtk_tree_model_sort_unref_node         (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);


/* TreeSortable interface */
static gboolean     gtk_tree_model_sort_get_sort_column_id (GtkTreeSortable        *sortable,
                                                            gint                   *sort_column_id,
                                                            GtkSortType            *order);
static void         gtk_tree_model_sort_set_sort_column_id (GtkTreeSortable        *sortable,
                                                            gint                    sort_column_id,
                                                            GtkSortType        order);
static void         gtk_tree_model_sort_set_sort_func      (GtkTreeSortable        *sortable,
                                                            gint                    sort_column_id,
                                                            GtkTreeIterCompareFunc  func,
                                                            gpointer                data,
                                                            GtkDestroyNotify        destroy);

/* internal functions */
static gboolean     gtk_tree_model_sort_get_iter_helper    (GtkTreeModelSort       *tree_model_sort,
							    GArray                 *array,
							    GtkTreeIter            *iter,
							    gint                    depth,
							    GtkTreePath            *path);
static gint         gtk_tree_model_sort_compare_func       (gconstpointer           a,
							    gconstpointer           b,
							    gpointer                user_data);
static void         gtk_tree_model_sort_sort_helper        (GtkTreeModelSort       *tree_model_sort,
							    GArray                 *parent,
							    gboolean                recurse,
							    gboolean                emit_reordered);
static void         gtk_tree_model_sort_sort               (GtkTreeModelSort       *tree_model_sort);
static gint         gtk_tree_model_sort_array_find_insert  (GtkTreeModelSort       *tree_model_sort,
							    GArray                 *array,
							    GtkTreeIter            *iter,
							    gboolean                skip_sort_elt);
static GtkTreePath *gtk_tree_model_sort_generate_path_index(SortElt                *item,
							    GtkTreeModelSort       *tree_model_sort);
static GtkTreePath *gtk_tree_model_sort_generate_path      (SortElt              *item);
static GtkTreePath *gtk_tree_model_sort_convert_path_real  (GtkTreeModelSort       *tree_model_sort,
							    GtkTreePath            *child_path,
							    gboolean                build_children);
static void         gtk_tree_model_sort_convert_iter_real  (GtkTreeModelSort       *tree_model_sort,
							    GtkTreeIter            *sort_iter,
							    GtkTreeIter            *child_iter,
							    gboolean                build_children);
static void         gtk_tree_model_sort_build_level        (GtkTreeModelSort       *tree_model_sort,
							    SortElt                *place);
static void         gtk_tree_model_sort_free_level         (GArray                 *array);
static void         sort_elt_get_iter                      (GtkTreeModelSort       *tree_model_sort,
							    SortElt                *elt,
							    GtkTreeIter            *child_iter);



GType
gtk_tree_model_sort_get_type (void)
{
  static GType tree_model_sort_type = 0;
  
  if (!tree_model_sort_type)
    {
      static const GTypeInfo tree_model_sort_info =
      {
        sizeof (GtkTreeModelSortClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gtk_tree_model_sort_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
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

      static const GInterfaceInfo sortable_info =
      {
        (GInterfaceInitFunc) gtk_tree_model_sort_tree_sortable_init,
        NULL,
        NULL
      };

      tree_model_sort_type = g_type_register_static (G_TYPE_OBJECT,
						     "GtkTreeModelSort",
						     &tree_model_sort_info, 0);
      g_type_add_interface_static (tree_model_sort_type,
                                   GTK_TYPE_TREE_MODEL,
                                   &tree_model_info);

      g_type_add_interface_static (tree_model_sort_type,
                                   GTK_TYPE_TREE_SORTABLE,
                                   &sortable_info);
    }

  return tree_model_sort_type;
}

static void
gtk_tree_model_sort_class_init (GtkTreeModelSortClass *tree_model_sort_class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) tree_model_sort_class;

  object_class->finalize = gtk_tree_model_sort_finalize;
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
  iface->ref_node = gtk_tree_model_sort_ref_node;
  iface->unref_node = gtk_tree_model_sort_unref_node;
}

static void
gtk_tree_model_sort_tree_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = gtk_tree_model_sort_get_sort_column_id;
  iface->set_sort_column_id = gtk_tree_model_sort_set_sort_column_id;
  iface->set_sort_func = gtk_tree_model_sort_set_sort_func;
}

static void
gtk_tree_model_sort_init (GtkTreeModelSort *tree_model_sort)
{
  tree_model_sort->sort_column_id = -1;
  tree_model_sort->stamp = g_random_int ();
  tree_model_sort->cache_child_iters = FALSE;
  
  tree_model_sort->root = NULL;
  tree_model_sort->sort_list = NULL;
}

/**
 * gtk_tree_model_sort_new:
 * 
 * Creates a new #GtkTreeModel without child_model.
 *
 * Returns: A new #GtkTreeModel.
 */
GtkTreeModel *
gtk_tree_model_sort_new (void)
{
  return GTK_TREE_MODEL (g_object_new (gtk_tree_model_sort_get_type (), NULL));
}

/**
 * gtk_tree_model_sort_new_with_model:
 * @child_model: A #GtkTreeModel
 *
 * Creates a new #GtkTreeModel, with @child_model as the child_model.
 *
 * Return value: A new #GtkTreeModel.
 */
GtkTreeModel *
gtk_tree_model_sort_new_with_model (GtkTreeModel      *child_model)
{
  GtkTreeModel *retval;

  retval = gtk_tree_model_sort_new ();
  gtk_tree_model_sort_set_model (GTK_TREE_MODEL_SORT (retval), child_model);

  return retval;
}

/**
 * gtk_tree_model_sort_set_model:
 * @tree_model_sort: The #GtkTreeModelSort.
 * @child_model: A #GtkTreeModel, or NULL.
 *
 * Sets the model of @tree_model_sort to be @model.  If @model is NULL, then the * old model is unset.
 **/
void
gtk_tree_model_sort_set_model (GtkTreeModelSort *tree_model_sort,
                               GtkTreeModel     *child_model)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));

  if (child_model)
    g_object_ref (G_OBJECT (child_model));

  if (tree_model_sort->child_model)
    {
      g_signal_handler_disconnect (G_OBJECT (tree_model_sort->child_model),
                                   tree_model_sort->changed_id);
      g_signal_handler_disconnect (G_OBJECT (tree_model_sort->child_model),
                                   tree_model_sort->inserted_id);
      g_signal_handler_disconnect (G_OBJECT (tree_model_sort->child_model),
                                   tree_model_sort->has_child_toggled_id);
      g_signal_handler_disconnect (G_OBJECT (tree_model_sort->child_model),
                                   tree_model_sort->deleted_id);
      g_signal_handler_disconnect (G_OBJECT (tree_model_sort->child_model),
				   tree_model_sort->reordered_id);

      gtk_tree_model_sort_free_level (tree_model_sort->root);
      g_object_unref (G_OBJECT (tree_model_sort->child_model));
    }

  if (tree_model_sort->root)
    {
      gtk_tree_model_sort_free_level (tree_model_sort->root);
      tree_model_sort->root = NULL;
    }

  if (tree_model_sort->sort_list)
    {
      _gtk_tree_data_list_header_free (tree_model_sort->sort_list);
      tree_model_sort->sort_list = NULL;
    }

  tree_model_sort->child_model = child_model;
  if (child_model)
    {
      GType *types;
      gint i, n_columns;

      tree_model_sort->changed_id =
        g_signal_connect (child_model,
                          "row_changed",
                          G_CALLBACK (gtk_tree_model_sort_row_changed),
                          tree_model_sort);
      tree_model_sort->inserted_id =
        g_signal_connect (child_model,
                          "row_inserted",
                          G_CALLBACK (gtk_tree_model_sort_row_inserted),
                          tree_model_sort);
      tree_model_sort->has_child_toggled_id =
        g_signal_connect (child_model,
                          "row_has_child_toggled",
                          G_CALLBACK (gtk_tree_model_sort_row_has_child_toggled),
                          tree_model_sort);
      tree_model_sort->deleted_id =
        g_signal_connect (child_model,
                          "row_deleted",
                          G_CALLBACK (gtk_tree_model_sort_row_deleted),
                          tree_model_sort);
      tree_model_sort->reordered_id =
	g_signal_connect (child_model,
			  "rows_reordered",
			  G_CALLBACK (gtk_tree_model_sort_rows_reordered),
			  tree_model_sort);

      tree_model_sort->flags = gtk_tree_model_get_flags (child_model);
      n_columns = gtk_tree_model_get_n_columns (child_model);

      types = g_new (GType, n_columns);
      for (i = 0; i < n_columns; i++)
        types[i] = gtk_tree_model_get_column_type (child_model, i);

      tree_model_sort->sort_list = _gtk_tree_data_list_header_new (n_columns,
								   types);
      g_free (types);

      if (tree_model_sort->flags & GTK_TREE_MODEL_ITERS_PERSIST)
        tree_model_sort->cache_child_iters = TRUE;
      else
        tree_model_sort->cache_child_iters = FALSE;
    }
}

/**
 * gtk_tree_model_sort_get_model:
 * @tree_model: a #GtkTreeModelSort
 *
 * Returns the model the #GtkTreeModelSort is sorting.
 *
 * Return value: the "child model" being sorted
 **/
GtkTreeModel*
gtk_tree_model_sort_get_model (GtkTreeModelSort  *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), NULL);

  return tree_model->child_model;
}

/**
 * gtk_tree_model_sort_convert_path:
 * @tree_model_sort: The #GtkTreeModelSort.
 * @child_path: A #GtkTreePath, relative to the child model.
 *
 * Converts the @child_path to a new path, relative to the sorted position.  In other
 * words, the value found in the @tree_model_sort ->child_model at the @child_path, is
 * identical to that found in the @tree_model_sort and the return value.
 *
 * Return value: A new path, or NULL if @child_path does not exist in @tree_model_sort
 * ->child_model.
 **/
GtkTreePath *
gtk_tree_model_sort_convert_path (GtkTreeModelSort *tree_model_sort,
                                  GtkTreePath      *child_path)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort), NULL);
  g_return_val_if_fail (child_path != NULL, NULL);

  return gtk_tree_model_sort_convert_path_real (tree_model_sort, child_path, TRUE);
}

/**
 * gtk_tree_model_sort_convert_iter:
 * @tree_model_sort: The #GtkTreeModelSort
 * @sort_iter: A pointer to a #GtkTreeIter
 * @child_iter: A #GtkTreeIter, relative to the child model
 *
 * Converts the @child_iter to a new iter, relative to the sorted position. In other
 * words, the value found in the @tree_model_sort ->child_model at the @child_iter, is
 * identical to that found in @tree_model_sort at the @sort_iter. The @sort_iter will be
 * set.
 */
void
gtk_tree_model_sort_convert_iter (GtkTreeModelSort *tree_model_sort,
                                  GtkTreeIter      *sort_iter,
                                  GtkTreeIter      *child_iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));
  g_return_if_fail (sort_iter != NULL);
  g_return_if_fail (child_iter != NULL);
  
  gtk_tree_model_sort_convert_iter_real (tree_model_sort,
                                         sort_iter,
                                         child_iter,
                                         TRUE);
}

static void
gtk_tree_model_sort_finalize (GObject *object)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) object;

  if (tree_model_sort->root)
    gtk_tree_model_sort_free_level (tree_model_sort->root);

  if (tree_model_sort->child_model)
    {
      g_object_unref (G_OBJECT (tree_model_sort->child_model));
      tree_model_sort->child_model = NULL;
    }
  if (tree_model_sort->sort_list)
    {
      _gtk_tree_data_list_header_free (tree_model_sort->sort_list);
      tree_model_sort->sort_list = NULL;
    }
}

static void
gtk_tree_model_sort_row_changed (GtkTreeModel *s_model,
				 GtkTreePath  *s_path,
				 GtkTreeIter  *s_iter,
				 gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter tmpiter;
  SortElt *elt;
  GArray *array;
  gboolean free_s_path = FALSE;
  gint i;
  gint offset;
  gint index;
  SortElt tmp;

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
  elt = iter.user_data;
  array = get_array (elt, tree_model_sort);

  if (array->len < 2)
    {
      /* we're not going to care about this */
      if (free_s_path)
	gtk_tree_path_free (s_path);
      gtk_tree_path_free (path);
      return;
    }

  if (tree_model_sort->cache_child_iters)
    sort_elt_get_iter (tree_model_sort, elt, &tmpiter);

  offset = elt->offset;
  
  for (i = 0; i < array->len; i++)
    if (elt->offset == g_array_index (array, SortElt, i).offset)
      {
	index = i;
      }
  
  memcpy (&tmp, elt, sizeof (SortElt));
  g_array_remove_index (array, index);

  /* _kris_: don't know if this FIXME was originally at this position...
   * FIXME: as an optimization for when the column other then the one we're
   * sorting is changed, we can check the prev and next element to see if
   * they're different.
   */
  
  /* now we need to resort things */
  if (tree_model_sort->cache_child_iters)
    index = gtk_tree_model_sort_array_find_insert (tree_model_sort,
						   array,
						   &tmp.iter,
						   TRUE);
  else
    index = gtk_tree_model_sort_array_find_insert (tree_model_sort,
						   array,
						   &tmpiter,
						   TRUE);
  
  g_array_insert_val (array, index, tmp);

  gtk_tree_model_row_changed (GTK_TREE_MODEL (data), path, &iter);
  
  gtk_tree_path_free (path);
  if (free_s_path)
    gtk_tree_path_free (s_path);
}

/* Returns FALSE if the value was inserted, TRUE otherwise */
static gboolean
gtk_tree_model_sort_insert_value (GtkTreeModelSort *tree_model_sort,
				  GtkTreePath      *s_path,
				  GtkTreeIter      *s_iter)
{
  GtkTreePath *tmp_path;
  GArray *array = NULL;
  gint index;
  GtkTreeIter iter;
  SortElt elt;
  gint offset;
  gint j;
  SortElt *tmp_elt;

  offset = gtk_tree_path_get_indices (s_path)[gtk_tree_path_get_depth (s_path) - 1];
  
  if (tree_model_sort->cache_child_iters)
    elt.iter = *s_iter;
  elt.children = NULL;
  elt.offset = offset;
  elt.ref = 0;

  tmp_path = gtk_tree_path_copy (s_path);

  if (gtk_tree_path_up (tmp_path))
    {
      GtkTreePath *parent_path;

      parent_path = gtk_tree_model_sort_convert_path_real (tree_model_sort,
							   tmp_path,
							   FALSE);

      if (!parent_path)
	{
	  gtk_tree_path_free (tmp_path);
	  return FALSE;
	}

      if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort), &iter,
				    parent_path))
	{
	  gtk_tree_path_free (tmp_path);
	  return FALSE;
	}

      elt.parent = iter.user_data;
      array = elt.parent->children;
      gtk_tree_path_free (parent_path);
      
      if (!array)
	{
	  gtk_tree_path_free (tmp_path);
	  return FALSE;
	}
    }
  else
    {
      if (!tree_model_sort->root)
	tree_model_sort->root =
	  g_array_sized_new (FALSE, FALSE, sizeof (SortElt), 1);
      array = tree_model_sort->root;
      elt.parent = NULL;
    }
  gtk_tree_path_free (tmp_path);
  
  if (tree_model_sort->cache_child_iters)
    index = gtk_tree_model_sort_array_find_insert (tree_model_sort,
						   array,
						   &elt.iter,
						   FALSE);
  else
    {
      GtkTreeIter tmpiter;
      sort_elt_get_iter (tree_model_sort, &elt, &tmpiter);
      index = gtk_tree_model_sort_array_find_insert (tree_model_sort,
						     array,
						     &tmpiter,
						     FALSE);
    }

  g_array_insert_vals (array, index, &elt, 1);

  /* update all the larger offsets */
  tmp_elt = (SortElt *)array->data;
  for (j = 0; j < array->len; j++, tmp_elt++)
    {
      if ((tmp_elt->offset >= offset) && j != index)
	tmp_elt->offset++;
    }

  return TRUE;
}

static void
gtk_tree_model_sort_row_inserted (GtkTreeModel *s_model,
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

      if (!gtk_tree_model_sort_insert_value (tree_model_sort,
					     s_path, &real_s_iter))
        return;
    }

  if (!tree_model_sort->root)
    path = gtk_tree_model_sort_convert_path_real (tree_model_sort,
						  s_path, TRUE);
  else
    path = gtk_tree_model_sort_convert_path_real (tree_model_sort, 
						  s_path, FALSE);
  
  if (path == NULL)
    return;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  tree_model_sort->stamp++;
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (data), path, &iter);
  gtk_tree_path_free (path);
}

static void
gtk_tree_model_sort_row_has_child_toggled (GtkTreeModel *s_model,
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

  path = gtk_tree_model_sort_convert_path_real (tree_model_sort, s_path, FALSE);  if (path == NULL)
    return;
  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (data), path, &iter);
  gtk_tree_path_free (path);
  if (free_s_path)
    gtk_tree_path_free (s_path);
}

static void
gtk_tree_model_sort_row_deleted (GtkTreeModel *s_model,
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
      elt = iter.user_data;
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
	  for (i = 0; i < array->len; i++)
	    if (elt->offset == g_array_index (array, SortElt, i).offset)
	      break;

	  g_array_remove_index (array, i);
	  
	  /* update all offsets */
          for (i = 0; i < array->len; i++)
            {
              elt = & (g_array_index (array, SortElt, i));
              if (elt->offset > offset)
                elt->offset--;
            }
        }
    }

  gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);
  tree_model_sort->stamp++;
  gtk_tree_path_free (path);
}

static void
gtk_tree_model_sort_rows_reordered (GtkTreeModel *s_model,
				    GtkTreePath  *s_path,
				    GtkTreeIter  *s_iter,
				    gint         *new_order,
				    gpointer      data)
{
  gint i = 0;
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  SortElt *elt = NULL;
  GArray *array;
  gint len;

  /* header is used for checking if we can already sort things */
  GtkTreeDataSortHeader *header = 
    _gtk_tree_data_list_get_header (tree_model_sort->sort_list,
				    tree_model_sort->sort_column_id);


  g_return_if_fail (s_path != NULL || s_iter != NULL);
  g_return_if_fail (new_order != NULL);
  
  if (!s_path)
    s_path = gtk_tree_model_get_path (s_model, s_iter);
  
  if (!gtk_tree_path_get_indices (s_path))
    len = gtk_tree_model_iter_n_children (s_model, NULL);
  else
    len = gtk_tree_model_iter_n_children (s_model, s_iter);

  if (len < 2)
    return;
  
  if (!gtk_tree_path_get_indices (s_path))
    {
      array = (GArray *)tree_model_sort->root;

      if (!array)
	{
	  gtk_tree_model_sort_build_level (tree_model_sort, NULL);
	  array = (GArray *)tree_model_sort->root;
	  
	  if (header)
	    gtk_tree_model_sort_sort_helper (tree_model_sort,
					     array,
					     FALSE,
					     TRUE);
	  
	  return;
	}
      
      if (!tree_model_sort->cache_child_iters)
	{
	  if (array)
	    gtk_tree_model_sort_free_level (tree_model_sort->root);
	  tree_model_sort->root = NULL;
	  gtk_tree_model_sort_build_level (tree_model_sort, NULL);
	  
	  array = tree_model_sort->root;
	  
	  if (header)
	    gtk_tree_model_sort_sort_helper (tree_model_sort,
					     tree_model_sort->root,
					     FALSE,
					     FALSE);
	  
	  return;
	}
    }
  else
    {
      path = gtk_tree_model_sort_convert_path_real (tree_model_sort,
						    s_path,
						    FALSE);
      
      if (!path)
	return;
      
      if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path))
	/* no iter for me */
	  return;
      elt = iter.user_data;
      gtk_tree_path_free (path);

      if (!s_iter)
	gtk_tree_model_get_iter (s_model, s_iter, s_path);

      if (!elt->children)
	return;

      array = elt->children;

      if (!tree_model_sort->cache_child_iters)
	{
	  if (array)
	    gtk_tree_model_sort_free_level (elt->children);
	  elt->children = NULL;
	  gtk_tree_model_sort_build_level (tree_model_sort, elt);

	  array = elt->children;

	  if (header)
	    gtk_tree_model_sort_sort_helper (tree_model_sort,
					     array,
					     FALSE,
					     FALSE);
	  
	  return;
	}
    }
  
  if (len != array->len)
    /* length mismatch, pretty bad, shouldn't happen */
    return;
  
  for (i = 0; i < array->len; i++)
    g_array_index (array, SortElt, i).offset = new_order[i];
}

static gint
gtk_tree_model_sort_get_n_columns (GtkTreeModel *tree_model)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), 0);

  if (tree_model_sort->child_model == 0)
    return 0;

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
				     GArray           *array,
				     GtkTreeIter      *iter,
				     gint              depth,
				     GtkTreePath      *path)
{
  SortElt *elt = NULL;
  GtkTreeIter child_iter;

  if (array == NULL)
    return FALSE;
  
  if (gtk_tree_path_get_indices (path)[depth] >= array->len)
    return FALSE;

  elt = &g_array_index (array, SortElt, gtk_tree_path_get_indices (path)[depth]);
  
  if (!elt)
    return FALSE;

  if (depth == gtk_tree_path_get_depth (path) - 1)
    {
      iter->stamp = tree_model_sort->stamp;
      iter->user_data = elt;

      return TRUE;
    }

  if (elt->children != NULL)
    return gtk_tree_model_sort_get_iter_helper (tree_model_sort,
						elt->children,
						iter,
						depth + 1,
						path);
  
  sort_elt_get_iter (tree_model_sort, elt, &child_iter);
  if (gtk_tree_model_iter_has_child (tree_model_sort->child_model, &child_iter))
    {
      gtk_tree_model_sort_build_level (tree_model_sort, elt);
      if (elt->children)
	gtk_tree_model_sort_sort_helper (tree_model_sort, elt->children,
					 TRUE,
					 FALSE);
    }

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

  if (iter->stamp == GTK_TREE_MODEL_SORT (tree_model)->stamp)
    {
      SortElt *elt;
      GtkTreePath *path;
      
      elt = iter->user_data;
      path = gtk_tree_model_sort_generate_path_index 
	(elt, GTK_TREE_MODEL_SORT (tree_model));
      return path;
    }
  
  return gtk_tree_model_get_path (GTK_TREE_MODEL_SORT (tree_model)->child_model, iter);
}

static void
gtk_tree_model_sort_get_value (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               gint          column,
                               GValue        *value)
{
  SortElt *elt;
  GtkTreeIter child_iter;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp);

  elt = iter->user_data;
  
  sort_elt_get_iter (GTK_TREE_MODEL_SORT (tree_model), elt, &child_iter);
  gtk_tree_model_get_value (GTK_TREE_MODEL_SORT (tree_model)->child_model,
			    &child_iter, column, value);
}

static gboolean
gtk_tree_model_sort_iter_next (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  GArray *array;
  SortElt *elt;
  gint i = 0;
  gint offset;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, FALSE);

  elt = iter->user_data;
  array = get_array (elt, GTK_TREE_MODEL_SORT (tree_model));

  if (elt - ((SortElt *)array->data) >= array->len - 1)
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->user_data = elt + 1;

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_children (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter,
                                   GtkTreeIter  *parent)
{
  gint i;
  GArray *array;
  SortElt *elt;
  GtkTreeIter child_iter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);

  if (parent)
    g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == parent->stamp, FALSE);

  if (GTK_TREE_MODEL_SORT (tree_model)->root == NULL)
    gtk_tree_model_sort_build_level (GTK_TREE_MODEL_SORT (tree_model), NULL);

  if (parent)
    elt = parent->user_data;
  else
    {
      if (!GTK_TREE_MODEL_SORT (tree_model)->root)
	return FALSE;
      else
	{
	  array = GTK_TREE_MODEL_SORT (tree_model)->root;
	  
	  iter->stamp = GTK_TREE_MODEL_SORT (tree_model)->stamp;
	  iter->user_data = &g_array_index (array, SortElt, 0);
	  
	  return TRUE;
	}
    }
  
  if (!elt)
    return FALSE;

  sort_elt_get_iter (GTK_TREE_MODEL_SORT (tree_model), elt, &child_iter);

  if (!elt->children &&
      gtk_tree_model_iter_has_child 
      (GTK_TREE_MODEL_SORT (tree_model)->child_model, &child_iter))
    {
      gtk_tree_model_sort_build_level (GTK_TREE_MODEL_SORT (tree_model), elt);
      if (elt->children)
	gtk_tree_model_sort_sort_helper (GTK_TREE_MODEL_SORT (tree_model),
					 elt->children,
					 FALSE, 
					 FALSE);
    }

  if (!elt->children)
    return FALSE;

  array = elt->children;

  iter->stamp = GTK_TREE_MODEL_SORT (tree_model)->stamp;
  iter->user_data = &g_array_index (array, SortElt, 0);

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_has_child (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter)
{
  SortElt *elt;
  GtkTreeIter child_iter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, FALSE);

  elt = iter->user_data;
  
  if (elt->children)
    return TRUE;

  sort_elt_get_iter (GTK_TREE_MODEL_SORT (tree_model), elt, &child_iter);

  return gtk_tree_model_iter_has_child
    (GTK_TREE_MODEL_SORT (tree_model)->child_model, &child_iter);
}

static gint
gtk_tree_model_sort_iter_n_children (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter)
{
  SortElt *elt;
  GtkTreeIter child_iter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, 0);
  if (iter)
    g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, 0);

  if (GTK_TREE_MODEL_SORT (tree_model)->root == NULL)
    gtk_tree_model_sort_build_level (GTK_TREE_MODEL_SORT (tree_model), NULL);

  if (iter)
    elt = iter->user_data;
  else
    {
      if (!GTK_TREE_MODEL_SORT (tree_model)->root)
	return 0;
      else
	return ((GArray *)GTK_TREE_MODEL_SORT (tree_model)->root)->len;
    }

  g_return_val_if_fail (elt != NULL, 0);
  
  if (elt->children)
    return elt->children->len;

  sort_elt_get_iter (GTK_TREE_MODEL_SORT (tree_model), elt, &child_iter);

  return gtk_tree_model_iter_n_children
    (GTK_TREE_MODEL_SORT (tree_model)->child_model, &child_iter);
}

static gboolean
gtk_tree_model_sort_iter_nth_child (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *parent,
                                    gint          n)
{
  gint i;
  SortElt *elt;
  GArray *array;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  if (parent)
    g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == parent->stamp, FALSE);
  
  if (GTK_TREE_MODEL_SORT (tree_model)->root == NULL)
    gtk_tree_model_sort_build_level (GTK_TREE_MODEL_SORT (tree_model), NULL);

  if (parent)
    elt = parent->user_data;
  else
    {
      if (!GTK_TREE_MODEL_SORT (tree_model)->root)
	return FALSE;
      else
	{
	  elt = NULL;

	  array = GTK_TREE_MODEL_SORT (tree_model)->root;

	  elt = &g_array_index (array, SortElt, n);

	  if (!elt)
	    return FALSE;
	}
    }
  
  if (!elt->children)
    {
      GtkTreeIter child_iter;

      sort_elt_get_iter (GTK_TREE_MODEL_SORT (tree_model), elt, &child_iter);

      if (gtk_tree_model_iter_has_child 
	  (GTK_TREE_MODEL_SORT (tree_model)->child_model, &child_iter))
	{
	  gtk_tree_model_sort_build_level (GTK_TREE_MODEL_SORT (tree_model),
					   elt);
	  if (elt->children)
	    gtk_tree_model_sort_sort_helper (GTK_TREE_MODEL_SORT (tree_model),
					     elt->children,
					     FALSE,
					     FALSE);
	}
      else
	return FALSE;
    }

  if (!elt->children)
    return FALSE;
  
  if (n >= elt->children->len)
    return FALSE;

  array = elt->children;

  iter->stamp = GTK_TREE_MODEL_SORT (tree_model)->stamp;
  iter->user_data = &g_array_index (array, SortElt, n);
  
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

  elt = child->user_data;

  if (elt->parent)
    {
      iter->stamp = GTK_TREE_MODEL_SORT (tree_model)->stamp;
      iter->user_data = elt->parent;
      
      return TRUE;
    }
  
  return FALSE;
}

static void
gtk_tree_model_sort_ref_node (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  SortElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp);

  elt = iter->user_data;
  if (elt->parent)
    elt->parent->ref++;
}

static void
gtk_tree_model_sort_unref_node (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter)
{
  SortElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp);
  
  elt = iter->user_data;
  if (elt->parent)
    {
      elt->parent->ref--;
      
      if (elt->parent->ref == 0)
	gtk_tree_model_sort_free_level (elt->parent->children);
    }
}

/* sortable interface */
static gboolean
gtk_tree_model_sort_get_sort_column_id (GtkTreeSortable  *sortable,
                                        gint             *sort_column_id,
                                        GtkSortType      *order)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) sortable;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (sortable), FALSE);

  if (tree_model_sort->sort_column_id == -1)
    return FALSE;

  if (sort_column_id)
    *sort_column_id = tree_model_sort->sort_column_id;
  if (order)
    *order = tree_model_sort->order;

  return TRUE;
}

static void
gtk_tree_model_sort_set_sort_column_id (GtkTreeSortable  *sortable,
                                        gint              sort_column_id,
                                        GtkSortType       order)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) sortable;
  GList *list;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (sortable));

  for (list = tree_model_sort->sort_list; list; list = list->next)
    {
      GtkTreeDataSortHeader *header = (GtkTreeDataSortHeader*) list->data;
      if (header->sort_column_id == sort_column_id)
        break;
    }

  g_return_if_fail (list != NULL);

  if ((tree_model_sort->sort_column_id == sort_column_id) &&
      (tree_model_sort->order == order))
    return;
  
  tree_model_sort->sort_column_id = sort_column_id;
  tree_model_sort->order = order;
  
  if (tree_model_sort->sort_column_id >= 0)
    gtk_tree_model_sort_sort (tree_model_sort);
  
  gtk_tree_sortable_sort_column_changed (sortable);
}

static void
gtk_tree_model_sort_set_sort_func (GtkTreeSortable        *sortable,
                                   gint                    sort_column_id,
                                   GtkTreeIterCompareFunc  func,
                                   gpointer                data,
                                   GtkDestroyNotify        destroy)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) sortable;
  GtkTreeDataSortHeader *header = NULL;
  GList *list;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (sortable));
  g_return_if_fail (func != NULL);

  for (list = tree_model_sort->sort_list; list; list = list->next)
    {
      header = (GtkTreeDataSortHeader*) list->data;
      if (header->sort_column_id == sort_column_id)
        break;
    }
  
  if (header == NULL)
    {
      header = g_new0 (GtkTreeDataSortHeader, 1);
      header->sort_column_id = sort_column_id;
      tree_model_sort->sort_list = g_list_append (tree_model_sort->sort_list,
						  header);
    }
  
  if (header->destroy)
    (* header->destroy) (header->data);
  
  header->func = func;
  header->data = data;
  header->destroy = destroy;
}

/* sorting core */
static gint
gtk_tree_model_sort_compare_func (gconstpointer a,
                                  gconstpointer b,
                                  gpointer      user_data)
{
  gint retval;
  SortElt *sa = ((SortTuple *)a)->elt;
  SortElt *sb = ((SortTuple *)b)->elt;
  GtkTreeIter iter_a;
  GtkTreeIter iter_b;
  SortData *data = (SortData *)user_data;
  GtkTreeModelSort *tree_model_sort = data->tree_model_sort;
  GtkTreeDataSortHeader *header = NULL;

  /* sortcut, if we've the same offsets here, they should be equal */
  if (sa->offset == sb->offset)
    return 0;

  header = _gtk_tree_data_list_get_header (tree_model_sort->sort_list,
                                           tree_model_sort->sort_column_id);

  g_return_val_if_fail (header != NULL, 0);
  g_return_val_if_fail (header->func != NULL, 0);

  sort_elt_get_iter (tree_model_sort, sa, &iter_a);
  sort_elt_get_iter (tree_model_sort, sb, &iter_b);

  retval = (*header->func) (GTK_TREE_MODEL (tree_model_sort->child_model),
                            &iter_a, &iter_b, header->data);
  
  if (tree_model_sort->order == GTK_SORT_DESCENDING)
    {
      if (retval > 0)
        retval = -1;
      else if (retval < 0)
        retval = 1;
    }
  
  return retval;
}

static void
gtk_tree_model_sort_sort_helper (GtkTreeModelSort *tree_model_sort,
				 GArray           *parent,
				 gboolean          recurse,
				 gboolean          emit_reordered)
{
  gint i;
  gint *new_order;
  GArray *sort_array;
  GArray *new_array;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeDataSortHeader *header = NULL;

  SortData *data = NULL;
  
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));
  g_return_if_fail (parent != NULL);
  
  if (parent->len < 1 && !((SortElt *)parent->data)->children)
    return;
  
  header = _gtk_tree_data_list_get_header (tree_model_sort->sort_list,
                                           tree_model_sort->sort_column_id);
  
  /* making sure we have a compare function */
  g_return_if_fail (header != NULL);
  g_return_if_fail (header->func != NULL);

  data = g_new (SortData, 1);

  if (((SortElt *)parent->data)->parent)
    {
      data->parent_a = gtk_tree_model_sort_generate_path
	(((SortElt *)parent->data)->parent);
      data->parent_b = gtk_tree_path_copy (data->parent_a);
    }
  else
    {
      data->parent_a = gtk_tree_path_new ();
      data->parent_b = gtk_tree_path_new ();
    }

  data->tree_model_sort = tree_model_sort;

  sort_array = g_array_sized_new (FALSE, TRUE, sizeof (SortTuple),
				  parent->len);
  for (i = 0; i < parent->len; i++)
    {
      SortTuple tuple;
      
      tuple.elt = &g_array_index (parent, SortElt, i);
      tuple.children = tuple.elt->children;
      tuple.offset = tuple.elt->offset;

      g_array_append_val (sort_array, tuple);
    }

  g_array_sort_with_data (sort_array, gtk_tree_model_sort_compare_func,
			  data);


  gtk_tree_path_free (data->parent_a);
  gtk_tree_path_free (data->parent_b);
  g_free (data);

  /* let the world know about our absolutely great new order */
  new_array = g_array_sized_new (FALSE, TRUE, sizeof (SortElt), parent->len);
  g_array_set_size (new_array, parent->len);
  new_order = g_new (gint, parent->len);

  for (i = 0; i < parent->len; i++)
    {
      SortElt *elt1;
      SortElt *elt2;
      SortElt  tmp;
      gint j;
      GArray *c;

      elt1 = &g_array_index (parent, SortElt, i);
      
      for (j = 0; j < sort_array->len; j++)
	if (elt1->offset == g_array_index (sort_array, SortTuple, j).offset)
	  break;

      if (j >= parent->len)
	/* isn't supposed to happen */
	break;

      new_order[i] = j;
      
      /* swap (hackety hack, or not? ;-) */
      memcpy (&g_array_index (new_array, SortElt, j), elt1, sizeof (SortElt));
      elt2 = &g_array_index (new_array, SortElt, j);

      /* point children to correct parent */
      if (elt2->children)
	{
	  gint k;
	  
	  c = elt2->children;
	  for (k = 0; k < c->len; k++)
	    g_array_index (c, SortElt, k).parent = elt2;
	}
    }

  {
    /* a bit hackish ? */

    SortElt *p = g_array_index (new_array, SortElt, 0).parent;

    if (!p && parent == tree_model_sort->root)
      {
	g_array_free (tree_model_sort->root, TRUE);
	tree_model_sort->root = parent = new_array;
      }
    else if (p && parent == p->children)
      {
	g_array_free (p->children, TRUE);
	p->children = parent = new_array;
      }
  }
  
  g_array_free (sort_array, TRUE);      

  if (emit_reordered)
    {
      tree_model_sort->stamp++;

      iter.stamp = tree_model_sort->stamp;
      iter.user_data = ((SortElt *)parent->data)->parent;
      if (iter.user_data)
	{
	  path = gtk_tree_model_sort_generate_path (iter.user_data);

	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort), path,
					 &iter, new_order);
	}
      else
	{
	  /* toplevel list */

	  path = gtk_tree_path_new ();
	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort),
					 path, NULL, new_order);
	}

      gtk_tree_path_free (path);
    }
  
  g_free (new_order);
  
  /* recurse, check if possible */
  if (recurse)
    {
      for (i = 0; i < parent->len; i++)
	{
	  SortElt *elt = (SortElt *)&g_array_index (parent, SortElt, i);

	  if (elt->children)
	    {
	      gtk_tree_model_sort_sort_helper (tree_model_sort,
					       elt->children,
					       TRUE, emit_reordered);
	    }
	}
    }
}

static void
gtk_tree_model_sort_sort (GtkTreeModelSort *tree_model_sort)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));

  if (!tree_model_sort->root)
    return;
  
  gtk_tree_model_sort_sort_helper (tree_model_sort, tree_model_sort->root,
                                   TRUE, TRUE);
}

static gint
gtk_tree_model_sort_array_find_insert (GtkTreeModelSort *tree_model_sort,
                                       GArray           *array,
                                       GtkTreeIter      *iter,
                                       gboolean          skip_sort_elt)
{
  gint middle;
  gint cmp;
  SortElt *tmp_elt;
  GtkTreeIter tmp_iter;
  GtkTreeDataSortHeader *header;
  
  if (tree_model_sort->sort_column_id < 0)
     return 0;

  header = _gtk_tree_data_list_get_header (tree_model_sort->sort_list,
                                           tree_model_sort->sort_column_id);
  
  g_return_val_if_fail (header != NULL, 0);
  g_return_val_if_fail (header->func != NULL, 0);

  for (middle = 0; middle < array->len; middle++)
    {
      tmp_elt = &(g_array_index (array, SortElt, middle));
      if (!skip_sort_elt && (SortElt *) iter == tmp_elt)
        continue;

      sort_elt_get_iter (tree_model_sort, tmp_elt, &tmp_iter);
      
      if (tree_model_sort->order == GTK_SORT_ASCENDING)
        cmp = (* header->func) (GTK_TREE_MODEL (tree_model_sort->child_model),
                                &tmp_iter, iter, header->data);
      else
        cmp = (* header->func) (GTK_TREE_MODEL (tree_model_sort->child_model),
                                iter, &tmp_iter, header->data);
      
      if (cmp > 0)
        break;
    }
  
  return middle;
}

/* sort_elt helpers */
static void
sort_elt_get_iter (GtkTreeModelSort *tree_model_sort,
		   SortElt          *elt,
		   GtkTreeIter      *child_iter)
{
  if (tree_model_sort->cache_child_iters)
    *child_iter = elt->iter;
  else
    {
      GtkTreePath *path = gtk_tree_model_sort_generate_path (elt);
      gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort->child_model),
			       child_iter, path);
      gtk_tree_path_free (path);
    }
}

/* another helper */

/* index based */
static GtkTreePath *
gtk_tree_model_sort_generate_path_index (SortElt *item,
					 GtkTreeModelSort *tree_model_sort)
{
  gchar *str = NULL;
  GList *i;
  GList *offsets = NULL;
  SortElt *walker = item;
  GtkTreePath *path;

  g_return_val_if_fail (item != NULL, NULL);

  while (walker)
    {
      gint j;

      GArray *array = get_array (walker, tree_model_sort);
      for (j = 0; j < array->len; j++)
	if (walker == &g_array_index (array, SortElt, j))
	  break;

      if (j >= array->len)
	{
	  g_assert_not_reached ();
	  return NULL;
	}

      offsets = g_list_prepend (offsets,
				g_strdup_printf ("%d", j));
      walker = walker->parent;
    }

  g_return_val_if_fail (g_list_length (offsets) > 0, NULL);
  
  for (i = offsets; i; i = i->next)
    {
      gchar *copy = str;

      if (str)
	str = g_strconcat (copy, ":", i->data, NULL);
      else
	str = g_strdup (i->data);

      if (copy)
	g_free (copy);
    }
  
  g_list_free (offsets);
  
  path = gtk_tree_path_new_from_string (str);
  g_free (str);
  
  return path;
}

/* offset based */
static GtkTreePath *
gtk_tree_model_sort_generate_path (SortElt *item)
{
  gchar *str = NULL;
  GList *i;
  GList *offsets = NULL;
  SortElt *walker = item;
  GtkTreePath *path;

  g_return_val_if_fail (item != NULL, NULL);
  
  while (walker)
    {
      offsets = g_list_prepend (offsets,
      				g_strdup_printf ("%d", walker->offset));
      walker = walker->parent;
    }

  g_return_val_if_fail (g_list_length (offsets) > 0, NULL);

  for (i = offsets; i; i = i->next)
    {
      gchar *copy = str;
      
      if (str)
	str = g_strconcat (copy, ":", i->data, NULL);
      else
	str = g_strdup (i->data);

      if (copy)
	g_free (copy);

      g_free (i->data);
    }

  g_list_free (offsets);
  
  path = gtk_tree_path_new_from_string (str);
  g_free (str);

  return path;
}

/* model cache/child model conversion and cache management */
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

  if (!indices && !gtk_tree_path_get_depth (child_path))
    /* just a new path */
    return retval;
  
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
	if (elt->offset == indices[i])
	  {
	    found = TRUE;
	    break;
	  }

      if (!found)
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
	      if (elt->children)
		gtk_tree_model_sort_sort_helper (tree_model_sort,
						 elt->children,
						 FALSE,
						 FALSE);
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
gtk_tree_model_sort_convert_iter_real (GtkTreeModelSort *tree_model_sort,
                                       GtkTreeIter      *sort_iter,
                                       GtkTreeIter      *child_iter,
                                       gboolean          build_children)
{
  GtkTreePath *sort_path;
  GtkTreePath *child_path;

  child_path = gtk_tree_model_get_path (tree_model_sort->child_model,
					child_iter);
  sort_path = gtk_tree_model_sort_convert_path_real (tree_model_sort,
						     child_path,
						     build_children);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort),
			   sort_iter, sort_path);

  gtk_tree_path_free (sort_path);
  gtk_tree_path_free (child_path);
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

  if (place && place->children)
    return;

  if (place)
    {
      parent_iter = g_new (GtkTreeIter, 1);
      
      sort_elt_get_iter (tree_model_sort, place, parent_iter);
    }

  n = gtk_tree_model_iter_n_children (tree_model_sort->child_model,
                                      parent_iter);

  if (n == 0)
    {
      if (parent_iter)
        gtk_tree_iter_free (parent_iter);
      return;
    }

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
      if (tree_model_sort->cache_child_iters)
        elt.iter = iter;
      elt.parent = place;
      elt.children = NULL;
      elt.offset = i;
      elt.ref = 0;
      
      g_array_append_vals (children, &elt, 1);
      i++;
    }
  while (gtk_tree_model_iter_next (tree_model_sort->child_model, &iter));
  
  if (parent_iter)
    gtk_tree_iter_free (parent_iter);
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
        gtk_tree_model_sort_free_level (elt->children);
    }

  g_array_free (array, TRUE);
}

/* USEFUL DEBUGGING CODE */

#if 0
static void
_dump_tree (GtkTreeModelSort *tree_model_sort, const char *tag)
{
  gint i;
  GArray *a;

  g_return_if_fail (tree_model_sort != NULL);

  g_print ("-----------%s-----------------\n", tag);

  a = (GArray *)tree_model_sort->root;
  for (i = 0; i < a->len; i++)
    {
      GValue value = {0,};
      GtkTreeIter iter;

      sort_elt_get_iter (tree_model_sort, &g_array_index (a, SortElt, i),
			 &iter);
      gtk_tree_model_get_value (tree_model_sort->child_model,
				&iter, 0, &value);
      g_print ("I/O=%d/%d --- %s\n", i, g_array_index (a, SortElt, i).offset,
	       g_value_get_string (&value));
      g_value_unset (&value);
    }

  g_print ("-------------------------\n");
}
#endif

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
                                sort->sort_column_id,
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
