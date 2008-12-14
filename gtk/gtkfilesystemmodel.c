/* GTK - The GIMP Toolkit
 * gtkfilesystemmodel.c: GtkTreeModel wrapping a GtkFileSystem
 * Copyright (C) 2003, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <string.h>

#include "gtkfilechooserprivate.h"
#include "gtkfilesystemmodel.h"
#include "gtkfilesystem.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtktreednd.h"
#include "gtktreemodel.h"
#include "gtkalias.h"

typedef struct _GtkFileSystemModelClass GtkFileSystemModelClass;

#define GTK_FILE_SYSTEM_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_SYSTEM_MODEL, GtkFileSystemModelClass))
#define GTK_IS_FILE_SYSTEM_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_SYSTEM_MODEL))
#define GTK_FILE_SYSTEM_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_SYSTEM_MODEL, GtkFileSystemModelClass))

struct _GtkFileSystemModelClass
{
  GObjectClass parent_class;

  /* Signals */

  void (*finished_loading) (GtkFileSystemModel *model);
};


static void gtk_file_system_model_iface_init   (GtkTreeModelIface       *iface);
static void gtk_file_system_model_finalize     (GObject                 *object);
static void gtk_file_system_model_dispose      (GObject                 *object);

static void drag_source_iface_init (GtkTreeDragSourceIface *iface);

static GtkTreeModelFlags gtk_file_system_model_get_flags       (GtkTreeModel *tree_model);
static gint              gtk_file_system_model_get_n_columns   (GtkTreeModel *tree_model);
static GType             gtk_file_system_model_get_column_type (GtkTreeModel *tree_model,
								gint          index);
static gboolean          gtk_file_system_model_get_iter        (GtkTreeModel *tree_model,
								GtkTreeIter  *iter,
								GtkTreePath  *path);
static GtkTreePath *     gtk_file_system_model_get_path        (GtkTreeModel *tree_model,
								GtkTreeIter  *iter);
static void              gtk_file_system_model_get_value       (GtkTreeModel *tree_model,
								GtkTreeIter  *iter,
								gint          column,
								GValue       *value);
static gboolean          gtk_file_system_model_iter_next       (GtkTreeModel *tree_model,
								GtkTreeIter  *iter);
static gboolean          gtk_file_system_model_iter_children   (GtkTreeModel *tree_model,
								GtkTreeIter  *iter,
								GtkTreeIter  *parent);
static gboolean          gtk_file_system_model_iter_has_child  (GtkTreeModel *tree_model,
								GtkTreeIter  *iter);
static gint              gtk_file_system_model_iter_n_children (GtkTreeModel *tree_model,
								GtkTreeIter  *iter);
static gboolean          gtk_file_system_model_iter_nth_child  (GtkTreeModel *tree_model,
								GtkTreeIter  *iter,
								GtkTreeIter  *parent,
								gint          n);
static gboolean          gtk_file_system_model_iter_parent     (GtkTreeModel *tree_model,
								GtkTreeIter  *iter,
								GtkTreeIter  *child);
static void              gtk_file_system_model_ref_node        (GtkTreeModel *tree_model,
								GtkTreeIter  *iter);
static void              gtk_file_system_model_unref_node      (GtkTreeModel *tree_model,
								GtkTreeIter  *iter);

static gboolean drag_source_row_draggable (GtkTreeDragSource   *drag_source,
					   GtkTreePath         *path);
static gboolean drag_source_drag_data_get (GtkTreeDragSource   *drag_source,
					   GtkTreePath         *path,
					   GtkSelectionData    *selection_data);

static FileModelNode *file_model_node_new        (GtkFileSystemModel *model,
						  GFile              *file);
static void           file_model_node_free       (FileModelNode      *node);
static void           file_model_node_ref        (FileModelNode      *node);
static void           file_model_node_unref      (GtkFileSystemModel *model,
						  FileModelNode      *node);

static void file_model_node_idle_clear        (FileModelNode *node);
static void file_model_node_idle_clear_cancel (FileModelNode *node);
static void file_model_node_child_unref       (FileModelNode *parent);

static GFileInfo *        file_model_node_get_info     (GtkFileSystemModel *model,
							FileModelNode      *node);
static gboolean           file_model_node_is_visible   (GtkFileSystemModel *model,
							FileModelNode      *node);
static void               file_model_node_clear        (GtkFileSystemModel *model,
							FileModelNode      *node);
static FileModelNode *    file_model_node_get_children (GtkFileSystemModel *model,
							FileModelNode      *node);

static void deleted_callback       (GFile         *folder,
				    FileModelNode *node);
static void files_added_callback   (GFile         *folder,
				    GSList        *paths,
				    FileModelNode *node);
static void files_changed_callback (GFile         *folder,
				    GSList        *paths,
				    FileModelNode *node);
static void files_removed_callback (GFile         *folder,
				    GSList        *paths,
				    FileModelNode *node);

static void root_deleted_callback       (GFile              *folder,
					 GtkFileSystemModel *model);
static void root_files_added_callback   (GFile              *folder,
					 GSList             *paths,
					 GtkFileSystemModel *model);
static void root_files_changed_callback (GFile              *folder,
					 GSList             *paths,
					 GtkFileSystemModel *model);
static void root_files_removed_callback (GFile              *folder,
					 GSList             *paths,
					 GtkFileSystemModel *model);

/* Signal IDs */
enum {
  FINISHED_LOADING,
  LAST_SIGNAL
};

static guint file_system_model_signals[LAST_SIGNAL] = { 0 };



G_DEFINE_TYPE_WITH_CODE (GtkFileSystemModel, _gtk_file_system_model, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_file_system_model_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						drag_source_iface_init))

