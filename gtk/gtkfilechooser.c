/* GTK - The GIMP Toolkit
 * gtkfilechooser.c: Abstract interface for file selector GUIs
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

#include "gtkfilechooser.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserenums.h"
#include "gtkfilesystem.h"

#define _(str) (str)

static void gtk_file_chooser_class_init (gpointer g_iface);

static GtkFilePath *gtk_file_chooser_get_path         (GtkFileChooser *chooser);

GType
gtk_file_chooser_get_type (void)
{
  static GType file_chooser_type = 0;

  if (!file_chooser_type)
    {
      static const GTypeInfo file_chooser_info =
      {
	sizeof (GtkFileChooserIface),  /* class_size */
	NULL,                          /* base_init */
	NULL,			       /* base_finalize */
	(GClassInitFunc)gtk_file_chooser_class_init, /* class_init */
      };

      file_chooser_type = g_type_register_static (G_TYPE_INTERFACE,
						  "GtkFileChooser",
						  &file_chooser_info, 0);

      g_type_interface_add_prerequisite (file_chooser_type, GTK_TYPE_WIDGET);
    }

  return file_chooser_type;
}

static void
gtk_file_chooser_class_init (gpointer g_iface)
{
  GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);

  g_signal_new ("current-folder-changed",
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, current_folder_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
  g_signal_new ("selection-changed",
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
  g_signal_new ("update-preview",
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, update_preview),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
  
  g_object_interface_install_property (g_iface,
				       g_param_spec_enum ("action",
							  _("Action"),
							  _("The type of operation that the file selector is performing"),
							  GTK_TYPE_FILE_CHOOSER_ACTION,
							  GTK_FILE_CHOOSER_ACTION_OPEN,
							  G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_object ("file-system",
							    _("File System"),
							    _("File system object to use"),
							    GTK_TYPE_FILE_SYSTEM,
							    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_interface_install_property (g_iface,
				       g_param_spec_object ("filter",
							    _("Filter"),
							    _("The current filter for selecting which files are displayed"),
							    GTK_TYPE_FILE_FILTER,
							    G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("folder-mode",
							     _("Folder Mode"),
							     _("Whether to select folders rather than files"),
							     FALSE,
							     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("local-only",
							     _("Local Only"),
							     _("Whether the selected file(s) should be limited to local file: URLs"),
							     TRUE,
							     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_object ("preview-widget",
							    _("Preview widget"),
							    _("Application supplied widget for custom previews."),
							    GTK_TYPE_WIDGET,
							    G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("preview-widget-active",
							     _("Preview Widget Active"),
							     _("Whether the application supplied widget for custom previews should be shown."),
							     TRUE,
							     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_object ("extra-widget",
							    _("Extra widget"),
							    _("Application supplied widget for extra options."),
							    GTK_TYPE_WIDGET,
							    G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("select-multiple",
							     _("Select Multiple"),
							     _("Whether to allow multiple files to be selected"),
							     FALSE,
							     G_PARAM_READWRITE));
  
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("show-hidden",
							     _("Show Hidden"),
							     _("Whether the hidden files and folders should be displayed"),
							     FALSE,
							     G_PARAM_READWRITE));
}

/**
 * gtk_file_chooser_set_action:
 * @chooser: a #GtkFileChooser
 * @action: the action that the file selector is performing
 * 
 * Sets the type of operation that that the chooser is performing; the
 * user interface is adapted to suit the selected action. For example,
 * an option to create a new folder might be shown if the action is
 * %GTK_FILE_CHOOSER_ACTION_SAVE but not if the action is
 * %GTK_FILE_CHOOSER_ACTION_OPEN.
 **/
void
gtk_file_chooser_set_action (GtkFileChooser       *chooser,
			     GtkFileChooserAction  action)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "action", action, NULL);
}

/**
 * gtk_file_chooser_get_action:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the type of operation that the file chooser is performing; see
 * gtk_file_chooser_set_action().
 * 
 * Return value: the action that the file selector is performing
 **/
GtkFileChooserAction
gtk_file_chooser_get_action (GtkFileChooser *chooser)
{
  GtkFileChooserAction action;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "action", &action, NULL);

  return action;
}

/**
 * gtk_file_chooser_set_folder_mode:
 * @chooser: a #GtkFileChooser
 * @folder_mode: %TRUE if the file chooser is used to select folders
 *               rather than files.
 * 
 * Sets whether the file chooser is used to select folders
 * rather than files. If in folder mode, only folders are displayed
 * to the use, and not the individual files inside the folders
 * and the user selects a single folder rather than one or
 * more files.
 **/
void
gtk_file_chooser_set_folder_mode (GtkFileChooser *chooser,
				  gboolean        folder_mode)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "folder-mode", folder_mode, NULL);
}

/**
 * gtk_file_chooser_get_folder_mode:
 * @chooser: a #GtkFileChooser
 * 
 * Gets whether the file chooser is used to select folders
 * rather than files. See gtk_file_chooser_set_folder_mode()
 * 
 * Return value: %TRUE if the file chooser is used to select
                 folders rather than files.
 **/
gboolean
gtk_file_chooser_get_folder_mode (GtkFileChooser *chooser)
{
  gboolean folder_mode;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "folder-mode", &folder_mode, NULL);

  return folder_mode;
}

/**
 * gtk_file_chooser_set_local_only:
 * @chooser: a #GtkFileChooser
 * @local_only: %TRUE if only local files can be selected
 * 
 * Sets whether only local files can be selected in the
 * file selector. If @local_only is %TRUE (the default),
 * then the selected file are files are guaranteed to be
 * accessible through the operating systems native file
 * file system and therefore the application only
 * needs to worry about the filename functions in
 * #GtkFileChooser, like gtk_file_chooser_get_filename(),
 * rather than the URI functions like
 * gtk_file_chooser_get_uri(),
 **/
void
gtk_file_chooser_set_local_only (GtkFileChooser *chooser,
				 gboolean        local_only)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "local-only", local_only, NULL);
}

/**
 * gtk_file_chooser_get_local_only:
 * @chooser: a #GtkFileChoosre
 * 
 * Gets whether only local files can be selected in the
 * file selector. See gtk_file_chooser_set_local_only()
 * 
 * Return value: %TRUE if only local files can be selected.
 **/
gboolean
gtk_file_chooser_get_local_only (GtkFileChooser *chooser)
{
  gboolean local_only;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "local-only", &local_only, NULL);

  return local_only;
}

