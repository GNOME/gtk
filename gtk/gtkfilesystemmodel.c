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

#include "gtkfilesystemmodel.h"
#include "gtkfilesystem.h"
#include <gtk/gtktreemodel.h>
#include <string.h>

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

  GSource *dummy_idle;
  GSList *dummy_idle_nodes;

  gushort max_depth;
  
  guint show_hidden : 1;
  guint show_folders : 1;
  guint show_files : 1;
  guint folders_only : 1;
};

struct _FileModelNode
{
  gchar *uri;
  FileModelNode *next;

  GtkFileInfo *info;
  GtkFileFolder *folder;
  
  FileModelNode *children;
  FileModelNode *parent;

  guint ref_count;

  gushort depth;

  guint loaded : 1;
  guint has_dummy : 1;
  guint is_visible : 1;
  guint has_children : 1;
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

static void queue_dummy_idle   (GtkFileSystemModel *model,
				FileModelNode      *node);
static void unqueue_dummy_idle (GtkFileSystemModel *model,
				FileModelNode      *node);

static FileModelNode *file_model_node_new        (const gchar        *uri);
static void           file_model_node_free       (FileModelNode      *node);
static void           file_model_node_ref        (FileModelNode      *node);
static void           file_model_node_unref      (GtkFileSystemModel *model,
						  FileModelNode      *node);

static const GtkFileInfo *file_model_node_get_info     (GtkFileSystemModel *model,
							FileModelNode      *node);
static gboolean           file_model_node_is_visible   (GtkFileSystemModel *model,
							FileModelNode      *node);
static void               file_model_node_clear        (GtkFileSystemModel *model,
							FileModelNode      *node);
static FileModelNode *    file_model_node_get_children (GtkFileSystemModel *model,
							FileModelNode      *node);

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
    case GTK_FILE_SYSTEM_MODEL_URI:
      return G_TYPE_STRING;
    case GTK_FILE_SYSTEM_MODEL_INFO:
      return GTK_TYPE_FILE_INFO;
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
    case GTK_FILE_SYSTEM_MODEL_URI:
      g_value_set_string (value, node->uri);
      break;
    case GTK_FILE_SYSTEM_MODEL_INFO:
      g_value_set_boxed (value, file_model_node_get_info (model, node));
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

  /* We never want to go into a directory just to
   * find out if it has children
   */
  if (node->loaded)
    return node->has_children;
  else
    {
      GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (tree_model);

      if (node->depth == model->max_depth)
	return FALSE;
      else
	{
	  const GtkFileInfo *info = file_model_node_get_info (model, node);
	  return gtk_file_info_get_is_folder (info);
	}
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
 * @root_uri: the URI of root of the file system to display,
 *            or %NULL to display starting from the
 *            root or roots of the fielsystem.
 * @max_depth: the maximum depth from the children of @root_uri
 *             or the roots of the file system to display in
 *             the file selector). A depth of 0 displays
 *             only the immediate children of @root_uri,
 *             or the roots of the filesystem. -1 for no
 *             maximum depth.
 * @types: a bitmask indicating the types of information
 *         that is desired about the files. This will
 *         determine what information is returned by
 *         _gtk_file_system_model_get_info().
 *
 * Creates a new #GtkFileSystemModel object. The #GtkFileSystemModel
 * object wraps a #GtkFileSystem interface as a #GtkTreeModel.
 * Using the @root_uri and @max_depth parameters, the tree model
 * can be restricted to a subportion of the entire file system.
 * 
 * Return value: the newly created #GtkFileSystemModel object.
 **/
GtkFileSystemModel *
_gtk_file_system_model_new (GtkFileSystem  *file_system,
			    const gchar    *root_uri,
			    gint            max_depth,
			    GtkFileInfoType types)
{
  GtkFileSystemModel *model;
  GSList *roots, *tmp_list;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);

  model = g_object_new (GTK_TYPE_FILE_SYSTEM_MODEL, NULL);
  model->file_system = g_object_ref (file_system);
  if (max_depth < 0)
    model->max_depth = G_MAXUSHORT;
  else
    model->max_depth = MIN (max_depth, G_MAXUSHORT);
  model->types = types | GTK_FILE_INFO_IS_FOLDER | GTK_FILE_INFO_IS_HIDDEN;

  if (root_uri)
    {
      GSList *child_uris;
      
      model->root_folder = gtk_file_system_get_folder (file_system, root_uri,
						       model->types,
						       NULL);   /* NULL-GError */

      if (model->root_folder &&
	  gtk_file_folder_list_children (model->root_folder,
					 &child_uris,
					 NULL)) /* NULL-GError */
	roots = child_uris;
    }
  else
    roots = gtk_file_system_list_roots (file_system);
  