static void
_gtk_file_system_model_class_init (GtkFileSystemModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gtk_file_system_model_finalize;
  gobject_class->dispose = gtk_file_system_model_dispose;

  file_system_model_signals[FINISHED_LOADING] =
    g_signal_new (I_("finished-loading"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFileSystemModelClass, finished_loading),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_file_system_model_iface_init (GtkTreeModelIface *iface)
{
  iface->get_flags =       gtk_file_system_model_get_flags;
  iface->get_n_columns =   gtk_file_system_model_get_n_columns;
  iface->get_column_type = gtk_file_system_model_get_column_type;
  iface->get_iter =        gtk_file_system_model_get_iter;
  iface->get_path =        gtk_file_system_model_get_path;
  iface->get_value =       gtk_file_system_model_get_value;
  iface->iter_next =       gtk_file_system_model_iter_next;
  iface->iter_children =   gtk_file_system_model_iter_children;
  iface->iter_has_child =  gtk_file_system_model_iter_has_child;
  iface->iter_n_children = gtk_file_system_model_iter_n_children;
  iface->iter_nth_child =  gtk_file_system_model_iter_nth_child;
  iface->iter_parent =     gtk_file_system_model_iter_parent;
  iface->ref_node =        gtk_file_system_model_ref_node;
  iface->unref_node =      gtk_file_system_model_unref_node;
}

static void
_gtk_file_system_model_init (GtkFileSystemModel *model)
{
  model->show_files = TRUE;
  model->show_folders = TRUE;
  model->show_hidden = FALSE;
}

static void
gtk_file_system_model_finalize (GObject *object)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (object);
  FileModelNode *children, *next;

  if (model->root_folder)
    g_object_unref (model->root_folder);

  if (model->root_file)
    g_object_unref (model->root_file);

  if (model->file_system)
    g_object_unref (model->file_system);

  children = model->roots;
  while (children)
    {
      next = children->next;
      file_model_node_free (children);
      children = next;
    }

  g_free (model->attributes);

  G_OBJECT_CLASS (_gtk_file_system_model_parent_class)->finalize (object);
}


static void
gtk_file_system_model_dispose (GObject *object)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (object);

  if (model->pending_cancellables)
    {
      GSList *l;

      for (l = model->pending_cancellables; l; l = l->next)
	g_cancellable_cancel (l->data);
      g_slist_free (model->pending_cancellables);
      model->pending_cancellables = NULL;
    }

  G_OBJECT_CLASS (_gtk_file_system_model_parent_class)->dispose (object);
}

static void
drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = drag_source_row_draggable;
  iface->drag_data_get = drag_source_drag_data_get;
  iface->drag_data_delete = NULL;
}

/*
 * ******************** GtkTreeModel methods ********************
 */

static GtkTreeModelFlags
gtk_file_system_model_get_flags (GtkTreeModel *tree_model)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  GtkTreeModelFlags flags = GTK_TREE_MODEL_ITERS_PERSIST;

  if (model->max_depth == 0)
    flags |= GTK_TREE_MODEL_LIST_ONLY;

  return flags;
}

static gint
gtk_file_system_model_get_n_columns (GtkTreeModel *tree_model)
{
  return GTK_FILE_SYSTEM_MODEL_N_COLUMNS;
}

static GType
gtk_file_system_model_get_column_type (GtkTreeModel *tree_model,
				       gint          index)
{
  switch (index)
    {
    case GTK_FILE_SYSTEM_MODEL_INFO:
      return G_TYPE_FILE_INFO;
    case GTK_FILE_SYSTEM_MODEL_DISPLAY_NAME:
      return G_TYPE_STRING;
   default:
      g_assert_not_reached ();
      return G_TYPE_NONE;
    }
}

static gboolean
gtk_file_system_model_get_iter (GtkTreeModel *tree_model,
				GtkTreeIter  *iter,
				GtkTreePath  *path)
{
  GtkTreeIter parent;
  gint *indices;
  gint depth, i;

  indices = gtk_tree_path_get_indices (path);
  depth = gtk_tree_path_get_depth (path);

  g_return_val_if_fail (depth > 0, FALSE);

  if (!gtk_tree_model_iter_nth_child (tree_model, iter, NULL, indices[0]))
    return FALSE;

  for (i = 1; i < depth; i++)
    {
      parent = *iter;
      if (!gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[i]))
	return FALSE;
    }

  return TRUE;
}

static GtkTreePath *
gtk_file_system_model_get_path (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  FileModelNode *node = iter->user_data;

  GtkTreePath *result = gtk_tree_path_new ();

  while (node)
    {
      FileModelNode *parent = node->parent;
      FileModelNode *children;
      int n = 0;

      if (parent)
	children = parent->children;
      else
	children = model->roots;

      while (children != node)
	{
	  if (children->is_visible)
	    n++;
	  children = children->next;
	}
      
      gtk_tree_path_prepend_index (result, n);

      node = parent;
    }

  return result;
}

static void
gtk_file_system_model_get_value (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 gint          column,
				 GValue       *value)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  FileModelNode *node = iter->user_data;
  GFileInfo *info;
  
  switch (column)
    {
    case GTK_FILE_SYSTEM_MODEL_INFO:
      if (model->has_editable && node == model->roots)
	info = NULL;
      else
	info = file_model_node_get_info (model, node);

      g_value_set_object (value, info);
      break;
    case GTK_FILE_SYSTEM_MODEL_DISPLAY_NAME:
      {
	g_value_init (value, G_TYPE_STRING);

	if (model->has_editable && node == model->roots)
	  g_value_set_static_string (value, "");
	else
	  {
	    GFileInfo *info = file_model_node_get_info (model, node);

	    g_value_set_string (value, g_file_info_get_display_name (info));
	  }
      }
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
gtk_file_system_model_iter_next (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter)
{
  FileModelNode *node = iter->user_data;

  node = node->next;
  while (node && !node->is_visible)
    node = node->next;
  
  iter->user_data = node;

  return node != NULL;
}

static gboolean
gtk_file_system_model_iter_children (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter,
				     GtkTreeIter  *parent)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  FileModelNode *children;

  if (parent)
    {
      FileModelNode *parent_node = parent->user_data;
      children = file_model_node_get_children (model, parent_node);
    }
  else
    {
      children = model->roots;
    }

  while (children && !children->is_visible)
    children = children->next;

  iter->user_data = children;

  return children != NULL;
}

static gboolean
gtk_file_system_model_iter_has_child (GtkTreeModel *tree_model,
				      GtkTreeIter  *iter)
{
  FileModelNode *node = iter->user_data;
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);

  if (node->depth == model->max_depth)
    return FALSE;
  else
    {
      GFileInfo *info = file_model_node_get_info (model, node);
      return _gtk_file_info_consider_as_directory (info);
    }
}