/**
 * gtk_file_chooser_set_select_multiple:
 * @chooser: a #GtkFileChooser
 * @select_multiple: %TRUE if multiple files can be selected.
 * 
 * Sets whether multiple files can be selected in the file
 * selector. If the file selector if in folder mode (see
 * gtk_file_selector_set_folder_mode()) then only one folder
 * can be selected, without regard to this setting.
 **/
void
gtk_file_chooser_set_select_multiple (GtkFileChooser *chooser,
				      gboolean        select_multiple)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "select-multiple", select_multiple, NULL);
}

/**
 * gtk_file_chooser_get_select_multiple:
 * @chooser: a #GtkFileChooser
 * 
 * Gets whether multiple files can be selected in the file
 * selector. See gtk_file_chooser_set_select_multiple().
 * 
 * Return value: %TRUE if multiple files can be selected.
 **/
gboolean
gtk_file_chooser_get_select_multiple (GtkFileChooser *chooser)
{
  gboolean select_multiple;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "select-multiple", &select_multiple, NULL);

  return select_multiple;
}

/**
 * gtk_file_chooser_get_filename:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the filename for the currently selected file in
 * the file selector. If multiple files are selected,
 * one of the filenames will be returned at random.
 * 
 * Return value: The currently selected filename, or %NULL
 *  if no file is selected, or the selected file can't
 *  be represented with a local filename. Free with g_free().
 **/
gchar *
gtk_file_chooser_get_filename (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  path = gtk_file_chooser_get_path (chooser);
  if (path)
    {
      result = gtk_file_system_path_to_filename (file_system, path);
      gtk_file_path_free (path);
    }

  return result;
}

