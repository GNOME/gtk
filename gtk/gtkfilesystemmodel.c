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

#include <string.h>

#include "gtkfilesystemmodel.h"
#include "gtkfilesystem.h"
#include "gtkintl.h"
#include "gtktreemodel.h"

typedef struct _GtkFileSystemModelClass GtkFileSystemModelClass;
typedef struct _FileModelNode           FileModelNode;

#define GTK_FILE_SYSTEM_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_SYSTEM_MODEL, GtkFileSystemModelClass))
#define GTK_IS_FILE_SYSTEM_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_SYSTEM_MODEL))
#define GTK_FILE_SYSTEM_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_SYSTEM_MODEL, GtkFileSystemModelClass))

struct _GtkFileSystemModelClass
{
  GObjectClass parent_class;
};

struct _GtkFileSystemModel
{
  GObject parent_instance;

  GtkFileSystem  *file_system;
  GtkFileInfoType types;
  FileModelNode  *roots;
  GtkFileFolder  *root_folder;

  GtkFileSystemModelFilter filter_func;
  gpointer filter_data;

  GSList *idle_clears;
  GSource *idle_clear_source;

  gushort max_depth;
  
  guint show_hidden : 1;
  guint show_folders : 1;
  guint show_files : 1;
  guint folders_only : 1;
};

struct _FileModelNode
{
  GtkFilePath *path;
  FileModelNode *next;

  GtkFileInfo *info;
  GtkFileFolder *folder;
  
  FileModelNode *children;
  FileModelNode *parent;
  GtkFileSystemModel *model;

  guint ref_count;
  guint n_referenced_children;

  gushort depth;

  guint has_dummy : 1;
  guint is_dummy : 1;
  guint is_visible : 1;
  guint loaded : 1;
  guint idle_clear : 1;
};

static void gtk_file_system_model_class_init   (GtkFileSystemModelClass *class);
static void gtk_file_system_model_iface_init   (GtkTreeModelIface       *iface);
static void gtk_file_system_model_init         (GtkFileSystemModel      *model);
static void gtk_file_system_model_finalize     (GObject                 *object);

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

static FileModelNode *file_model_node_new        (GtkFileSystemModel *model,
						  const GtkFilePath  *path);
static void           file_model_node_free       (FileModelNode      *node);
static void           file_model_node_ref        (FileModelNode      *node);
static void           file_model_node_unref      (GtkFileSystemModel *model,
						  FileModelNode      *node);

static void file_model_node_idle_clear        (FileModelNode *node);
static void file_model_node_idle_clear_cancel (FileModelNode *node);
static void file_model_node_child_unref       (FileModelNode *parent);

static const GtkFileInfo *file_model_node_get_info     (GtkFileSystemModel *model,
							FileModelNode      *node);
static gboolean           file_model_node_is_visible   (GtkFileSystemModel *model,
							FileModelNode      *node);
static void               file_model_node_clear        (GtkFileSystemModel *model,
							FileModelNode      *node);
static FileModelNode *    file_model_node_get_children (GtkFileSystemModel *model,
							FileModelNode      *node);

static void roots_changed_callback (GtkFileSystem      *file_system,
				    GtkFileSystemModel *model);
				    
static void deleted_callback       (GtkFileFolder *folder,
				    FileModelNode *node);
static void files_added_callback   (GtkFileFolder *folder,
				    GSList        *paths,
				    FileModelNode *node);
static void files_changed_callback (GtkFileFolder *folder,
				    GSList        *paths,
				    FileModelNode *node);
static void files_removed_callback (GtkFileFolder *folder,
				    GSList        *paths,
				    FileModelNode *node);

static void root_deleted_callback       (GtkFileFolder      *folder,
					 GtkFileSystemModel *model);
static void root_files_added_callback   (GtkFileFolder      *folder,
					 GSList             *paths,
					 GtkFileSystemModel *model);
static void root_files_changed_callback (GtkFileFolder      *folder,
					 GSList             *paths,
					 GtkFileSystemModel *model);
static void root_files_removed_callback (GtkFileFolder      *folder,
					 GSList             *paths,
					 GtkFileSystemModel *model);