static gint
gtk_file_system_model_iter_n_children (GtkTreeModel *tree_model,
				       GtkTreeIter  *iter)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  FileModelNode *children;
  gint n = 0;

  if (iter)
    {
      FileModelNode *node = iter->user_data;
      children = file_model_node_get_children (model, node);
    }
  else
    {
      children = model->roots;
    }

  while (children)
    {
      if (children->is_visible)
	n++;
      children = children->next;
    }

  return n;
}

static gboolean
gtk_file_system_model_iter_nth_child (GtkTreeModel *tree_model,
				      GtkTreeIter  *iter,
				      GtkTreeIter  *parent,
				      gint          n)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  FileModelNode *children;

  if (parent)
    {
      FileModelNode *parent_node = parent->user_data;
      children = file_model_node_get_children (model, parent_node);
    }
  else
    {
      children = model->roots;
    }

  while (children && !children->is_visible)
    children = children->next;

  while (n && children)
    {
      n--;
      children = children->next;
      while (children && !children->is_visible)
	children = children->next;
    }

  iter->user_data = children;

  return children != NULL;
}

static gboolean
gtk_file_system_model_iter_parent (GtkTreeModel *tree_model,
				   GtkTreeIter  *iter,
				   GtkTreeIter  *child)
{
  FileModelNode *node = child->user_data;
  
  node = node->parent;
  iter->user_data = node;

  return node != NULL;
}

static void
gtk_file_system_model_ref_node (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  file_model_node_ref (iter->user_data);
}

static void
gtk_file_system_model_unref_node (GtkTreeModel *tree_model,
				  GtkTreeIter  *iter)
{
  file_model_node_unref (GTK_FILE_SYSTEM_MODEL (tree_model),
			 iter->user_data);
}

static gboolean
drag_source_row_draggable (GtkTreeDragSource *drag_source,
			   GtkTreePath       *path)
{
  GtkFileSystemModel *model;
  GtkTreeIter iter;
  FileModelNode *node;

  model = GTK_FILE_SYSTEM_MODEL (drag_source);

  if (!gtk_file_system_model_get_iter (GTK_TREE_MODEL (model), &iter, path))
    return FALSE;

  if (!model->has_editable)
    return TRUE;

  node = iter.user_data;
  return (node != model->roots);
}

static gboolean
drag_source_drag_data_get (GtkTreeDragSource *drag_source,
			   GtkTreePath       *path,
			   GtkSelectionData  *selection_data)
{
  GtkFileSystemModel *model;
  GtkTreeIter iter;
  GFile *file;
  char *uris[2]; 

  model = GTK_FILE_SYSTEM_MODEL (drag_source);

  if (!gtk_file_system_model_get_iter (GTK_TREE_MODEL (model), &iter, path))
    return FALSE;

  file = _gtk_file_system_model_get_file (model, &iter);
  g_assert (file != NULL);

  uris[0] = g_file_get_uri (file);
  uris[1] = NULL;

  gtk_selection_data_set_uris (selection_data, uris);

  g_free (uris[0]);

  return TRUE;
}

/* Callback used when the root folder finished loading */
static void
root_folder_finished_loading_cb (GFile              *folder,
				 GtkFileSystemModel *model)
{
  g_signal_emit (model, file_system_model_signals[FINISHED_LOADING], 0);
}

static void
got_root_folder_cb (GCancellable *cancellable,
		    GtkFolder    *folder,
		    const GError *error,
		    gpointer      data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GtkFileSystemModel *model = data;
  GSList *tmp_list;

  tmp_list = g_slist_find (model->pending_cancellables, cancellable);
  if (!tmp_list)
    goto out;

  model->pending_cancellables = g_slist_delete_link (model->pending_cancellables,
						     tmp_list);

  if (cancelled || !folder)
    goto out;

  model->root_folder = g_object_ref (folder);

  g_signal_connect_object (model->root_folder, "finished-loading",
			   G_CALLBACK (root_folder_finished_loading_cb), model, 0);
  g_signal_connect_object (model->root_folder, "deleted",
			   G_CALLBACK (root_deleted_callback), model, 0);
  g_signal_connect_object (model->root_folder, "files-added",
			   G_CALLBACK (root_files_added_callback), model, 0);
  g_signal_connect_object (model->root_folder, "files-changed",
			   G_CALLBACK (root_files_changed_callback), model, 0);
  g_signal_connect_object (model->root_folder, "files-removed",
			   G_CALLBACK (root_files_removed_callback), model, 0);

out:
  g_object_unref (model);
  g_object_unref (cancellable);
}

/**
 * _gtk_file_system_model_new:
 * @file_system: an object implementing #GtkFileSystem
 * @root_file: the root file path to show.
 * @max_depth: the maximum depth from the children of @root_file
 *             or the roots of the file system to display in
 *             the file selector). A depth of 0 displays
 *             only the immediate children of @root_file,
 *             or the roots of the filesystem. -1 for no
 *             maximum depth.
 * @error: location to store error, or %NULL.
 *
 * Creates a new #GtkFileSystemModel object. The #GtkFileSystemModel
 * object wraps a #GtkFileSystem interface as a #GtkTreeModel.
 * Using the @root_file and @max_depth parameters, the tree model
 * can be restricted to a subportion of the entire file system.
 * 
 * Return value: the newly created #GtkFileSystemModel object, or NULL if there
 * was an error.
 **/