/**
 * gtk_file_chooser_set_filename:
 * @chooser: a #GtkFileChooser
 * @filename: the filename to set as current
 * 
 * Sets @filename as the current filename for the the file chooser;
 * If the file name isn't in the current folder of @chooser, then the
 * current folder of @chooser will be changed to the folder containing
 * @filename. This is equivalent to a sequence of
 * gtk_file_chooser_unselect_all() followed by gtk_file_chooser_select_filename().
 *
 * Note that the file must exist, or nothing will be done except
 * for the directory change. To pre-enter a filename for the user, as in
 * a save-as dialog, use gtk_file_chooser_set_current_name()
 **/
void
gtk_file_chooser_set_filename (GtkFileChooser *chooser,
			       const gchar    *filename)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  gtk_file_chooser_unselect_all (chooser);
  gtk_file_chooser_select_filename (chooser, filename);
}

/**
 * gtk_file_chooser_select_filename:
 * @chooser: a #GtkFileChooser
 * @filename: the filename to select
 * 
 * Selects a filename. If the file name isn't in the current
 * folder of @chooser, then the current folder of @chooser will
 * be changed to the folder containing @filename.
 **/
void
gtk_file_chooser_select_filename (GtkFileChooser *chooser,
				  const gchar    *filename)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (filename != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_filename_to_path (file_system, filename);
  if (path)
    {
      _gtk_file_chooser_select_path (chooser, path);
      gtk_file_path_free (path);
    }
}

/**
 * gtk_file_chooser_unselect_filename:
 * @chooser: a #GtkFileChooser
 * @filename: the filename to unselect
 * 
 * Unselects a currently selected filename. If the filename
 * is not in the current directory, does not exist, or
 * is otherwise not currently selected, does nothing.
 **/
void
gtk_file_chooser_unselect_filename (GtkFileChooser *chooser,
				    const char     *filename)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (filename != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_filename_to_path (file_system, filename);
  if (path)
    {
      _gtk_file_chooser_unselect_path (chooser, path);
      gtk_file_path_free (path);
    }
}

/**
 * gtk_file_chooser_get_filenames:
 * @chooser: a #GtkFileChooser
 * 
 * Lists all the files and subfolders in the current folder of
 * @chooser. The returned names are full absolute paths. If files
 * in the current folder cannot be represented as local filenames
 * they will be ignored. (See gtk_file_chooser_get_uris())
 * 
 * Return value: a #GList containing the filenames of all
 *   files and subfolders in the current folder. Free the returned list
 *   with g_lists_free(), and the filenames with g_free().
 **/
GSList *
gtk_file_chooser_get_filenames (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GSList *paths;
  GSList *tmp_list;
  GSList *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  paths = _gtk_file_chooser_get_paths (chooser);

  for (tmp_list = paths; tmp_list; tmp_list = tmp_list->next)
    {
      gchar *filename = gtk_file_system_path_to_filename (file_system, tmp_list->data);
      if (filename)
	result = g_slist_prepend (result, filename);
    }

  gtk_file_paths_free (paths);

  return g_slist_reverse (result);
}

/**
 * gtk_file_chooser_set_current_folder:
 * @chooser: a #GtkFileChooser
 * @filename: the full path of the new current folder
 * 
 * Sets the current folder for @chooser from a local filename.
 * The user will be shown the full contents of the current folder,
 * plus user interface elements for navigating to other folders.
 **/
void
gtk_file_chooser_set_current_folder (GtkFileChooser *chooser,
				     const gchar    *filename)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (filename != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_filename_to_path (file_system, filename);
  if (path)
    {
      _gtk_file_chooser_set_current_folder_path (chooser, path);
      gtk_file_path_free (path);
    }
}

/**
 * gtk_file_chooser_get_current_folder:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current folder of @chooser as a local filename.
 * See gtk_file_chooser_set_current_folder().
 * 
 * Return value: the full path of the current folder, or %NULL
 *  if the current path cannot be represented as a local filename.
 *  Free with g_free().
 **/
gchar *
gtk_file_chooser_get_current_folder (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *filename;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = _gtk_file_chooser_get_current_folder_path (chooser);
  filename = gtk_file_system_path_to_filename (file_system, path);
  gtk_file_path_free (path);

  return filename;
}