GType
_gtk_file_system_model_get_type (void)
{
  static GType file_system_model_type = 0;

  if (!file_system_model_type)
    {
      static const GTypeInfo file_system_model_info =
      {
	sizeof (GtkFileSystemModelClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_system_model_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileSystemModel),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_system_model_init,
      };
      
      static const GInterfaceInfo file_system_info =
      {
	(GInterfaceInitFunc) gtk_file_system_model_iface_init, /* interface_init */
	NULL,			                              /* interface_finalize */
	NULL			                              /* interface_data */
      };

      file_system_model_type = g_type_register_static (G_TYPE_OBJECT,
						      "GtkFileSystemModel",
						      &file_system_model_info, 0);
      g_type_add_interface_static (file_system_model_type,
				   GTK_TYPE_TREE_MODEL,
				   &file_system_info);
    }

  return file_system_model_type;
}

static void
gtk_file_system_model_class_init (GtkFileSystemModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  
  gobject_class->finalize = gtk_file_system_model_finalize;
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
gtk_file_system_model_init (GtkFileSystemModel *model)
{
  model->show_files = TRUE;
  model->show_folders = TRUE;
  model->show_hidden = FALSE;
}

static void
gtk_file_system_model_finalize (GObject *object)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (object);
  FileModelNode *children;

  if (model->root_folder)
    g_object_unref (model->root_folder);

  children = model->roots;
  while (children)
    {
      file_model_node_free (children);
      children = children->next;
    }
}

/*
 * ******************** GtkTreeModel methods ********************
 */