GtkFileSystemModel *
_gtk_file_system_model_new (GtkFileSystem     *file_system,
			    GFile             *root_file,
			    gint               max_depth,
			    const gchar       *attributes,
			    GError           **error)
{
  GtkFileSystemModel *model;
  GCancellable *cancellable;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (G_IS_FILE (root_file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* Then, actually create the model and the root nodes */

  model = g_object_new (GTK_TYPE_FILE_SYSTEM_MODEL, NULL);
  model->file_system = g_object_ref (file_system);
  if (max_depth < 0)
    model->max_depth = G_MAXUSHORT;
  else
    model->max_depth = MIN (max_depth, G_MAXUSHORT);

  model->attributes = g_strdup (attributes);
  model->root_folder = NULL;
  model->root_file = g_object_ref (root_file);

  model->roots = NULL;

  cancellable = _gtk_file_system_get_folder (file_system, root_file,
					     attributes,
					     got_root_folder_cb,
					     g_object_ref (model));
  if (!cancellable)
    {
      /* In this case got_root_folder_cb() will never be called, so we
       * need to unref model twice.
       */
      g_object_unref (model);
      g_object_unref (model);

      g_set_error_literal (error,
                           GTK_FILE_CHOOSER_ERROR,
                           GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
                           _("Could not obtain root folder"));

      return NULL;
    }

  model->pending_cancellables = g_slist_append (model->pending_cancellables, cancellable);

  return model;
}

static void
model_refilter_recurse (GtkFileSystemModel *model,
			FileModelNode      *parent,
			GtkTreePath        *path)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  int i = 0;
  FileModelNode *nodes;
  gboolean has_children = FALSE;

  if (parent && !parent->loaded)
    return;

  if (parent)
    nodes = parent->children;
  else
    nodes = model->roots;

  while (nodes)
    {
      FileModelNode *next = nodes->next;
      gboolean is_visible;
      
      gtk_tree_path_append_index (path, i);

      is_visible = file_model_node_is_visible (model, nodes);
      
      if (!is_visible && nodes->is_visible)
	{
	  file_model_node_clear (model, nodes);
	  gtk_tree_model_row_deleted (tree_model, path);

	  nodes->is_visible = FALSE;
	}
      else if (is_visible && !nodes->is_visible)
	{
	  GtkTreeIter iter;

	  iter.user_data = nodes;
	  nodes->is_visible = TRUE;
	  gtk_tree_model_row_inserted (tree_model, path, &iter);
	}
      else
	model_refilter_recurse (model, nodes, path);

      if (is_visible)
	{
	  has_children = TRUE;
	  i++;
	}
      
      gtk_tree_path_up (path);
      
      nodes = next;
    }

  if (parent && !has_children)
    {
      /* Fixme - need to insert dummy node here */
    }
}

static void
model_refilter_all (GtkFileSystemModel *model)
{
  GtkTreePath *path;

  path = gtk_tree_path_new ();
  model_refilter_recurse (model, NULL, path);
  gtk_tree_path_free (path);
}

/**
 * _gtk_file_system_model_set_show_hidden:
 * @model: a #GtkFileSystemModel
 * @show_hidden: whether hidden files should be displayed
 * 
 * Sets whether hidden files should be included in the #GtkTreeModel
 * for display.
 **/
void
_gtk_file_system_model_set_show_hidden (GtkFileSystemModel *model,
					gboolean            show_hidden)
{
  show_hidden = show_hidden != FALSE;

  if (show_hidden != model->show_hidden)
    {
      model->show_hidden = show_hidden;
      model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_set_show_folders:
 * @model: a #GtkFileSystemModel
 * @show_folders: whether folders should be displayed
 * 
 * Sets whether folders should be included in the #GtkTreeModel for
 * display.
 **/
void
_gtk_file_system_model_set_show_folders (GtkFileSystemModel *model,
					 gboolean            show_folders)
{
  show_folders = show_folders != FALSE;

  if (show_folders != model->show_folders)
    {
      model->show_folders = show_folders;
      model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_set_show_files:
 * @model: a #GtkFileSystemModel
 * @show_files: whether files (as opposed to folders) should
 *              be displayed.
 * 
 * Sets whether files (as opposed to folders) should be included
 * in the #GtkTreeModel for display.
 **/
void
_gtk_file_system_model_set_show_files (GtkFileSystemModel *model,
				       gboolean            show_files)
{
  show_files = show_files != FALSE;

  if (show_files != model->show_files)
    {
      model->show_files = show_files;
      model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_get_info:
 * @model: a #GtkFileSystemModel
 * @iter: a #GtkTreeIter pointing to a row of @model
 * 
 * Gets the #GFileInfo structure for a particular row
 * of @model. The information included in this structure
 * is determined by the @types parameter to
 * _gtk_file_system_model_new().
 * 
 * Return value: a #GFileInfo structure. This structure
 *   is owned by @model and must not be modified or freed.
 *   If you want to save the information for later use,
 *   you must make a copy, since the structure may be
 *   freed on later changes to the file system.  If you have
 *   called _gtk_file_system_model_add_editable() and the @iter
 *   corresponds to the row that this function returned, the
 *   return value will be NULL.
 **/
GFileInfo *
_gtk_file_system_model_get_info (GtkFileSystemModel *model,
				 GtkTreeIter        *iter)
{
  FileModelNode *node;

  node = iter->user_data;
  if (model->has_editable && node == model->roots)
    return NULL;
  else
    return file_model_node_get_info (model, node);
}

/**
 * _gtk_file_system_model_get_file:
 * @model: a #GtkFileSystemModel
 * @iter: a #GtkTreeIter pointing to a row of @model
 * 
 * Gets the path for a particular row in @model. 
 *
 * Return value: the file. This object is owned by @model and
 *   or freed. If you want to save the path for later use,
 *   you must take a ref, since the object may be freed
 *   on later changes to the file system.
 **/
GFile *
_gtk_file_system_model_get_file (GtkFileSystemModel *model,
				 GtkTreeIter        *iter)
{
  FileModelNode *node = iter->user_data;

  if (model->has_editable && node == model->roots)
    return NULL;

  if (node->is_dummy)
    return node->parent->file;
  else
    return node->file;
}

static void
unref_node_and_parents (GtkFileSystemModel *model,
			FileModelNode      *node)
{
  file_model_node_unref (model, node);
  if (node->parent)
    file_model_node_unref (model, node->parent);
}

static FileModelNode *
find_child_node (GtkFileSystemModel *model,
		 FileModelNode      *parent_node,
		 GFile              *file)
{
  FileModelNode *children;
  
  if (parent_node)
    children = file_model_node_get_children (model, parent_node);
  else
    children = model->roots;

  while (children)
    {
      if (children->is_visible &&
	  children->file &&
	  g_file_equal (children->file, file))
	return children;

      children = children->next;
    }

  return NULL;
}

/**
 * _gtk_file_system_model_set_filter:
 * @mode: a #GtkFileSystemModel
 * @filter: function to be called for each file
 * @user_data: data to pass to @filter
 * 
 * Sets a callback called for each file/directory to see whether
 * it should be included in model. If this function was made
 * public, we'd want to include a GDestroyNotify as well.
 **/
void
_gtk_file_system_model_set_filter (GtkFileSystemModel      *model,
				   GtkFileSystemModelFilter filter,
				   gpointer                 user_data)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  
  model->filter_func = filter;
  model->filter_data = user_data;

  model_refilter_all (model);
}


struct RefPathData
{
  GtkFileSystemModel *model;
  FileModelNode *node;
  FileModelNode *parent_node;
  GSList *files;
  GSList *cleanups;
  GtkFileSystemModelPathFunc func;
  gpointer user_data;
};

/* FIXME: maybe we have to wait on finished-loading? */
static void
ref_path_cb (GCancellable *cancellable,
	     GtkFolder    *folder,
	     const GError *error,
	     gpointer      data)
{
  struct RefPathData *info = data;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);

  if (!g_slist_find (info->model->pending_cancellables, cancellable))
    goto out;

  info->model->pending_cancellables = g_slist_remove (info->model->pending_cancellables, cancellable);

  /* Note that !folder means that the child node was already
   * found, without using get_folder.
   */
  if (cancelled || error)
    goto out;

  if (folder)
    info->cleanups = g_slist_prepend (info->cleanups, folder);
  else if ((info->files != NULL && info->files->next == NULL) /* g_slist_length == 1 */
           && g_file_equal (info->node->file, info->files->data))
    {
      /* Done, now call the function */
      if (info->node)
        {
          GtkTreeIter iter;
          GtkTreePath *path;

          iter.user_data = info->node;
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (info->model), &iter);

          (* info->func) (info->model, path, &iter, info->user_data);

          gtk_tree_path_free (path);
        }

      goto out;
    }

  info->node = find_child_node (info->model, info->parent_node, info->files->data);
  if (info->node)
    file_model_node_ref (info->node);
  else
    {
      goto out;
    }

  g_object_unref (info->files->data);
  info->files = g_slist_remove (info->files, info->files->data);

  if (info->files == NULL)
    {
      /* Done, now call the function */
      if (info->node)
        {
          GtkTreeIter iter;
          GtkTreePath *path;

          iter.user_data = info->node;
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (info->model), &iter);

          (* info->func) (info->model, path, &iter, info->user_data);

          gtk_tree_path_free (path);
        }

      goto out;
    }
  else
    {
      info->parent_node = info->node;

      if (info->parent_node->loaded)
        {
          info->node = find_child_node (info->model, info->parent_node, info->files->data);
          ref_path_cb (NULL, NULL, NULL, info);
        }
      else
        {
	  GCancellable *cancellable;

          cancellable = _gtk_file_system_get_folder (info->model->file_system,
						     info->files->data,
						     info->model->attributes,
						     ref_path_cb, data);
	  info->model->pending_cancellables =
	    g_slist_append (info->model->pending_cancellables, cancellable);
        }

      return;
    }

out:
  if (info->node)
    unref_node_and_parents (info->model, info->node);
  g_slist_foreach (info->files, (GFunc)g_object_unref, NULL);
  g_slist_free (info->files);
  g_slist_foreach (info->cleanups, (GFunc)g_object_unref, NULL);
  g_slist_free (info->cleanups);
  g_object_unref (info->model);
  g_free (info);

  g_object_unref (cancellable);
}

/**
 * _gtk_file_system_model_path_do:
 * @model: a #GtkFileSystemModel
 * @path: a path pointing to a file in the filesystem
 *       for @model.
 * @func: Function to call with the path and iter corresponding
 *        to @path.
 * @user_data: data to pass to @func
 * 
 * Locates @path within @model, referencing
 * (gtk_tree_model_ref_node()) all parent nodes,
 * calls @func passing in the path and iter for @path,
 * then unrefs all the parent nodes.
 *
 * The reason for doing this operation as a callback
 * is so that if the operation performed with the
 * path and iter results in referencing the node
 * and/or parent nodes, we don't load all the information
 * about the nodes.
 *
 * This function is particularly useful for expanding
 * a #GtkTreeView to a particular point in the file system.
 */
void
_gtk_file_system_model_path_do (GtkFileSystemModel        *model,
				GFile                     *file,
			        GtkFileSystemModelPathFunc func,
			        gpointer                   user_data)
{
  GFile *parent_file;
  GSList *files = NULL;
  FileModelNode *node;
  struct RefPathData *info;

  if (g_file_equal (file, model->root_file))
    return;

  parent_file = g_file_get_parent (file);

  if (!parent_file)
    return;

  files = g_slist_prepend (files, g_object_ref (file));
  while (!g_file_equal (parent_file, model->root_file))
    {
      files = g_slist_prepend (files, parent_file);
      parent_file = g_file_get_parent (parent_file);
      if (!parent_file)
        {
	  g_slist_foreach (files, (GFunc) g_object_unref, NULL);
	  g_slist_free (files);
	  return;
	}
    }
  g_object_unref (parent_file);

  if (files == NULL)
    return;

  /* Now we have all paths, except the root path */
  node = find_child_node (model, NULL, files->data);
  if (!node)
    {
      g_slist_foreach (files, (GFunc) g_object_unref, NULL);
      g_slist_free (files);
      return;
    }

  file_model_node_ref (node);

  g_object_unref (files->data);
  files = g_slist_remove (files, files->data);

  if (files == NULL)
    {
      /* Done, now call the function */
      if (node)
        {
          GtkTreeIter iter;
          GtkTreePath *path;

          iter.user_data = node;
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);

          (* func) (model, path, &iter, user_data);

          gtk_tree_path_free (path);
          unref_node_and_parents (model, node);
        }
    }
  else
    {
      info = g_new0 (struct RefPathData, 1);
      info->files = files;
      info->model = g_object_ref (model);
      info->func = func;
      info->user_data = user_data;
      info->node = node;

      if (info->node->loaded)
        {
	  info->parent_node = info->node;
          info->node = find_child_node (model, info->parent_node, info->files->data);
          ref_path_cb (NULL, NULL, NULL, info);
        }
      else
        {
	  GCancellable *cancellable;

          cancellable = _gtk_file_system_get_folder (model->file_system,
						     files->data,
						     model->attributes,
						     ref_path_cb, info);
	  model->pending_cancellables = g_slist_append (model->pending_cancellables, cancellable);
        }
    }
}

/**
 * _gtk_file_system_model_add_editable:
 * @model: a #GtkFileSystemModel
 * @iter: Location to return the iter corresponding to the editable row
 * 
 * Adds an "empty" row at the beginning of the model.  This does not refer to
 * any file, but is a temporary placeholder for a file name that the user will
 * type when a corresponding cell is made editable.  When your code is done
 * using this temporary row, call _gtk_file_system_model_remove_editable().
 **/
void
_gtk_file_system_model_add_editable (GtkFileSystemModel *model, GtkTreeIter *iter)
{
  FileModelNode *node;
  GtkTreePath *path;

  g_return_if_fail (!model->has_editable);

  model->has_editable = TRUE;

  node = file_model_node_new (model, NULL);
  node->is_visible = TRUE;

  node->next = model->roots;
  model->roots = node;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);
  iter->user_data = node;

  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, iter);

  gtk_tree_path_free (path);
}

