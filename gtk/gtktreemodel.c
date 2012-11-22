/* gtktreemodel.c
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gobject/gvaluecollector.h>
#include "gtktreemodel.h"
#include "gtktreeview.h"
#include "gtktreeprivate.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"

/**
 * SECTION:gtktreemodel
 * @Title: GtkTreeModel
 * @Short_description: The tree interface used by GtkTreeView
 * @See_also: #GtkTreeView, #GtkTreeStore, #GtkListStore,
 *     <link linkend="gtk-GtkTreeView-drag-and-drop">GtkTreeDnd</link>,
 *     #GtkTreeSortable
 *
 * The #GtkTreeModel interface defines a generic tree interface for
 * use by the #GtkTreeView widget. It is an abstract interface, and
 * is designed to be usable with any appropriate data structure. The
 * programmer just has to implement this interface on their own data
 * type for it to be viewable by a #GtkTreeView widget.
 *
 * The model is represented as a hierarchical tree of strongly-typed,
 * columned data. In other words, the model can be seen as a tree where
 * every node has different values depending on which column is being
 * queried. The type of data found in a column is determined by using
 * the GType system (ie. #G_TYPE_INT, #GTK_TYPE_BUTTON, #G_TYPE_POINTER,
 * etc). The types are homogeneous per column across all nodes. It is
 * important to note that this interface only provides a way of examining
 * a model and observing changes. The implementation of each individual
 * model decides how and if changes are made.
 *
 * In order to make life simpler for programmers who do not need to
 * write their own specialized model, two generic models are provided
 * &mdash; the #GtkTreeStore and the #GtkListStore. To use these, the
 * developer simply pushes data into these models as necessary. These
 * models provide the data structure as well as all appropriate tree
 * interfaces. As a result, implementing drag and drop, sorting, and
 * storing data is trivial. For the vast majority of trees and lists,
 * these two models are sufficient.
 *
 * Models are accessed on a node/column level of granularity. One can
 * query for the value of a model at a certain node and a certain
 * column on that node. There are two structures used to reference
 * a particular node in a model. They are the #GtkTreePath and the
 * #GtkTreeIter<footnote><para>Here, <abbrev>iter</abbrev> is short
 * for <quote>iterator</quote></para></footnote>. Most of the interface
 * consists of operations on a #GtkTreeIter.
 *
 * A path is essentially a potential node. It is a location on a model
 * that may or may not actually correspond to a node on a specific
 * model. The #GtkTreePath struct can be converted into either an
 * array of unsigned integers or a string. The string form is a list
 * of numbers separated by a colon. Each number refers to the offset
 * at that level. Thus, the path <quote>0</quote> refers to the root
 * node and the path <quote>2:4</quote> refers to the fifth child of
 * the third node.
 *
 * By contrast, a #GtkTreeIter is a reference to a specific node on
 * a specific model. It is a generic struct with an integer and three
 * generic pointers. These are filled in by the model in a model-specific
 * way. One can convert a path to an iterator by calling
 * gtk_tree_model_get_iter(). These iterators are the primary way
 * of accessing a model and are similar to the iterators used by
 * #GtkTextBuffer. They are generally statically allocated on the
 * stack and only used for a short time. The model interface defines
 * a set of operations using them for navigating the model.
 *
 * It is expected that models fill in the iterator with private data.
 * For example, the #GtkListStore model, which is internally a simple
 * linked list, stores a list node in one of the pointers. The
 * #GtkTreeModelSort stores an array and an offset in two of the
 * pointers. Additionally, there is an integer field. This field is
 * generally filled with a unique stamp per model. This stamp is for
 * catching errors resulting from using invalid iterators with a model.
 *
 * The lifecycle of an iterator can be a little confusing at first.
 * Iterators are expected to always be valid for as long as the model
 * is unchanged (and doesn't emit a signal). The model is considered
 * to own all outstanding iterators and nothing needs to be done to
 * free them from the user's point of view. Additionally, some models
 * guarantee that an iterator is valid for as long as the node it refers
 * to is valid (most notably the #GtkTreeStore and #GtkListStore).
 * Although generally uninteresting, as one always has to allow for
 * the case where iterators do not persist beyond a signal, some very
 * important performance enhancements were made in the sort model.
 * As a result, the #GTK_TREE_MODEL_ITERS_PERSIST flag was added to
 * indicate this behavior.
 *
 * To help show some common operation of a model, some examples are
 * provided. The first example shows three ways of getting the iter at
 * the location <quote>3:2:5</quote>. While the first method shown is
 * easier, the second is much more common, as you often get paths from
 * callbacks.
 *
 * <example>
 * <title>Acquiring a <structname>GtkTreeIter</structname></title>
 * <programlisting>
 *  /&ast; Three ways of getting the iter pointing to the location &ast;/
 * GtkTreePath *path;
 * GtkTreeIter iter;
 * GtkTreeIter parent_iter;
 *
 * /&ast; get the iterator from a string &ast;/
 * gtk_tree_model_get_iter_from_string (model, &amp;iter, "3:2:5");
 *
 * /&ast; get the iterator from a path &ast;/
 * path = gtk_tree_path_new_from_string ("3:2:5");
 * gtk_tree_model_get_iter (model, &amp;iter, path);
 * gtk_tree_path_free (path);
 *
 * /&ast; walk the tree to find the iterator &ast;/
 * gtk_tree_model_iter_nth_child (model, &amp;iter, NULL, 3);
 * parent_iter = iter;
 * gtk_tree_model_iter_nth_child (model, &amp;iter, &amp;parent_iter, 2);
 * parent_iter = iter;
 * gtk_tree_model_iter_nth_child (model, &amp;iter, &amp;parent_iter, 5);
 * </programlisting>
 * </example>
 *
 * This second example shows a quick way of iterating through a list
 * and getting a string and an integer from each row. The
 * <function>populate_model</function> function used below is not
 * shown, as it is specific to the #GtkListStore. For information on
 * how to write such a function, see the #GtkListStore documentation.
 *
 * <example>
 * <title>Reading data from a <structname>GtkTreeModel</structname></title>
 * <programlisting>
 * enum
 * {
 *   STRING_COLUMN,
 *   INT_COLUMN,
 *   N_COLUMNS
 * };
 *
 * ...
 *
 * GtkTreeModel *list_store;
 * GtkTreeIter iter;
 * gboolean valid;
 * gint row_count = 0;
 *
 * /&ast; make a new list_store &ast;/
 * list_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
 *
 * /&ast; Fill the list store with data &ast;/
 * populate_model (list_store);
 *
 * /&ast; Get the first iter in the list, check it is valid and walk
 *  &ast; through the list, reading each row. &ast;/
 * for (valid = gtk_tree_model_get_iter_first (list_store, &amp;iter);
 *      valid;
 *      valid = gtk_tree_model_iter_next (list_store, &amp;iter))
 *  {
 *    gchar *str_data;
 *    gint   int_data;
 *
 *    /&ast; Make sure you terminate calls to gtk_tree_model_get()
 *     &ast; with a '-1' value
 *     &ast;/
 *    gtk_tree_model_get (list_store, &amp;iter,
 *                        STRING_COLUMN, &amp;str_data,
 *                        INT_COLUMN, &amp;int_data,
 *                        -1);
 *
 *    /&ast; Do something with the data &ast;/
 *    g_print ("Row &percnt;d: (&percnt;s,&percnt;d)\n", row_count, str_data, int_data);
 *    g_free (str_data);
 *
 *    row_count++;
 *  }
 * </programlisting>
 * </example>
 *
 * The #GtkTreeModel interface contains two methods for reference
 * counting: gtk_tree_model_ref_node() and gtk_tree_model_unref_node().
 * These two methods are optional to implement. The reference counting
 * is meant as a way for views to let models know when nodes are being
 * displayed. #GtkTreeView will take a reference on a node when it is
 * visible, which means the node is either in the toplevel or expanded.
 * Being displayed does not mean that the node is currently directly
 * visible to the user in the viewport. Based on this reference counting
 * scheme a caching model, for example, can decide whether or not to cache
 * a node based on the reference count. A file-system based model would
 * not want to keep the entire file hierarchy in memory, but just the
 * folders that are currently expanded in every current view.
 *
 * When working with reference counting, the following rules must be taken
 * into account:
 * <itemizedlist>
 * <listitem><para>Never take a reference on a node without owning a
 * reference on its parent. This means that all parent nodes of a referenced
 * node must be referenced as well.</para></listitem>
 * <listitem><para>Outstanding references on a deleted node are not released.
 * This is not possible because the node has already been deleted by the
 * time the row-deleted signal is received.
 * </para></listitem>
 * <listitem><para>Models are not obligated to emit a signal on rows of
 * which none of its siblings are referenced. To phrase this differently,
 * signals are only required for levels in which nodes are referenced. For
 * the root level however, signals must be emitted at all times (however the
 * root level is always referenced when any view is attached).
 * </para></listitem>
 * </itemizedlist>
 */