static GtkTreeModelFlags
gtk_file_system_model_get_flags (GtkTreeModel *tree_model)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);
  GtkTreeModelFlags flags = GTK_TREE_MODEL_ITERS_PERSIST;

  if (model->max_depth == 1)
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
      return GTK_TYPE_FILE_INFO; 
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
  
  switch (column)
    {
    case GTK_FILE_SYSTEM_MODEL_INFO:
      g_value_init (value, GTK_TYPE_FILE_INFO);
      g_value_set_boxed (value, file_model_node_get_info (model, node));
      break;
    case GTK_FILE_SYSTEM_MODEL_DISPLAY_NAME:
      {
	const GtkFileInfo *info = file_model_node_get_info (model, node);
	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, gtk_file_info_get_display_name (info));
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
      const GtkFileInfo *info = file_model_node_get_info (model, node);
      return gtk_file_info_get_is_folder (info);
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

/**
 * _gtk_file_system_model_new:
 * @file_system: an object implementing #GtkFileSystem
 * @root_path: the path of root of the file system to display,
 *            or %NULL to display starting from the
 *            root or roots of the fielsystem.
 * @max_depth: the maximum depth from the children of @root_path
 *             or the roots of the file system to display in
 *             the file selector). A depth of 0 displays
 *             only the immediate children of @root_path,
 *             or the roots of the filesystem. -1 for no
 *             maximum depth.
 * @types: a bitmask indicating the types of information
 *         that is desired about the files. This will
 *         determine what information is returned by
 *         _gtk_file_system_model_get_info().
 *
 * Creates a new #GtkFileSystemModel object. The #GtkFileSystemModel
 * object wraps a #GtkFileSystem interface as a #GtkTreeModel.
 * Using the @root_path and @max_depth parameters, the tree model
 * can be restricted to a subportion of the entire file system.
 * 
 * Return value: the newly created #GtkFileSystemModel object.
 **/
GtkFileSystemModel *
_gtk_file_system_model_new (GtkFileSystem     *file_system,
			    const GtkFilePath *root_path,
			    gint               max_depth,
			    GtkFileInfoType    types)
{
  GtkFileSystemModel *model;
  GSList *roots = NULL;
  GSList *tmp_list;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);

  model = g_object_new (GTK_TYPE_FILE_SYSTEM_MODEL, NULL);
  model->file_system = g_object_ref (file_system);
  if (max_depth < 0)
    model->max_depth = G_MAXUSHORT;
  else
    model->max_depth = MIN (max_depth, G_MAXUSHORT);
  model->types = types | GTK_FILE_INFO_IS_FOLDER | GTK_FILE_INFO_IS_HIDDEN;

  if (root_path)
    {
      GSList *child_paths;
      
      model->root_folder = gtk_file_system_get_folder (file_system, root_path,
						       model->types,
						       NULL);   /* NULL-GError */

      if (model->root_folder &&
	  gtk_file_folder_list_children (model->root_folder,
					 &child_paths,
					 NULL)) /* NULL-GError */
	{
	  roots = child_paths;
	  
	  g_signal_connect_object (model->root_folder, "deleted",
				   G_CALLBACK (root_deleted_callback), model, 0);
	  g_signal_connect_object (model->root_folder, "files-added",
				   G_CALLBACK (root_files_added_callback), model, 0);
	  g_signal_connect_object (model->root_folder, "files-changed",
				   G_CALLBACK (root_files_changed_callback), model, 0);
	  g_signal_connect_object (model->root_folder, "files-removed",
				   G_CALLBACK (root_files_removed_callback), model, 0);
	}
    }
  else
    {
      roots = gtk_file_system_list_roots (file_system);
      g_signal_connect_object (file_system, "roots-changed",
			       G_CALLBACK (roots_changed_callback), model, 0);
    }

  roots = gtk_file_paths_sort (roots);
  
  for (tmp_list = roots; tmp_list; tmp_list = tmp_list->next)
    {
      FileModelNode *node = file_model_node_new (model, tmp_list->data);
      gtk_file_path_free (tmp_list->data);
      node->is_visible = file_model_node_is_visible (model, node);
      node->next = model->roots;
      node->depth = 0;
      model->roots = node;
    }
  g_slist_free (roots);

  model->roots = (FileModelNode *)g_slist_reverse ((GSList *)model->roots);
  
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
	  gtk_tree_model_row_inserted (tree_model, path, &iter);

	  nodes->is_visible = TRUE;
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
 * Gets the #GtkFileInfo structure for a particular row
 * of @model. The information included in this structure
 * is determined by the @types parameter to
 * _gtk_file_system_model_new().
 * 
 * Return value: a #GtkFileInfo structure. This structure
 *   is owned by @model and must not be modified or freed.
 *   If you want to save the information for later use,
 *   you must make a copy, since the structure may be
 *   freed on later changes to the file system.
 **/
const GtkFileInfo *
_gtk_file_system_model_get_info (GtkFileSystemModel *model,
				 GtkTreeIter        *iter)
{
  return file_model_node_get_info (model, iter->user_data);
}

/**
 * _gtk_file_system_model_get_path:
 * @model: a #GtkFileSystemModel
 * @iter: a #GtkTreeIter pointing to a row of @model
 * 
 * Gets the path for a particular row in @model. 
 *
 * Return value: the path. This string is owned by @model and
 *   or freed. If you want to save the path for later use,
 *   you must make a copy, since the string may be freed
 *   on later changes to the file system.
 **/
const GtkFilePath *
_gtk_file_system_model_get_path (GtkFileSystemModel *model,
				 GtkTreeIter        *iter)
{
  FileModelNode *node = iter->user_data;

  if (node->is_dummy)
    return node->parent->path;
  else
    return node->path;
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
		 const GtkFilePath  *path)
{
  FileModelNode *children;
  
  if (parent_node)
    children = file_model_node_get_children (model, parent_node);
  else
    children = model->roots;

  while (children)
    {
      if (children->is_visible &&
	  children->path &&
	  gtk_file_path_compare (children->path, path) == 0)
	return children;

      children = children->next;
    }

  return NULL;
}
		 

static FileModelNode *
find_and_ref_path (GtkFileSystemModel  *model,
		   const GtkFilePath   *path,
		   GSList             **cleanups)
{
  GtkFilePath *parent_path;
  FileModelNode *parent_node;
  FileModelNode *child_node;
  GtkFileFolder *folder;

  if (!gtk_file_system_get_parent (model->file_system, path, &parent_path, NULL))
    return NULL;

  if (parent_path)
    {
      parent_node = find_and_ref_path (model, parent_path, cleanups);
      gtk_file_path_free (parent_path);

      if (!parent_node)
	return NULL;
    }
  else
    parent_node = NULL;

  child_node = find_child_node (model, parent_node, path);
  if (child_node)
    {
      file_model_node_ref (child_node);
      return child_node;
    }

  folder = gtk_file_system_get_folder (model->file_system,
				       path,
				       model->types,
				       NULL);	/* NULL-GError */
  if (folder)
    {
      *cleanups = g_slist_prepend (*cleanups, folder);

      child_node = find_child_node (model, parent_node, path);
      if (child_node)
	{
	  file_model_node_ref (child_node);
	  return child_node;
	}
    }

  if (parent_node)
    unref_node_and_parents (model, parent_node);

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
 * (gtk_tree_model_ref_node ()) all parent nodes,
 * calls @func passing in the path and iter for @path,
 * then unrefs all the parent nodes.
 *
 * The reason for doing this operation as a callback
 * is so that if the operation performed with the the
 * path and iter results in referencing the the node
 * and/or parent nodes, we don't load all the information
 * about the nodes.
 *
 * This function is particularly useful for expanding
 * a #GtkTreeView to a particular point in the file system.
 * 
 * Return value: %TRUE if the path was successfully
 *  found in @model and @func was called.
 **/
gboolean
_gtk_file_system_model_path_do (GtkFileSystemModel       *model,
			       const GtkFilePath         *path,
			       GtkFileSystemModelPathFunc func,
			       gpointer                   user_data)
{
  GSList *cleanups = NULL;
  FileModelNode *node = find_and_ref_path (model, path, &cleanups);

  if (node)
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      iter.user_data = node;
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);

      (*func) (model, path, &iter, user_data);

      gtk_tree_path_free (path);
      unref_node_and_parents (model, node);
    }

  g_slist_foreach (cleanups, (GFunc)g_object_unref, NULL);
  g_slist_free (cleanups);

  return node != NULL;
}

