/* gtkmodelsimple.c
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

#include "gtkmodelsimple.h"
#include "gtkmarshal.h"
#include "gtksignal.h"
enum {
  GET_N_COLUMNS,
  GET_NODE,
  GET_PATH,
  NODE_GET_VALUE,
  NODE_COPY,
  NODE_NEXT,
  NODE_CHILDREN,
  NODE_HAS_CHILD,
  NODE_N_CHILDREN,
  NODE_NTH_CHILD,
  NODE_PARENT,
  LAST_SIGNAL
};

static void         gtk_model_simple_init                 (GtkModelSimple      *model_simple);
static void         gtk_model_simple_class_init           (GtkModelSimpleClass *class);
static gint         gtk_real_model_simple_get_n_columns   (GtkTreeModel        *tree_model);
static GtkTreeNode  gtk_real_model_simple_get_node        (GtkTreeModel        *tree_model,
							   GtkTreePath         *path);
static GtkTreePath *gtk_real_model_simple_get_path        (GtkTreeModel        *tree_model,
							   GtkTreeNode          node);
static void         gtk_real_model_simple_node_get_value  (GtkTreeModel        *tree_model,
							   GtkTreeNode          node,
							   gint                 column,
							   GValue              *value);
static gboolean     gtk_real_model_simple_node_next       (GtkTreeModel        *tree_model,
							   GtkTreeNode         *node);
static GtkTreeNode  gtk_real_model_simple_node_children   (GtkTreeModel        *tree_model,
							   GtkTreeNode          node);
static gboolean     gtk_real_model_simple_node_has_child  (GtkTreeModel        *tree_model,
							   GtkTreeNode          node);
static gint         gtk_real_model_simple_node_n_children (GtkTreeModel        *tree_model,
							   GtkTreeNode          node);
static GtkTreeNode  gtk_real_model_simple_node_nth_child  (GtkTreeModel        *tree_model,
							   GtkTreeNode          node,
							   gint                 n);
static GtkTreeNode  gtk_real_model_simple_node_parent     (GtkTreeModel        *tree_model,
							   GtkTreeNode          node);



static GtkTreeModelClass *parent_class = NULL;
static guint model_simple_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_model_simple_get_type (void)
{
  static GtkType model_simple_type = 0;

  if (!model_simple_type)
    {
      static const GTypeInfo model_simple_info =
      {
        sizeof (GtkModelSimpleClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_model_simple_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkModelSimple),
	0,
        (GInstanceInitFunc) gtk_model_simple_init
      };

      model_simple_type = g_type_register_static (GTK_TYPE_TREE_MODEL, "GtkModelSimple", &model_simple_info);
    }

  return model_simple_type;
}

GtkObject *
gtk_model_simple_new (void)
{
  return GTK_OBJECT (gtk_type_new (GTK_TYPE_MODEL_SIMPLE));
}


typedef gint (*GtkSignal_INT__NONE) (GtkObject * object,
				     gpointer user_data);
void
gtk_marshal_INT__NONE (GtkObject * object,
			GtkSignalFunc func, gpointer func_data, GtkArg * args)
{
  GtkSignal_INT__NONE rfunc;
  gint *return_val;
  return_val = GTK_RETLOC_INT (args[0]);
  rfunc = (GtkSignal_INT__NONE) func;
  *return_val = (*rfunc) (object, func_data);
}

typedef gpointer (*GtkSignal_POINTER__NONE) (GtkObject * object,
					 gpointer user_data);
void
gtk_marshal_POINTER__NONE (GtkObject * object,
			   GtkSignalFunc func, gpointer func_data, GtkArg * args)
{
  GtkSignal_POINTER__NONE rfunc;
  gpointer *return_val;
  return_val = GTK_RETLOC_POINTER (args[0]);
  rfunc = (GtkSignal_POINTER__NONE) func;
  *return_val = (*rfunc) (object, func_data);
}

typedef gpointer (*GtkSignal_POINTER__POINTER) (GtkObject * object,
						gpointer arg1,
						gpointer user_data);
void
gtk_marshal_POINTER__POINTER (GtkObject * object,
			      GtkSignalFunc func,
			      gpointer func_data, GtkArg * args)
{
  GtkSignal_POINTER__POINTER rfunc;
  gpointer *return_val;
  return_val = GTK_RETLOC_POINTER (args[1]);
  rfunc = (GtkSignal_POINTER__POINTER) func;
  *return_val = (*rfunc) (object, GTK_VALUE_POINTER (args[0]), func_data);
}

typedef gpointer (*GtkSignal_POINTER__POINTER_INT) (GtkObject * object,
						    gpointer arg1,
						    gint arg2,
						    gpointer user_data);
void
gtk_marshal_POINTER__POINTER_INT (GtkObject * object,
				  GtkSignalFunc func,
				  gpointer func_data, GtkArg * args)
{
  GtkSignal_POINTER__POINTER_INT rfunc;
  gpointer *return_val;
  return_val = GTK_RETLOC_POINTER (args[2]);
  rfunc = (GtkSignal_POINTER__POINTER_INT) func;
  *return_val = (*rfunc) (object, GTK_VALUE_POINTER (args[0]), GTK_VALUE_INT (args[1]), func_data);
}

static void
gtk_model_simple_class_init (GtkModelSimpleClass *class)
{
  GtkObjectClass *object_class;

  GtkTreeModelClass *tree_model_class;

  object_class = (GtkObjectClass*) class;
  tree_model_class = (GtkTreeModelClass*) class;
  parent_class = g_type_class_peek_parent (class);


  model_simple_signals[GET_N_COLUMNS] =
    gtk_signal_new ("get_n_columns",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_INT__NONE,
                    GTK_TYPE_INT, 0);
  model_simple_signals[GET_NODE] =
    gtk_signal_new ("get_node",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_POINTER__POINTER,
                    GTK_TYPE_POINTER, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[GET_PATH] =
    gtk_signal_new ("get_path",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_POINTER__POINTER,
                    GTK_TYPE_POINTER, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_GET_VALUE] =
    gtk_signal_new ("node_get_value",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_NONE__POINTER_INT_POINTER,
                    GTK_TYPE_NONE, 3,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_INT,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_NEXT] =
    gtk_signal_new ("node_next",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_BOOL__POINTER,
                    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_CHILDREN] =
    gtk_signal_new ("node_children",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_POINTER__POINTER,
                    GTK_TYPE_POINTER, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_HAS_CHILD] =
    gtk_signal_new ("node_has_child",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_POINTER__POINTER,
                    GTK_TYPE_POINTER, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_N_CHILDREN] =
    gtk_signal_new ("node_n_children",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_POINTER__POINTER,
                    GTK_TYPE_POINTER, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_NTH_CHILD] =
    gtk_signal_new ("node_nth_child",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_POINTER__POINTER_INT,
                    GTK_TYPE_POINTER, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_PARENT] =
    gtk_signal_new ("node_parent",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_POINTER__POINTER,
                    GTK_TYPE_POINTER, 1,
		    GTK_TYPE_POINTER);


  tree_model_class->get_n_columns = gtk_real_model_simple_get_n_columns;
  tree_model_class->get_node = gtk_real_model_simple_get_node;
  tree_model_class->get_path = gtk_real_model_simple_get_path;
  tree_model_class->node_get_value = gtk_real_model_simple_node_get_value;
  tree_model_class->node_next = gtk_real_model_simple_node_next;
  tree_model_class->node_children = gtk_real_model_simple_node_children;
  tree_model_class->node_has_child = gtk_real_model_simple_node_has_child;
  tree_model_class->node_n_children = gtk_real_model_simple_node_n_children;
  tree_model_class->node_nth_child = gtk_real_model_simple_node_nth_child;
  tree_model_class->node_parent = gtk_real_model_simple_node_parent;

  gtk_object_class_add_signals (object_class, model_simple_signals, LAST_SIGNAL);
}


static void
gtk_model_simple_init (GtkModelSimple *model_simple)
{
}

static gint
gtk_real_model_simple_get_n_columns (GtkTreeModel *tree_model)
{
  gint retval = 0;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[GET_N_COLUMNS], &retval);

  return retval;
}

static GtkTreeNode
gtk_real_model_simple_get_node (GtkTreeModel *tree_model,
				GtkTreePath  *path)
{
  GtkTreeNode retval;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[GET_NODE], path, &retval);

  return retval;
}

static GtkTreePath *
gtk_real_model_simple_get_path (GtkTreeModel *tree_model,
				GtkTreeNode   node)
{
  GtkTreePath *retval;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[GET_PATH], node, &retval);

  return retval;
}

static void
gtk_real_model_simple_node_get_value (GtkTreeModel *tree_model,
				      GtkTreeNode   node,
				      gint        column,
				      GValue     *value)
{
  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[NODE_GET_VALUE], node, column, value);
}

static gboolean
gtk_real_model_simple_node_next (GtkTreeModel  *tree_model,
				 GtkTreeNode   *node)
{
  gboolean retval = FALSE;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[NODE_NEXT], node, &retval);

  return retval;
}

static GtkTreeNode
gtk_real_model_simple_node_children (GtkTreeModel *tree_model,
				     GtkTreeNode   node)
{
  GtkTreeNode retval = NULL;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[NODE_CHILDREN], node, &retval);

  return retval;
}

static gboolean
gtk_real_model_simple_node_has_child (GtkTreeModel *tree_model,
				      GtkTreeNode   node)
{
  gboolean retval = FALSE;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[NODE_HAS_CHILD], node, &retval);

  return retval;
}

static gint
gtk_real_model_simple_node_n_children (GtkTreeModel *tree_model,
				       GtkTreeNode   node)
{
  gint retval = 0;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[NODE_N_CHILDREN], node, &retval);

  return retval;
}

static GtkTreeNode
gtk_real_model_simple_node_nth_child (GtkTreeModel *tree_model,
				      GtkTreeNode   node,
				      gint        n)
{
  GtkTreeNode retval = NULL;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[NODE_NTH_CHILD], node, n, &retval);

  return retval;
}

static GtkTreeNode
gtk_real_model_simple_node_parent (GtkTreeModel *tree_model,
				   GtkTreeNode   node)
{
  GtkTreeNode retval = NULL;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[NODE_PARENT], node, &retval);

  return retval;
}

/* Public functions */
void
gtk_model_simple_node_changed (GtkModelSimple *simple,
			       GtkTreePath      *path,
			       GtkTreeNode      *tree_node)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "node_changed", path, tree_node);
}

void
gtk_model_simple_node_inserted (GtkModelSimple *simple,
				GtkTreePath    *path,
				GtkTreeNode    *tree_node)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "node_inserted", path, tree_node);
}

void
gtk_model_simple_node_child_toggled (GtkModelSimple *simple,
				     GtkTreePath    *path,
				     GtkTreeNode    *tree_node)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "node_child_toggled", path, tree_node);
}

void
gtk_model_simple_node_deleted (GtkModelSimple *simple,
			       GtkTreePath    *path,
			       GtkTreeNode    *tree_node)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "node_deleted", path, tree_node);
}
