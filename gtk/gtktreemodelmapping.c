/* gtktreemodelmapping.c
 * Copyright (C) 2000  Red Hat, Inc.,  Alexander Larsson <alexl@redhat.com>
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

#include "gtktreemodelmapping.h"
#include "gtksignal.h"

enum {
  CHANGED,
  INSERTED,
  CHILD_TOGGLED,
  DELETED,
  LAST_SIGNAL
};

static guint tree_model_mapping_signals[LAST_SIGNAL] = { 0 };

static void         gtk_tree_model_mapping_init            (GtkTreeModelMapping      *tree_model_mapping);
static void         gtk_tree_model_mapping_class_init      (GtkTreeModelMappingClass *tree_model_mapping_class);
static void         gtk_tree_model_mapping_tree_model_init (GtkTreeModelIface        *iface);
static void         gtk_tree_model_mapping_finalize        (GObject                  *object);
/* signals */
static void         gtk_tree_model_mapping_changed         (GtkTreeModel             *model,
							    GtkTreePath              *path,
							    GtkTreeIter              *iter,
							    gpointer                  data);
static void         gtk_tree_model_mapping_inserted        (GtkTreeModel             *model,
							    GtkTreePath              *path,
							    GtkTreeIter              *iter,
							    gpointer                  data);
static void         gtk_tree_model_mapping_child_toggled   (GtkTreeModel             *model,
							    GtkTreePath              *path,
							    GtkTreeIter              *iter,
							    gpointer                  data);
static void         gtk_tree_model_mapping_deleted         (GtkTreeModel             *model,
							    GtkTreePath              *path,
							    gpointer                  data);
static void         gtk_tree_model_mapping_reordered       (GtkTreeModel             *model,
							    GtkTreePath              *path,
							    gint                     *new_order);
/* vtable */
static GtkTreeModelFlags gtk_tree_model_mapping_get_flags  (GtkTreeModel             *tree_model);
static gint         gtk_tree_model_mapping_get_n_columns   (GtkTreeModel             *tree_model);
static GType        gtk_tree_model_mapping_get_column_type (GtkTreeModel             *tree_model,
							    gint                      index);
static gboolean     gtk_tree_model_mapping_get_iter        (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter,
							    GtkTreePath              *path);
static GtkTreePath *gtk_tree_model_mapping_get_path        (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter);
static void         gtk_tree_model_mapping_get_value       (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter,
							    gint                      column,
							    GValue                   *value);
static gboolean     gtk_tree_model_mapping_iter_next       (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter);
static gboolean     gtk_tree_model_mapping_iter_children   (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter,
							    GtkTreeIter              *parent);
static gboolean     gtk_tree_model_mapping_iter_has_child  (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter);
static gint         gtk_tree_model_mapping_iter_n_children (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter);
static gboolean     gtk_tree_model_mapping_iter_nth_child  (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter,
							    GtkTreeIter              *parent,
							    gint                      n);
static gboolean     gtk_tree_model_mapping_iter_parent     (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter,
							    GtkTreeIter              *child);
static void         gtk_tree_model_mapping_ref_iter        (GtkTreeModel             *tree_model,
							    GtkTreeIter              *iter);
static void         gtk_tree_model_mapping_unref_iter      (GtkTreeModel          *tree_model,
							    GtkTreeIter              *iter);