#define INITIALIZE_TREE_ITER(Iter) \
    G_STMT_START{ \
      (Iter)->stamp = 0; \
      (Iter)->user_data  = NULL; \
      (Iter)->user_data2 = NULL; \
      (Iter)->user_data3 = NULL; \
    }G_STMT_END

#define ROW_REF_DATA_STRING "gtk-tree-row-refs"

enum {
  ROW_CHANGED,
  ROW_INSERTED,
  ROW_HAS_CHILD_TOGGLED,
  ROW_DELETED,
  ROWS_REORDERED,
  LAST_SIGNAL
};

static guint tree_model_signals[LAST_SIGNAL] = { 0 };

struct _GtkTreePath
{
  gint depth;    /* Number of elements */
  gint alloc;    /* Number of allocated elements */
  gint *indices;
};

typedef struct
{
  GSList *list;
} RowRefList;

static void      gtk_tree_model_base_init   (gpointer           g_class);

/* custom closures */
static void      row_inserted_marshal       (GClosure          *closure,
                                             GValue /* out */  *return_value,
                                             guint              n_param_value,
                                             const GValue      *param_values,
                                             gpointer           invocation_hint,
                                             gpointer           marshal_data);
static void      row_deleted_marshal        (GClosure          *closure,
                                             GValue /* out */  *return_value,
                                             guint              n_param_value,
                                             const GValue      *param_values,
                                             gpointer           invocation_hint,
                                             gpointer           marshal_data);
static void      rows_reordered_marshal     (GClosure          *closure,
                                             GValue /* out */  *return_value,
                                             guint              n_param_value,
                                             const GValue      *param_values,
                                             gpointer           invocation_hint,
                                             gpointer           marshal_data);

static void      gtk_tree_row_ref_inserted  (RowRefList        *refs,
                                             GtkTreePath       *path,
                                             GtkTreeIter       *iter);
static void      gtk_tree_row_ref_deleted   (RowRefList        *refs,
                                             GtkTreePath       *path);
static void      gtk_tree_row_ref_reordered (RowRefList        *refs,
                                             GtkTreePath       *path,
                                             GtkTreeIter       *iter,
                                             gint              *new_order);

GType
gtk_tree_model_get_type (void)
{
  static GType tree_model_type = 0;

  if (! tree_model_type)
    {
      const GTypeInfo tree_model_info =
      {
        sizeof (GtkTreeModelIface), /* class_size */
        gtk_tree_model_base_init,   /* base_init */
        NULL,           /* base_finalize */
        NULL,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        0,
        0,              /* n_preallocs */
        NULL
      };

      tree_model_type =
        g_type_register_static (G_TYPE_INTERFACE, I_("GtkTreeModel"),
                                &tree_model_info, 0);

      g_type_interface_add_prerequisite (tree_model_type, G_TYPE_OBJECT);
    }

  return tree_model_type;
}

static void
gtk_tree_model_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;
  GClosure *closure;

  if (! initialized)
    {
      GType row_inserted_params[2];
      GType row_deleted_params[1];
      GType rows_reordered_params[3];

      row_inserted_params[0] = GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE;
      row_inserted_params[1] = GTK_TYPE_TREE_ITER;

      row_deleted_params[0] = GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE;

      rows_reordered_params[0] = GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE;
      rows_reordered_params[1] = GTK_TYPE_TREE_ITER;
      rows_reordered_params[2] = G_TYPE_POINTER;

      /**
       * GtkTreeModel::row-changed:
       * @tree_model: the #GtkTreeModel on which the signal is emitted
       * @path: a #GtkTreePath identifying the changed row
       * @iter: a valid #GtkTreeIter pointing to the changed row
       *
       * This signal is emitted when a row in the model has changed.
       */
      tree_model_signals[ROW_CHANGED] =
        g_signal_new (I_("row-changed"),
                      GTK_TYPE_TREE_MODEL,
                      G_SIGNAL_RUN_LAST, 
                      G_STRUCT_OFFSET (GtkTreeModelIface, row_changed),
                      NULL, NULL,
                      _gtk_marshal_VOID__BOXED_BOXED,
                      G_TYPE_NONE, 2,
                      GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE,
                      GTK_TYPE_TREE_ITER);

      /* We need to get notification about structure changes
       * to update row references., so instead of using the
       * standard g_signal_new() with an offset into our interface
       * structure, we use a customs closures for the class
       * closures (default handlers) that first update row references
       * and then calls the function from the interface structure.
       *
       * The reason we don't simply update the row references from
       * the wrapper functions (gtk_tree_model_row_inserted(), etc.)
       * is to keep proper ordering with respect to signal handlers
       * connected normally and after.
       */

      /**
       * GtkTreeModel::row-inserted:
       * @tree_model: the #GtkTreeModel on which the signal is emitted
       * @path: a #GtkTreePath identifying the new row
       * @iter: a valid #GtkTreeIter pointing to the new row
       *
       * This signal is emitted when a new row has been inserted in
       * the model.
       *
       * Note that the row may still be empty at this point, since
       * it is a common pattern to first insert an empty row, and
       * then fill it with the desired values.
       */
      closure = g_closure_new_simple (sizeof (GClosure), NULL);
      g_closure_set_marshal (closure, row_inserted_marshal);
      tree_model_signals[ROW_INSERTED] =
        g_signal_newv (I_("row-inserted"),
                       GTK_TYPE_TREE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       closure,
                       NULL, NULL,
                       _gtk_marshal_VOID__BOXED_BOXED,
                       G_TYPE_NONE, 2,
                       row_inserted_params);

      /**
       * GtkTreeModel::row-has-child-toggled:
       * @tree_model: the #GtkTreeModel on which the signal is emitted
       * @path: a #GtkTreePath identifying the row
       * @iter: a valid #GtkTreeIter pointing to the row
       *
       * This signal is emitted when a row has gotten the first child
       * row or lost its last child row.
       */
      tree_model_signals[ROW_HAS_CHILD_TOGGLED] =
        g_signal_new (I_("row-has-child-toggled"),
                      GTK_TYPE_TREE_MODEL,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GtkTreeModelIface, row_has_child_toggled),
                      NULL, NULL,
                      _gtk_marshal_VOID__BOXED_BOXED,
                      G_TYPE_NONE, 2,
                      GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE,
                      GTK_TYPE_TREE_ITER);

      /**
       * GtkTreeModel::row-deleted:
       * @tree_model: the #GtkTreeModel on which the signal is emitted
       * @path: a #GtkTreePath identifying the row
       *
       * This signal is emitted when a row has been deleted.
       *
       * Note that no iterator is passed to the signal handler,
       * since the row is already deleted.
       *
       * This should be called by models after a row has been removed.
       * The location pointed to by @path should be the location that
       * the row previously was at. It may not be a valid location anymore.
       */
      closure = g_closure_new_simple (sizeof (GClosure), NULL);
      g_closure_set_marshal (closure, row_deleted_marshal);
      tree_model_signals[ROW_DELETED] =
        g_signal_newv (I_("row-deleted"),
                       GTK_TYPE_TREE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       closure,
                       NULL, NULL,
                       _gtk_marshal_VOID__BOXED,
                       G_TYPE_NONE, 1,
                       row_deleted_params);

      /**
       * GtkTreeModel::rows-reordered: (skip)
       * @tree_model: the #GtkTreeModel on which the signal is emitted
       * @path: a #GtkTreePath identifying the tree node whose children
       *     have been reordered
       * @iter: a valid #GtkTreeIter pointing to the node whose
       * @new_order: an array of integers mapping the current position
       *     of each child to its old position before the re-ordering,
       *     i.e. @new_order<literal>[newpos] = oldpos</literal>
       *
       * This signal is emitted when the children of a node in the
       * #GtkTreeModel have been reordered.
       *
       * Note that this signal is <emphasis>not</emphasis> emitted
       * when rows are reordered by DND, since this is implemented
       * by removing and then reinserting the row.
       */
      closure = g_closure_new_simple (sizeof (GClosure), NULL);
      g_closure_set_marshal (closure, rows_reordered_marshal);
      tree_model_signals[ROWS_REORDERED] =
        g_signal_newv (I_("rows-reordered"),
                       GTK_TYPE_TREE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       closure,
                       NULL, NULL,
                       _gtk_marshal_VOID__BOXED_BOXED_POINTER,
                       G_TYPE_NONE, 3,
                       rows_reordered_params);
      initialized = TRUE;
    }
}