/**
 * _gtk_file_system_model_remove_editable:
 * @model: a #GtkFileSystemModel
 * 
 * Removes the "empty" row at the beginning of the model that was
 * created with _gtk_file_system_model_add_editable().  You should call
 * this function when your code is finished editing this temporary row.
 **/
void
_gtk_file_system_model_remove_editable (GtkFileSystemModel *model)
{
  GtkTreePath *path;
  FileModelNode *node;

  g_return_if_fail (model->has_editable);

  model->has_editable = FALSE;

  node = model->roots;
  model->roots = model->roots->next;
  file_model_node_free (node);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);

  gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

  gtk_tree_path_free (path);
}

static FileModelNode *
file_model_node_new (GtkFileSystemModel *model,
		     GFile              *file)
{
  FileModelNode *node = g_new0 (FileModelNode, 1);

  node->model = model;
  node->file = file ? g_object_ref (file) : NULL;

  return node;
}

static void
file_model_node_free (FileModelNode *node)
{
  file_model_node_clear (node->model, node);
  
  if (node->file)
    g_object_unref (node->file);

  if (node->info)
    g_object_unref (node->info);

  g_free (node);
}

static GFileInfo *
file_model_node_get_info (GtkFileSystemModel *model,
			  FileModelNode      *node)
{
  if (!node->info)
    {
      if (node->is_dummy)
	{
	  node->info = g_file_info_new ();
	  g_file_info_set_display_name (node->info, _("(Empty)"));
	}
      else if (node->parent || model->root_folder)
	{
	  node->info = _gtk_folder_get_info ((node->parent != NULL) ? node->parent->folder : model->root_folder,
					     node->file);
	}
      else
	g_assert_not_reached ();
    }

  return node->info;
}