GtkType
gtk_tree_model_mapping_get_type (void)
{
  static GtkType tree_model_mapping_type = 0;

  if (!tree_model_mapping_type)
    {
      static const GTypeInfo tree_model_mapping_info =
      {
        sizeof (GtkTreeModelMappingClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_tree_model_mapping_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkTreeModelMapping),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_tree_model_mapping_init
      };

      static const GInterfaceInfo tree_model_info =
      {
	(GInterfaceInitFunc) gtk_tree_model_mapping_tree_model_init,
	NULL,
	NULL
      };

      tree_model_mapping_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkTreeModelMapping", &tree_model_mapping_info, 0);
      g_type_add_interface_static (tree_model_mapping_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
    }

  return tree_model_mapping_type;
}

static void
gtk_tree_model_mapping_class_init (GtkTreeModelMappingClass *tree_model_mapping_class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) tree_model_mapping_class;

  object_class->finalize = gtk_tree_model_mapping_finalize;

  tree_model_mapping_signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeModelMappingClass, changed),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_model_mapping_signals[INSERTED] =
    gtk_signal_new ("inserted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeModelMappingClass, inserted),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_model_mapping_signals[CHILD_TOGGLED] =
    gtk_signal_new ("child_toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeModelMappingClass, child_toggled),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_model_mapping_signals[DELETED] =
    gtk_signal_new ("deleted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeModelMappingClass, deleted),
                    gtk_marshal_VOID__POINTER,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
}

static void
gtk_tree_model_mapping_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_model_mapping_get_flags;
  iface->get_n_columns = gtk_tree_model_mapping_get_n_columns;
  iface->get_column_type = gtk_tree_model_mapping_get_column_type;
  iface->get_iter = gtk_tree_model_mapping_get_iter;
  iface->get_path = gtk_tree_model_mapping_get_path;
  iface->get_value = gtk_tree_model_mapping_get_value;
  iface->iter_next = gtk_tree_model_mapping_iter_next;
  iface->iter_children = gtk_tree_model_mapping_iter_children;
  iface->iter_has_child = gtk_tree_model_mapping_iter_has_child;
  iface->iter_n_children = gtk_tree_model_mapping_iter_n_children;
  iface->iter_nth_child = gtk_tree_model_mapping_iter_nth_child;
  iface->iter_parent = gtk_tree_model_mapping_iter_parent;
  iface->ref_iter = gtk_tree_model_mapping_ref_iter;
  iface->unref_iter = gtk_tree_model_mapping_unref_iter;
}

static void
gtk_tree_model_mapping_init (GtkTreeModelMapping *tree_model_mapping)
{
}

GtkTreeModel *
gtk_tree_model_mapping_new (void)
{
  return GTK_TREE_MODEL (gtk_type_new (gtk_tree_model_mapping_get_type ()));
}

GtkTreeModel *
gtk_tree_model_mapping_new_with_model (GtkTreeModel      *child_model)
{
  GtkTreeModel *retval;

  retval = gtk_tree_model_mapping_new ();
  gtk_tree_model_mapping_set_model (GTK_TREE_MODEL_MAPPING (retval), child_model);
  return retval;
}

void
gtk_tree_model_mapping_set_n_columns (GtkTreeModelMapping *tree_model_mapping,
				      gint                 n_columns)
{
  g_return_if_fail (tree_model_mapping != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model_mapping));
  
  tree_model_mapping->n_columns = n_columns;
  tree_model_mapping->column_maps = g_realloc (tree_model_mapping->column_maps, sizeof (GtkTreeModelMapping)*n_columns);
}

void
gtk_tree_model_mapping_set_column_mapping (GtkTreeModelMapping *tree_model_mapping,
					   gint                 column,
					   gint                 src_column,
					   GType                col_type,
					   GValueMapFunc        map_func,
					   gpointer             user_data)
{
  GtkTreeModelMappingMap *map;
  
  g_return_if_fail (tree_model_mapping != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model_mapping));
  g_return_if_fail (column < tree_model_mapping->n_columns);

  map = &tree_model_mapping->column_maps[column];
  map->src_column = src_column;
  map->col_type = col_type;
  map->map_func = map_func;
  map->user_data = user_data;
}

/**
 * gtk_tree_model_mapping_set_model:
 * @tree_model_mapping: The #GtkTreeModelMapping.
 * @child_model: A #GtkTreeModel, or NULL.
 * 
 * Sets the model of @tree_model_mapping to be @model.  If @model is NULL, then the
 * old model is unset.
 **/