static void
row_inserted_marshal (GClosure          *closure,
                      GValue /* out */  *return_value,
                      guint              n_param_values,
                      const GValue      *param_values,
                      gpointer           invocation_hint,
                      gpointer           marshal_data)
{
  GtkTreeModelIface *iface;

  void (* row_inserted_callback) (GtkTreeModel *tree_model,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter) = NULL;

  GObject *model = g_value_get_object (param_values + 0);
  GtkTreePath *path = (GtkTreePath *)g_value_get_boxed (param_values + 1);
  GtkTreeIter *iter = (GtkTreeIter *)g_value_get_boxed (param_values + 2);

  /* first, we need to update internal row references */
  gtk_tree_row_ref_inserted ((RowRefList *)g_object_get_data (model, ROW_REF_DATA_STRING),
                             path, iter);

  /* fetch the interface ->row_inserted implementation */
  iface = GTK_TREE_MODEL_GET_IFACE (model);
  row_inserted_callback = G_STRUCT_MEMBER (gpointer, iface,
                              G_STRUCT_OFFSET (GtkTreeModelIface,
                                               row_inserted));

  /* Call that default signal handler, it if has been set */
  if (row_inserted_callback)
    row_inserted_callback (GTK_TREE_MODEL (model), path, iter);
}

static void
row_deleted_marshal (GClosure          *closure,
                     GValue /* out */  *return_value,
                     guint              n_param_values,
                     const GValue      *param_values,
                     gpointer           invocation_hint,
                     gpointer           marshal_data)
{
  GtkTreeModelIface *iface;
  void (* row_deleted_callback) (GtkTreeModel *tree_model,
                                 GtkTreePath  *path) = NULL;
  GObject *model = g_value_get_object (param_values + 0);
  GtkTreePath *path = (GtkTreePath *)g_value_get_boxed (param_values + 1);

  /* first, we need to update internal row references */
  gtk_tree_row_ref_deleted ((RowRefList *)g_object_get_data (model, ROW_REF_DATA_STRING),
                            path);

  /* fetch the interface ->row_deleted implementation */
  iface = GTK_TREE_MODEL_GET_IFACE (model);
  row_deleted_callback = G_STRUCT_MEMBER (gpointer, iface,
                              G_STRUCT_OFFSET (GtkTreeModelIface,
                                               row_deleted));

  /* Call that default signal handler, it if has been set */
  if (row_deleted_callback)
    row_deleted_callback (GTK_TREE_MODEL (model), path);
}

static void
rows_reordered_marshal (GClosure          *closure,
                        GValue /* out */  *return_value,
                        guint              n_param_values,
                        const GValue      *param_values,
                        gpointer           invocation_hint,
                        gpointer           marshal_data)
{
  GtkTreeModelIface *iface;
  void (* rows_reordered_callback) (GtkTreeModel *tree_model,
                                    GtkTreePath  *path,
                                    GtkTreeIter  *iter,
                                    gint         *new_order);

  GObject *model = g_value_get_object (param_values + 0);
  GtkTreePath *path = (GtkTreePath *)g_value_get_boxed (param_values + 1);
  GtkTreeIter *iter = (GtkTreeIter *)g_value_get_boxed (param_values + 2);
  gint *new_order = (gint *)g_value_get_pointer (param_values + 3);

  /* first, we need to update internal row references */
  gtk_tree_row_ref_reordered ((RowRefList *)g_object_get_data (model, ROW_REF_DATA_STRING),
                              path, iter, new_order);

  /* fetch the interface ->rows_reordered implementation */
  iface = GTK_TREE_MODEL_GET_IFACE (model);
  rows_reordered_callback = G_STRUCT_MEMBER (gpointer, iface,
                              G_STRUCT_OFFSET (GtkTreeModelIface,
                                               rows_reordered));

  /* Call that default signal handler, it if has been set */
  if (rows_reordered_callback)
    rows_reordered_callback (GTK_TREE_MODEL (model), path, iter, new_order);
}

/**
 * gtk_tree_path_new:
 *
 * Creates a new #GtkTreePath.
 * This structure refers to a row.
 *
 * Return value: A newly created #GtkTreePath.
 */
GtkTreePath *
gtk_tree_path_new (void)
{
  GtkTreePath *retval;
  retval = g_slice_new (GtkTreePath);
  retval->depth = 0;
  retval->alloc = 0;
  retval->indices = NULL;

  return retval;
}

/**
 * gtk_tree_path_new_from_string:
 * @path: The string representation of a path
 *
 * Creates a new #GtkTreePath initialized to @path.
 *
 * @path is expected to be a colon separated list of numbers.
 * For example, the string "10:4:0" would create a path of depth
 * 3 pointing to the 11th child of the root node, the 5th
 * child of that 11th child, and the 1st child of that 5th child.
 * If an invalid path string is passed in, %NULL is returned.
 *
 * Return value: A newly-created #GtkTreePath, or %NULL
 */
GtkTreePath *
gtk_tree_path_new_from_string (const gchar *path)
{
  GtkTreePath *retval;
  const gchar *orig_path = path;
  gchar *ptr;
  gint i;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (*path != '\000', NULL);

  retval = gtk_tree_path_new ();

  while (1)
    {
      i = strtol (path, &ptr, 10);
      if (i < 0)
        {
          g_warning (G_STRLOC ": Negative numbers in path %s passed to gtk_tree_path_new_from_string", orig_path);
          gtk_tree_path_free (retval);
          return NULL;
        }

      gtk_tree_path_append_index (retval, i);

      if (*ptr == '\000')
        break;
      if (ptr == path || *ptr != ':')
        {
          g_warning (G_STRLOC ": Invalid path %s passed to gtk_tree_path_new_from_string", orig_path);
          gtk_tree_path_free (retval);
          return NULL;
        }
      path = ptr + 1;
    }

  return retval;
}

/**
 * gtk_tree_path_new_from_indices:
 * @first_index: first integer
 * @...: list of integers terminated by -1
 *
 * Creates a new path with @first_index and @varargs as indices.
 *
 * Return value: A newly created #GtkTreePath
 *
 * Since: 2.2
 */
GtkTreePath *
gtk_tree_path_new_from_indices (gint first_index,
                                ...)
{
  int arg;
  va_list args;
  GtkTreePath *path;

  path = gtk_tree_path_new ();

  va_start (args, first_index);
  arg = first_index;

  while (arg != -1)
    {
      gtk_tree_path_append_index (path, arg);
      arg = va_arg (args, gint);
    }

  va_end (args);

  return path;
}

/**
 * gtk_tree_path_to_string:
 * @path: A #GtkTreePath
 *
 * Generates a string representation of the path.
 *
 * This string is a ':' separated list of numbers.
 * For example, "4:10:0:3" would be an acceptable
 * return value for this string.
 *
 * Return value: A newly-allocated string.
 *     Must be freed with g_free().
 */
gchar *
gtk_tree_path_to_string (GtkTreePath *path)
{
  gchar *retval, *ptr, *end;
  gint i, n;

  g_return_val_if_fail (path != NULL, NULL);

  if (path->depth == 0)
    return NULL;

  n = path->depth * 12;
  ptr = retval = g_new0 (gchar, n);
  end = ptr + n;
  g_snprintf (retval, end - ptr, "%d", path->indices[0]);
  while (*ptr != '\000')
    ptr++;

  for (i = 1; i < path->depth; i++)
    {
      g_snprintf (ptr, end - ptr, ":%d", path->indices[i]);
      while (*ptr != '\000')
        ptr++;
    }

  return retval;
}

/**
 * gtk_tree_path_new_first:
 *
 * Creates a new #GtkTreePath.
 *
 * The string representation of this path is "0".
 *
 * Return value: A new #GtkTreePath
 */
GtkTreePath *
gtk_tree_path_new_first (void)
{
  GtkTreePath *retval;

  retval = gtk_tree_path_new ();
  gtk_tree_path_append_index (retval, 0);

  return retval;
}

/**
 * gtk_tree_path_append_index:
 * @path: a #GtkTreePath
 * @index_: the index
 *
 * Appends a new index to a path.
 *
 * As a result, the depth of the path is increased.
 */
