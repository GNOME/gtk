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

/* ITER FORMAT:
 *
 * iter->stamp = tree_model_sort->stamp
 * iter->user_data = SortLevel
 * iter->user_data2 = SortElt
 */

#include "gtktreemodelsort.h"
#include "gtktreesortable.h"
#include "gtktreestore.h"
#include "gtksignal.h"
#include "gtktreedatalist.h"
#include <string.h>

typedef struct _SortElt SortElt;
typedef struct _SortLevel SortLevel;
typedef struct _SortData SortData;
typedef struct _SortTuple SortTuple;

struct _SortElt
{
  GtkTreeIter  iter;
  SortLevel   *children;
  gint         offset;
  gint         ref_count;
  gint         zero_ref_count;
};

struct _SortLevel
{
  GArray    *array;
  gint       ref_count;
  SortElt   *parent_elt;
  SortLevel *parent_level;
};

struct _SortData
{
  GtkTreeModelSort *tree_model_sort;
  GtkTreePath *parent_a;
  GtkTreePath *parent_b;
};

struct _SortTuple
{
  SortElt   *elt;
  SortLevel *level;
  SortLevel *children;
  gint       offset;  
};

#define GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS(tree_model_sort) \
	(((GtkTreeModelSort *)tree_model_sort)->child_flags&GTK_TREE_MODEL_ITERS_PERSIST)
#define SORT_ELT(sort_elt) ((SortElt *)sort_elt)
#define SORT_LEVEL(sort_level) ((SortLevel *)sort_level)

//#define GET_CHILD_ITER(tree_model_sort,child_iter,sort_iter) ((((GtkTreeModelSort *)tree_model_sort)->child_flags&GTK_TREE_MODEL_ITERS_PERSIST)?((*child_iter)=SORT_ELT(sort_iter->user_data)->iter):gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT(tree_model_sort),sort_iter,child_iter))

#define GET_CHILD_ITER(tree_model_sort,child_iter,sort_iter) gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT (tree_model_sort), child_iter, sort_iter);

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
static void gtk_tree_model_sort_destroy               (GtkObject             *gobject);

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
static guint        gtk_tree_model_sort_get_flags          (GtkTreeModel          *tree_model);
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
static void         gtk_tree_model_sort_set_default_sort_func (GtkTreeSortable        *sortable,
							       GtkTreeIterCompareFunc  func,
							       gpointer                data,
							       GtkDestroyNotify        destroy);
static gboolean     gtk_tree_model_sort_has_default_sort_func (GtkTreeSortable     *sortable);

/* Private functions */
static void         gtk_tree_model_sort_build_level     (GtkTreeModelSort *tree_model_sort,
							 SortLevel        *parent_level,
							 SortElt          *parent_elt);
static void         gtk_tree_model_sort_free_level      (GtkTreeModelSort *tree_model_sort,
							 SortLevel        *sort_level);
static void         gtk_tree_model_sort_increment_stamp (GtkTreeModelSort *tree_model_sort);
static void         gtk_tree_model_sort_sort_level      (GtkTreeModelSort *tree_model_sort,
							 SortLevel        *level,
							 gboolean          recurse,
							 gboolean          emit_reordered);
static void         gtk_tree_model_sort_sort            (GtkTreeModelSort *tree_model_sort);

static gint         gtk_tree_model_sort_level_find_insert (GtkTreeModelSort *tree_model_sort,
							   SortLevel        *level,
							   GtkTreeIter      *iter,
							   gboolean          skip_sort_elt);
static gboolean     gtk_tree_model_sort_insert_value      (GtkTreeModelSort *tree_model_sort,
							   GtkTreePath      *s_path,
							   GtkTreeIter      *s_iter);
static GtkTreePath *gtk_tree_model_sort_elt_get_path    (SortLevel        *level,
							 SortElt          *elt);
static void         get_child_iter_from_elt_no_cache    (GtkTreeModelSort *tree_model_sort,
							 GtkTreeIter      *child_iter,
							 SortLevel        *level,
							 SortElt          *elt);
static void         get_child_iter_from_elt             (GtkTreeModelSort *tree_model_sort,
							 GtkTreeIter      *child_iter,
							 SortLevel        *level,
							 SortElt          *elt);
static void         gtk_tree_model_sort_set_model       (GtkTreeModelSort *tree_model_sort,
							 GtkTreeModel     *child_model);


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
gtk_tree_model_sort_init (GtkTreeModelSort *tree_model_sort)
{
  tree_model_sort->sort_column_id = GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID;
  tree_model_sort->stamp = 0;
  tree_model_sort->zero_ref_count = 0;
  tree_model_sort->root = NULL;
  tree_model_sort->sort_list = NULL;
}

static void
gtk_tree_model_sort_class_init (GtkTreeModelSortClass *class)
{
  GObjectClass *object_class;
  GtkObjectClass *gobject_class;

  object_class = (GObjectClass *) class;
  gobject_class = (GtkObjectClass *) class;

  object_class->finalize = gtk_tree_model_sort_finalize;
  gobject_class->destroy = gtk_tree_model_sort_destroy;
}

static void
gtk_tree_model_sort_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_model_sort_get_flags;
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
  iface->set_default_sort_func = gtk_tree_model_sort_set_default_sort_func;
  iface->has_default_sort_func = gtk_tree_model_sort_has_default_sort_func;
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

  g_return_val_if_fail (GTK_IS_TREE_MODEL (child_model), NULL);

  retval = GTK_TREE_MODEL (g_object_new (gtk_tree_model_sort_get_type (), NULL));

  gtk_tree_model_sort_set_model (GTK_TREE_MODEL_SORT (retval), child_model);

  return retval;
}

/* GObject callbacks */
static void
gtk_tree_model_sort_finalize (GObject *object)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) object;

  if (tree_model_sort->root)
    gtk_tree_model_sort_free_level (tree_model_sort, tree_model_sort->root);

  if (tree_model_sort->sort_list)
    {
      _gtk_tree_data_list_header_free (tree_model_sort->sort_list);
      tree_model_sort->sort_list = NULL;
    }
}

/* GtkObject callbacks */
static void
gtk_tree_model_sort_destroy (GtkObject *gobject)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) gobject;

  if (tree_model_sort->child_model)
    gtk_tree_model_sort_set_model (tree_model_sort, NULL);
}

