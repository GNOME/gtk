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
#include "gtksignal.h"
enum {
  CHANGED,
  INSERTED,
  CHILD_TOGGLED,
  DELETED,

  GET_N_COLUMNS,
  GET_COLUMN_TYPE,
  GET_ITER,
  GET_PATH,
  GET_VALUE,
  ITER_NEXT,
  ITER_CHILDREN,
  ITER_HAS_CHILD,
  ITER_N_CHILDREN,
  ITER_NTH_CHILD,
  ITER_PARENT,
  LAST_SIGNAL
};

static void         gtk_model_simple_init                 (GtkModelSimple      *model_simple);
static void         gtk_model_simple_class_init           (GtkModelSimpleClass *class);
static void         gtk_model_simple_tree_model_init      (GtkTreeModelIface   *iface);
static gint         gtk_real_model_simple_get_n_columns   (GtkTreeModel        *tree_model);
static GType        gtk_real_model_simple_get_column_type (GtkTreeModel        *tree_model,
							   gint                 index);
static gboolean     gtk_real_model_simple_get_iter        (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   GtkTreePath         *path);
static GtkTreePath *gtk_real_model_simple_get_path        (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter);
static void         gtk_real_model_simple_get_value       (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   gint                 column,
							   GValue              *value);
static gboolean     gtk_real_model_simple_iter_next       (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter);
static gboolean     gtk_real_model_simple_iter_children   (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   GtkTreeIter         *parent);
static gboolean     gtk_real_model_simple_iter_has_child  (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter);
static gint         gtk_real_model_simple_iter_n_children (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter);
static gboolean     gtk_real_model_simple_iter_nth_child  (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   GtkTreeIter         *parent,
							   gint                 n);
static gboolean     gtk_real_model_simple_iter_parent     (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   GtkTreeIter         *child);




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

GtkModelSimple *
gtk_model_simple_new (void)
{
  GtkModelSimple *retval;

  retval = GTK_MODEL_SIMPLE (gtk_type_new (GTK_TYPE_MODEL_SIMPLE));

  return retval;
}

static void
gtk_model_simple_class_init (GtkModelSimpleClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  model_simple_signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkModelSimpleClass, changed),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[INSERTED] =
    gtk_signal_new ("inserted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkModelSimpleClass, inserted),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[CHILD_TOGGLED] =
    gtk_signal_new ("child_toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkModelSimpleClass, child_toggled),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[DELETED] =
    gtk_signal_new ("deleted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkModelSimpleClass, deleted),
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
  model_simple_signals[GET_COLUMN_TYPE] =
    gtk_signal_new ("get_column_type",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_INT__INT,
                    GTK_TYPE_INT, 1,
		    GTK_TYPE_INT);
  model_simple_signals[GET_ITER] =
    gtk_signal_new ("get_iter",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_BOOLEAN__POINTER_POINTER,
                    GTK_TYPE_BOOL, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[GET_PATH] =
    gtk_signal_new ("get_path",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_POINTER__POINTER,
                    GTK_TYPE_POINTER, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[GET_VALUE] =
    gtk_signal_new ("get_value",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_VOID__POINTER_INT_POINTER,
                    GTK_TYPE_NONE, 3,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_INT,
		    GTK_TYPE_POINTER);
  model_simple_signals[ITER_NEXT] =
    gtk_signal_new ("iter_next",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_BOOLEAN__POINTER,
                    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[ITER_CHILDREN] =
    gtk_signal_new ("iter_children",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_BOOLEAN__POINTER_POINTER,
                    GTK_TYPE_POINTER, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[ITER_HAS_CHILD] =
    gtk_signal_new ("iter_has_child",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_BOOLEAN__POINTER,
                    GTK_TYPE_BOOL, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[ITER_N_CHILDREN] =
    gtk_signal_new ("iter_n_children",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_INT__POINTER,
                    GTK_TYPE_INT, 1,
		    GTK_TYPE_POINTER);
  model_simple_signals[ITER_NTH_CHILD] =
    gtk_signal_new ("node_nth_child",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_BOOLEAN__POINTER_POINTER_INT,
                    GTK_TYPE_BOOL, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  model_simple_signals[ITER_PARENT] =
    gtk_signal_new ("node_parent",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    0,
                    gtk_marshal_BOOLEAN__POINTER_POINTER,
                    GTK_TYPE_BOOL, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, model_simple_signals, LAST_SIGNAL);
}

static void
gtk_model_simple_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = gtk_real_model_simple_get_n_columns;
  iface->get_column_type = gtk_real_model_simple_get_column_type;
  iface->get_iter = gtk_real_model_simple_get_iter;
  iface->get_path = gtk_real_model_simple_get_path;
  iface->get_value = gtk_real_model_simple_get_value;
  iface->iter_next = gtk_real_model_simple_iter_next;
  iface->iter_children = gtk_real_model_simple_iter_children;
  iface->iter_has_child = gtk_real_model_simple_iter_has_child;
  iface->iter_n_children = gtk_real_model_simple_iter_n_children;
  iface->iter_nth_child = gtk_real_model_simple_iter_nth_child;
  iface->iter_parent = gtk_real_model_simple_iter_parent;
}


static void
gtk_model_simple_init (GtkModelSimple *model_simple)
{
  model_simple->stamp = 1;
}

static gint
gtk_real_model_simple_get_n_columns (GtkTreeModel *tree_model)
{
  gint retval = 0;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[GET_N_COLUMNS], &retval);

  return retval;
}

static GType
gtk_real_model_simple_get_column_type (GtkTreeModel *tree_model,
				       gint          index)
{
  GType retval = G_TYPE_INVALID;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[GET_COLUMN_TYPE], index, &retval);

  return retval;
}

static gboolean
gtk_real_model_simple_get_iter (GtkTreeModel *tree_model,
				GtkTreeIter  *iter,
				GtkTreePath  *path)
{
  gboolean retval;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[GET_ITER], iter, path, &retval);

  return retval;
}

static GtkTreePath *
gtk_real_model_simple_get_path (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GtkTreePath *retval;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[GET_PATH], iter, &retval);

  return retval;
}

static void
gtk_real_model_simple_get_value (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 gint          column,
				 GValue       *value)
{
  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[GET_VALUE], iter, column, value);
}

static gboolean
gtk_real_model_simple_iter_next (GtkTreeModel  *tree_model,
				 GtkTreeIter   *iter)
{
  gboolean retval = FALSE;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[ITER_NEXT], iter, &retval);

  return retval;
}

static gboolean
gtk_real_model_simple_iter_children (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter,
				     GtkTreeIter  *parent)
{
  gboolean retval = FALSE;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[ITER_CHILDREN], iter, parent, &retval);

  return retval;
}