void
gtk_tree_path_append_index (GtkTreePath *path,
                            gint         index_)
{
  g_return_if_fail (path != NULL);
  g_return_if_fail (index_ >= 0);

  if (path->depth == path->alloc)
    {
      gint *indices;
      path->alloc = MAX (path->alloc * 2, 1);
      indices = g_new (gint, path->alloc);
      memcpy (indices, path->indices, path->depth * sizeof (gint));
      g_free (path->indices);
      path->indices = indices;
    }

  path->depth += 1;
  path->indices[path->depth - 1] = index_;
}

/**
 * gtk_tree_path_prepend_index:
 * @path: a #GtkTreePath
 * @index_: the index
 *
 * Prepends a new index to a path.
 *
 * As a result, the depth of the path is increased.
 */
void
gtk_tree_path_prepend_index (GtkTreePath *path,
                             gint       index)
{
  if (path->depth == path->alloc)
    {
      gint *indices;
      path->alloc = MAX (path->alloc * 2, 1);
      indices = g_new (gint, path->alloc);
      memcpy (indices + 1, path->indices, path->depth * sizeof (gint));
      g_free (path->indices);
      path->indices = indices;
    }
  else if (path->depth > 0)
    memmove (path->indices + 1, path->indices, path->depth * sizeof (gint));

  path->depth += 1;
  path->indices[0] = index;
}

/**
 * gtk_tree_path_get_depth:
 * @path: a #GtkTreePath
 *
 * Returns the current depth of @path.
 *
 * Return value: The depth of @path
 */
gint
gtk_tree_path_get_depth (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, 0);

  return path->depth;
}

/**
 * gtk_tree_path_get_indices: (skip)
 * @path: a #GtkTreePath
 *
 * Returns the current indices of @path.
 *
 * This is an array of integers, each representing a node in a tree.
 * This value should not be freed.
 *
 * The length of the array can be obtained with gtk_tree_path_get_depth().
 *
 * Return value: The current indices, or %NULL
 */
gint *
gtk_tree_path_get_indices (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, NULL);

  return path->indices;
}

/**
 * gtk_tree_path_get_indices_with_depth:
 * @path: a #GtkTreePath
 * @depth: (allow-none): return location for number of elements
 *     returned in the integer array, or %NULL
 *
 * Returns the current indices of @path.
 *
 * This is an array of integers, each representing a node in a tree.
 * It also returns the number of elements in the array.
 * The array should not be freed.
 *
 * Return value: (array length=depth) (transfer none): The current
 *     indices, or %NULL
 *
 * Since: 3.0
 *
 * Rename to: gtk_tree_path_get_indices
 */
gint *
gtk_tree_path_get_indices_with_depth (GtkTreePath *path,
                                      gint        *depth)
{
  g_return_val_if_fail (path != NULL, NULL);

  if (depth)
    *depth = path->depth;

  return path->indices;
}

/**
 * gtk_tree_path_free:
 * @path: (allow-none): a #GtkTreePath
 *
 * Frees @path. If @path is %NULL, it simply returns.
 */
void
gtk_tree_path_free (GtkTreePath *path)
{
  if (!path)
    return;

  g_free (path->indices);
  g_slice_free (GtkTreePath, path);
}

/**
 * gtk_tree_path_copy:
 * @path: a #GtkTreePath
 *
 * Creates a new #GtkTreePath as a copy of @path.
 *
 * Return value: a new #GtkTreePath
 */
GtkTreePath *
gtk_tree_path_copy (const GtkTreePath *path)
{
  GtkTreePath *retval;

  g_return_val_if_fail (path != NULL, NULL);

  retval = g_slice_new (GtkTreePath);
  retval->depth = path->depth;
  retval->alloc = retval->depth;
  retval->indices = g_new (gint, path->alloc);
  memcpy (retval->indices, path->indices, path->depth * sizeof (gint));
  return retval;
}

G_DEFINE_BOXED_TYPE (GtkTreePath, gtk_tree_path,
                     gtk_tree_path_copy,
                     gtk_tree_path_free)

/**
 * gtk_tree_path_compare:
 * @a: a #GtkTreePath
 * @b: a #GtkTreePath to compare with
 *
 * Compares two paths.
 *
 * If @a appears before @b in a tree, then -1 is returned.
 * If @b appears before @a, then 1 is returned.
 * If the two nodes are equal, then 0 is returned.
 *
 * Return value: the relative positions of @a and @b
 */
gint
gtk_tree_path_compare (const GtkTreePath *a,
                       const GtkTreePath *b)
{
  gint p = 0, q = 0;

  g_return_val_if_fail (a != NULL, 0);
  g_return_val_if_fail (b != NULL, 0);
  g_return_val_if_fail (a->depth > 0, 0);
  g_return_val_if_fail (b->depth > 0, 0);

  do
    {
      if (a->indices[p] == b->indices[q])
        continue;
      return (a->indices[p] < b->indices[q]?-1:1);
    }
  while (++p < a->depth && ++q < b->depth);
  if (a->depth == b->depth)
    return 0;
  return (a->depth < b->depth?-1:1);
}

/**
 * gtk_tree_path_is_ancestor:
 * @path: a #GtkTreePath
 * @descendant: another #GtkTreePath
 *
 * Returns %TRUE if @descendant is a descendant of @path.
 *
 * Return value: %TRUE if @descendant is contained inside @path
 */
gboolean
gtk_tree_path_is_ancestor (GtkTreePath *path,
                           GtkTreePath *descendant)
{
  gint i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (descendant != NULL, FALSE);

  /* can't be an ancestor if we're deeper */
  if (path->depth >= descendant->depth)
    return FALSE;

  i = 0;
  while (i < path->depth)
    {
      if (path->indices[i] != descendant->indices[i])
        return FALSE;
      ++i;
    }

  return TRUE;
}

/**
 * gtk_tree_path_is_descendant:
 * @path: a #GtkTreePath
 * @ancestor: another #GtkTreePath
 *
 * Returns %TRUE if @path is a descendant of @ancestor.
 *
 * Return value: %TRUE if @ancestor contains @path somewhere below it
 */
gboolean
gtk_tree_path_is_descendant (GtkTreePath *path,
                             GtkTreePath *ancestor)
{
  gint i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);

  /* can't be a descendant if we're shallower in the tree */
  if (path->depth <= ancestor->depth)
    return FALSE;

  i = 0;
  while (i < ancestor->depth)
    {
      if (path->indices[i] != ancestor->indices[i])
        return FALSE;
      ++i;
    }

  return TRUE;
}


/**
 * gtk_tree_path_next:
 * @path: a #GtkTreePath
 *
 * Moves the @path to point to the next node at the current depth.
 */
void
gtk_tree_path_next (GtkTreePath *path)
{
  g_return_if_fail (path != NULL);
  g_return_if_fail (path->depth > 0);

  path->indices[path->depth - 1] ++;
}

/**
 * gtk_tree_path_prev:
 * @path: a #GtkTreePath
 *
 * Moves the @path to point to the previous node at the
 * current depth, if it exists.
 *
 * Return value: %TRUE if @path has a previous node, and
 *     the move was made
 */
gboolean
gtk_tree_path_prev (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->depth == 0)
    return FALSE;

  if (path->indices[path->depth - 1] == 0)
    return FALSE;

  path->indices[path->depth - 1] -= 1;

  return TRUE;
}

/**
 * gtk_tree_path_up:
 * @path: a #GtkTreePath
 *
 * Moves the @path to point to its parent node, if it has a parent.
 *
 * Return value: %TRUE if @path has a parent, and the move was made
 */
gboolean
gtk_tree_path_up (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->depth == 0)
    return FALSE;

  path->depth--;

  return TRUE;
}

/**
 * gtk_tree_path_down:
 * @path: a #GtkTreePath
 *
 * Moves @path to point to the first child of the current path.
 */
void
gtk_tree_path_down (GtkTreePath *path)
{
  g_return_if_fail (path != NULL);

  gtk_tree_path_append_index (path, 0);
}

/**
 * gtk_tree_iter_copy:
 * @iter: a #GtkTreeIter
 *
 * Creates a dynamically allocated tree iterator as a copy of @iter.
 *
 * This function is not intended for use in applications,
 * because you can just copy the structs by value
 * (<literal>GtkTreeIter new_iter = iter;</literal>).
 * You must free this iter with gtk_tree_iter_free().
 *
 * Return value: a newly-allocated copy of @iter
 */
GtkTreeIter *
gtk_tree_iter_copy (GtkTreeIter *iter)
{
  GtkTreeIter *retval;

  g_return_val_if_fail (iter != NULL, NULL);

  retval = g_slice_new (GtkTreeIter);
  *retval = *iter;

  return retval;
}