static void
gtk_tree_model_sort_row_changed (GtkTreeModel *s_model,
				 GtkTreePath  *start_s_path,
				 GtkTreeIter  *start_s_iter,
				 gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter tmpiter;
  
  SortElt tmp;
  SortElt *elt;
  SortLevel *level;

  gboolean free_s_path;

  gint offset, index, i;
  
  g_return_if_fail (start_s_path != NULL || start_s_iter != NULL);

  if (!start_s_path)
    {
      free_s_path = TRUE;
      start_s_path = gtk_tree_model_get_path (s_model, start_s_iter);
    }

  path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort,
							 start_s_path);
  if (!path)
    {
      if (free_s_path)
	gtk_tree_path_free (start_s_path);
      return;
    }

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  
  level = iter.user_data;
  elt = iter.user_data2;
  
  if (level->array->len < 2)
    {
      if (free_s_path)
	gtk_tree_path_free (start_s_path);

      gtk_tree_model_row_changed (GTK_TREE_MODEL (data), path, &iter);
      
      gtk_tree_path_free (path);
      
      return;
    }

  if (!GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    get_child_iter_from_elt (tree_model_sort, &tmpiter, level, elt);
  
  offset = elt->offset;

  for (i = 0; i < level->array->len; i++)
    if (elt->offset == g_array_index (level->array, SortElt, i).offset)
      index = i;
  
  memcpy (&tmp, elt, sizeof (SortElt));
  g_array_remove_index (level->array, index);
  
  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    index = gtk_tree_model_sort_level_find_insert (tree_model_sort,
						   level,
						   &tmp.iter,
						   TRUE);
  else
    index = gtk_tree_model_sort_level_find_insert (tree_model_sort,
						   level,
						   &tmpiter,
						   TRUE);

  g_array_insert_val (level->array, index, tmp);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (data), path, &iter);
  /* FIXME: update stamp?? */
  
  gtk_tree_path_free (path);
  if (free_s_path)
    gtk_tree_path_free (start_s_path);
}

static void
gtk_tree_model_sort_row_inserted (GtkTreeModel          *s_model,
				  GtkTreePath           *s_path,
				  GtkTreeIter           *s_iter,
				  gpointer               data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (s_path != NULL || s_iter != NULL);
  
  if (!s_path)
    s_path = gtk_tree_model_get_path (s_model, s_iter);

  if (!GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort)
      && tree_model_sort->root)
    {
      gtk_tree_model_sort_free_level (tree_model_sort,
				      SORT_LEVEL (tree_model_sort->root));
      tree_model_sort->root = NULL;
    }
  else
    {
      GtkTreeIter real_s_iter;

      if (!s_iter)
	gtk_tree_model_get_iter (s_model, &real_s_iter, s_path);
      else
	real_s_iter = (* s_iter);

      if (!gtk_tree_model_sort_insert_value (tree_model_sort,
					     s_path, &real_s_iter))
	return;
    }

  if (!tree_model_sort->root)
    gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);

  path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort,
							 s_path);
  
  if (!path)
    return;

  gtk_tree_model_sort_increment_stamp (tree_model_sort);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
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

  /* we don't handle signals which we don't cover */
  if (!tree_model_sort->root)
    return;

  if (!GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    {
      gtk_tree_model_sort_free_level (tree_model_sort,
				      SORT_LEVEL (tree_model_sort->root));
      tree_model_sort->root = NULL;
    }

  if (!s_path)
    {
      s_path = gtk_tree_model_get_path (s_model, s_iter);
      free_s_path = TRUE;
    }

  path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort,
							 s_path);
  if (!path)
    return;
  
  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (data), path,
					&iter);
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

  /* we don't handle signals which we don't cover */
  if (!tree_model_sort->root)
    return;

  g_return_if_fail (s_path != NULL);
  path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort,
							 s_path);
  g_return_if_fail (path != NULL);

  if (!GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    {
      gtk_tree_model_sort_free_level (tree_model_sort,
				      SORT_LEVEL (tree_model_sort->root));
      tree_model_sort->root = NULL;
    }
  else
    {
      SortElt *elt;
      SortLevel *level;
      GtkTreeIter iter;
      gint offset;
      gint i;
      
      gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort),
			       &iter, path);
      level = SORT_LEVEL (iter.user_data);
      elt = SORT_ELT (iter.user_data2);
      offset = elt->offset;

      if (level->array->len == 1)
	{
	  //	  if (SORT_ELT (level->array->data)->parent == NULL)
	  if (level->parent_elt == NULL)
	    tree_model_sort->root = NULL;
	  else
	    //	    (SORT_ELT (level->array->data)->parent)->children = NULL;
	    level->parent_level->array = NULL;
	  gtk_tree_model_sort_free_level (tree_model_sort, level);
	}
      else
	{
	  for (i = 0; i < level->array->len; i++)
	    if (elt->offset == g_array_index (level->array, SortElt, i).offset)
	      break;

	  g_array_remove_index (level->array, i);

	  /* update all offsets */
	  for (i = 0; i < level->array->len; i++)
	    {
	      elt = & (g_array_index (level->array, SortElt, i));
	      if (elt->offset > offset)
		elt->offset--;
	    }
	}
    }

  gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);
  gtk_tree_model_sort_increment_stamp (tree_model_sort);
  gtk_tree_path_free (path);
}