void
gtk_tree_model_mapping_set_model (GtkTreeModelMapping *tree_model_mapping,
				  GtkTreeModel     *child_model)
{
  g_return_if_fail (tree_model_mapping != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model_mapping));

  if (child_model)
    g_object_ref (G_OBJECT (child_model));

  if (tree_model_mapping->child_model)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_model_mapping->child_model),
				     gtk_tree_model_mapping_changed,
				     tree_model_mapping);
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_model_mapping->child_model),
				     gtk_tree_model_mapping_inserted,
				     tree_model_mapping);
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_model_mapping->child_model),
				     gtk_tree_model_mapping_child_toggled,
				     tree_model_mapping);
      gtk_signal_disconnect_by_func (GTK_OBJECT (tree_model_mapping->child_model),
				     gtk_tree_model_mapping_deleted,
				     tree_model_mapping);

      g_object_unref (G_OBJECT (tree_model_mapping->child_model));
    }

  tree_model_mapping->child_model = child_model;

  if (child_model)
    {
      gtk_signal_connect (GTK_OBJECT (child_model),
			  "changed",
			  gtk_tree_model_mapping_changed,
			  tree_model_mapping);
      gtk_signal_connect (GTK_OBJECT (child_model),
			  "inserted",
			  gtk_tree_model_mapping_inserted,
			  tree_model_mapping);
      gtk_signal_connect (GTK_OBJECT (child_model),
			  "child_toggled",
			  gtk_tree_model_mapping_child_toggled,
			  tree_model_mapping);
      gtk_signal_connect (GTK_OBJECT (child_model),
			  "deleted",
			  gtk_tree_model_mapping_deleted,
			  tree_model_mapping);
    }
}

/**
 * gtk_tree_model_mapping_get_model:
 * @tree_model: a #GtkTreeModelMapping
 * 
 * Returns the model the #GtkTreeModelMapping is mappinging.
 * 
 * Return value: the "child model" being mappinged
 **/
GtkTreeModel*
gtk_tree_model_mapping_get_model (GtkTreeModelMapping  *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), NULL);

  return tree_model->child_model;
}

static void
gtk_tree_model_mapping_finalize (GObject *object)
{
  GtkTreeModelMapping *tree_model_mapping = (GtkTreeModelMapping *) object;

  g_free (tree_model_mapping->column_maps);
  tree_model_mapping->column_maps = NULL;
  
  if (tree_model_mapping->child_model)
    {
      g_object_unref (G_OBJECT (tree_model_mapping->child_model));
      tree_model_mapping->child_model = NULL;
    }
}

static void
gtk_tree_model_mapping_changed (GtkTreeModel             *model,
				GtkTreePath              *path,
				GtkTreeIter              *iter,
				gpointer                  data)
{
  gtk_signal_emit_by_name (GTK_OBJECT (data), "changed", path, iter);
}

static void
gtk_tree_model_mapping_inserted (GtkTreeModel             *model,
				 GtkTreePath              *path,
				 GtkTreeIter              *iter,
				 gpointer                  data)
{
  gtk_signal_emit_by_name (GTK_OBJECT (data), "inserted", path, iter);
}

static void
gtk_tree_model_mapping_child_toggled (GtkTreeModel             *model,
				      GtkTreePath              *path,
				      GtkTreeIter              *iter,
				      gpointer                  data)
{
  gtk_signal_emit_by_name (GTK_OBJECT (data), "child_toggled", path, iter);
}

static void
gtk_tree_model_mapping_deleted (GtkTreeModel             *model,
				GtkTreePath              *path,
				gpointer                  data)
{
  gtk_signal_emit_by_name (GTK_OBJECT (data), "deleted", path);
}


static GtkTreeModelFlags
gtk_tree_model_mapping_get_flags (GtkTreeModel *tree_model)
{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  
  return gtk_tree_model_get_flags (child_model);
}