/**
 * gtk_tree_iter_free:
 * @iter: a dynamically allocated tree iterator
 *
 * Frees an iterator that has been allocated by gtk_tree_iter_copy().
 *
 * This function is mainly used for language bindings.
 */
void
gtk_tree_iter_free (GtkTreeIter *iter)
{
  g_return_if_fail (iter != NULL);

  g_slice_free (GtkTreeIter, iter);
}

G_DEFINE_BOXED_TYPE (GtkTreeIter,  gtk_tree_iter,
                     gtk_tree_iter_copy,
                     gtk_tree_iter_free)

/**
 * gtk_tree_model_get_flags:
 * @tree_model: a #GtkTreeModel
 *
 * Returns a set of flags supported by this interface.
 *
 * The flags are a bitwise combination of #GtkTreeModelFlags.
 * The flags supported should not change during the lifetime
 * of the @tree_model.
 *
 * Return value: the flags supported by this interface
 */
GtkTreeModelFlags
gtk_tree_model_get_flags (GtkTreeModel *tree_model)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  if (iface->get_flags)
    return (* iface->get_flags) (tree_model);

  return 0;
}

/**
 * gtk_tree_model_get_n_columns:
 * @tree_model: a #GtkTreeModel
 *
 * Returns the number of columns supported by @tree_model.
 *
 * Return value: the number of columns
 */
gint
gtk_tree_model_get_n_columns (GtkTreeModel *tree_model)
{
  GtkTreeModelIface *iface;
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->get_n_columns != NULL, 0);

  return (* iface->get_n_columns) (tree_model);
}

/**
 * gtk_tree_model_get_column_type:
 * @tree_model: a #GtkTreeModel
 * @index_: the column index
 *
 * Returns the type of the column.
 *
 * Return value: (transfer none): the type of the column
 */
GType
gtk_tree_model_get_column_type (GtkTreeModel *tree_model,
                                gint          index)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), G_TYPE_INVALID);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->get_column_type != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (index >= 0, G_TYPE_INVALID);

  return (* iface->get_column_type) (tree_model, index);
}

/**
 * gtk_tree_model_get_iter:
 * @tree_model: a #GtkTreeModel
 * @iter: (out): the uninitialized #GtkTreeIter
 * @path: the #GtkTreePath
 *
 * Sets @iter to a valid iterator pointing to @path.  If @path does
 * not exist, @iter is set to an invalid iterator and %FALSE is returned.
 *
 * Return value: %TRUE, if @iter was set
 */
gboolean
gtk_tree_model_get_iter (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter,
                         GtkTreePath  *path)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->get_iter != NULL, FALSE);
  g_return_val_if_fail (path->depth > 0, FALSE);

  INITIALIZE_TREE_ITER (iter);

  return (* iface->get_iter) (tree_model, iter, path);
}

/**
 * gtk_tree_model_get_iter_from_string:
 * @tree_model: a #GtkTreeModel
 * @iter: (out): an uninitialized #GtkTreeIter
 * @path_string: a string representation of a #GtkTreePath
 *
 * Sets @iter to a valid iterator pointing to @path_string, if it
 * exists. Otherwise, @iter is left invalid and %FALSE is returned.
 *
 * Return value: %TRUE, if @iter was set
 */
gboolean
gtk_tree_model_get_iter_from_string (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     const gchar  *path_string)
{
  gboolean retval;
  GtkTreePath *path;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (path_string != NULL, FALSE);

  path = gtk_tree_path_new_from_string (path_string);

  g_return_val_if_fail (path != NULL, FALSE);

  retval = gtk_tree_model_get_iter (tree_model, iter, path);
  gtk_tree_path_free (path);

  return retval;
}

/**
 * gtk_tree_model_get_string_from_iter:
 * @tree_model: a #GtkTreeModel
 * @iter: a #GtkTreeIter
 *
 * Generates a string representation of the iter.
 *
 * This string is a ':' separated list of numbers.
 * For example, "4:10:0:3" would be an acceptable
 * return value for this string.
 *
 * Return value: a newly-allocated string.
 *     Must be freed with g_free().
 *
 * Since: 2.2
 */
gchar *
gtk_tree_model_get_string_from_iter (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter)
{
  GtkTreePath *path;
  gchar *ret;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  path = gtk_tree_model_get_path (tree_model, iter);

  g_return_val_if_fail (path != NULL, NULL);

  ret = gtk_tree_path_to_string (path);
  gtk_tree_path_free (path);

  return ret;
}

/**
 * gtk_tree_model_get_iter_first:
 * @tree_model: a #GtkTreeModel
 * @iter: (out): the uninitialized #GtkTreeIter
 *
 * Initializes @iter with the first iterator in the tree
 * (the one at the path "0") and returns %TRUE. Returns
 * %FALSE if the tree is empty.
 *
 * Return value: %TRUE, if @iter was set
 */
gboolean
gtk_tree_model_get_iter_first (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  GtkTreePath *path;
  gboolean retval;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  path = gtk_tree_path_new_first ();
  retval = gtk_tree_model_get_iter (tree_model, iter, path);
  gtk_tree_path_free (path);

  return retval;
}

/**
 * gtk_tree_model_get_path:
 * @tree_model: a #GtkTreeModel
 * @iter: the #GtkTreeIter
 *
 * Returns a newly-created #GtkTreePath referenced by @iter.
 *
 * This path should be freed with gtk_tree_path_free().
 *
 * Return value: a newly-created #GtkTreePath
 */
GtkTreePath *
gtk_tree_model_get_path (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->get_path != NULL, NULL);

  return (* iface->get_path) (tree_model, iter);
}

/**
 * gtk_tree_model_get_value:
 * @tree_model: a #GtkTreeModel
 * @iter: the #GtkTreeIter
 * @column: the column to lookup the value at
 * @value: (out) (transfer none): an empty #GValue to set
 *
 * Initializes and sets @value to that at @column.
 *
 * When done with @value, g_value_unset() needs to be called
 * to free any allocated memory.
 */
void
gtk_tree_model_get_value (GtkTreeModel *tree_model,
                          GtkTreeIter  *iter,
                          gint          column,
                          GValue       *value)
{
  GtkTreeModelIface *iface;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (value != NULL);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_if_fail (iface->get_value != NULL);

  (* iface->get_value) (tree_model, iter, column, value);
}

/**
 * gtk_tree_model_iter_next:
 * @tree_model: a #GtkTreeModel
 * @iter: (in): the #GtkTreeIter
 *
 * Sets @iter to point to the node following it at the current level.
 *
 * If there is no next @iter, %FALSE is returned and @iter is set
 * to be invalid.
 *
 * Return value: %TRUE if @iter has been changed to the next node
 */
gboolean
gtk_tree_model_iter_next (GtkTreeModel  *tree_model,
                          GtkTreeIter   *iter)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_next != NULL, FALSE);

  return (* iface->iter_next) (tree_model, iter);
}

static gboolean
gtk_tree_model_iter_previous_default (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter)
{
  gboolean retval;
  GtkTreePath *path;

  path = gtk_tree_model_get_path (tree_model, iter);
  if (path == NULL)
    return FALSE;

  retval = gtk_tree_path_prev (path) &&
           gtk_tree_model_get_iter (tree_model, iter, path);
  if (retval == FALSE)
    iter->stamp = 0;

  gtk_tree_path_free (path);

  return retval;
}

/**
 * gtk_tree_model_iter_previous:
 * @tree_model: a #GtkTreeModel
 * @iter: (in): the #GtkTreeIter
 *
 * Sets @iter to point to the previous node at the current level.
 *
 * If there is no previous @iter, %FALSE is returned and @iter is
 * set to be invalid.
 *
 * Return value: %TRUE if @iter has been changed to the previous node
 *
 * Since: 3.0
 */
gboolean
gtk_tree_model_iter_previous (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  gboolean retval;
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);

  if (iface->iter_previous)
    retval = (* iface->iter_previous) (tree_model, iter);
  else
    retval = gtk_tree_model_iter_previous_default (tree_model, iter);

  return retval;
}

/**
 * gtk_tree_model_iter_children:
 * @tree_model: a #GtkTreeModel
 * @iter: (out): the new #GtkTreeIter to be set to the child
 * @parent: (allow-none): the #GtkTreeIter, or %NULL
 *
 * Sets @iter to point to the first child of @parent.
 *
 * If @parent has no children, %FALSE is returned and @iter is
 * set to be invalid. @parent will remain a valid node after this
 * function has been called.
 *
 * If @parent is %NULL returns the first node, equivalent to
 * <literal>gtk_tree_model_get_iter_first (tree_model, iter);</literal>
 *
 * Return value: %TRUE, if @child has been set to the first child
 */