  for (tmp_list = roots; tmp_list; tmp_list = tmp_list->next)
    {
      FileModelNode *node = file_model_node_new (tmp_list->data);
      g_free (tmp_list->data);
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

  if (parent && (has_children != parent->has_children))
    {
      GtkTreeIter iter;

      parent->has_children = has_children;

      iter.user_data = parent;
      gtk_tree_model_row_has_child_toggled (tree_model, path, &iter);
    }
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
      GtkTreePath *path;

      model->show_hidden = show_hidden;

      path = gtk_tree_path_new ();
      model_refilter_recurse (model, NULL, path);
      gtk_tree_path_free (path);
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
      GtkTreePath *path;

      model->show_folders = show_folders;

      path = gtk_tree_path_new ();
      model_refilter_recurse (model, NULL, path);
      gtk_tree_path_free (path);
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
      GtkTreePath *path;

      model->show_files = show_files;

      path = gtk_tree_path_new ();
      model_refilter_recurse (model, NULL, path);
      gtk_tree_path_free (path);
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
 * _gtk_file_system_model_get_uri:
 * @model: a #GtkFileSystemModel
 * @iter: a #GtkTreeIter pointing to a row of @model
 * 
 * Gets the URI for a particular row in @model. 
 *
 * Return value: the URI. This string is owned by @model and
 *   or freed. If you want to save the URI for later use,
 *   you must make a copy, since the string may be freed
 *   on later changes to the file system.
 **/
const gchar *
_gtk_file_system_model_get_uri (GtkFileSystemModel *model,
				GtkTreeIter        *iter)
{
  FileModelNode *node = iter->user_data;

  return node->uri;
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
find_and_ref_uri (GtkFileSystemModel *model,
		  const gchar        *uri)
{
  gchar *parent_uri;
  FileModelNode *parent_node;
  FileModelNode *children;
  
  if (!gtk_file_system_get_parent (model->file_system, uri, &parent_uri, NULL))
    return NULL;

  if (parent_uri)
    {
      parent_node = find_and_ref_uri (model, parent_uri);
      g_free (parent_uri);

      if (!parent_node)
	return NULL;
    }
  else
    parent_node = NULL;

  if (parent_node)
    children = file_model_node_get_children (model, parent_node);
  else
    children = model->roots;

  while (children)
    {
      if (children->is_visible &&
	  strcmp (children->uri, uri) == 0)
	{
	  file_model_node_ref (children);
	  return children;
	}

      children = children->next;
    }

  if (parent_node)
    unref_node_and_parents (model, parent_node);

  return FALSE;
}

/**
 * _gtk_file_system_model_uri_do:
 * @model: a #GtkFileSystemModel
 * @uri: a URI pointing to a file in the filesystem
 *       for @model.
 * @func: Function to call with the path and iter corresponding
 *        to @uri.
 * @user_data: data to pass to @func
 * 
 * Locates @uri within @model, referencing
 * (gtk_tree_model_ref_node ()) all parent nodes,
 * calls @func passing in the path and iter for @uri,
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
 * Return value: %TRUE if the URI was successfully
 *  found in @model and @func was called.
 **/
gboolean
_gtk_file_system_model_uri_do (GtkFileSystemModel       *model,
			       const gchar              *uri,
			       GtkFileSystemModelURIFunc func,
			       gpointer                  user_data)
{
  FileModelNode *node = find_and_ref_uri (model, uri);

  if (node)
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      iter.user_data = node;
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);

      (*func) (model, path, &iter, user_data);

      gtk_tree_path_free (path);
      unref_node_and_parents (model, node);

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
dummy_idle_callback (GtkFileSystemModel *model)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (model);
  GSList *tmp_list;

  for (tmp_list = model->dummy_idle_nodes; tmp_list; tmp_list = tmp_list->next)
    {
      GtkTreeIter iter;
      GtkTreePath *path;
      
      FileModelNode *node = tmp_list->data;
      g_assert (node->children && !node->children->next && !node->children->children);

      iter.user_data = node->children;
      path = gtk_tree_model_get_path (tree_model, &iter);
      
      if (node->children->ref_count)
	gtk_tree_model_row_deleted (tree_model, path);

      gtk_tree_path_up (path);
      iter.user_data = node;
      gtk_tree_model_row_has_child_toggled (tree_model, path, &iter);

      gtk_tree_path_free (path);

      file_model_node_free (node->children);
      node->children = NULL;
      node->has_children = FALSE;
      node->has_dummy = FALSE;
    }

  model->dummy_idle_nodes = FALSE;
  model->dummy_idle = NULL;

  return FALSE;
}

static void
queue_dummy_idle (GtkFileSystemModel *model,
		  FileModelNode      *node)
{
  model->dummy_idle_nodes = g_slist_prepend (model->dummy_idle_nodes, node);

  if (!model->dummy_idle)
    {
      model->dummy_idle = g_idle_source_new ();
      g_source_set_priority (model->dummy_idle, G_PRIORITY_HIGH_IDLE);
      g_source_set_closure (model->dummy_idle,
			    g_cclosure_new_object (G_CALLBACK (dummy_idle_callback), G_OBJECT (model)));
      g_source_attach (model->dummy_idle, NULL);
    }
}

static void
unqueue_dummy_idle (GtkFileSystemModel *model,
		    FileModelNode      *node)
{
  model->dummy_idle_nodes = g_slist_remove (model->dummy_idle_nodes, node);
  
  if (!model->dummy_idle_nodes)
    g_source_destroy (model->dummy_idle);
}

static FileModelNode *
file_model_node_new (const gchar *uri)
{
  FileModelNode *node = g_new0 (FileModelNode, 1);

  node->uri = g_strdup (uri);

  return node;
}

static void
file_model_node_free (FileModelNode *node)
{
  if (node->children)
    {
      FileModelNode *children;
      
      for (children = node->children; children; children = children->next)
	file_model_node_free (children);
    }
  
  if (node->uri)
    g_free (node->uri);

  if (node->info)
    gtk_file_info_free (node->info);

  if (node->folder)
    g_object_unref (node->folder);

  g_free (node);
}

static const GtkFileInfo *
file_model_node_get_info (GtkFileSystemModel *model,
			  FileModelNode      *node)
{
  if (!node->info)
    {
      if (node->parent && node->parent->has_dummy)
	{
	  node->info = gtk_file_info_new ();
	  gtk_file_info_set_display_name (node->info, "Loading...");
	}
      else if (node->parent || model->root_folder)
	{
	  node->info = gtk_file_folder_get_info (node->parent ? node->parent->folder : model->root_folder,
						 node->uri,
						 NULL);	/* NULL-GError */
	}
      else
	{
	  node->info = gtk_file_system_get_root_info (model->file_system,
						      node->uri,
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

      return TRUE;
    }
}

static void
file_model_node_clear (GtkFileSystemModel *model,
		       FileModelNode      *node)
{
  FileModelNode *children;
  
  if (node->has_dummy)
    unqueue_dummy_idle (model, node);
  
  if (node->folder)
    {
      g_object_unref (node->folder);
      node->folder = NULL;
    }
  
  children = node->children;
  node->children = NULL;
  node->has_children = FALSE;
  node->loaded = FALSE;
  
  while (children)
    {
      FileModelNode *next = children->next;
      
      file_model_node_clear (model, children);
      file_model_node_free (children);
      
      children = next;
    }

  node->ref_count = 0;
}

static void
file_model_node_ref (FileModelNode *node)
{
  node->ref_count++;
}

static void
file_model_node_unref (GtkFileSystemModel *model,
		       FileModelNode       *node)
{
  node->ref_count--;
  if (node->ref_count == 0)
    file_model_node_clear (model, node);
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
      
      if (is_folder)
	node->folder = gtk_file_system_get_folder (model->file_system,
						   node->uri,
						   model->types,
						   NULL);	/* NULL-GError */

      if (node->folder)
	{
	  GSList *child_uris, *tmp_list;
	  
	  if (gtk_file_folder_list_children (node->folder, &child_uris, NULL)) /* NULL-GError */
	    {
	      for (tmp_list = child_uris; tmp_list; tmp_list = tmp_list->next)
		{
		  FileModelNode *child_node = file_model_node_new (tmp_list->data);
		  g_free (tmp_list->data);
		  child_node->next = node->children;
		  child_node->parent = node;
		  child_node->depth = node->depth + 1;
		  child_node->is_visible = file_model_node_is_visible (model, child_node);
		  if (child_node->is_visible)
		    has_children = TRUE;
		  node->children = child_node;
		}
	      g_slist_free (child_uris);
	    }

	  node->children = (FileModelNode *)g_slist_reverse ((GSList *)node->children);
	}
      
      node->has_children = has_children;

      if (is_folder && !node->has_children)
	{
	  /* The hard case ... we claimed this folder had children, but actually
	   * it didn't. We have to add a dummy child, then remove it later
	   */
	  FileModelNode *child_node = file_model_node_new ("***dummy***");
	  child_node->is_visible = TRUE;
	  child_node->parent = node;

	  node->children = child_node;
	  node->has_children = TRUE;
	  node->has_dummy = TRUE;

	  queue_dummy_idle (model, node);
	}

      node->loaded = TRUE;
    }

  return node->children;
}