/**
 * gtk_file_chooser_set_current_name:
 * @chooser: a #GtkFileChooser
 * @name: the filename to use, as a UTF-8 string
 * 
 * Sets the current name in the file selector, as if entered
 * by the user. Note that the name passed in here is a UTF-8
 * string rather than a filename. This function is meant for
 * such uses as a suggested name in a "Save As..." dialog.
 *
 * If you want to preselect a particular existing file, you
 * should use gtk_file_chooser_set_filename() instead.
 **/
void
gtk_file_chooser_set_current_name  (GtkFileChooser *chooser,
				    const gchar    *name)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (name != NULL);
  
  GTK_FILE_CHOOSER_GET_IFACE (chooser)->set_current_name (chooser, name);
}

/**
 * gtk_file_chooser_get_uri:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the URI for the currently selected file in
 * the file selector. If multiple files are selected,
 * one of the filenames will be returned at random.
 * 
 * Return value: The currently selected URI, or %NULL
 *  if no file is selected. Free with g_free()
 **/
gchar *
gtk_file_chooser_get_uri (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  path = gtk_file_chooser_get_path (chooser);
  if (path)
    {
      result = gtk_file_system_path_to_uri (file_system, path);
      gtk_file_path_free (path);
    }

  return result;
}

/**
 * gtk_file_chooser_set_uri:
 * @chooser: a #GtkFileChooser
 * @uri: the URI to set as current
 * 
 * Sets the file referred to by @uri as the current file for the the
 * file chooser; If the file name isn't in the current folder of @chooser,
 * then the current folder of @chooser will be changed to the folder containing
 * @uri. This is equivalent to a sequence of gtk_file_chooser_unselect_all()
 * followed by gtk_file_chooser_select_uri().
 *
 * Note that the file must exist, or nothing will be done except
 * for the directory change. To pre-enter a filename for the user, as in
 * a save-as dialog, use gtk_file_chooser_set_current_name()
 **/
void
gtk_file_chooser_set_uri (GtkFileChooser *chooser,
			  const char     *uri)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  gtk_file_chooser_unselect_all (chooser);
  gtk_file_chooser_select_uri (chooser, uri);
}

/**
 * gtk_file_chooser_select_uri:
 * @chooser: a #GtkFileChooser
 * @uri: the URI to select
 * 
 * Selects the file to by @uri. If the URI doesn't refer to a
 * file in the current folder of @chooser, then the current folder of
 * @chooser will be changed to the folder containing @filename.
 **/
void
gtk_file_chooser_select_uri (GtkFileChooser *chooser,
			     const char     *uri)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (uri != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_uri_to_path (file_system, uri);
  if (path)
    {
      _gtk_file_chooser_select_path (chooser, path);
      gtk_file_path_free (path);
    }
}

/**
 * gtk_file_chooser_unselect_uri:
 * @chooser: a #GtkFileChooser
 * @uri: the URI to unselect
 * 
 * Unselects the file referred to by @uri. If the file
 * is not in the current directory, does not exist, or
 * is otherwise not currently selected, does nothing.
 **/
void
gtk_file_chooser_unselect_uri (GtkFileChooser *chooser,
			       const char     *uri)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (uri != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_uri_to_path (file_system, uri);
  if (path)
    {
      _gtk_file_chooser_unselect_path (chooser, path);
      gtk_file_path_free (path);
    }
}

void
gtk_file_chooser_select_all (GtkFileChooser *chooser)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_all (chooser);
}

void
gtk_file_chooser_unselect_all (GtkFileChooser *chooser)
{

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_all (chooser);
}

/**
 * gtk_file_chooser_get_filenames:
 * @chooser: a #GtkFileChooser
 * 
 * Lists all the files and subfolders in the current folder of
 * @chooser. The returned names are full absolute URIs.
 * 
 * Return value: a #GList containing the URIs of all
 *   files and subfolders in the current folder. Free the returned list
 *   with g_lists_free(), and the filenames with g_free().
 **/