static void
gtk_tree_model_sort_rows_reordered (GtkTreeModel *s_model,
				    GtkTreePath  *s_path,
				    GtkTreeIter  *s_iter,
				    gint         *new_order,
				    gpointer      data)
{
  int i;
  int len;
  int *my_new_order;
  SortElt *elt;
  SortLevel *level;
  gboolean free_s_path = FALSE;
  
  GtkTreeIter  iter;
  GtkTreePath *path;

  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  
  g_return_if_fail (s_path != NULL || s_iter != NULL);
  g_return_if_fail (new_order != NULL);

  if (!s_path)
    {
      s_path = gtk_tree_model_get_path (s_model, s_iter);
      free_s_path = TRUE;
    }

  if (!gtk_tree_path_get_indices (s_path))
    len = gtk_tree_model_iter_n_children (s_model, NULL);
  else
    len = gtk_tree_model_iter_n_children (s_model, s_iter);

  if (len < 2)
    {
      if (free_s_path)
	gtk_tree_path_free (s_path);
      return;
    }

  /** get correct sort level **/

  if (!gtk_tree_path_get_indices (s_path))
    level = SORT_LEVEL (tree_model_sort->root);
  else
    {
      path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort,
							     s_path);
      
      if (!path)
	{
	  if (free_s_path)
	    gtk_tree_path_free (s_path);
	  return;
	}

      if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path))
	{
	  /* no iter for me */
	  if (free_s_path)
	    gtk_tree_path_free (s_path);
	  gtk_tree_path_free (path);
	  return;
	}
      
      level = SORT_LEVEL (iter.user_data);
      elt = SORT_ELT (iter.user_data2);
      gtk_tree_path_free (path);

      /* FIXME: is this needed ? */
      if (!s_iter)
	gtk_tree_model_get_iter (s_model, s_iter, s_path);

      if (!elt->children)
	return;

      level = elt->children;
    }

  if (!level)
    {
      if (free_s_path)
	gtk_tree_path_free (s_path);

      /* ignore signal */
      return;
    }

  if (len != level->array->len)
    {
      if (free_s_path)
	gtk_tree_path_free (s_path);

      /* length mismatch, pretty bad, shouldn't happen */
      g_warning ("length mismatch!");
      
      return;
    }
  
  /** unsorted: set offsets, resort without reordered emission **/
  if (tree_model_sort->sort_column_id == -1)
    {
      path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort,
							     s_path);

      if (!path)
	{
	  if (free_s_path)
	    gtk_tree_path_free (s_path);
	  
	  return;
	}

      for (i = 0; i < level->array->len; i++)
	g_array_index (level->array, SortElt, i).offset = new_order[i];
      
      gtk_tree_model_sort_increment_stamp (tree_model_sort);
      
      gtk_tree_model_sort_sort_level (tree_model_sort, level,
				      FALSE, FALSE);

      if (gtk_tree_path_get_depth (path))
	{
	  gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort),
				   &iter,
				   path);
	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort),
					 path, &iter, new_order);
	}
      else
	gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort),
				       path, NULL, new_order);
      
      gtk_tree_path_free (path);

      if (free_s_path)
	gtk_tree_path_free (s_path);

      return;
    }

  /** sorted: update offsets, no emission of reordered signal **/
  g_print ("B");
  for (i = 0; i < level->array->len; i++)
    g_print ("%3d", g_array_index (level->array, SortElt, i).offset);
  g_print ("\n");

  g_print ("N");
  for (i = 0; i < level->array->len; i++)
    g_print ("%3d", new_order[i]);
  g_print ("\n");

  for (i = 0; i < level->array->len; i++)
    g_array_index (level->array, SortElt, i).offset =
      new_order[g_array_index (level->array, SortElt, i).offset];

  g_print ("A");
  for (i = 0; i < level->array->len; i++)
    g_print ("%3d", g_array_index (level->array, SortElt, i).offset);
  g_print ("\n");
  
  gtk_tree_model_sort_increment_stamp (tree_model_sort);

  if (free_s_path)
    gtk_tree_path_free (s_path);
}

/* Fulfill our model requirements */
static guint
gtk_tree_model_sort_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), 0);

  return 0;
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
gtk_tree_model_sort_get_iter (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreePath  *path)
{
  GtkTreeModelSort *tree_model_sort;
  gint *indices;
  SortLevel *level;
  gint depth, i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);

  tree_model_sort = (GtkTreeModelSort *) tree_model;
  indices = gtk_tree_path_get_indices (path);

  if (tree_model_sort->root == NULL)
    gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
  level = SORT_LEVEL (tree_model_sort->root);

  depth = gtk_tree_path_get_depth (path);
  if (depth == 0)
    return FALSE;

  for (i = 0; i < depth - 1; i++)
    {
      if ((level == NULL) ||
	  (level->array->len < indices[i]))
	return FALSE;

      if (g_array_index (level->array, SortElt, indices[i]).children == NULL)
	gtk_tree_model_sort_build_level (tree_model_sort, level, &g_array_index (level->array, SortElt, indices[i]));
      level = g_array_index (level->array, SortElt, indices[i]).children;
    }

  if (level == NULL)
    return FALSE;
  iter->stamp = tree_model_sort->stamp;
  iter->user_data = level;
  iter->user_data2 = &g_array_index (level->array, SortElt, indices[depth - 1]);

  return TRUE;
}

static GtkTreePath *
gtk_tree_model_sort_get_path (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter)
{
  GtkTreePath *retval;
  SortLevel *level;
  SortElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, NULL);

  retval = gtk_tree_path_new ();
  level = iter->user_data;
  elt = iter->user_data2;
  while (level != NULL)
    {
      gtk_tree_path_prepend_index (retval, elt - (SortElt *)level->array->data);

      elt = level->parent_elt;
      level = level->parent_level;
    }

  return retval;
}

static void
gtk_tree_model_sort_get_value (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       gint          column,
			       GValue       *value)
{
  GtkTreeIter child_iter;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp);

  GET_CHILD_ITER (tree_model, &child_iter, iter);
  gtk_tree_model_get_value (GTK_TREE_MODEL_SORT (tree_model)->child_model,
			    &child_iter, column, value);
}

static gboolean
gtk_tree_model_sort_iter_next (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  SortLevel *level;
  SortElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, FALSE);

  level = iter->user_data;
  elt = iter->user_data2;

  if (elt - (SortElt *)level->array->data >= level->array->len - 1)
    {
      iter->stamp = 0;
      return FALSE;
    }
  iter->user_data2 = elt + 1;

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_children (GtkTreeModel *tree_model,
				   GtkTreeIter  *iter,
				   GtkTreeIter  *parent)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  SortLevel *level;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (tree_model_sort->child_model != NULL, FALSE);
  if (parent) g_return_val_if_fail (tree_model_sort->stamp == parent->stamp, FALSE);

  if (parent == NULL)
    {

      if (tree_model_sort->root == NULL)
	gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
      if (tree_model_sort->root == NULL)
	return FALSE;

      level = tree_model_sort->root;
      iter->stamp = tree_model_sort->stamp;
      iter->user_data = level;
      iter->user_data2 = level->array->data;
    }
  else
    {
      if (((SortElt *)parent->user_data2)->children == NULL)
	gtk_tree_model_sort_build_level (tree_model_sort,
					 (SortLevel *)parent->user_data,
					 (SortElt *)parent->user_data2);
      if (((SortElt *)parent->user_data2)->children == NULL)
	return FALSE;
      iter->stamp = tree_model_sort->stamp;
      iter->user_data = ((SortElt *)parent->user_data2)->children;
      iter->user_data2 = ((SortLevel *)iter->user_data)->array->data;
    }
  
  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_has_child (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter)
{
  GtkTreeIter child_iter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, FALSE);

  GET_CHILD_ITER (tree_model, &child_iter, iter);

  return gtk_tree_model_iter_has_child (GTK_TREE_MODEL_SORT (tree_model)->child_model, &child_iter);
}

static gint
gtk_tree_model_sort_iter_n_children (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter)
{
  GtkTreeIter child_iter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, 0);
  if (iter) g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp, 0);

  if (iter == NULL)
    return gtk_tree_model_iter_n_children (GTK_TREE_MODEL_SORT (tree_model)->child_model, NULL);

  GET_CHILD_ITER (tree_model, &child_iter, iter);

  return gtk_tree_model_iter_n_children (GTK_TREE_MODEL_SORT (tree_model)->child_model, &child_iter);
}

