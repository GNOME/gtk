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
#if 0
#include "gtkmodelsimple.h"
#include "gtksignal.h"
enum {
  NODE_CHANGED,
  NODE_INSERTED,
  NODE_CHILD_TOGGLED,
  NODE_DELETED,

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
static void         gtk_model_simple_tree_model_init      (GtkTreeModelIface   *iface);
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

      static const GInterfaceInfo tree_model_info =
      {
	(GInterfaceInitFunc) gtk_model_simple_tree_model_init,
	NULL,
	NULL
      };

      model_simple_type = g_type_register_static (GTK_TYPE_TREE_MODEL, "GtkModelSimple", &model_simple_info, 0);
      g_type_add_interface_static (model_simple_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
    }

  return model_simple_type;
}

GtkObject *
gtk_model_simple_new (void)
{
  return GTK_OBJECT (gtk_type_new (GTK_TYPE_MODEL_SIMPLE));
}

static void
gtk_model_simple_class_init (GtkModelSimpleClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  model_simple_signals[NODE_CHANGED] =
    gtk_signal_new ("node_changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkModelSimpleClass, node_changed),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_INSERTED] =
    gtk_signal_new ("node_inserted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkModelSimpleClass, node_inserted),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_CHILD_TOGGLED] =
    gtk_signal_new ("node_child_toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkModelSimpleClass, node_child_toggled),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_DELETED] =
    gtk_signal_new ("node_deleted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkModelSimpleClass, node_deleted),
                    gtk_marshal_VOID__POINTER,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);

  model_simple_signals[GET_N_COLUMNS] =
    gtk_signal_new ("get_n_columns",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_INT__VOID,
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
                    gtk_marshal_VOID__POINTER_INT_POINTER,
                    GTK_TYPE_NONE, 3,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_INT,
		    GTK_TYPE_POINTER);
  model_simple_signals[NODE_NEXT] =
    gtk_signal_new ("node_next",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_BOOLEAN__POINTER,
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

  gtk_object_class_add_signals (object_class, model_simple_signals, LAST_SIGNAL);
}

static void
gtk_model_simple_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = gtk_real_model_simple_get_n_columns;
  iface->get_node = gtk_real_model_simple_get_node;
  iface->get_path = gtk_real_model_simple_get_path;
  iface->node_get_value = gtk_real_model_simple_node_get_value;
  iface->node_next = gtk_real_model_simple_node_next;
  iface->node_children = gtk_real_model_simple_node_children;
  iface->node_has_child = gtk_real_model_simple_node_has_child;
  iface->node_n_children = gtk_real_model_simple_node_n_children;
  iface->node_nth_child = gtk_real_model_simple_node_nth_child;
  iface->node_parent = gtk_real_model_simple_node_parent;
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
				      gint          column,
				      GValue       *value)
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
				      gint          n)
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
			       GtkTreePath    *path,
			       GtkTreeNode     node)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "node_changed", path, node);
}

void
gtk_model_simple_node_inserted (GtkModelSimple *simple,
				GtkTreePath    *path,
				GtkTreeNode     node)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "node_inserted", path, node);
}

void
gtk_model_simple_node_child_toggled (GtkModelSimple *simple,
				     GtkTreePath    *path,
				     GtkTreeNode     node)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "node_child_toggled", path, node);
}

void
gtk_model_simple_node_deleted (GtkModelSimple *simple,
			       GtkTreePath    *path,
			       GtkTreeNode     node)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "node_deleted", path, node);
}
#endif