static gboolean
file_model_node_is_visible (GtkFileSystemModel *model,
			    FileModelNode      *node)
{
  if (model->show_folders != model->show_files ||
      !model->show_hidden ||
      model->filter_func)
    {
      GFileInfo *info = file_model_node_get_info (model, node);
      gboolean is_folder;

      if (!info)
	{
	  /* File probably disappeared underneath us or resides in a
	     directory where we have only partial access rights.  */
	  return FALSE;
	}

      is_folder = _gtk_file_info_consider_as_directory (info);

      if (model->show_folders != model->show_files &&
	  model->show_folders != is_folder)
	return FALSE;

      if (!model->show_hidden && g_file_info_get_is_hidden (info))
	return FALSE;

      if (model->filter_func &&
	  !model->filter_func (model, node->file, info, model->filter_data))
	return FALSE;
    }

  return TRUE;
}

static void
file_model_node_clear (GtkFileSystemModel *model,
		       FileModelNode      *node)
{
  FileModelNode *children;
  
  file_model_node_idle_clear_cancel (node);
  
  children = node->children;
  node->children = NULL;
  node->loaded = FALSE;
  
  while (children)
    {
      FileModelNode *next = children->next;
      
      file_model_node_free (children);
      
      children = next;
    }

  if (node->folder)
    {
      /* Unreffing node->folder may cause roots_changed,
       * so we need to be careful about ordering.
       */
      GtkFolder *folder = node->folder;
      node->folder = NULL;

      g_signal_handlers_disconnect_by_func (folder, G_CALLBACK (deleted_callback), node);
      g_signal_handlers_disconnect_by_func (folder, G_CALLBACK (files_added_callback), node);
      g_signal_handlers_disconnect_by_func (folder, G_CALLBACK (files_changed_callback), node);
      g_signal_handlers_disconnect_by_func (folder, G_CALLBACK (files_removed_callback), node);
      
      g_object_unref (folder);
    }
}

static void
file_model_node_ref (FileModelNode *node)
{
  node->ref_count++;
  if (node->ref_count == 1 && node->parent)
    node->parent->n_referenced_children++;
}

static gboolean
idle_clear_callback (GtkFileSystemModel *model)
{
  while (model->idle_clears)
    {
      FileModelNode *node = model->idle_clears->data;
      model->idle_clears = g_slist_delete_link (model->idle_clears, model->idle_clears);

      node->idle_clear = FALSE;
      file_model_node_clear (node->model, node);
    }

  return FALSE;
}
 
static void
file_model_node_idle_clear (FileModelNode *node)
{
  if (!node->idle_clear)
    {
      GtkFileSystemModel *model = node->model;

      node->idle_clear = TRUE;
      if (!model->idle_clears)
	{
	  model->idle_clear_source = g_idle_source_new ();
	  g_source_set_priority (model->idle_clear_source, G_PRIORITY_HIGH);
	  g_source_set_closure (model->idle_clear_source,
				g_cclosure_new_object (G_CALLBACK (idle_clear_callback),
						       G_OBJECT (model)));
	  g_source_attach (model->idle_clear_source, NULL);
	}

      model->idle_clears = g_slist_prepend (model->idle_clears, node);
      node->idle_clear = TRUE;
    }
}

static void
file_model_node_idle_clear_cancel (FileModelNode *node)
{
  if (node->idle_clear)
    {
      GtkFileSystemModel *model = node->model;

      model->idle_clears = g_slist_remove (model->idle_clears, node);
      if (!model->idle_clears)
	{
	  g_source_destroy (model->idle_clear_source);
	  model->idle_clear_source = NULL;
	}
      
      node->idle_clear = FALSE;
    }
}