static gboolean
gtk_tree_model_sort_iter_nth_child (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *parent,
				    gint          n)
{
  SortLevel *level;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);

  if (gtk_tree_model_sort_iter_children (tree_model, iter, parent) == FALSE)
    return FALSE;

  level = iter->user_data;
  if (n >= level->array->len)
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->user_data2 = &g_array_index (level->array, SortElt, n);
  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_parent (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 GtkTreeIter  *child)
{
  SortLevel *level;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == child->stamp, FALSE);

  level = child->user_data;

  if (level->parent_level)
    {
      iter->stamp = GTK_TREE_MODEL_SORT (tree_model)->stamp;
      iter->user_data = level->parent_level;
      iter->user_data2 = level->parent_elt;

      return TRUE;
    }
  return FALSE;
}

static void
gtk_tree_model_sort_ref_node (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  SortLevel *level;
  SortElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp);

  level = iter->user_data;
  elt = iter->user_data2;

  elt->ref_count++;
  level->ref_count++;
  if (level->ref_count == 1)
    {
      SortLevel *parent_level = level->parent_level;
      SortElt *parent_elt = level->parent_elt;
      /* We were at zero -- time to decrement the zero_ref_count val */
      do
	{
	  if (parent_elt)
	    parent_elt->zero_ref_count--;
	  else
	    tree_model_sort->zero_ref_count--;
	  
	  if (parent_level)
	    {
	      parent_elt = parent_level->parent_elt;
	      parent_level = parent_level->parent_level;
	    }
	}
      while (parent_level);
    }
}

static void
gtk_tree_model_sort_unref_node (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  SortLevel *level;
  SortElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_SORT (tree_model)->stamp == iter->stamp);

  level = iter->user_data;
  elt = iter->user_data2;

  elt->ref_count--;
  level->ref_count--;
  if (level->ref_count == 0)
    {
      SortLevel *parent_level = level->parent_level;
      SortElt *parent_elt = level->parent_elt;
      /* We were at zero -- time to decrement the zero_ref_count val */
      do
	{
	  if (parent_elt)
	    parent_elt->zero_ref_count++;
	  else
	    tree_model_sort->zero_ref_count++;
	  
	  if (parent_level)
	    {
	      parent_elt = parent_level->parent_elt;
	      parent_level = parent_level->parent_level;
	    }
	}
      while (parent_level);
    }
}

/* Sortable interface */
static gboolean
gtk_tree_model_sort_get_sort_column_id (GtkTreeSortable *sortable,
					gint            *sort_column_id,
					GtkSortType     *order)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)sortable;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (sortable), FALSE);

  if (tree_model_sort->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    return FALSE;

  if (sort_column_id)
    *sort_column_id = tree_model_sort->sort_column_id;
  if (order)
    *order = tree_model_sort->order;

  return TRUE;
}

static void
gtk_tree_model_sort_set_sort_column_id (GtkTreeSortable *sortable,
					gint             sort_column_id,
					GtkSortType      order)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)sortable;
  GList *list;
  
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (sortable));
  
  if (sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    {
      GtkTreeDataSortHeader *header = NULL;
      
      header = _gtk_tree_data_list_get_header (tree_model_sort->sort_list,
					       sort_column_id);

      /* we want to make sure that we have a function */
      g_return_if_fail (header != NULL);
      g_return_if_fail (header->func != NULL);
    }
  else
    {
      g_return_if_fail (tree_model_sort->default_sort_func != NULL);
    }

  if (tree_model_sort->sort_column_id == sort_column_id)
    {
      if (sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
	{
	  if (tree_model_sort->order == order)
	    return;
	}
      else
	return;
    }

  tree_model_sort->sort_column_id = sort_column_id;
  tree_model_sort->order = order;

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
      header = (GtkTreeDataSortHeader *) list->data;
      
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

static void
gtk_tree_model_sort_set_default_sort_func (GtkTreeSortable        *sortable,
					   GtkTreeIterCompareFunc  func,
					   gpointer                data,
					   GtkDestroyNotify        destroy)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)sortable;
  
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (sortable));
  
  if (tree_model_sort->default_sort_destroy)
    (* tree_model_sort->default_sort_destroy) (tree_model_sort->default_sort_data);

  tree_model_sort->default_sort_func = func;
  tree_model_sort->default_sort_data = data;
  tree_model_sort->default_sort_destroy = destroy;
}

static gboolean
gtk_tree_model_sort_has_default_sort_func (GtkTreeSortable *sortable)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)sortable;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (sortable), FALSE);

  return (tree_model_sort->default_sort_func != NULL);
}