GSList *
gtk_file_chooser_get_uris (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GSList *paths;
  GSList *tmp_list;
  GSList *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  paths = _gtk_file_chooser_get_paths (chooser);

  for (tmp_list = paths; tmp_list; tmp_list = tmp_list->next)
    {
      gchar *uri = gtk_file_system_path_to_uri (file_system, tmp_list->data);
      if (uri)
	result = g_slist_prepend (result, uri);
    }

  gtk_file_paths_free (paths);

  return g_slist_reverse (result);
}

/**
 * gtk_file_chooser_set_current_folder_uri:
 * @chooser: a #GtkFileChooser
 * @uri: the URI for the new current folder
 * 
 * Sets the current folder for @chooser from an URI.
 * The user will be shown the full contents of the current folder,
 * plus user interface elements for navigating to other folders.
 **/
void
gtk_file_chooser_set_current_folder_uri (GtkFileChooser *chooser,
					 const gchar    *uri)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (uri != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_uri_to_path (file_system, uri);
  if (path)
    {
      _gtk_file_chooser_set_current_folder_path (chooser, path);
      gtk_file_path_free (path);
    }
}

/**
 * gtk_file_chooser_get_current_folder_uri:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current folder of @chooser as an URI.
 * See gtk_file_chooser_set_current_folder_uri().
 * 
 * Return value: the URI for the current folder.
 *  Free with g_free().
 */
gchar *
gtk_file_chooser_get_current_folder_uri (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *uri;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = _gtk_file_chooser_get_current_folder_path (chooser);
  uri = gtk_file_system_path_to_uri (file_system, path);
  gtk_file_path_free (path);

  return uri;
}

/**
 * _gtk_file_chooser_set_current_folder_path:
 * @chooser: a #GtkFileChooser
 * @path: the #GtkFilePath for the new folder
 * 
 * Sets the current folder for @chooser from a #GtkFilePath.
 * Internal function, see gtk_file_chooser_set_current_folder_uri().
 **/
void
_gtk_file_chooser_set_current_folder_path (GtkFileChooser    *chooser,
					   const GtkFilePath *path)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (path != NULL);

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->set_current_folder (chooser, path);
}

/**
 * _gtk_file_chooser_get_current_folder_path:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current folder of @chooser as #GtkFilePath.
 * See gtk_file_chooser_get_current_folder_uri().
 * 
 * Return value: the #GtkFilePath for the current folder.
 * Fre with  gtk_file_path_free ().
 */
GtkFilePath *
_gtk_file_chooser_get_current_folder_path (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_current_folder (chooser);  
}

/**
 * _gtk_file_chooser_select_path:
 * @chooser: a #GtkFileChooser
 * @path: the path to select
 * 
 * Selects the file referred to by @path. An internal function. See
 * _gtk_file_chooser_select_uri().
 **/
void
_gtk_file_chooser_select_path (GtkFileChooser    *chooser,
			       const GtkFilePath *path)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_path (chooser, path);
}

/**
 * _gtk_file_chooser_unselect_path:
 * @chooser: a #GtkFileChooser
 * @path: the filename to path
 * 
 * Unselects the file referred to by @path. An internal
 * function. See _gtk_file_chooser_unselect_uri().
 **/
void
_gtk_file_chooser_unselect_path (GtkFileChooser    *chooser,
				 const GtkFilePath *path)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_path (chooser, path);
}

/**
 * _gtk_file_chooser_get_paths:
 * @chooser: a #GtkFileChooser
 * 
 * Lists all the files and subfolders in the current folder of
 * @chooser as #GtkFilePath. An internal function, see
 * gtk_file_chooser_get_uris().
 * 
 * Return value: a #GList containing a #GtkFilePath for each
 *   files and subfolder in the current folder. Free the returned list
 *   with g_lists_free(), and the paths with gtk_file_path_free().
 **/
GSList *
_gtk_file_chooser_get_paths (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_paths (chooser);
}

static GtkFilePath *
gtk_file_chooser_get_path (GtkFileChooser *chooser)
{
  GSList *list;
  GtkFilePath *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  list = _gtk_file_chooser_get_paths (chooser);
  if (list)
    {
      result = list->data;
      list = g_slist_delete_link (list, list);
      gtk_file_paths_free (list);
    }

  return result;
}