static FileModelNode *
file_model_node_new (GtkFileSystemModel *model,
		     const GtkFilePath  *path)
{
  FileModelNode *node = g_new0 (FileModelNode, 1);

  node->model = model;
  node->path = path ? gtk_file_path_copy (path) : NULL;

  return node;
}

static void
file_model_node_free (FileModelNode *node)
{
  file_model_node_clear (node->model, node);
  
  if (node->path)
    gtk_file_path_free (node->path);

  if (node->info)
    gtk_file_info_free (node->info);

  g_free (node);
}

static const GtkFileInfo *
file_model_node_get_info (GtkFileSystemModel *model,
			  FileModelNode      *node)
{
  if (!node->info)
    {
      if (node->is_dummy)
	{
	  node->info = gtk_file_info_new ();
	  gtk_file_info_set_display_name (node->info, _("(Empty)"));
	}
      else if (node->parent || model->root_folder)
	{
	  node->info = gtk_file_folder_get_info (node->parent ? node->parent->folder : model->root_folder,
						 node->path,
						 NULL);	/* NULL-GError */
	}
      else
	{
	  node->info = gtk_file_system_get_root_info (model->file_system,
						      node->path,
						      model->types,
						      NULL);  /* NULL-GError */
	}
    }

  return node->info;
}