/* sorting code - private */
static gint
gtk_tree_model_sort_compare_func (gconstpointer a,
				  gconstpointer b,
				  gpointer      user_data)
{
  gint retval;

  SortElt *sa = ((SortTuple *)a)->elt;
  SortElt *sb = ((SortTuple *)b)->elt;

  GtkTreeIter iter_a, iter_b;

  SortData *data = (SortData *)user_data;
  GtkTreeModelSort *tree_model_sort = data->tree_model_sort;

  GtkTreeIterCompareFunc func;
  gpointer f_data;

  if (tree_model_sort->sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    {
      GtkTreeDataSortHeader *header = NULL;

      header = 
	_gtk_tree_data_list_get_header (tree_model_sort->sort_list,
					tree_model_sort->sort_column_id);
      
      g_return_val_if_fail (header != NULL, 0);
      g_return_val_if_fail (header->func != NULL, 0);
      
      func = header->func;
      f_data = header->data;
    }
  else
    {
      /* absolutely SHOULD NOT happen: */
      g_return_val_if_fail (tree_model_sort->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, 0);
      g_return_val_if_fail (tree_model_sort->default_sort_func != (GtkTreeIterCompareFunc) 0x1, 0);
      g_return_val_if_fail (tree_model_sort->default_sort_func != NULL, 0);

      func = tree_model_sort->default_sort_func;
      f_data = tree_model_sort->default_sort_data;
    }

  /* shortcut, if we've the same offsets here, they should be equal */
  if (sa->offset == sb->offset)
    return 0;
  
  get_child_iter_from_elt (tree_model_sort, &iter_a, ((SortTuple *)a)->level, sa);
  get_child_iter_from_elt (tree_model_sort, &iter_b, ((SortTuple *)b)->level, sb);

  retval = (* func) (GTK_TREE_MODEL (tree_model_sort->child_model),
		     &iter_a, &iter_b, f_data);

  if (tree_model_sort->order == GTK_SORT_DESCENDING)
    {
      if (retval > 0)
	retval = -1;
      else if (retval < 0)
	retval = 1;
    }

  return retval;
}

static gint
gtk_tree_model_sort_offset_compare_func (gconstpointer a,
					 gconstpointer b,
					 gpointer      user_data)
{
  gint retval;

  SortElt *sa = ((SortTuple *)a)->elt;
  SortElt *sb = ((SortTuple *)b)->elt;

  SortData *data = (SortData *)user_data;

  if (sa->offset < sb->offset)
    retval = -1;
  else if (sa->offset > sb->offset)
    retval = 1;
  else
    retval = 0;

  if (data->tree_model_sort->order == GTK_SORT_DESCENDING)
    {
      if (retval > 0)
	retval = -1;
      else if (retval < 0)
	retval = 1;
    }

  return retval;
}

static void
gtk_tree_model_sort_sort_level (GtkTreeModelSort *tree_model_sort,
				SortLevel        *level,
				gboolean          recurse,
				gboolean          emit_reordered)
{
  gint i;
  GArray *sort_array;
  GArray *new_array;
  gint *new_order;

  GtkTreeIter iter;
  GtkTreePath *path;
  
  SortData *data;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));
  g_return_if_fail (level != NULL);
  
  if (level->array->len < 1 && !((SortElt *)level->array->data)->children)
    return;
  
  data = g_new0 (SortData, 1);

  if (level->parent_elt)
    {
      data->parent_a = gtk_tree_model_sort_elt_get_path (level->parent_level,
							 level->parent_elt);
      data->parent_b = gtk_tree_path_copy (data->parent_a);
    }
  else
    {
      data->parent_a = gtk_tree_path_new ();
      data->parent_b = gtk_tree_path_new ();
    }

  data->tree_model_sort = tree_model_sort;

  sort_array = g_array_sized_new (FALSE, FALSE, sizeof (SortTuple), level->array->len);
  
  for (i = 0; i < level->array->len; i++)
    {
      SortTuple tuple;

      tuple.elt = &g_array_index (level->array, SortElt, i);
      tuple.level = level;
      tuple.children = tuple.elt->children;
      tuple.offset = tuple.elt->offset;

      g_array_append_val (sort_array, tuple);
    }

  g_print ("-- sort: sort_column_id = %d // default_sort_func = %.2x\n",
	   tree_model_sort->sort_column_id, tree_model_sort->default_sort_func);

  if (tree_model_sort->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    g_array_sort_with_data (sort_array,
			    gtk_tree_model_sort_offset_compare_func,
			    data);
  else
    g_array_sort_with_data (sort_array,
			    gtk_tree_model_sort_compare_func,
			    data);

  gtk_tree_path_free (data->parent_a);
  gtk_tree_path_free (data->parent_b);
  g_free (data);

  /* let the world know about our absolutely great new order */
  new_array = g_array_sized_new (FALSE, FALSE, sizeof (SortElt), level->array->len);
  g_array_set_size (new_array, level->array->len);
  new_order = g_new (gint, level->array->len);

  for (i = 0; i < level->array->len; i++)
    {
      SortElt *elt1;
      SortElt *elt2;
      gint j;
      
      elt1 = &g_array_index (level->array, SortElt, i);

      for (j = 0; j < sort_array->len; j++)
	if (elt1->offset == g_array_index (sort_array, SortTuple, j).offset)
	  break;

      if (j >= level->array->len)
	/* isn't supposed to happen */
	break;

      new_order[i] = j;

      /* copy ... */
      memcpy (&g_array_index (new_array, SortElt, j), elt1, sizeof (SortElt));
      elt2 = &g_array_index (new_array, SortElt, j);

      /* point children to correct parent */
      if (elt2->children)
	{
	  elt2->children->parent_elt = elt2;
	  elt2->children->parent_level = level;
	}
    }

  g_array_free (level->array, TRUE);
  level->array = new_array;
  
  g_array_free (sort_array, TRUE);

  if (emit_reordered)
    {
      gtk_tree_model_sort_increment_stamp (tree_model_sort);

      if (level->parent_elt)
	{
	  iter.stamp = tree_model_sort->stamp;
	  iter.user_data = level->parent_level;
	  iter.user_data2 = level->parent_elt;

	  path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_model_sort),
					  &iter);

	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort), path,
					 &iter, new_order);
	}
      else
	{
	  /* toplevel list */
	  path = gtk_tree_path_new ();
	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort), path,
					 NULL, new_order);
	}

      gtk_tree_path_free (path);
    }
  
  g_free (new_order);

  /* recurse, if possible */
  if (recurse)
    {
      for (i = 0; i < level->array->len; i++)
	{
	  SortElt *elt = &g_array_index (level->array, SortElt, i);

	  if (elt->children)
	    gtk_tree_model_sort_sort_level (tree_model_sort,
					    elt->children,
					    TRUE, emit_reordered);
	}
    }
}

static void
gtk_tree_model_sort_sort (GtkTreeModelSort *tree_model_sort)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));
  
  if (!tree_model_sort->root)
    {
      g_print ("sort: bailing out, no level built yet\n");
      return;
    }

  if (tree_model_sort->sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    {
      GtkTreeDataSortHeader *header = NULL;

      header = _gtk_tree_data_list_get_header (tree_model_sort->sort_list,
					       tree_model_sort->sort_column_id);

      /* we want to make sure that we have a function */
      g_return_if_fail (header != NULL);
      g_return_if_fail (header->func != NULL);
    }
  else
    {
      g_return_if_fail (tree_model_sort->default_sort_func != NULL);
    }

  gtk_tree_model_sort_sort_level (tree_model_sort, tree_model_sort->root,
				  TRUE, TRUE);
}

/* signal helpers */
static gint
gtk_tree_model_sort_level_find_insert (GtkTreeModelSort *tree_model_sort,
				       SortLevel        *level,
				       GtkTreeIter      *iter,
				       gboolean          skip_sort_elt)
{
  gint middle;
  gint cmp;
  SortElt *tmp_elt;
  GtkTreeIter tmp_iter;

  GtkTreeIterCompareFunc func;
  gpointer data;

  if (tree_model_sort->sort_column_id == -1)
    return level->array->len;
  
  {
    GtkTreeDataSortHeader *header;

    header = _gtk_tree_data_list_get_header (tree_model_sort->sort_list,
					     tree_model_sort->sort_column_id);
    
    g_return_val_if_fail (header != NULL, 0);
    g_return_val_if_fail (header->func != NULL, 0);

    func = header->func;
    data = header->data;
  }
  
  for (middle = 0; middle < level->array->len; middle++)
    {
      tmp_elt = &(g_array_index (level->array, SortElt, middle));

      if (!skip_sort_elt && SORT_ELT (iter) == tmp_elt)
	continue;

      get_child_iter_from_elt (tree_model_sort, &tmp_iter,
			       level, tmp_elt);

      if (tree_model_sort->order == GTK_SORT_ASCENDING)
	cmp = (* func) (GTK_TREE_MODEL (tree_model_sort->child_model),
			&tmp_iter, iter, data);
      else
	cmp = (* func) (GTK_TREE_MODEL (tree_model_sort->child_model),
			iter, &tmp_iter, data);
      
      if (cmp > 0)
	break;
    }
  
  return middle;
}