/**
 * _gtk_file_chooser_get_file_system:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the #GtkFileSystem of @chooser; this is an internal
 * implementation detail, used for conversion between paths
 * and filenames and URIs.
 * 
 * Return value: the file system for @chooser.
 **/
GtkFileSystem *
_gtk_file_chooser_get_file_system (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_file_system (chooser);
}

/* Preview widget
 */
/**
 * gtk_file_chooser_set_preview_widget:
 * @chooser: a #GtkFileChooser
 * @preview_widget: widget for displaying preview.
 *
 * Sets an application-supplied widget to use to display a custom preview
 * of the currently selected file. To implement a preview, after setting the
 * preview widget, you connect to the ::selection-changed
 * signal, and call gtk_file_chooser_get_preview_filename() or
 * gtk_file_chooser_get_preview_uri() on each change. If you can
 * display a preview of the new file, update your widget
 * and set the preview active using gtk_file_chooser_set_preview_widget_active().
 * Otherwise, set the preview inactive.
 *
 * When there is application-supplied preview widget, or the application-supplied
 * preview widget is not active, the file chooser may display an internally
 * generated preview of the current file or it may display no preview at all.
 **/
void
gtk_file_chooser_set_preview_widget (GtkFileChooser *chooser,
				     GtkWidget      *preview_widget)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "preview-widget", preview_widget, NULL);
}

/**
 * gtk_file_chooser_get_preview_widget:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current preview widget; see
 * gtk_file_chooser_set_preview_widget().
 * 
 * Return value: the current preview widget, or %NULL
 **/
GtkWidget *
gtk_file_chooser_get_preview_widget (GtkFileChooser *chooser)
{
  GtkWidget *preview_widget;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  g_object_get (chooser, "preview-widget", &preview_widget, NULL);
  
  /* Horrid hack; g_object_get() refs returned objects but
   * that contradicts the memory management conventions
   * for accessors.
   */
  if (preview_widget)
    g_object_unref (preview_widget);

  return preview_widget;
}

/**
 * gtk_file_chooser_set_preview_widget_active:
 * @chooser: a #GtkFileChooser
 * @active: whether to display the user-specified preview widget
 * 
 * Sets whether the preview widget set by
 * gtk_file_chooser_set_preview_widget_active() should be shown for the
 * current filename. When @active is set to false, the file chooser
 * may display an internally generated preview of the current file
 * or it may display no preview at all. See
 * gtk_file_chooser_set_preview_widget() for more details.
 **/
void
gtk_file_chooser_set_preview_widget_active (GtkFileChooser *chooser,
					    gboolean        active)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  g_object_set (chooser, "preview-widget-active", active, NULL);
}

/**
 * gtk_file_chooser_get_preview_widget_active:
 * @chooser: a #GtkFileChooser
 * 
 * Gets whether the preview widget set by
 * gtk_file_chooser_set_preview_widget_active() should be shown for the
 * current filename. See gtk_file_chooser_set_preview_widget_active().
 * 
 * Return value: %TRUE if the preview widget is active for the
 *  current filename.
 **/
gboolean
gtk_file_chooser_get_preview_widget_active (GtkFileChooser *chooser)
{
  gboolean active;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "preview-widget-active", &active, NULL);

  return active;
}

/**
 * gtk_file_chooser_get_preview_filename:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the filename that should be previewed in a custom preview
 * Internal function, see gtk_file_chooser_get_preview_uri().n
 * 
 * Return value: the #GtkFilePath for the file to preview, or %NULL if no file
 *  is selected. Free with gtk_file_path_free().
 **/
GtkFilePath *
_gtk_file_chooser_get_preview_path (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_preview_path (chooser);
}

/**
 * gtk_file_chooser_get_preview_filename:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the filename that should be previewed in a custom preview
 * widget. See gtk_file_chooser_set_preview_widget().
 * 
 * Return value: the filename to preview, or %NULL if no file
 *  is selected, or if the selected file cannot be represented
 *  as a local filename. Free with g_free()
 **/
char *
gtk_file_chooser_get_preview_filename (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  path = _gtk_file_chooser_get_preview_path (chooser);
  if (path)
    {
      result = gtk_file_system_path_to_filename (file_system, path);
      gtk_file_path_free (path);
    }

  return result;
}