static void
file_model_node_unref (GtkFileSystemModel *model,
		       FileModelNode       *node)
{
  node->ref_count--;
  if (node->ref_count == 0)
    {
      file_model_node_clear (model, node);
      if (node->parent)
	file_model_node_child_unref (node->parent);
    }
}

static void
file_model_node_child_unref (FileModelNode *parent)
{
  parent->n_referenced_children--;
  if (parent->n_referenced_children == 0)
    file_model_node_idle_clear (parent);
}

struct GetChildrenData
{
  GtkFileSystemModel *model;
  FileModelNode *node;
};

static void
get_children_get_folder_cb (GCancellable *cancellable,
			    GtkFolder    *folder,
			    const GError *error,
			    gpointer      callback_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct GetChildrenData *data = callback_data;
  FileModelNode *child_node;
  GSList *tmp_list;

  tmp_list = g_slist_find (data->model->pending_cancellables, cancellable);

  if (!tmp_list)
    goto out;

  data->model->pending_cancellables = g_slist_delete_link (data->model->pending_cancellables, tmp_list);

  if (cancelled || !folder)
    {
      /* error, no folder, remove dummy child */
      if (data->node->parent && data->node->parent->has_dummy)
        {
          data->node->parent->children = NULL;
          data->node->parent->has_dummy = FALSE;
	}

      file_model_node_free (data->node);

      goto out;
    }

  data->node->folder = folder;
  data->node->load_pending = FALSE;

  g_signal_connect (data->node->folder, "deleted",
		    G_CALLBACK (deleted_callback), data->node);
  g_signal_connect (data->node->folder, "files-added",
		    G_CALLBACK (files_added_callback), data->node);
  g_signal_connect (data->node->folder, "files-changed",
		    G_CALLBACK (files_changed_callback), data->node);
  g_signal_connect (data->node->folder, "files-removed",
		    G_CALLBACK (files_removed_callback), data->node);

  data->node->loaded = TRUE;

  /* We claimed this folder had children, so we
   * have to add a dummy child, possibly to remove later.
   */
  child_node = file_model_node_new (data->model, NULL);
  child_node->is_visible = TRUE;
  child_node->parent = data->node;
  child_node->is_dummy = TRUE;

  data->node->children = child_node;
  data->node->has_dummy = TRUE;

  g_object_set_data (G_OBJECT (data->node->folder), I_("model-node"), data->node);

out:
  g_object_unref (data->model);
  g_free (data);

  g_object_unref (cancellable);
}

static FileModelNode *
file_model_node_get_children (GtkFileSystemModel *model,
			      FileModelNode      *node)
{
  if (node->ref_count == 0)
    return NULL;

  if (!node->loaded && !node->load_pending)
    {
      GFileInfo *info = file_model_node_get_info (model, node);
      gboolean has_children = FALSE;
      gboolean is_folder = _gtk_file_info_consider_as_directory (info);

      file_model_node_idle_clear_cancel (node);

      if (node->depth < model->max_depth && is_folder)
        {
	  struct GetChildrenData *data;
	  GCancellable *cancellable;

	  data = g_new (struct GetChildrenData, 1);
	  data->model = g_object_ref (model);
	  data->node = node;

	  cancellable =
	    _gtk_file_system_get_folder (model->file_system,
				         node->file,
				 	 model->attributes,
				         get_children_get_folder_cb,
				         data);

	  model->pending_cancellables = g_slist_append (model->pending_cancellables, cancellable);
	  node->load_pending = TRUE;

	  if (!has_children)
	    {
	      /* The hard case ... we claimed this folder had children, but actually
	       * it didn't. We have to add a dummy child, possibly to remove later.
	       */
	      FileModelNode *child_node = file_model_node_new (model, NULL);
	      child_node->is_visible = TRUE;
	      child_node->parent = node;
	      child_node->is_dummy = TRUE;

	      node->children = child_node;
	      node->has_dummy = TRUE;
	    }
	}
    }

  return node->children;
}

static gint
file_compare_func (GFile *file1,
		   GFile *file2)
{
  gchar *uri1, *uri2;
  gint result;

  uri1 = g_file_get_uri (file1);
  uri2 = g_file_get_uri (file2);

  result = g_strcmp0 (uri1, uri2);

  g_free (uri1);
  g_free (uri2);

  return result;
}

static GSList *
sort_file_list (GSList *list)
{
  GSList *copy;

  copy = g_slist_copy (list);
  return g_slist_sort (copy, (GCompareFunc) file_compare_func);
}

static void
do_files_added (GtkFileSystemModel *model,
		FileModelNode      *parent_node,
		GSList             *files)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  FileModelNode *children;
  FileModelNode *prev = NULL;
  GtkTreeIter iter;
  GtkTreePath *path;
  GSList *sorted_files, *tmp_list;

  sorted_files = sort_file_list (files);

  if (parent_node)
    {
      iter.user_data = parent_node;
      path = gtk_tree_model_get_path (tree_model, &iter);
      children = parent_node->children;
    }
  else
    {
      path = gtk_tree_path_new ();
      children = model->roots;
    }

  gtk_tree_path_down (path);
  
  if (parent_node && parent_node->has_dummy)
    {
      prev = children;
      children = children->next;
      gtk_tree_path_next (path);
    }

  for (tmp_list = sorted_files; tmp_list; tmp_list = tmp_list->next)
    {
      GFile *file = tmp_list->data;

      while (children &&
	     (!children->file || !g_file_equal (children->file, file)))
	{
	  prev = children;
	  if (children->is_visible)
	    gtk_tree_path_next (path);
	  
	  children = children->next;
	}
  
      if (children &&
	  children->file && g_file_equal (children->file, file))
	{
	  /* Shouldn't happen */
	}
      else
	{
	  FileModelNode *new;
	  
	  new = file_model_node_new (model, file);
	  
	  if (children)
	    new->next = children;
	  if (prev)
	    prev->next = new;
	  else if (parent_node)
	    parent_node->children = new;
	  else
	    model->roots = new;

	  prev = new;
	  
	  if (parent_node)
	    {
	      new->parent = parent_node;
	      new->depth = parent_node->depth + 1;
	    }
	  
	  new->is_visible = file_model_node_is_visible (model, new);
	  
	  if (new->is_visible)
	    {
	      iter.user_data = new;
	      gtk_tree_path_free (path);
	      path = gtk_tree_model_get_path (tree_model, &iter);
	      gtk_tree_model_row_inserted (tree_model, path, &iter);
	      
	      if (gtk_file_system_model_iter_has_child (tree_model, &iter))
		gtk_tree_model_row_has_child_toggled (tree_model, path, &iter);
	      
	      if (parent_node && parent_node->has_dummy)
		{
		  FileModelNode *dummy = parent_node->children;
		  GtkTreePath *dummy_path;
		  
		  parent_node->children = parent_node->children->next;
		  parent_node->has_dummy = FALSE;

		  dummy_path = gtk_tree_path_copy (path);
		  gtk_tree_path_up (dummy_path);
		  gtk_tree_path_down (dummy_path);
		  
		  gtk_tree_model_row_deleted (tree_model, dummy_path);
		  gtk_tree_path_free (dummy_path);

		  if (dummy->ref_count)
		    file_model_node_child_unref (parent_node);
		  file_model_node_free (dummy);
		}
	      
	      gtk_tree_path_next (path);
	    }
	}
    }

  gtk_tree_path_free (path);
  g_slist_free (sorted_files);
}