gboolean
gtk_tree_model_iter_children (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter,
                              GtkTreeIter  *parent)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_children != NULL, FALSE);

  INITIALIZE_TREE_ITER (iter);

  return (* iface->iter_children) (tree_model, iter, parent);
}

/**
 * gtk_tree_model_iter_has_child:
 * @tree_model: a #GtkTreeModel
 * @iter: the #GtkTreeIter to test for children
 *
 * Returns %TRUE if @iter has children, %FALSE otherwise.
 *
 * Return value: %TRUE if @iter has children
 */
gboolean
gtk_tree_model_iter_has_child (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_has_child != NULL, FALSE);

  return (* iface->iter_has_child) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_n_children:
 * @tree_model: a #GtkTreeModel
 * @iter: (allow-none): the #GtkTreeIter, or %NULL
 *
 * Returns the number of children that @iter has.
 *
 * As a special case, if @iter is %NULL, then the number
 * of toplevel nodes is returned.
 *
 * Return value: the number of children of @iter
 */
gint
gtk_tree_model_iter_n_children (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_n_children != NULL, 0);

  return (* iface->iter_n_children) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_nth_child:
 * @tree_model: a #GtkTreeModel
 * @iter: (out): the #GtkTreeIter to set to the nth child
 * @parent: (allow-none): the #GtkTreeIter to get the child from, or %NULL.
 * @n: the index of the desired child
 *
 * Sets @iter to be the child of @parent, using the given index.
 *
 * The first index is 0. If @n is too big, or @parent has no children,
 * @iter is set to an invalid iterator and %FALSE is returned. @parent
 * will remain a valid node after this function has been called. As a
 * special case, if @parent is %NULL, then the @n<!-- -->th root node
 * is set.
 *
 * Return value: %TRUE, if @parent has an @n<!-- -->th child
 */
gboolean
gtk_tree_model_iter_nth_child (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *parent,
                               gint          n)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (n >= 0, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_nth_child != NULL, FALSE);

  INITIALIZE_TREE_ITER (iter);

  return (* iface->iter_nth_child) (tree_model, iter, parent, n);
}

/**
 * gtk_tree_model_iter_parent:
 * @tree_model: a #GtkTreeModel
 * @iter: (out): the new #GtkTreeIter to set to the parent
 * @child: the #GtkTreeIter
 *
 * Sets @iter to be the parent of @child.
 *
 * If @child is at the toplevel, and doesn't have a parent, then
 * @iter is set to an invalid iterator and %FALSE is returned.
 * @child will remain a valid node after this function has been
 * called.
 *
 * Return value: %TRUE, if @iter is set to the parent of @child
 */
gboolean
gtk_tree_model_iter_parent (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter,
                            GtkTreeIter  *child)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (child != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_parent != NULL, FALSE);

  INITIALIZE_TREE_ITER (iter);

  return (* iface->iter_parent) (tree_model, iter, child);
}

/**
 * gtk_tree_model_ref_node:
 * @tree_model: a #GtkTreeModel
 * @iter: the #GtkTreeIter
 *
 * Lets the tree ref the node.
 *
 * This is an optional method for models to implement.
 * To be more specific, models may ignore this call as it exists
 * primarily for performance reasons.
 *
 * This function is primarily meant as a way for views to let
 * caching models know when nodes are being displayed (and hence,
 * whether or not to cache that node). Being displayed means a node
 * is in an expanded branch, regardless of whether the node is currently
 * visible in the viewport. For example, a file-system based model
 * would not want to keep the entire file-hierarchy in memory,
 * just the sections that are currently being displayed by
 * every current view.
 *
 * A model should be expected to be able to get an iter independent
 * of its reffed state.
 */
void
gtk_tree_model_ref_node (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  if (iface->ref_node)
    (* iface->ref_node) (tree_model, iter);
}

/**
 * gtk_tree_model_unref_node:
 * @tree_model: a #GtkTreeModel
 * @iter: the #GtkTreeIter
 *
 * Lets the tree unref the node.
 *
 * This is an optional method for models to implement.
 * To be more specific, models may ignore this call as it exists
 * primarily for performance reasons. For more information on what
 * this means, see gtk_tree_model_ref_node().
 *
 * Please note that nodes that are deleted are not unreffed.
 */
void
gtk_tree_model_unref_node (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  if (iface->unref_node)
    (* iface->unref_node) (tree_model, iter);
}

/**
 * gtk_tree_model_get:
 * @tree_model: a #GtkTreeModel
 * @iter: a row in @tree_model
 * @...: pairs of column number and value return locations,
 *     terminated by -1
 *
 * Gets the value of one or more cells in the row referenced by @iter.
 * The variable argument list should contain integer column numbers,
 * each column number followed by a place to store the value being
 * retrieved.  The list is terminated by a -1. For example, to get a
 * value from column 0 with type %G_TYPE_STRING, you would
 * write: <literal>gtk_tree_model_get (model, iter, 0, &amp;place_string_here, -1)</literal>,
 * where <literal>place_string_here</literal> is a <type>gchar*</type>
 * to be filled with the string.
 *
 * Returned values with type %G_TYPE_OBJECT have to be unreferenced,
 * values with type %G_TYPE_STRING or %G_TYPE_BOXED have to be freed.
 * Other values are passed by value.
 */
void
gtk_tree_model_get (GtkTreeModel *tree_model,
                    GtkTreeIter  *iter,
                    ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  va_start (var_args, iter);
  gtk_tree_model_get_valist (tree_model, iter, var_args);
  va_end (var_args);
}

/**
 * gtk_tree_model_get_valist:
 * @tree_model: a #GtkTreeModel
 * @iter: a row in @tree_model
 * @var_args: <type>va_list</type> of column/return location pairs
 *
 * See gtk_tree_model_get(), this version takes a <type>va_list</type>
 * for language bindings to use.
 */
void
gtk_tree_model_get_valist (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter,
                           va_list      var_args)
{
  gint column;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  column = va_arg (var_args, gint);

  while (column != -1)
    {
      GValue value = G_VALUE_INIT;
      gchar *error = NULL;

      if (column >= gtk_tree_model_get_n_columns (tree_model))
        {
          g_warning ("%s: Invalid column number %d accessed (remember to end your list of columns with a -1)", G_STRLOC, column);
          break;
        }

      gtk_tree_model_get_value (GTK_TREE_MODEL (tree_model), iter, column, &value);

      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);

          /* we purposely leak the value here, it might not be
           * in a sane state if an error condition occurred
           */
          break;
        }

      g_value_unset (&value);

      column = va_arg (var_args, gint);
    }
}

/**
 * gtk_tree_model_row_changed:
 * @tree_model: a #GtkTreeModel
 * @path: a #GtkTreePath pointing to the changed row
 * @iter: a valid #GtkTreeIter pointing to the changed row
 *
 * Emits the #GtkTreeModel::row-changed signal on @tree_model.
 */
void
gtk_tree_model_row_changed (GtkTreeModel *tree_model,
                            GtkTreePath  *path,
                            GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROW_CHANGED], 0, path, iter);
}

/**
 * gtk_tree_model_row_inserted:
 * @tree_model: a #GtkTreeModel
 * @path: a #GtkTreePath pointing to the inserted row
 * @iter: a valid #GtkTreeIter pointing to the inserted row
 *
 * Emits the #GtkTreeModel::row-inserted signal on @tree_model.
 */
void
gtk_tree_model_row_inserted (GtkTreeModel *tree_model,
                             GtkTreePath  *path,
                             GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROW_INSERTED], 0, path, iter);
}

/**
 * gtk_tree_model_row_has_child_toggled:
 * @tree_model: a #GtkTreeModel
 * @path: a #GtkTreePath pointing to the changed row
 * @iter: a valid #GtkTreeIter pointing to the changed row
 *
 * Emits the #GtkTreeModel::row-has-child-toggled signal on
 * @tree_model. This should be called by models after the child
 * state of a node changes.
 */
void
gtk_tree_model_row_has_child_toggled (GtkTreeModel *tree_model,
                                      GtkTreePath  *path,
                                      GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROW_HAS_CHILD_TOGGLED], 0, path, iter);
}