static gboolean
file_model_node_is_visible (GtkFileSystemModel *model,
			    FileModelNode      *node)
{
  if (model->show_hidden && model->show_folders && model->show_files)
    return TRUE;
  else
    {
      const GtkFileInfo *info = file_model_node_get_info (model, node);
      gboolean is_folder = gtk_file_info_get_is_folder (info);

      if (!model->show_folders && is_folder)
	return FALSE;
      if (!model->show_files && !is_folder)
	return FALSE;
      if (!model->show_hidden && gtk_file_info_get_is_hidden (info))
	return FALSE;
      if (model->filter_func && !model->filter_func (model, node->path, info, model->filter_data))
	return FALSE;

      return TRUE;
    }
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
      GtkFileFolder *folder = node->folder;
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

static FileModelNode *
file_model_node_get_children (GtkFileSystemModel *model,
			      FileModelNode      *node)
{
  if (node->ref_count == 0)
    return NULL;

  if (!node->loaded)
    {
      const GtkFileInfo *info = file_model_node_get_info (model, node);
      gboolean has_children = FALSE;
      gboolean is_folder = node->depth < model->max_depth && gtk_file_info_get_is_folder (info);

      file_model_node_idle_clear_cancel (node);

      if (is_folder)
	node->folder = gtk_file_system_get_folder (model->file_system,
						   node->path,
						   model->types,
						   NULL);	/* NULL-GError */

      if (node->folder)
	{
	  GSList *child_paths, *tmp_list;
	  
	  if (gtk_file_folder_list_children (node->folder, &child_paths, NULL)) /* NULL-GError */
	    {
	      child_paths = gtk_file_paths_sort (child_paths);

	      for (tmp_list = child_paths; tmp_list; tmp_list = tmp_list->next)
		{
		  FileModelNode *child_node = file_model_node_new (model, tmp_list->data);
		  gtk_file_path_free (tmp_list->data);
		  child_node->next = node->children;
		  child_node->parent = node;
		  child_node->depth = node->depth + 1;
		  child_node->is_visible = file_model_node_is_visible (model, child_node);
		  if (child_node->is_visible)
		    has_children = TRUE;
		  node->children = child_node;
		}
	      g_slist_free (child_paths);
	    }

	  node->children = (FileModelNode *)g_slist_reverse ((GSList *)node->children);

	  g_signal_connect (node->folder, "deleted",
			    G_CALLBACK (deleted_callback), node);
	  g_signal_connect (node->folder, "files-added",
			    G_CALLBACK (files_added_callback), node);
	  g_signal_connect (node->folder, "files-changed",
			    G_CALLBACK (files_changed_callback), node);
	  g_signal_connect (node->folder, "files-removed",
			    G_CALLBACK (files_removed_callback), node);

	  g_object_set_data (G_OBJECT (node->folder), "model-node", node);
	}
      
      if (is_folder && !has_children)
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

      node->loaded = TRUE;
    }

  return node->children;
}

static void
do_files_added (GtkFileSystemModel *model,
		FileModelNode      *parent_node,
		GSList             *paths)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  FileModelNode *children;
  FileModelNode *prev = NULL;
  GtkTreeIter iter;
  GtkTreePath *path;
  GSList *sorted_paths;
  GSList *tmp_list;

  sorted_paths = gtk_file_paths_sort (g_slist_copy (paths));
  
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

  for (tmp_list = sorted_paths; tmp_list; tmp_list = tmp_list->next)
    {
      const GtkFilePath *file_path = tmp_list->data;
      
      while (children &&
	     (!children->path || gtk_file_path_compare (children->path, file_path) < 0))
	{
	  prev = children;
	  if (children->is_visible)
	    gtk_tree_path_next (path);
	  
	  children = children->next;
	}
  
      if (children &&
	  children->path && gtk_file_path_compare (children->path, file_path) == 0)
	{
	  /* Shouldn't happen */
	}
      else
	{
	  FileModelNode *new;
	  
	  new = file_model_node_new (model, file_path);
	  
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
  g_slist_free (sorted_paths);
}

static void
do_files_changed (GtkFileSystemModel *model,
		  FileModelNode      *parent_node,
		  GSList             *paths)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  FileModelNode *children;
  FileModelNode *prev = NULL;
  GtkTreeIter iter;
  GtkTreePath *path;
  GSList *sorted_paths;
  GSList *tmp_list;

  sorted_paths = gtk_file_paths_sort (g_slist_copy (paths));
  
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

  for (tmp_list = sorted_paths; tmp_list; tmp_list = tmp_list->next)
    {
      const GtkFilePath *file_path = tmp_list->data;
      
      while (children &&
	     (!children->path || gtk_file_path_compare (children->path, file_path) < 0))
	{
	  prev = children;
	  if (children->is_visible)
	    gtk_tree_path_next (path);
	  
	  children = children->next;
	}
  
      if (children &&
	  children->path && gtk_file_path_compare (children->path, file_path) == 0)
	{
	  gtk_tree_model_row_changed (tree_model, path, &iter);
	}
      else
	{
	  /* Shouldn't happen */
	}
    }

  gtk_tree_path_free (path);
  g_slist_free (sorted_paths);
}

static void
do_files_removed (GtkFileSystemModel *model,
		  FileModelNode      *parent_node,
		  GSList             *paths)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  FileModelNode *children;
  FileModelNode *prev = NULL;
  GtkTreeIter iter;
  GtkTreePath *path;
  GSList *sorted_paths;
  GSList *tmp_list;
  FileModelNode *tmp_child;
  gint n_visible;

  sorted_paths = gtk_file_paths_sort (g_slist_copy (paths));
  
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

  for (tmp_list = sorted_paths; tmp_list; tmp_list = tmp_list->next)
    {
      const GtkFilePath *file_path = tmp_list->data;
      
      while (children &&
	     (!children->path || gtk_file_path_compare (children->path, file_path) < 0))
	{
	  prev = children;
	  if (children->is_visible)
	    gtk_tree_path_next (path);
	  
	  children = children->next;
	}
  
      if (children &&
	  children->path && gtk_file_path_compare (children->path, file_path) == 0)
	{
	  FileModelNode *next = children->next;

	  if (children->is_visible)
	    n_visible--;
	  
	  if (n_visible == 0)
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
  g_slist_free (sorted_paths);
}

static void
roots_changed_callback (GtkFileSystem      *file_system,
			GtkFileSystemModel *model)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  GSList *new_roots;
  GSList *tmp_list;
  FileModelNode *children;
  FileModelNode *prev = NULL;
  GtkTreePath *path;
      
  new_roots = gtk_file_system_list_roots (file_system);
  new_roots = gtk_file_paths_sort (new_roots);

  children = model->roots;
  tmp_list = new_roots;
  path = gtk_tree_path_new ();
  gtk_tree_path_down (path);

  while (children || tmp_list)
    {
      FileModelNode *next = NULL;
      int cmp;

      if (tmp_list && children)
	cmp = gtk_file_path_compare (children->path, tmp_list->data);
      else if (children)
	cmp = -1;
      else
	cmp = 1;

      if (cmp < 0)
	{
	  next = children->next;
	  
	  if (prev)
	    prev->next = children->next;
	  else
	    model->roots = children->next;

	  if (children->is_visible)
	    gtk_tree_model_row_deleted (tree_model, path);

	  file_model_node_free (children);
	}
      else if (cmp == 0)
	{
	  /* Already there
	   */
	  next = children->next;
	  prev = children;
	  if (children->is_visible)
	    gtk_tree_path_next (path);
	}
      else 
	{
	  GtkTreeIter iter;
	  FileModelNode *node = file_model_node_new (model, tmp_list->data);
	  node->is_visible = file_model_node_is_visible (model, node);
	  node->next = children;
	  node->depth = 0;

	  if (prev)
	    prev->next = node;
	  else
	    model->roots = node;

	  if (node->is_visible)
	    {
	      iter.user_data = node;
	      gtk_tree_model_row_inserted (tree_model, path, &iter);

	      if (gtk_file_system_model_iter_has_child (tree_model, &iter))
		gtk_tree_model_row_has_child_toggled (tree_model, path, &iter);
	      
	      gtk_tree_path_next (path);
	    }
	  
	  prev = node;
	}

      if (cmp <= 0)
	{
	  children = next;
	}

      if (cmp >= 0)
	{
	  gtk_file_path_free (tmp_list->data);
	  tmp_list = tmp_list->next;
	}
    }
  
  g_slist_free (new_roots);
  gtk_tree_path_free (path);
}

static void
deleted_callback (GtkFileFolder      *folder,
		  FileModelNode      *node)
{
}

static void
files_added_callback (GtkFileFolder      *folder,
		      GSList             *paths,
		      FileModelNode      *node)
{
  do_files_added (node->model, node, paths);
}

static void
files_changed_callback (GtkFileFolder      *folder,
			GSList             *paths,
			FileModelNode      *node)
{
  do_files_changed (node->model, node, paths);
}

static void
files_removed_callback (GtkFileFolder      *folder,
			GSList             *paths,
			FileModelNode      *node)
{
  do_files_removed (node->model, node, paths);
}

static void
root_deleted_callback (GtkFileFolder      *folder,
		       GtkFileSystemModel *model)
{
}

static void
root_files_added_callback (GtkFileFolder      *folder,
			   GSList             *paths,
			   GtkFileSystemModel *model)
{
  do_files_added (model, NULL, paths);
}

static void
root_files_changed_callback (GtkFileFolder      *folder,
			     GSList             *paths,
			     GtkFileSystemModel *model)
{
  do_files_changed (model, NULL, paths);
}

static void
root_files_removed_callback (GtkFileFolder      *folder,
			     GSList             *paths,
			     GtkFileSystemModel *model)
{
  do_files_removed (model, NULL, paths);
}