static gint
gtk_tree_model_mapping_get_n_columns (GtkTreeModel *tree_model)
{
  GtkTreeModelMapping *tree_model_mapping;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);

  tree_model_mapping = GTK_TREE_MODEL_MAPPING (tree_model);
  
  return tree_model_mapping->n_columns;
}

static GType
gtk_tree_model_mapping_get_column_type (GtkTreeModel  *tree_model,
					gint           index)
{
  GtkTreeModelMapping *tree_model_mapping;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);

  tree_model_mapping = GTK_TREE_MODEL_MAPPING (tree_model);
  
  return tree_model_mapping->column_maps[index].col_type;
}

static gboolean
gtk_tree_model_mapping_get_iter (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 GtkTreePath  *path)
{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;

  return gtk_tree_model_get_iter (child_model, iter, path);
}

static GtkTreePath *
gtk_tree_model_mapping_get_path (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter)
{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  
  return gtk_tree_model_get_path (child_model, iter);
}

static void
gtk_tree_model_mapping_get_value (GtkTreeModel  *tree_model,
				  GtkTreeIter   *iter,
				  gint           column,
				  GValue        *value)
{
  GValue src_val = { 0, };
  GtkTreeModel *child_model;
  GtkTreeModelMapping *tree_model_mapping;
  GtkTreeModelMappingMap *map;
  
  g_return_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL);

  tree_model_mapping = GTK_TREE_MODEL_MAPPING (tree_model);
  child_model = tree_model_mapping->child_model;

  map = &tree_model_mapping->column_maps[column];

  gtk_tree_model_get_value (child_model, iter, map->src_column, &src_val);
  (* map->map_func) (&src_val, value, map->user_data);
  g_value_unset (&src_val);
}

static gboolean
gtk_tree_model_mapping_iter_next (GtkTreeModel *tree_model,
				  GtkTreeIter  *iter)
{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;

  return gtk_tree_model_iter_next (child_model, iter);
}

static gboolean
gtk_tree_model_mapping_iter_children (GtkTreeModel *tree_model,
				      GtkTreeIter  *iter,
				      GtkTreeIter  *parent)
{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  return gtk_tree_model_iter_children (child_model, iter, parent);
}

static gboolean
gtk_tree_model_mapping_iter_has_child (GtkTreeModel *tree_model,
				       GtkTreeIter  *iter)

{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  return gtk_tree_model_iter_has_child (child_model, iter);
}

static gint
gtk_tree_model_mapping_iter_n_children (GtkTreeModel *tree_model,
					GtkTreeIter  *iter)
{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  return gtk_tree_model_iter_n_children (child_model, iter);
}

static gboolean
gtk_tree_model_mapping_iter_nth_child (GtkTreeModel *tree_model,
				       GtkTreeIter  *iter,
				       GtkTreeIter  *parent,
				       gint         n)
{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  return gtk_tree_model_iter_nth_child (child_model, iter, parent, n);
}

static gboolean
gtk_tree_model_mapping_iter_parent (GtkTreeModel             *tree_model,
				    GtkTreeIter              *iter,
				    GtkTreeIter              *child)
{
  GtkTreeModel *child_model;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL, 0);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  return gtk_tree_model_iter_parent (child_model, iter, child);
}

static void
gtk_tree_model_mapping_ref_iter (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter)
{
  GtkTreeModel *child_model;
  
  g_return_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  gtk_tree_model_ref_iter (child_model, iter);
}

static void
gtk_tree_model_mapping_unref_iter (GtkTreeModel *tree_model,
				   GtkTreeIter  *iter)
{
  GtkTreeModel *child_model;
  
  g_return_if_fail (GTK_IS_TREE_MODEL_MAPPING (tree_model));
  g_return_if_fail (GTK_TREE_MODEL_MAPPING (tree_model)->child_model != NULL);

  child_model = GTK_TREE_MODEL_MAPPING (tree_model)->child_model;
  gtk_tree_model_unref_iter (child_model, iter);
}