/**
 * gtk_tree_model_row_deleted:
 * @tree_model: a #GtkTreeModel
 * @path: a #GtkTreePath pointing to the previous location of
 *     the deleted row
 *
 * Emits the #GtkTreeModel::row-deleted signal on @tree_model.
 *
 * This should be called by models after a row has been removed.
 * The location pointed to by @path should be the location that
 * the row previously was at. It may not be a valid location anymore.
 *
 * Nodes that are deleted are not unreffed, this means that any
 * outstanding references on the deleted node should not be released.
 */
void
gtk_tree_model_row_deleted (GtkTreeModel *tree_model,
                            GtkTreePath  *path)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROW_DELETED], 0, path);
}

/**
 * gtk_tree_model_rows_reordered: (skip)
 * @tree_model: a #GtkTreeModel
 * @path: a #GtkTreePath pointing to the tree node whose children
 *     have been reordered
 * @iter: a valid #GtkTreeIter pointing to the node whose children
 *     have been reordered, or %NULL if the depth of @path is 0
 * @new_order: an array of integers mapping the current position of
 *     each child to its old position before the re-ordering,
 *     i.e. @new_order<literal>[newpos] = oldpos</literal>
 *
 * Emits the #GtkTreeModel::rows-reordered signal on @tree_model.
 *
 * This should be called by models when their rows have been
 * reordered.
 */
void
gtk_tree_model_rows_reordered (GtkTreeModel *tree_model,
                               GtkTreePath  *path,
                               GtkTreeIter  *iter,
                               gint         *new_order)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (new_order != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROWS_REORDERED], 0, path, iter, new_order);
}


static gboolean
gtk_tree_model_foreach_helper (GtkTreeModel            *model,
                               GtkTreeIter             *iter,
                               GtkTreePath             *path,
                               GtkTreeModelForeachFunc  func,
                               gpointer                 user_data)
{
  do
    {
      GtkTreeIter child;

      if ((* func) (model, path, iter, user_data))
        return TRUE;

      if (gtk_tree_model_iter_children (model, &child, iter))
        {
          gtk_tree_path_down (path);
          if (gtk_tree_model_foreach_helper (model, &child, path, func, user_data))
            return TRUE;
          gtk_tree_path_up (path);
        }

      gtk_tree_path_next (path);
    }
  while (gtk_tree_model_iter_next (model, iter));

  return FALSE;
}

/**
 * gtk_tree_model_foreach:
 * @model: a #GtkTreeModel
 * @func: (scope call): a function to be called on each row
 * @user_data: user data to passed to @func
 *
 * Calls func on each node in model in a depth-first fashion.
 *
 * If @func returns %TRUE, then the tree ceases to be walked,
 * and gtk_tree_model_foreach() returns.
 */
void
gtk_tree_model_foreach (GtkTreeModel            *model,
                        GtkTreeModelForeachFunc  func,
                        gpointer                 user_data)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_TREE_MODEL (model));
  g_return_if_fail (func != NULL);

  path = gtk_tree_path_new_first ();
  if (gtk_tree_model_get_iter (model, &iter, path) == FALSE)
    {
      gtk_tree_path_free (path);
      return;
    }

  gtk_tree_model_foreach_helper (model, &iter, path, func, user_data);
  gtk_tree_path_free (path);
}


/*
 * GtkTreeRowReference
 */

static void gtk_tree_row_reference_unref_path (GtkTreePath  *path,
                                               GtkTreeModel *model,
                                               gint          depth);


G_DEFINE_BOXED_TYPE (GtkTreeRowReference, gtk_tree_row_reference,
                     gtk_tree_row_reference_copy,
                     gtk_tree_row_reference_free)

struct _GtkTreeRowReference
{
  GObject *proxy;
  GtkTreeModel *model;
  GtkTreePath *path;
};


static void
release_row_references (gpointer data)
{
  RowRefList *refs = data;
  GSList *tmp_list = NULL;

  tmp_list = refs->list;
  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->proxy == (GObject *)reference->model)
        reference->model = NULL;
      reference->proxy = NULL;

      /* we don't free the reference, users are responsible for that. */

      tmp_list = g_slist_next (tmp_list);
    }

  g_slist_free (refs->list);
  g_free (refs);
}

static void
gtk_tree_row_ref_inserted (RowRefList  *refs,
                           GtkTreePath *path,
                           GtkTreeIter *iter)
{
  GSList *tmp_list;

  if (refs == NULL)
    return;

  /* This function corrects the path stored in the reference to
   * account for an insertion. Note that it's called _after_ the
   * insertion with the path to the newly-inserted row. Which means
   * that the inserted path is in a different "coordinate system" than
   * the old path (e.g. if the inserted path was just before the old
   * path, then inserted path and old path will be the same, and old
   * path must be moved down one).
   */

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->path == NULL)
        goto done;

      if (reference->path->depth >= path->depth)
        {
          gint i;
          gboolean ancestor = TRUE;

          for (i = 0; i < path->depth - 1; i ++)
            {
              if (path->indices[i] != reference->path->indices[i])
                {
                  ancestor = FALSE;
                  break;
                }
            }
          if (ancestor == FALSE)
            goto done;

          if (path->indices[path->depth-1] <= reference->path->indices[path->depth-1])
            reference->path->indices[path->depth-1] += 1;
        }
    done:
      tmp_list = g_slist_next (tmp_list);
    }
}

static void
gtk_tree_row_ref_deleted (RowRefList  *refs,
                          GtkTreePath *path)
{
  GSList *tmp_list;

  if (refs == NULL)
    return;

  /* This function corrects the path stored in the reference to
   * account for an deletion. Note that it's called _after_ the
   * deletion with the old path of the just-deleted row. Which means
   * that the deleted path is the same now-defunct "coordinate system"
   * as the path saved in the reference, which is what we want to fix.
   */

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->path)
        {
          gint i;

          if (path->depth > reference->path->depth)
            goto next;
          for (i = 0; i < path->depth - 1; i++)
            {
              if (path->indices[i] != reference->path->indices[i])
                goto next;
            }

          /* We know it affects us. */
          if (path->indices[i] == reference->path->indices[i])
            {
              if (reference->path->depth > path->depth)
                /* some parent was deleted, trying to unref any node
                 * between the deleted parent and the node the reference
                 * is pointing to is bad, as those nodes are already gone.
                 */
                gtk_tree_row_reference_unref_path (reference->path, reference->model, path->depth - 1);
              else
                gtk_tree_row_reference_unref_path (reference->path, reference->model, reference->path->depth - 1);
              gtk_tree_path_free (reference->path);
              reference->path = NULL;
            }
          else if (path->indices[i] < reference->path->indices[i])
            {
              reference->path->indices[path->depth-1]-=1;
            }
        }

next:
      tmp_list = g_slist_next (tmp_list);
    }
}

static void
gtk_tree_row_ref_reordered (RowRefList  *refs,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gint        *new_order)
{
  GSList *tmp_list;
  gint length;

  if (refs == NULL)
    return;

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      length = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (reference->model), iter);

      if (length < 2)
        return;

      if ((reference->path) &&
          (gtk_tree_path_is_ancestor (path, reference->path)))
        {
          gint ref_depth = gtk_tree_path_get_depth (reference->path);
          gint depth = gtk_tree_path_get_depth (path);

          if (ref_depth > depth)
            {
              gint i;
              gint *indices = gtk_tree_path_get_indices (reference->path);

              for (i = 0; i < length; i++)
                {
                  if (new_order[i] == indices[depth])
                    {
                      indices[depth] = i;
                      break;
                    }
                }
            }
        }

      tmp_list = g_slist_next (tmp_list);
    }
}

/* We do this recursively so that we can unref children nodes
 * before their parent
 */
static void
gtk_tree_row_reference_unref_path_helper (GtkTreePath  *path,
                                          GtkTreeModel *model,
                                          GtkTreeIter  *parent_iter,
                                          gint          depth,
                                          gint          current_depth)
{
  GtkTreeIter iter;

  if (depth == current_depth)
    return;

  gtk_tree_model_iter_nth_child (model, &iter, parent_iter, path->indices[current_depth]);
  gtk_tree_row_reference_unref_path_helper (path, model, &iter, depth, current_depth + 1);
  gtk_tree_model_unref_node (model, &iter);
}

static void
gtk_tree_row_reference_unref_path (GtkTreePath  *path,
                                   GtkTreeModel *model,
                                   gint          depth)
{
  GtkTreeIter iter;

  if (depth <= 0)
    return;

  gtk_tree_model_iter_nth_child (model, &iter, NULL, path->indices[0]);
  gtk_tree_row_reference_unref_path_helper (path, model, &iter, depth, 1);
  gtk_tree_model_unref_node (model, &iter);
}