static void
do_files_changed (GtkFileSystemModel *model,
		  FileModelNode      *parent_node,
		  GSList             *files)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  FileModelNode *children;
  GtkTreeIter iter;
  GtkTreePath *path;
  GSList *sorted_files;
  GSList *tmp_list;

  sorted_files = sort_file_list (files);

  if (parent_node)
    {
      iter.user_data = parent_node;
      path = gtk_tree_model_get_path (tree_model, &iter);
      children = parent_node->children;
    }
  else
    {
      path = gtk_tree_path_new ();
      children = model->roots;
    }

  gtk_tree_path_down (path);
  
  if (parent_node && parent_node->has_dummy)
    {
      children = children->next;
      gtk_tree_path_next (path);
    }

  for (tmp_list = sorted_files; tmp_list; tmp_list = tmp_list->next)
    {
      GFile *file = tmp_list->data;

      while (children &&
	     (!children->file || !g_file_equal (children->file, file)))
	{
	  if (children->is_visible)
	    gtk_tree_path_next (path);
	  
	  children = children->next;
	}
  
      if (children &&
	  children->file && g_file_equal (children->file, file))
	{
	  gtk_tree_model_row_changed (tree_model, path, &iter);
	}
      else
	{
	  /* Shouldn't happen */
	}
    }

  gtk_tree_path_free (path);
  g_slist_free (sorted_files);
}

static void
do_files_removed (GtkFileSystemModel *model,
		  FileModelNode      *parent_node,
		  GSList             *files)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  FileModelNode *children;
  FileModelNode *prev = NULL;
  GtkTreeIter iter;
  GtkTreePath *path;
  GSList *sorted_files;
  GSList *tmp_list;
  FileModelNode *tmp_child;
  gint n_visible;

  sorted_files = sort_file_list (files);

  if (parent_node)
    {
      iter.user_data = parent_node;
      path = gtk_tree_model_get_path (tree_model, &iter);
      children = parent_node->children;
    }
  else
    {
      path = gtk_tree_path_new ();
      children = model->roots;
    }

  /* Count the number of currently visible children, so that
   * can catch when we need to insert a dummy node.
   */
  n_visible = 0;
  for (tmp_child = children; tmp_child; tmp_child = tmp_child->next)
    {
      if (tmp_child->is_visible)
	n_visible++;
    }

  gtk_tree_path_down (path);
  
  if (parent_node && parent_node->has_dummy)
    {
      prev = children;
      children = children->next;
      gtk_tree_path_next (path);
    }

  for (tmp_list = sorted_files; tmp_list; tmp_list = tmp_list->next)
    {
      GFile *file = tmp_list->data;

      while (children &&
	     (!children->file || !g_file_equal (children->file, file)))
	{
	  prev = children;
	  if (children->is_visible)
	    gtk_tree_path_next (path);
	  
	  children = children->next;
	}
  
      if (children &&
	  children->file && g_file_equal (children->file, file))
	{
	  FileModelNode *next = children->next;

	  if (children->is_visible)
	    n_visible--;
	  
	  if (parent_node && n_visible == 0)
	    {
	      FileModelNode *dummy = file_model_node_new (model, NULL);
	      dummy->is_visible = TRUE;
	      dummy->parent = parent_node;
	      dummy->is_dummy = TRUE;

	      parent_node->children = dummy;
	      parent_node->has_dummy = TRUE;

	      iter.user_data = dummy;
	      gtk_tree_model_row_inserted (tree_model, path, &iter);
	      gtk_tree_path_next (path);

	      prev = dummy;
	    }
	  
	  if (prev)
	    prev->next = next;
	  else if (parent_node)
	    parent_node->children = next;
	  else
	    model->roots = next;

	  if (parent_node && children->ref_count)
	    file_model_node_child_unref (parent_node);
	      
	  if (children->is_visible)
	    gtk_tree_model_row_deleted (tree_model, path);

	  file_model_node_free (children);

	  children = next;
	}
      else
	{
	  /* Shouldn't happen */
	}
    }

  gtk_tree_path_free (path);
  g_slist_free (sorted_files);
}

static void
deleted_callback (GFile         *folder,
		  FileModelNode *node)
{
}

static void
files_added_callback (GFile         *folder,
		      GSList        *files,
		      FileModelNode *node)
{
  do_files_added (node->model, node, files);
}

static void
files_changed_callback (GFile         *folder,
			GSList        *files,
			FileModelNode *node)
{
  do_files_changed (node->model, node, files);
}

static void
files_removed_callback (GFile         *folder,
			GSList        *files,
			FileModelNode *node)
{
  do_files_removed (node->model, node, files);
}

static void
root_deleted_callback (GFile              *folder,
		       GtkFileSystemModel *model)
{
}

static void
root_files_added_callback (GFile              *folder,
			   GSList             *files,
			   GtkFileSystemModel *model)
{
  do_files_added (model, NULL, files);
}

static void
root_files_changed_callback (GFile              *folder,
			     GSList             *files,
			     GtkFileSystemModel *model)
{
  do_files_changed (model, NULL, files);
}

static void
root_files_removed_callback (GFile              *folder,
			     GSList             *files,
			     GtkFileSystemModel *model)
{
  do_files_removed (model, NULL, files);
}