static gboolean
gtk_tree_model_sort_insert_value (GtkTreeModelSort *tree_model_sort,
				  GtkTreePath      *s_path,
				  GtkTreeIter      *s_iter)
{
  gint offset, index, j;
  SortLevel *level;
  SortElt elt;
  SortElt *tmp_elt;
  GtkTreeIter iter;
  GtkTreePath *tmp_path;

  return FALSE;

  if (!tree_model_sort->root)
    {
      gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
      return FALSE;
    }

  offset = gtk_tree_path_get_indices (s_path)[gtk_tree_path_get_depth (s_path) - 1];
  
  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    elt.iter = *s_iter;
  elt.offset = offset;
  elt.zero_ref_count = 0;
  elt.ref_count = 0;
  elt.children = NULL;
  
  tmp_path = gtk_tree_path_copy (s_path);

  if (gtk_tree_path_up (tmp_path))
    {
      GtkTreePath *parent_path;

      parent_path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort, tmp_path);

      if (!parent_path)
	{
	  gtk_tree_path_free (tmp_path);
	  return FALSE;
	}

      if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort), &iter,
				    parent_path))
	{
	  gtk_tree_path_free (parent_path);
	  gtk_tree_path_free (tmp_path);
	  return FALSE;
	}

      level = SORT_LEVEL (iter.user_data);
      gtk_tree_path_free (parent_path);

      if (!level)
	{
	  gtk_tree_path_free (tmp_path);
	  return FALSE;
	}
    }
  else
    {
      if (!tree_model_sort->root)
	gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
      level = SORT_LEVEL (tree_model_sort->root);
    }

  gtk_tree_path_free (tmp_path);

  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    index = gtk_tree_model_sort_level_find_insert (tree_model_sort,
						   level,
						   &elt.iter,
						   FALSE);
  else
    {
      GtkTreeIter tmpiter;
      gtk_tree_model_sort_convert_child_iter_to_iter (tree_model_sort,
						      &tmpiter,
						      s_iter);
      index = gtk_tree_model_sort_level_find_insert (tree_model_sort,
						     level,
						     &tmpiter,
						     FALSE);
    }

  g_array_insert_vals (level->array, index, &elt, 1);
  
  /* update all larger offsets */
  tmp_elt = SORT_ELT (level->array->data);
  for (j = 0; j < level->array->len; j++, tmp_elt++)
    if ((tmp_elt->offset >= offset) && j != index)
      tmp_elt->offset++;

  return TRUE;
}

/* sort elt stuff */
static GtkTreePath *
gtk_tree_model_sort_elt_get_path (SortLevel *level,
				  SortElt *elt)
{
  gchar *str = NULL;
  GList *i;
  GList *offsets = NULL;
  SortLevel *walker = level;
  SortElt *walker2 = elt;
  GtkTreePath *path;
  
  g_return_val_if_fail (level != NULL, NULL);
  g_return_val_if_fail (elt != NULL, NULL);
  
  while (walker && walker2)
    {
      offsets = g_list_prepend (offsets,
                                g_strdup_printf ("%d", walker2->offset));
      walker2 = walker->parent_elt;
      walker = walker->parent_level;
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

static void
get_child_iter_from_elt_no_cache (GtkTreeModelSort *tree_model_sort,
				  GtkTreeIter      *child_iter,
				  SortLevel        *level,
				  SortElt          *elt)
{
  GtkTreePath *path;

  SortElt *elt_i = elt;
  SortLevel *level_i = level;
  
  path = gtk_tree_path_new ();
  
  while (level_i)
    {
      gtk_tree_path_prepend_index (path, elt_i->offset);
      
      elt_i = level_i->parent_elt;
      level_i = level_i->parent_level;
    }
  
  gtk_tree_model_get_iter (tree_model_sort->child_model, child_iter, path);
  gtk_tree_path_free (path);
}

static void
get_child_iter_from_elt (GtkTreeModelSort *tree_model_sort,
			 GtkTreeIter      *child_iter,
			 SortLevel        *level,
			 SortElt          *elt)
{
  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    *child_iter = elt->iter;
  else
    {
      GtkTreeIter tmp;
      GtkTreePath *path = gtk_tree_model_sort_elt_get_path (level, elt);
      gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort), &tmp, path);
      gtk_tree_path_free (path);

      GET_CHILD_ITER (tree_model_sort, child_iter, &tmp);
    }
}

/**
 * gtk_tree_model_sort_set_model:
 * @tree_model_sort: The #GtkTreeModelSort.
 * @child_model: A #GtkTreeModel, or NULL.
 *
 * Sets the model of @tree_model_sort to be @model.  If @model is NULL, then the
 * old model is unset.  The sort function is unset as a result of this call.
 * The model will be in an unsorted state until a sort function is set.
 **/
static void
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

      /* reset our state */
      gtk_tree_model_sort_free_level (tree_model_sort, tree_model_sort->root);
      tree_model_sort->root = NULL;
      _gtk_tree_data_list_header_free (tree_model_sort->sort_list);
      tree_model_sort->sort_list = NULL;
      g_object_unref (G_OBJECT (tree_model_sort->child_model));
    }

  tree_model_sort->child_model = child_model;

  if (child_model)
    {
      GType *types;
      gint i, n_columns;

      tree_model_sort->changed_id =
        g_signal_connect (child_model, "row_changed",
                          G_CALLBACK (gtk_tree_model_sort_row_changed),
                          tree_model_sort);
      tree_model_sort->inserted_id =
        g_signal_connect (child_model, "row_inserted",
                          G_CALLBACK (gtk_tree_model_sort_row_inserted),
                          tree_model_sort);
      tree_model_sort->has_child_toggled_id =
        g_signal_connect (child_model, "row_has_child_toggled",
                          G_CALLBACK (gtk_tree_model_sort_row_has_child_toggled),
                          tree_model_sort);
      tree_model_sort->deleted_id =
        g_signal_connect (child_model, "row_deleted",
                          G_CALLBACK (gtk_tree_model_sort_row_deleted),
                          tree_model_sort);
      tree_model_sort->reordered_id =
	g_signal_connect (child_model, "rows_reordered",
			  G_CALLBACK (gtk_tree_model_sort_rows_reordered),
			  tree_model_sort);

      tree_model_sort->child_flags = gtk_tree_model_get_flags (child_model);
      n_columns = gtk_tree_model_get_n_columns (child_model);

      types = g_new (GType, n_columns);
      for (i = 0; i < n_columns; i++)
        types[i] = gtk_tree_model_get_column_type (child_model, i);

      tree_model_sort->sort_list = _gtk_tree_data_list_header_new (n_columns, types);
      g_free (types);

      tree_model_sort->default_sort_func = (GtkTreeIterCompareFunc)0x1;
      tree_model_sort->stamp = g_random_int ();
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
GtkTreeModel *
gtk_tree_model_sort_get_model (GtkTreeModelSort  *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), NULL);

  return tree_model->child_model;
}