/**
 * gtk_tree_row_reference_new:
 * @model: a #GtkTreeModel
 * @path: a valid #GtkTreePath to monitor
 *
 * Creates a row reference based on @path.
 *
 * This reference will keep pointing to the node pointed to
 * by @path, so long as it exists. Any changes that occur on @model are
 * propagated, and the path is updated appropriately. If
 * @path isn't a valid path in @model, then %NULL is returned.
 *
 * Return value: a newly allocated #GtkTreeRowReference, or %NULL
 */
GtkTreeRowReference *
gtk_tree_row_reference_new (GtkTreeModel *model,
                            GtkTreePath  *path)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  /* We use the model itself as the proxy object; and call
   * gtk_tree_row_reference_inserted(), etc, in the
   * class closure (default handler) marshalers for the signal.
   */
  return gtk_tree_row_reference_new_proxy (G_OBJECT (model), model, path);
}

/**
 * gtk_tree_row_reference_new_proxy:
 * @proxy: a proxy #GObject
 * @model: a #GtkTreeModel
 * @path: a valid #GtkTreePath to monitor
 *
 * You do not need to use this function.
 *
 * Creates a row reference based on @path.
 *
 * This reference will keep pointing to the node pointed to
 * by @path, so long as it exists. If @path isn't a valid
 * path in @model, then %NULL is returned. However, unlike
 * references created with gtk_tree_row_reference_new(), it
 * does not listen to the model for changes. The creator of
 * the row reference must do this explicitly using
 * gtk_tree_row_reference_inserted(), gtk_tree_row_reference_deleted(),
 * gtk_tree_row_reference_reordered().
 *
 * These functions must be called exactly once per proxy when the
 * corresponding signal on the model is emitted. This single call
 * updates all row references for that proxy. Since built-in GTK+
 * objects like #GtkTreeView already use this mechanism internally,
 * using them as the proxy object will produce unpredictable results.
 * Further more, passing the same object as @model and @proxy
 * doesn't work for reasons of internal implementation.
 *
 * This type of row reference is primarily meant by structures that
 * need to carefully monitor exactly when a row reference updates
 * itself, and is not generally needed by most applications.
 *
 * Return value: a newly allocated #GtkTreeRowReference, or %NULL
 */
GtkTreeRowReference *
gtk_tree_row_reference_new_proxy (GObject      *proxy,
                                  GtkTreeModel *model,
                                  GtkTreePath  *path)
{
  GtkTreeRowReference *reference;
  RowRefList *refs;
  GtkTreeIter parent_iter;
  gint i;

  g_return_val_if_fail (G_IS_OBJECT (proxy), NULL);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path->depth > 0, NULL);

  /* check that the path is valid */
  if (gtk_tree_model_get_iter (model, &parent_iter, path) == FALSE)
    return NULL;

  /* Now we want to ref every node */
  gtk_tree_model_iter_nth_child (model, &parent_iter, NULL, path->indices[0]);
  gtk_tree_model_ref_node (model, &parent_iter);

  for (i = 1; i < path->depth; i++)
    {
      GtkTreeIter iter;
      gtk_tree_model_iter_nth_child (model, &iter, &parent_iter, path->indices[i]);
      gtk_tree_model_ref_node (model, &iter);
      parent_iter = iter;
    }

  /* Make the row reference */
  reference = g_new (GtkTreeRowReference, 1);

  g_object_ref (proxy);
  g_object_ref (model);
  reference->proxy = proxy;
  reference->model = model;
  reference->path = gtk_tree_path_copy (path);

  refs = g_object_get_data (G_OBJECT (proxy), ROW_REF_DATA_STRING);

  if (refs == NULL)
    {
      refs = g_new (RowRefList, 1);
      refs->list = NULL;

      g_object_set_data_full (G_OBJECT (proxy),
                              I_(ROW_REF_DATA_STRING),
                              refs, release_row_references);
    }

  refs->list = g_slist_prepend (refs->list, reference);

  return reference;
}

/**
 * gtk_tree_row_reference_get_path:
 * @reference: a #GtkTreeRowReference
 *
 * Returns a path that the row reference currently points to,
 * or %NULL if the path pointed to is no longer valid.
 *
 * Return value: a current path, or %NULL
 */
GtkTreePath *
gtk_tree_row_reference_get_path (GtkTreeRowReference *reference)
{
  g_return_val_if_fail (reference != NULL, NULL);

  if (reference->proxy == NULL)
    return NULL;

  if (reference->path == NULL)
    return NULL;

  return gtk_tree_path_copy (reference->path);
}

/**
 * gtk_tree_row_reference_get_model:
 * @reference: a #GtkTreeRowReference
 *
 * Returns the model that the row reference is monitoring.
 *
 * Return value: (transfer none): the model
 *
 * Since: 2.8
 */
GtkTreeModel *
gtk_tree_row_reference_get_model (GtkTreeRowReference *reference)
{
  g_return_val_if_fail (reference != NULL, NULL);

  return reference->model;
}

/**
 * gtk_tree_row_reference_valid:
 * @reference: (allow-none): a #GtkTreeRowReference, or %NULL
 *
 * Returns %TRUE if the @reference is non-%NULL and refers to
 * a current valid path.
 *
 * Return value: %TRUE if @reference points to a valid path
 */
gboolean
gtk_tree_row_reference_valid (GtkTreeRowReference *reference)
{
  if (reference == NULL || reference->path == NULL)
    return FALSE;

  return TRUE;
}


/**
 * gtk_tree_row_reference_copy:
 * @reference: a #GtkTreeRowReference
 *
 * Copies a #GtkTreeRowReference.
 *
 * Return value: a copy of @reference
 *
 * Since: 2.2
 */
GtkTreeRowReference *
gtk_tree_row_reference_copy (GtkTreeRowReference *reference)
{
  return gtk_tree_row_reference_new_proxy (reference->proxy,
                                           reference->model,
                                           reference->path);
}

/**
 * gtk_tree_row_reference_free:
 * @reference: (allow-none): a #GtkTreeRowReference, or %NULL
 *
 * Free's @reference. @reference may be %NULL
 */
void
gtk_tree_row_reference_free (GtkTreeRowReference *reference)
{
  RowRefList *refs;

  if (reference == NULL)
    return;

  refs = g_object_get_data (G_OBJECT (reference->proxy), ROW_REF_DATA_STRING);

  if (refs == NULL)
    {
      g_warning (G_STRLOC": bad row reference, proxy has no outstanding row references");
      return;
    }

  refs->list = g_slist_remove (refs->list, reference);

  if (refs->list == NULL)
    {
      g_object_set_data (G_OBJECT (reference->proxy),
                         I_(ROW_REF_DATA_STRING),
                         NULL);
    }

  if (reference->path)
    {
      gtk_tree_row_reference_unref_path (reference->path, reference->model, reference->path->depth);
      gtk_tree_path_free (reference->path);
    }

  g_object_unref (reference->proxy);
  g_object_unref (reference->model);
  g_free (reference);
}

/**
 * gtk_tree_row_reference_inserted:
 * @proxy: a #GObject
 * @path: the row position that was inserted
 *
 * Lets a set of row reference created by
 * gtk_tree_row_reference_new_proxy() know that the
 * model emitted the #GtkTreeModel::row-inserted signal.
 */
void
gtk_tree_row_reference_inserted (GObject     *proxy,
                                 GtkTreePath *path)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_inserted ((RowRefList *)g_object_get_data (proxy, ROW_REF_DATA_STRING), path, NULL);
}

/**
 * gtk_tree_row_reference_deleted:
 * @proxy: a #GObject
 * @path: the path position that was deleted
 *
 * Lets a set of row reference created by
 * gtk_tree_row_reference_new_proxy() know that the
 * model emitted the #GtkTreeModel::row-deleted signal.
 */
void
gtk_tree_row_reference_deleted (GObject     *proxy,
                                GtkTreePath *path)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_deleted ((RowRefList *)g_object_get_data (proxy, ROW_REF_DATA_STRING), path);
}

/**
 * gtk_tree_row_reference_reordered: (skip)
 * @proxy: a #GObject
 * @path: the parent path of the reordered signal
 * @iter: the iter pointing to the parent of the reordered
 * @new_order: the new order of rows
 *
 * Lets a set of row reference created by
 * gtk_tree_row_reference_new_proxy() know that the
 * model emitted the #GtkTreeModel::rows-reordered signal.
 */
void
gtk_tree_row_reference_reordered (GObject     *proxy,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter,
                                  gint        *new_order)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_reordered ((RowRefList *)g_object_get_data (proxy, ROW_REF_DATA_STRING), path, iter, new_order);
}