/**
 * gtk_file_chooser_get_preview_filename:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the URI that should be previewed in a custom preview
 * widget. See gtk_file_chooser_set_preview_widget().
 * 
 * Return value: the URI for the file to preview, or %NULL if no file
 *  is selected. Free with g_free().
 **/
char *
gtk_file_chooser_get_preview_uri (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  path = _gtk_file_chooser_get_preview_path (chooser);
  if (path)
    {
      result = gtk_file_system_path_to_uri (file_system, path);
      gtk_file_path_free (path);
    }

  return result;
}

/**
 * gtk_file_chooser_set_extra_widget:
 * @chooser: a #GtkFileChooser
 * @extra_widget: widget for extra options
 * 
 * Sets an application-supplied widget to provide extra options to the user.
 **/
void
gtk_file_chooser_set_extra_widget (GtkFileChooser *chooser,
				   GtkWidget      *extra_widget)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "extra-widget", extra_widget, NULL);
}

/**
 * gtk_file_chooser_get_extra_widget:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current preview widget; see
 * gtk_file_chooser_set_extra_widget().
 * 
 * Return value: the current extra widget, or %NULL
 **/
GtkWidget *
gtk_file_chooser_get_extra_widget (GtkFileChooser *chooser)
{
  GtkWidget *extra_widget;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  g_object_get (chooser, "extra-widget", &extra_widget, NULL);
  
  /* Horrid hack; g_object_get() refs returned objects but
   * that contradicts the memory management conventions
   * for accessors.
   */
  if (extra_widget)
    g_object_unref (extra_widget);

  return extra_widget;
}

/**
 * gtk_file_chooser_add_filter:
 * @chooser: a #GtkFileChooser
 * @filter: a #GtkFileFilter
 * 
 * Adds @filter to the list of filters that the user can select between.
 * When a filter is selected, only files that are passed by that
 * filter are displayed.
 **/
void
gtk_file_chooser_add_filter (GtkFileChooser *chooser,
			     GtkFileFilter  *filter)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_filter (chooser, filter);
}

/**
 * gtk_file_chooser_add_filter:
 * @chooser: a #GtkFileChooser
 * @filter: a #GtkFileFilter
 * 
 * Removes @filter from the list of filters that the user can select between.
 **/
void
gtk_file_chooser_remove_filter (GtkFileChooser *chooser,
				GtkFileFilter  *filter)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_filter (chooser, filter);
}

/**
 * gtk_file_chooser_list_filters:
 * @choooser: a #GtkFileChooser
 * 
 * Lists the current set of user-selectable filters; see
 * gtk_file_chooser_add_filter(), gtk_file_chooser_remove_filter().
 * 
 * Return value: a #GSList containing the current set of
 *  user selectable filters. The contents of the list are
 *  owned by GTK+, but you must free the list itself with
 *  g_slist_free() when you are done with it.
 **/
GSList *
gtk_file_chooser_list_filters  (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->list_filters (chooser);
}

/**
 * gtk_file_chooser_set_filter:
 * @chooser: a #GtkFileChooser
 * @filter: a #GtkFileFilter
 * 
 * Sets the current filter; only the files that pass the
 * filter will be displayed. If the user-selectable list of filters
 * is non-empty, then the filter should be one of the filters
 * in that list. Setting the current filter when the list of
 * filters is empty is useful if you want to restrict the displayed
 * set of files without letting the user change it.
 **/
void
gtk_file_chooser_set_filter (GtkFileChooser *chooser,
			     GtkFileFilter  *filter)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));

  g_object_set (chooser, "filter", filter, NULL);
}

/**
 * gtk_file_chooser_get_filter:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current filter; see gtk_file_chooser_set_filter().
 * 
 * Return value: the current filter, or %NULL
 **/
GtkFileFilter *
gtk_file_chooser_get_filter (GtkFileChooser *chooser)
{
  GtkFileFilter *filter;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  g_object_get (chooser, "filter", &filter, NULL);
  /* Horrid hack; g_object_get() refs returned objects but
   * that contradicts the memory management conventions
   * for accessors.
   */
  if (filter)
    g_object_unref (filter);

  return filter;
}