/**
 * gtk_tree_model_sort_convert_child_path_to_path:
 * @tree_model_sort: A #GtkTreeModelSort
 * @child_path: A #GtkTreePath to convert
 * 
 * Converts @child_path to a path relative to @tree_model_sort.  That is,
 * @child_path points to a path in the child model.  The returned path will
 * point to the same row in the sorted model.  If @child_path isn't a valid path
 * on the child model, then %NULL is returned.
 * 
 * Return value: A newly allocated #GtkTreePath, or %NULL
 **/
GtkTreePath *
gtk_tree_model_sort_convert_child_path_to_path (GtkTreeModelSort *tree_model_sort,
						GtkTreePath      *child_path)
{
  gint *child_indices;
  GtkTreePath *retval;
  SortLevel *level;
  gint i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort), NULL);
  g_return_val_if_fail (tree_model_sort->child_model != NULL, NULL);
  g_return_val_if_fail (child_path != NULL, NULL);

  retval = gtk_tree_path_new ();
  child_indices = gtk_tree_path_get_indices (child_path);

  level = SORT_LEVEL (tree_model_sort->root);
  for (i = 0; i < gtk_tree_path_get_depth (child_path); i++)
    {
      gint j;
      gboolean found_child = FALSE;

      if (!level)
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}

      if (child_indices[i] >= level->array->len)
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}
      for (j = 0; j < level->array->len; j++)
	{
	  if ((g_array_index (level->array, SortElt, j)).offset == child_indices[i])
	    {
	      gtk_tree_path_prepend_index (retval, j);
	      level = g_array_index (level->array, SortElt, j).children;
	      found_child = TRUE;
	      break;
	    }
	}
      if (! found_child)
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}
    }

  return retval;
}

/**
 * gtk_tree_model_sort_convert_child_iter_to_iter:
 * @tree_model_sort: A #GtkTreeModelSort
 * @sort_iter: An uninitialized #GtkTreeIter.
 * @child_iter: A valid #GtkTreeIter pointing to a row on the child model
 * 
 * Sets @sort_iter to point to the row in @tree_model_sort that corresponds to
 * the row pointed at by @child_iter.
 **/
void
gtk_tree_model_sort_convert_child_iter_to_iter (GtkTreeModelSort *tree_model_sort,
						GtkTreeIter      *sort_iter,
						GtkTreeIter      *child_iter)
{
  GtkTreePath *child_path, *path;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));
  g_return_if_fail (tree_model_sort->child_model != NULL);
  g_return_if_fail (sort_iter != NULL);
  g_return_if_fail (child_iter != NULL);

  sort_iter->stamp = 0;

  child_path = gtk_tree_model_get_path (tree_model_sort->child_model, child_iter);
  g_return_if_fail (child_path != NULL);

  path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort, child_path);
  gtk_tree_path_free (child_path);
  g_return_if_fail (path != NULL);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort), sort_iter, path);
  gtk_tree_path_free (path);
}

/**
 * gtk_tree_model_sort_convert_path_to_child_path:
 * @tree_model_sort: A #GtkTreeModelSort
 * @sorted_path: A #GtkTreePath to convert
 * 
 * Converts @sort_path to a path on the child model of @tree_model_sort.  That
 * is, @sort_path points ot a location in @tree_model_sort.  The returned path
 * will point to the same location in the model not being sorted.  If @path does not point to a 
 * 
 * Return value: A newly allocated #GtkTreePath, or %NULLL
 **/
GtkTreePath *
gtk_tree_model_sort_convert_path_to_child_path (GtkTreeModelSort *tree_model_sort,
						GtkTreePath      *sorted_path)
{
  gint *sorted_indices;
  GtkTreePath *retval;
  SortLevel *level;
  gint i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort), NULL);
  g_return_val_if_fail (tree_model_sort->child_model != NULL, NULL);
  g_return_val_if_fail (sorted_path != NULL, NULL);

  retval = gtk_tree_path_new ();
  sorted_indices = gtk_tree_path_get_indices (sorted_path);
  if (tree_model_sort->root == NULL)
    gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
  level = SORT_LEVEL (tree_model_sort->root);

  for (i = 0; i < gtk_tree_path_get_depth (sorted_path); i++)
    {
      if ((level == NULL) ||
	  (level->array->len > sorted_indices[i]))
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}
      if (g_array_index (level->array, SortElt, sorted_indices[i]).children == NULL)
	gtk_tree_model_sort_build_level (tree_model_sort, level, &g_array_index (level->array, SortElt, sorted_indices[i]));
      if (level == NULL)
	
      gtk_tree_path_append_index (retval, g_array_index (level->array, SortElt, i).offset);
    }
  
  return retval;
}

/**
 * gtk_tree_model_sort_convert_iter_to_child_iter:
 * @tree_model_sort: A #GtkTreeModelSort
 * @child_iter: An uninitialized #GtkTreeIter
 * @sorted_iter: A valid #GtkTreeIter pointing to a row on @tree_model_sort.
 * 
 * Sets @child_iter to point to the row pointed to by *sorted_iter.
 **/
void
gtk_tree_model_sort_convert_iter_to_child_iter (GtkTreeModelSort *tree_model_sort,
						GtkTreeIter      *child_iter,
						GtkTreeIter      *sorted_iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));
  g_return_if_fail (tree_model_sort->child_model != NULL);
  g_return_if_fail (child_iter != NULL);
  g_return_if_fail (sorted_iter != NULL);
  g_return_if_fail (sorted_iter->stamp == tree_model_sort->stamp);

  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    {
      *child_iter = SORT_ELT (sorted_iter->user_data2)->iter;
    }
  else
    {
      GtkTreePath *path;
      SortElt *elt;
      SortLevel *level;

      path = gtk_tree_path_new ();
      elt = SORT_ELT (sorted_iter->user_data2);
      level = SORT_LEVEL (sorted_iter->user_data);

      while (level)
	{
	  gtk_tree_path_prepend_index (path, elt->offset);

	  elt = level->parent_elt;
	  level = level->parent_level;
	}

      gtk_tree_model_get_iter (tree_model_sort->child_model, child_iter, path);
      gtk_tree_path_free (path);
    }
}