static gboolean
gtk_real_model_simple_iter_has_child (GtkTreeModel *tree_model,
				      GtkTreeIter  *iter)
{
  gboolean retval = FALSE;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[ITER_HAS_CHILD], iter, &retval);

  return retval;
}

static gint
gtk_real_model_simple_iter_n_children (GtkTreeModel *tree_model,
				       GtkTreeIter  *iter)
{
  gint retval = 0;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[ITER_N_CHILDREN], iter, &retval);

  return retval;
}

static gboolean
gtk_real_model_simple_iter_nth_child (GtkTreeModel *tree_model,
				      GtkTreeIter  *iter,
				      GtkTreeIter  *parent,
				      gint          n)
{
  gboolean retval = FALSE;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[ITER_NTH_CHILD], iter, parent, n, &retval);

  return retval;
}

static gboolean
gtk_real_model_simple_iter_parent (GtkTreeModel *tree_model,
				   GtkTreeIter  *iter,
				   GtkTreeIter  *child)
{
  gboolean retval = FALSE;

  gtk_signal_emit (GTK_OBJECT (tree_model), model_simple_signals[ITER_PARENT], iter, child, &retval);

  return retval;
}

/* Public functions */
void
gtk_model_simple_changed (GtkModelSimple *simple,
			  GtkTreePath    *path,
			  GtkTreeIter    *iter)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "changed", path, iter);
}

void
gtk_model_simple_inserted (GtkModelSimple *simple,
			   GtkTreePath    *path,
			   GtkTreeIter    *iter)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "inserted", path, iter);
}

void
gtk_model_simple_child_toggled (GtkModelSimple *simple,
				GtkTreePath    *path,
				GtkTreeIter    *iter)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "child_toggled", path, iter);
}

void
gtk_model_simple_iter_deleted (GtkModelSimple *simple,
			       GtkTreePath    *path,
			       GtkTreeIter    *iter)
{
  g_return_if_fail (simple != NULL);
  g_return_if_fail (GTK_IS_MODEL_SIMPLE (simple));
  g_return_if_fail (path != NULL);

  gtk_signal_emit_by_name (GTK_OBJECT (simple), "iter_deleted", path, iter);
}