static void
gtk_tree_model_sort_build_level (GtkTreeModelSort *tree_model_sort,
				 SortLevel        *parent_level,
				 SortElt          *parent_elt)
{
  GtkTreeIter iter;
  SortLevel *new_level;
  gint length = 0;
  gint i;

  g_assert (tree_model_sort->child_model != NULL);

  if (parent_level == NULL)
    {
      if (gtk_tree_model_get_iter_root (tree_model_sort->child_model, &iter) == FALSE)
	return;
      length = gtk_tree_model_iter_n_children (tree_model_sort->child_model, NULL);
    }
  else
    {
      GtkTreeIter parent_iter;
      GtkTreeIter child_parent_iter;

      parent_iter.stamp = tree_model_sort->stamp;
      parent_iter.user_data = parent_level;
      parent_iter.user_data2 = parent_elt;

      gtk_tree_model_sort_convert_iter_to_child_iter (tree_model_sort,
						      &child_parent_iter,
						      &parent_iter);
      if (gtk_tree_model_iter_children (tree_model_sort->child_model,
					&iter,
					&child_parent_iter) == FALSE)
	return;
      length = gtk_tree_model_iter_n_children (tree_model_sort->child_model, &child_parent_iter);
    }

  g_return_if_fail (length > 0);

  new_level = g_new (SortLevel, 1);
  new_level->array = g_array_sized_new (FALSE, FALSE, sizeof (SortElt), length);
  new_level->ref_count = 0;
  new_level->parent_elt = parent_elt;
  new_level->parent_level = parent_level;

  if (parent_elt)
    parent_elt->children = new_level;
  else
    tree_model_sort->root = new_level;

  /* increase the count of zero ref_counts.*/
  do
    {
      if (parent_elt)
	parent_elt->zero_ref_count++;
      else
	tree_model_sort->zero_ref_count++;

      if (parent_level)
	{
	  parent_elt = parent_level->parent_elt;
	  parent_level = parent_level->parent_level;
	}
    }
  while (parent_level);

  for (i = 0; i < length; i++)
    {
      SortElt sort_elt;
      sort_elt.offset = i;
      sort_elt.zero_ref_count = 0;
      sort_elt.ref_count = 0;
      sort_elt.children = NULL;

      if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
	{
	  sort_elt.iter = iter;
	  if (gtk_tree_model_iter_next (tree_model_sort->child_model, &iter) == FALSE &&
	      i < length - 1)
	    {
	      g_warning ("There is a discrepency between the sort model and the child model.");
	      return;
	    }
	}
      g_array_append_val (new_level->array, sort_elt);
    }

  /* sort level */
  gtk_tree_model_sort_sort_level (tree_model_sort, new_level,
				  FALSE, FALSE);
}

static void
gtk_tree_model_sort_free_level (GtkTreeModelSort *tree_model_sort,
				SortLevel        *sort_level)
{
  gint i;

  g_assert (sort_level);

  if (sort_level->ref_count == 0)
    {
      SortLevel *parent_level = sort_level->parent_level;
      SortElt *parent_elt = sort_level->parent_elt;

      do
	{
	  if (parent_elt)
	    parent_elt->zero_ref_count++;
	  else
	    tree_model_sort->zero_ref_count++;
	  
	  if (parent_level)
	    {
	      parent_elt = parent_level->parent_elt;
	      parent_level = parent_level->parent_level;
	    }
	}
      while (parent_level);
    }

  for (i = 0; i < sort_level->array->len; i++)
    {
      if (g_array_index (sort_level->array, SortElt, i).children)
	gtk_tree_model_sort_free_level (tree_model_sort, 
					(SortLevel *)&g_array_index (sort_level->array, SortElt, i).children);
    }

  if (sort_level->parent_elt)
    {
      sort_level->parent_elt->children = NULL;
    }
  else
    {
      tree_model_sort->root = NULL;
    }
  g_array_free (sort_level->array, TRUE);
  g_free (sort_level);
}

static void
gtk_tree_model_sort_increment_stamp (GtkTreeModelSort *tree_model_sort)
{
  tree_model_sort->stamp++;
  gtk_tree_model_sort_clear_cache (tree_model_sort);
}

static void
gtk_tree_model_sort_clear_cache_helper (GtkTreeModelSort *tree_model_sort,
					SortLevel        *level)
{
  gint i;

  g_assert (level != NULL);

  for (i = 0; i < level->array->len; i++)
    {
      if (g_array_index (level->array, SortElt, i).zero_ref_count > 0)
	{
	  gtk_tree_model_sort_clear_cache_helper (tree_model_sort, g_array_index (level->array, SortElt, i).children);
	}
    }

  if (level->ref_count == 0)
    {
      gtk_tree_model_sort_free_level (tree_model_sort, level);
      return;
    }

}

/**
 * gtk_tree_model_sort_reset_default_sort_func:
 * @tree_model_sort: A #GtkTreeModelSort
 * 
 * This resets the default sort function to be in the 'unsorted' state.  That
 * is, it is in the same order as the child model.
 **/
void
gtk_tree_model_sort_reset_default_sort_func (GtkTreeModelSort *tree_model_sort)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));

  g_print ("RESET DEFAULT SORT FUNC\n");

  if (tree_model_sort->default_sort_destroy)
    (* tree_model_sort->default_sort_destroy) (tree_model_sort->default_sort_data);

  tree_model_sort->default_sort_func = (GtkTreeIterCompareFunc) 0x1;
  tree_model_sort->default_sort_data = NULL;
  tree_model_sort->default_sort_destroy = NULL;
  tree_model_sort->sort_column_id = GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID;
}

/**
 * gtk_tree_model_sort_clear_cache:
 * @tree_model_sort: A #GtkTreeModelSort
 * 
 * This function should almost never be called.  It clears the @tree_model_sort
 * of any cached iterators that haven't been reffed with
 * gtk_tree_model_ref_node().  This might be useful if the child model being
 * sorted is static (and doesn't change often) and there has been a lot of
 * unreffed access to nodes.  As a side effect of this function, all unreffed
 * iters will be invalid.
 **/
void
gtk_tree_model_sort_clear_cache (GtkTreeModelSort *tree_model_sort)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));

  if (tree_model_sort->zero_ref_count)
    gtk_tree_model_sort_clear_cache_helper (tree_model_sort, (SortLevel *)tree_model_sort->root);
}
