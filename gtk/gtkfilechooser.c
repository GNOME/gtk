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

#include <config.h>
#include "gtkfilechooser.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilesystem.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkalias.h"

static void gtk_file_chooser_class_init (gpointer g_iface);

static GtkFilePath *gtk_file_chooser_get_path         (GtkFileChooser *chooser);

GType
gtk_file_chooser_get_type (void)
{
  static GType file_chooser_type = 0;

  if (!file_chooser_type)
    {
      file_chooser_type = g_type_register_static_simple (G_TYPE_INTERFACE,
							 I_("GtkFileChooser"),
							 sizeof (GtkFileChooserIface),
							 (GClassInitFunc) gtk_file_chooser_class_init,
							 0, NULL, 0);
      
      g_type_interface_add_prerequisite (file_chooser_type, GTK_TYPE_WIDGET);
    }

  return file_chooser_type;
}

static gboolean
confirm_overwrite_accumulator (GSignalInvocationHint *ihint,
			       GValue                *return_accu,
			       const GValue          *handler_return,
			       gpointer               dummy)
{
  gboolean continue_emission;
  GtkFileChooserConfirmation conf;

  conf = g_value_get_enum (handler_return);
  g_value_set_enum (return_accu, conf);
  continue_emission = (conf == GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM);

  return continue_emission;
}

static void
gtk_file_chooser_class_init (gpointer g_iface)
{
  GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);

  /**
   * GtkFileChooser::current-folder-changed
   * @chooser: the object which received the signal.
   *
   * This signal is emitted when the current folder in a #GtkFileChooser
   * changes.  This can happen due to the user performing some action that
   * changes folders, such as selecting a bookmark or visiting a folder on the
   * file list.  It can also happen as a result of calling a function to
   * explicitly change the current folder in a file chooser.
   *
   * Normally you do not need to connect to this signal, unless you need to keep
   * track of which folder a file chooser is showing.
   *
   * See also:  gtk_file_chooser_set_current_folder(),
   * gtk_file_chooser_get_current_folder(),
   * gtk_file_chooser_set_current_folder_uri(),
   * gtk_file_chooser_get_current_folder_uri().
   */
  g_signal_new (I_("current-folder-changed"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, current_folder_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  /**
   * GtkFileChooser::selection-changed
   * @chooser: the object which received the signal.
   *
   * This signal is emitted when there is a change in the set of selected files
   * in a #GtkFileChooser.  This can happen when the user modifies the selection
   * with the mouse or the keyboard, or when explicitly calling functions to
   * change the selection.
   *
   * Normally you do not need to connect to this signal, as it is easier to wait
   * for the file chooser to finish running, and then to get the list of
   * selected files using the functions mentioned below.
   *
   * See also: gtk_file_chooser_select_filename(),
   * gtk_file_chooser_unselect_filename(), gtk_file_chooser_get_filename(),
   * gtk_file_chooser_get_filenames(), gtk_file_chooser_select_uri(),
   * gtk_file_chooser_unselect_uri(), gtk_file_chooser_get_uri(),
   * gtk_file_chooser_get_uris().
   */
  g_signal_new (I_("selection-changed"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  /**
   * GtkFileChooser::update-preview
   * @chooser: the object which received the signal.
   *
   * This signal is emitted when the preview in a file chooser should be
   * regenerated.  For example, this can happen when the currently selected file
   * changes.  You should use this signal if you want your file chooser to have
   * a preview widget.
   *
   * Once you have installed a preview widget with
   * gtk_file_chooser_set_preview_widget(), you should update it when this
   * signal is emitted.  You can use the functions
   * gtk_file_chooser_get_preview_filename() or
   * gtk_file_chooser_get_preview_uri() to get the name of the file to preview.
   * Your widget may not be able to preview all kinds of files; your callback
   * must call gtk_file_chooser_set_preview_wiget_active() to inform the file
   * chooser about whether the preview was generated successfully or not.
   *
   * Please see the example code in <xref linkend="gtkfilechooser-preview"/>.
   *
   * See also: gtk_file_chooser_set_preview_widget(),
   * gtk_file_chooser_set_preview_widget_active(),
   * gtk_file_chooser_set_use_preview_label(),
   * gtk_file_chooser_get_preview_filename(),
   * gtk_file_chooser_get_preview_uri().
   */
  g_signal_new (I_("update-preview"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, update_preview),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  /**
   * GtkFileChooser::file-activated
   * @chooser: the object which received the signal.
   *
   * This signal is emitted when the user "activates" a file in the file
   * chooser.  This can happen by double-clicking on a file in the file list, or
   * by pressing <keycap>Enter</keycap>.
   *
   * Normally you do not need to connect to this signal.  It is used internally
   * by #GtkFileChooserDialog to know when to activate the default button in the
   * dialog.
   *
   * See also: gtk_file_chooser_get_filename(),
   * gtk_file_chooser_get_filenames(), gtk_file_chooser_get_uri(),
   * gtk_file_chooser_get_uris().
   */
  g_signal_new (I_("file-activated"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, file_activated),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  /* Documented in the docbook files */
  g_signal_new (I_("confirm-overwrite"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, confirm_overwrite),
		confirm_overwrite_accumulator, NULL,
		_gtk_marshal_ENUM__VOID,
		GTK_TYPE_FILE_CHOOSER_CONFIRMATION, 0);
  
  g_object_interface_install_property (g_iface,
				       g_param_spec_enum ("action",
							  P_("Action"),
							  P_("The type of operation that the file selector is performing"),
							  GTK_TYPE_FILE_CHOOSER_ACTION,
							  GTK_FILE_CHOOSER_ACTION_OPEN,
							  GTK_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_string ("file-system-backend",
							    P_("File System Backend"),
							    P_("Name of file system backend to use"),
							    NULL, 
							    GTK_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_interface_install_property (g_iface,
				       g_param_spec_object ("filter",
							    P_("Filter"),
							    P_("The current filter for selecting which files are displayed"),
							    GTK_TYPE_FILE_FILTER,
							    GTK_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("local-only",
							     P_("Local Only"),
							     P_("Whether the selected file(s) should be limited to local file: URLs"),
							     TRUE,
							     GTK_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_object ("preview-widget",
							    P_("Preview widget"),
							    P_("Application supplied widget for custom previews."),
							    GTK_TYPE_WIDGET,
							    GTK_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("preview-widget-active",
							     P_("Preview Widget Active"),
							     P_("Whether the application supplied widget for custom previews should be shown."),
							     TRUE,
							     GTK_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("use-preview-label",
							     P_("Use Preview Label"),
							     P_("Whether to display a stock label with the name of the previewed file."),
							     TRUE,
							     GTK_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_object ("extra-widget",
							    P_("Extra widget"),
							    P_("Application supplied widget for extra options."),
							    GTK_TYPE_WIDGET,
							    GTK_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("select-multiple",
							     P_("Select Multiple"),
							     P_("Whether to allow multiple files to be selected"),
							     FALSE,
							     GTK_PARAM_READWRITE));
  
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("show-hidden",
							     P_("Show Hidden"),
							     P_("Whether the hidden files and folders should be displayed"),
							     FALSE,
							     GTK_PARAM_READWRITE));

  /**
   * GtkFileChooser:do-overwrite-confirmation:
   * 
   * Whether a file chooser in %GTK_FILE_CHOOSER_ACTION_SAVE mode
   * will present an overwrite confirmation dialog if the user
   * selects a file name that already exists.
   *
   * Since: 2.8
   */
  g_object_interface_install_property (g_iface,
				       g_param_spec_boolean ("do-overwrite-confirmation",
							     P_("Do overwrite confirmation"),
							     P_("Whether a file chooser in save mode "
								"will present an overwrite confirmation dialog "
								"if necessary."),
							     FALSE,
							     GTK_PARAM_READWRITE));
}

/**
 * gtk_file_chooser_error_quark:
 *
 * Registers an error quark for #GtkFileChooser if necessary.
 * 
 * Return value: The error quark used for #GtkFileChooser errors.
 *
 * Since: 2.4
 **/
GQuark
gtk_file_chooser_error_quark (void)
{
  return g_quark_from_static_string ("gtk-file-chooser-error-quark");
}

/**
 * gtk_file_chooser_set_action:
 * @chooser: a #GtkFileChooser
 * @action: the action that the file selector is performing
 * 
 * Sets the type of operation that the chooser is performing; the
 * user interface is adapted to suit the selected action. For example,
 * an option to create a new folder might be shown if the action is
 * %GTK_FILE_CHOOSER_ACTION_SAVE but not if the action is
 * %GTK_FILE_CHOOSER_ACTION_OPEN.
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 * Sets whether multiple files can be selected in the file selector.  This is
 * only relevant if the action is set to be GTK_FILE_CHOOSER_ACTION_OPEN or
 * GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER.  
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 * If the file chooser is in folder mode, this function returns the selected
 * folder.
 * 
 * Return value: The currently selected filename, or %NULL
 *  if no file is selected, or the selected file can't
 *  be represented with a local filename. Free with g_free().
 *
 * Since: 2.4
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
 * Sets @filename as the current filename for the file chooser, by changing
 * to the file's parent folder and actually selecting the file in list.  If
 * the @chooser is in #GTK_FILE_CHOOSER_ACTION_SAVE mode, the file's base name
 * will also appear in the dialog's file name entry.
 *
 * If the file name isn't in the current folder of @chooser, then the current
 * folder of @chooser will be changed to the folder containing @filename. This
 * is equivalent to a sequence of gtk_file_chooser_unselect_all() followed by
 * gtk_file_chooser_select_filename().
 *
 * Note that the file must exist, or nothing will be done except
 * for the directory change.
 *
 * If you are implementing a <guimenuitem>File/Save As...</guimenuitem> dialog, you
 * should use this function if you already have a file name to which the user may save; for example,
 * when the user opens an existing file and then does <guimenuitem>File/Save As...</guimenuitem>
 * on it.  If you don't have a file name already &mdash; for example, if the user just created
 * a new file and is saving it for the first time, do not call this function.  Instead, use
 * something similar to this:
 *
 * <programlisting>
 * if (document_is_new)
 *   {
 *     /<!-- -->* the user just created a new document *<!-- -->/
 *     gtk_file_chooser_set_current_folder (chooser, default_folder_for_saving);
 *     gtk_file_chooser_set_current_name (chooser, "Untitled document");
 *   }
 * else
 *   {
 *     /<!-- -->* the user edited an existing document *<!-- -->/ 
 *     gtk_file_chooser_set_filename (chooser, existing_filename);
 *   }
 * </programlisting>
 * 
 * Return value: %TRUE if both the folder could be changed and the file was
 * selected successfully, %FALSE otherwise.
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_set_filename (GtkFileChooser *chooser,
			       const gchar    *filename)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  gtk_file_chooser_unselect_all (chooser);
  return gtk_file_chooser_select_filename (chooser, filename);
}

/**
 * gtk_file_chooser_select_filename:
 * @chooser: a #GtkFileChooser
 * @filename: the filename to select
 * 
 * Selects a filename. If the file name isn't in the current
 * folder of @chooser, then the current folder of @chooser will
 * be changed to the folder containing @filename.
 *
 * Return value: %TRUE if both the folder could be changed and the file was
 * selected successfully, %FALSE otherwise.
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_select_filename (GtkFileChooser *chooser,
				  const gchar    *filename)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_filename_to_path (file_system, filename);
  if (path)
    {
      result = _gtk_file_chooser_select_path (chooser, path, NULL);
      gtk_file_path_free (path);
    }
  else
    result = FALSE;

  return result;
}

/**
 * gtk_file_chooser_unselect_filename:
 * @chooser: a #GtkFileChooser
 * @filename: the filename to unselect
 * 
 * Unselects a currently selected filename. If the filename
 * is not in the current directory, does not exist, or
 * is otherwise not currently selected, does nothing.
 *
 * Since: 2.4
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

/* Converts a list of GtkFilePath* to a list of strings using the specified function */
static GSList *
file_paths_to_strings (GtkFileSystem *fs,
		       GSList        *paths,
		       gchar *      (*convert_func) (GtkFileSystem *fs, const GtkFilePath *path))
{
  GSList *strings;

  strings = NULL;

  for (; paths; paths = paths->next)
    {
      GtkFilePath *path;
      gchar *string;

      path = paths->data;
      string = (* convert_func) (fs, path);

      if (string)
	strings = g_slist_prepend (strings, string);
    }

  return g_slist_reverse (strings);
}

/**
 * gtk_file_chooser_get_filenames:
 * @chooser: a #GtkFileChooser
 * 
 * Lists all the selected files and subfolders in the current folder of
 * @chooser. The returned names are full absolute paths. If files in the current
 * folder cannot be represented as local filenames they will be ignored. (See
 * gtk_file_chooser_get_uris())
 * 
 * Return value: a #GSList containing the filenames of all selected
 *   files and subfolders in the current folder. Free the returned list
 *   with g_slist_free(), and the filenames with g_free().
 *
 * Since: 2.4
 **/
GSList *
gtk_file_chooser_get_filenames (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GSList *paths;
  GSList *result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  paths = _gtk_file_chooser_get_paths (chooser);

  result = file_paths_to_strings (file_system, paths, gtk_file_system_path_to_filename);
  gtk_file_paths_free (paths);
  return result;
}

/**
 * gtk_file_chooser_set_current_folder:
 * @chooser: a #GtkFileChooser
 * @filename: the full path of the new current folder
 * 
 * Sets the current folder for @chooser from a local filename.
 * The user will be shown the full contents of the current folder,
 * plus user interface elements for navigating to other folders.
 *
 * Return value: %TRUE if the folder could be changed successfully, %FALSE
 * otherwise.
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_set_current_folder (GtkFileChooser *chooser,
				     const gchar    *filename)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_filename_to_path (file_system, filename);
  if (path)
    {
      result = _gtk_file_chooser_set_current_folder_path (chooser, path, NULL);
      gtk_file_path_free (path);
    }
  else
    result = FALSE;

  return result;
}

/**
 * gtk_file_chooser_get_current_folder:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current folder of @chooser as a local filename.
 * See gtk_file_chooser_set_current_folder().
 *
 * Note that this is the folder that the file chooser is currently displaying
 * (e.g. "/home/username/Documents"), which is <emphasis>not the same</emphasis>
 * as the currently-selected folder if the chooser is in
 * #GTK_FILE_CHOOSER_SELECT_FOLDER mode
 * (e.g. "/home/username/Documents/selected-folder/".  To get the
 * currently-selected folder in that mode, use gtk_file_chooser_get_uri() as the
 * usual way to get the selection.
 * 
 * Return value: the full path of the current folder, or %NULL if the current
 * path cannot be represented as a local filename.  Free with g_free().  This
 * function will also return %NULL if the file chooser was unable to load the
 * last folder that was requested from it; for example, as would be for calling
 * gtk_file_chooser_set_current_folder() on a nonexistent folder.
 *
 * Since: 2.4
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
  if (!path)
    return NULL;

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
 * If you want to preselect a particular existing file, you should use
 * gtk_file_chooser_set_filename() or gtk_file_chooser_set_uri() instead.
 * Please see the documentation for those functions for an example of using
 * gtk_file_chooser_set_current_name() as well.
 *
 * Since: 2.4
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
 * If the file chooser is in folder mode, this function returns the selected
 * folder.
 * 
 * Return value: The currently selected URI, or %NULL
 *  if no file is selected. Free with g_free()
 *
 * Since: 2.4
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
 * Sets the file referred to by @uri as the current file for the file chooser,
 * by changing to the URI's parent folder and actually selecting the URI in the
 * list.  If the @chooser is #GTK_FILE_CHOOSER_ACTION_SAVE mode, the URI's base
 * name will also appear in the dialog's file name entry.
 *
 * If the URI isn't in the current folder of @chooser, then the current folder
 * of @chooser will be changed to the folder containing @uri. This is equivalent
 * to a sequence of gtk_file_chooser_unselect_all() followed by
 * gtk_file_chooser_select_uri().
 *
 * Note that the URI must exist, or nothing will be done except
 * for the directory change.
 * If you are implementing a <guimenuitem>File/Save As...</guimenuitem> dialog, you
 * should use this function if you already have a file name to which the user may save; for example,
 * when the user opens an existing file and then does <guimenuitem>File/Save As...</guimenuitem>
 * on it.  If you don't have a file name already &mdash; for example, if the user just created
 * a new file and is saving it for the first time, do not call this function.  Instead, use
 * something similar to this:
 *
 * <programlisting>
 * if (document_is_new)
 *   {
 *     /<!-- -->* the user just created a new document *<!-- -->/
 *     gtk_file_chooser_set_current_folder_uri (chooser, default_folder_for_saving);
 *     gtk_file_chooser_set_current_name (chooser, "Untitled document");
 *   }
 * else
 *   {
 *     /<!-- -->* the user edited an existing document *<!-- -->/ 
 *     gtk_file_chooser_set_uri (chooser, existing_uri);
 *   }
 * </programlisting>
 *
 * Return value: %TRUE if both the folder could be changed and the URI was
 * selected successfully, %FALSE otherwise.
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_set_uri (GtkFileChooser *chooser,
			  const char     *uri)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  gtk_file_chooser_unselect_all (chooser);
  return gtk_file_chooser_select_uri (chooser, uri);
}

/**
 * gtk_file_chooser_select_uri:
 * @chooser: a #GtkFileChooser
 * @uri: the URI to select
 * 
 * Selects the file to by @uri. If the URI doesn't refer to a
 * file in the current folder of @chooser, then the current folder of
 * @chooser will be changed to the folder containing @filename.
 *
 * Return value: %TRUE if both the folder could be changed and the URI was
 * selected successfully, %FALSE otherwise.
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_select_uri (GtkFileChooser *chooser,
			     const char     *uri)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_uri_to_path (file_system, uri);
  if (path)
    {
      result = _gtk_file_chooser_select_path (chooser, path, NULL);
      gtk_file_path_free (path);
    }
  else
    result = FALSE;

  return result;
}

/**
 * gtk_file_chooser_unselect_uri:
 * @chooser: a #GtkFileChooser
 * @uri: the URI to unselect
 * 
 * Unselects the file referred to by @uri. If the file
 * is not in the current directory, does not exist, or
 * is otherwise not currently selected, does nothing.
 *
 * Since: 2.4
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

/**
 * gtk_file_chooser_select_all:
 * @chooser: a #GtkFileChooser
 * 
 * Selects all the files in the current folder of a file chooser.
 *
 * Since: 2.4
 **/
void
gtk_file_chooser_select_all (GtkFileChooser *chooser)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_all (chooser);
}

/**
 * gtk_file_chooser_unselect_all:
 * @chooser: a #GtkFileChooser
 * 
 * Unselects all the files in the current folder of a file chooser.
 *
 * Since: 2.4
 **/
void
gtk_file_chooser_unselect_all (GtkFileChooser *chooser)
{

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_all (chooser);
}

/**
 * gtk_file_chooser_get_uris:
 * @chooser: a #GtkFileChooser
 * 
 * Lists all the selected files and subfolders in the current folder of
 * @chooser. The returned names are full absolute URIs.
 * 
 * Return value: a #GSList containing the URIs of all selected
 *   files and subfolders in the current folder. Free the returned list
 *   with g_slist_free(), and the filenames with g_free().
 *
 * Since: 2.4
 **/
GSList *
gtk_file_chooser_get_uris (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GSList *paths;
  GSList *result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  paths = _gtk_file_chooser_get_paths (chooser);

  result = file_paths_to_strings (file_system, paths, gtk_file_system_path_to_uri);
  gtk_file_paths_free (paths);
  return result;
}

/**
 * gtk_file_chooser_set_current_folder_uri:
 * @chooser: a #GtkFileChooser
 * @uri: the URI for the new current folder
 * 
 * Sets the current folder for @chooser from an URI.
 * The user will be shown the full contents of the current folder,
 * plus user interface elements for navigating to other folders.
 *
 * Return value: %TRUE if the folder could be changed successfully, %FALSE
 * otherwise.
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_set_current_folder_uri (GtkFileChooser *chooser,
					 const gchar    *uri)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_uri_to_path (file_system, uri);
  if (path)
    {
      result = _gtk_file_chooser_set_current_folder_path (chooser, path, NULL);
      gtk_file_path_free (path);
    }
  else
    result = FALSE;

  return result;
}

/**
 * gtk_file_chooser_get_current_folder_uri:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current folder of @chooser as an URI.
 * See gtk_file_chooser_set_current_folder_uri().
 *
 * Note that this is the folder that the file chooser is currently displaying
 * (e.g. "file:///home/username/Documents"), which is <emphasis>not the same</emphasis>
 * as the currently-selected folder if the chooser is in
 * #GTK_FILE_CHOOSER_SELECT_FOLDER mode
 * (e.g. "file:///home/username/Documents/selected-folder/".  To get the
 * currently-selected folder in that mode, use gtk_file_chooser_get_uri() as the
 * usual way to get the selection.
 * 
 * Return value: the URI for the current folder.  Free with g_free().  This
 * function will also return %NULL if the file chooser was unable to load the
 * last folder that was requested from it; for example, as would be for calling
 * gtk_file_chooser_set_current_folder_uri() on a nonexistent folder.
 *
 * Since: 2.4
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
 * @error: location to store error, or %NULL.
 * 
 * Sets the current folder for @chooser from a #GtkFilePath.
 * Internal function, see gtk_file_chooser_set_current_folder_uri().
 *
 * Return value: %TRUE if the folder could be changed successfully, %FALSE
 * otherwise.
 *
 * Since: 2.4
 **/
gboolean
_gtk_file_chooser_set_current_folder_path (GtkFileChooser    *chooser,
					   const GtkFilePath *path,
					   GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->set_current_folder (chooser, path, error);
}

/**
 * _gtk_file_chooser_get_current_folder_path:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the current folder of @chooser as #GtkFilePath.
 * See gtk_file_chooser_get_current_folder_uri().
 * 
 * Return value: the #GtkFilePath for the current folder.
 * Free with gtk_file_path_free().
 *
 * Since: 2.4
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
 * @error: location to store error, or %NULL
 * 
 * Selects the file referred to by @path. An internal function. See
 * _gtk_file_chooser_select_uri().
 *
 * Return value: %TRUE if both the folder could be changed and the path was
 * selected successfully, %FALSE otherwise.
 *
 * Since: 2.4
 **/
gboolean
_gtk_file_chooser_select_path (GtkFileChooser    *chooser,
			       const GtkFilePath *path,
			       GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_path (chooser, path, error);
}

/**
 * _gtk_file_chooser_unselect_path:
 * @chooser: a #GtkFileChooser
 * @path: the filename to path
 * 
 * Unselects the file referred to by @path. An internal
 * function. See _gtk_file_chooser_unselect_uri().
 *
 * Since: 2.4
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
 * Lists all the selected files and subfolders in the current folder of @chooser
 * as #GtkFilePath. An internal function, see gtk_file_chooser_get_uris().
 * 
 * Return value: a #GSList containing a #GtkFilePath for each selected
 *   file and subfolder in the current folder.  Free the returned list
 *   with g_slist_free(), and the paths with gtk_file_path_free().
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 * preview widget, you connect to the ::update-preview
 * signal, and call gtk_file_chooser_get_preview_filename() or
 * gtk_file_chooser_get_preview_uri() on each change. If you can
 * display a preview of the new file, update your widget and
 * set the preview active using gtk_file_chooser_set_preview_widget_active().
 * Otherwise, set the preview inactive.
 *
 * When there is no application-supplied preview widget, or the
 * application-supplied preview widget is not active, the file chooser
 * may display an internally generated preview of the current file or
 * it may display no preview at all.
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 * gtk_file_chooser_set_preview_widget() should be shown for the
 * current filename. When @active is set to false, the file chooser
 * may display an internally generated preview of the current file
 * or it may display no preview at all. See
 * gtk_file_chooser_set_preview_widget() for more details.
 *
 * Since: 2.4
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
 * Gets whether the preview widget set by gtk_file_chooser_set_preview_widget()
 * should be shown for the current filename. See
 * gtk_file_chooser_set_preview_widget_active().
 * 
 * Return value: %TRUE if the preview widget is active for the current filename.
 *
 * Since: 2.4
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
 * gtk_file_chooser_set_use_preview_label:
 * @chooser: a #GtkFileChooser
 * @use_label: whether to display a stock label with the name of the previewed file
 * 
 * Sets whether the file chooser should display a stock label with the name of
 * the file that is being previewed; the default is %TRUE.  Applications that
 * want to draw the whole preview area themselves should set this to %FALSE and
 * display the name themselves in their preview widget.
 *
 * See also: gtk_file_chooser_set_preview_widget()
 *
 * Since: 2.4
 **/
void
gtk_file_chooser_set_use_preview_label (GtkFileChooser *chooser,
					gboolean        use_label)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "use-preview-label", use_label, NULL);
}

/**
 * gtk_file_chooser_get_use_preview_label:
 * @chooser: a #GtkFileChooser
 * 
 * Gets whether a stock label should be drawn with the name of the previewed
 * file.  See gtk_file_chooser_set_use_preview_label().
 * 
 * Return value: %TRUE if the file chooser is set to display a label with the
 * name of the previewed file, %FALSE otherwise.
 **/
gboolean
gtk_file_chooser_get_use_preview_label (GtkFileChooser *chooser)
{
  gboolean use_label;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "use-preview-label", &use_label, NULL);

  return use_label;
}

/**
 * gtk_file_chooser_get_preview_filename:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the filename that should be previewed in a custom preview
 * Internal function, see gtk_file_chooser_get_preview_uri().
 * 
 * Return value: the #GtkFilePath for the file to preview, or %NULL if no file
 *  is selected. Free with gtk_file_path_free().
 *
 * Since: 2.4
 **/
GtkFilePath *
_gtk_file_chooser_get_preview_path (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_preview_path (chooser);
}

/**
 * _gtk_file_chooser_add_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @path: path of the folder to add
 * @error: location to store error, or %NULL
 * 
 * Adds a folder to be displayed with the shortcut folders in a file chooser.
 * Internal function, see gtk_file_chooser_add_shortcut_folder().
 * 
 * Return value: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.
 *
 * Since: 2.4
 **/
gboolean
_gtk_file_chooser_add_shortcut_folder (GtkFileChooser    *chooser,
				       const GtkFilePath *path,
				       GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_shortcut_folder (chooser, path, error);
}

/**
 * _gtk_file_chooser_remove_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @path: path of the folder to remove
 * @error: location to store error, or %NULL
 * 
 * Removes a folder from the shortcut folders in a file chooser.  Internal
 * function, see gtk_file_chooser_remove_shortcut_folder().
 * 
 * Return value: %TRUE if the folder could be removed successfully, %FALSE
 * otherwise.
 *
 * Since: 2.4
 **/
gboolean
_gtk_file_chooser_remove_shortcut_folder (GtkFileChooser    *chooser,
					  const GtkFilePath *path,
					  GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_shortcut_folder (chooser, path, error);
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
 *
 * Since: 2.4
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
 * gtk_file_chooser_get_preview_uri:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the URI that should be previewed in a custom preview
 * widget. See gtk_file_chooser_set_preview_widget().
 * 
 * Return value: the URI for the file to preview, or %NULL if no file is
 * selected. Free with g_free().
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 * 
 * Note that the @chooser takes ownership of the filter, so you have to 
 * ref and sink it if you want to keep a reference.
 *
 * Since: 2.4
 **/
void
gtk_file_chooser_add_filter (GtkFileChooser *chooser,
			     GtkFileFilter  *filter)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_filter (chooser, filter);
}

/**
 * gtk_file_chooser_remove_filter:
 * @chooser: a #GtkFileChooser
 * @filter: a #GtkFileFilter
 * 
 * Removes @filter from the list of filters that the user can select between.
 *
 * Since: 2.4
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
 * @chooser: a #GtkFileChooser
 * 
 * Lists the current set of user-selectable filters; see
 * gtk_file_chooser_add_filter(), gtk_file_chooser_remove_filter().
 * 
 * Return value: a #GSList containing the current set of
 *  user selectable filters. The contents of the list are
 *  owned by GTK+, but you must free the list itself with
 *  g_slist_free() when you are done with it.
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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
 *
 * Since: 2.4
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

/**
 * gtk_file_chooser_add_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @folder: filename of the folder to add
 * @error: location to store error, or %NULL
 * 
 * Adds a folder to be displayed with the shortcut folders in a file chooser.
 * Note that shortcut folders do not get saved, as they are provided by the
 * application.  For example, you can use this to add a
 * "/usr/share/mydrawprogram/Clipart" folder to the volume list.
 * 
 * Return value: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.  In the latter case, the @error will be set as appropriate.
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_add_shortcut_folder (GtkFileChooser    *chooser,
				      const char        *folder,
				      GError           **error)
{
  GtkFilePath *path;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (folder != NULL, FALSE);

  path = gtk_file_system_filename_to_path (_gtk_file_chooser_get_file_system (chooser), folder);
  if (!path)
    {
      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
		   _("Invalid filename: %s"),
		   folder);
      return FALSE;
    }

  result = GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_shortcut_folder (chooser, path, error);

  gtk_file_path_free (path);

  return result;
}

/**
 * gtk_file_chooser_remove_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @folder: filename of the folder to remove
 * @error: location to store error, or %NULL
 * 
 * Removes a folder from a file chooser's list of shortcut folders.
 * 
 * Return value: %TRUE if the operation succeeds, %FALSE otherwise.  
 * In the latter case, the @error will be set as appropriate.
 *
 * See also: gtk_file_chooser_add_shortcut_folder()
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_remove_shortcut_folder (GtkFileChooser    *chooser,
					 const char        *folder,
					 GError           **error)
{
  GtkFilePath *path;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (folder != NULL, FALSE);

  path = gtk_file_system_filename_to_path (_gtk_file_chooser_get_file_system (chooser), folder);
  if (!path)
    {
      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
		   _("Invalid filename: %s"),
		   folder);
      return FALSE;
    }

  result = GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_shortcut_folder (chooser, path, error);

  gtk_file_path_free (path);

  return result;
}

/**
 * gtk_file_chooser_list_shortcut_folders:
 * @chooser: a #GtkFileChooser
 * 
 * Queries the list of shortcut folders in the file chooser, as set by
 * gtk_file_chooser_add_shortcut_folder().
 * 
 * Return value: A list of folder filenames, or %NULL if there are no shortcut
 * folders.  Free the returned list with g_slist_free(), and the filenames with
 * g_free().
 *
 * Since: 2.4
 **/
GSList *
gtk_file_chooser_list_shortcut_folders (GtkFileChooser *chooser)
{
  GSList *folders;
  GSList *result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  folders = GTK_FILE_CHOOSER_GET_IFACE (chooser)->list_shortcut_folders (chooser);

  result = file_paths_to_strings (_gtk_file_chooser_get_file_system (chooser),
				  folders,
				  gtk_file_system_path_to_filename);
  gtk_file_paths_free (folders);
  return result;
}

/**
 * gtk_file_chooser_add_shortcut_folder_uri:
 * @chooser: a #GtkFileChooser
 * @uri: URI of the folder to add
 * @error: location to store error, or %NULL
 * 
 * Adds a folder URI to be displayed with the shortcut folders in a file
 * chooser.  Note that shortcut folders do not get saved, as they are provided
 * by the application.  For example, you can use this to add a
 * "file:///usr/share/mydrawprogram/Clipart" folder to the volume list.
 * 
 * Return value: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.  In the latter case, the @error will be set as appropriate.
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_add_shortcut_folder_uri (GtkFileChooser    *chooser,
					  const char        *uri,
					  GError           **error)
{
  GtkFilePath *path;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  path = gtk_file_system_uri_to_path (_gtk_file_chooser_get_file_system (chooser), uri);
  if (!path)
    {
      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
		   _("Invalid filename: %s"),
		   uri);
      return FALSE;
    }

  result = GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_shortcut_folder (chooser, path, error);

  gtk_file_path_free (path);

  return result;
}

/**
 * gtk_file_chooser_remove_shortcut_folder_uri:
 * @chooser: a #GtkFileChooser
 * @uri: URI of the folder to remove
 * @error: location to store error, or %NULL
 * 
 * Removes a folder URI from a file chooser's list of shortcut folders.
 * 
 * Return value: %TRUE if the operation succeeds, %FALSE otherwise.  
 * In the latter case, the @error will be set as appropriate.
 *
 * See also: gtk_file_chooser_add_shortcut_folder_uri()
 *
 * Since: 2.4
 **/
gboolean
gtk_file_chooser_remove_shortcut_folder_uri (GtkFileChooser    *chooser,
					     const char        *uri,
					     GError           **error)
{
  GtkFilePath *path;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  path = gtk_file_system_uri_to_path (_gtk_file_chooser_get_file_system (chooser), uri);
  if (!path)
    {
      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
		   _("Invalid filename: %s"),
		   uri);
      return FALSE;
    }

  result = GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_shortcut_folder (chooser, path, error);

  gtk_file_path_free (path);

  return result;
}

/**
 * gtk_file_chooser_list_shortcut_folder_uris:
 * @chooser: a #GtkFileChooser
 * 
 * Queries the list of shortcut folders in the file chooser, as set by
 * gtk_file_chooser_add_shortcut_folder_uri().
 * 
 * Return value: A list of folder URIs, or %NULL if there are no shortcut
 * folders.  Free the returned list with g_slist_free(), and the URIs with
 * g_free().
 *
 * Since: 2.4
 **/
GSList *
gtk_file_chooser_list_shortcut_folder_uris (GtkFileChooser *chooser)
{
  GSList *folders;
  GSList *result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  folders = GTK_FILE_CHOOSER_GET_IFACE (chooser)->list_shortcut_folders (chooser);

  result = file_paths_to_strings (_gtk_file_chooser_get_file_system (chooser),
				  folders,
				  gtk_file_system_path_to_uri);
  gtk_file_paths_free (folders);
  return result;
}


/**
 * gtk_file_chooser_set_show_hidden:
 * @chooser: a #GtkFileChooser
 * @show_hidden: %TRUE if hidden files and folders should be displayed.
 * 
 * Sets whether hidden files and folders are displayed in the file selector.  
 *
 * Since: 2.6
 **/
void
gtk_file_chooser_set_show_hidden (GtkFileChooser *chooser,
				  gboolean        show_hidden)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "show-hidden", show_hidden, NULL);
}

/**
 * gtk_file_chooser_get_show_hidden:
 * @chooser: a #GtkFileChooser
 * 
 * Gets whether hidden files and folders are displayed in the file selector.   
 * See gtk_file_chooser_set_show_hidden().
 * 
 * Return value: %TRUE if hidden files and folders are displayed.
 *
 * Since: 2.6
 **/
gboolean
gtk_file_chooser_get_show_hidden (GtkFileChooser *chooser)
{
  gboolean show_hidden;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "show-hidden", &show_hidden, NULL);

  return show_hidden;
}

/**
 * gtk_file_chooser_set_do_overwrite_confirmation:
 * @chooser: a #GtkFileChooser
 * @do_overwrite_confirmation: whether to confirm overwriting in save mode
 * 
 * Sets whether a file chooser in GTK_FILE_CHOOSER_ACTION_SAVE mode will present
 * a confirmation dialog if the user types a file name that already exists.  This
 * is %FALSE by default.
 *
 * Regardless of this setting, the @chooser will emit the "confirm-overwrite"
 * signal when appropriate.
 *
 * If all you need is the stock confirmation dialog, set this property to %TRUE.
 * You can override the way confirmation is done by actually handling the
 * "confirm-overwrite" signal; please refer to its documentation for the
 * details.
 *
 * Since: 2.8
 **/
void
gtk_file_chooser_set_do_overwrite_confirmation (GtkFileChooser *chooser,
						gboolean        do_overwrite_confirmation)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "do-overwrite-confirmation", do_overwrite_confirmation, NULL);
}

/**
 * gtk_file_chooser_get_do_overwrite_confirmation:
 * @chooser: a #GtkFileChooser
 * 
 * Queries whether a file chooser is set to confirm for overwriting when the user
 * types a file name that already exists.
 * 
 * Return value: %TRUE if the file chooser will present a confirmation dialog;
 * %FALSE otherwise.
 *
 * Since: 2.8
 **/
gboolean
gtk_file_chooser_get_do_overwrite_confirmation (GtkFileChooser *chooser)
{
  gboolean do_overwrite_confirmation;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "do-overwrite-confirmation", &do_overwrite_confirmation, NULL);

  return do_overwrite_confirmation;
}

#ifdef G_OS_WIN32

/* DLL ABI stability backward compatibility versions */

#undef gtk_file_chooser_get_filename

gchar *
gtk_file_chooser_get_filename (GtkFileChooser *chooser)
{
  gchar *utf8_filename = gtk_file_chooser_get_filename_utf8 (chooser);
  gchar *retval = g_locale_from_utf8 (utf8_filename, -1, NULL, NULL, NULL);

  g_free (utf8_filename);

  return retval;
}

#undef gtk_file_chooser_set_filename

gboolean
gtk_file_chooser_set_filename (GtkFileChooser *chooser,
			       const gchar    *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
  gboolean retval = gtk_file_chooser_set_filename_utf8 (chooser, utf8_filename);

  g_free (utf8_filename);

  return retval;
}

#undef gtk_file_chooser_select_filename

gboolean
gtk_file_chooser_select_filename (GtkFileChooser *chooser,
				  const gchar    *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
  gboolean retval = gtk_file_chooser_select_filename_utf8 (chooser, utf8_filename);

  g_free (utf8_filename);

  return retval;
}

#undef gtk_file_chooser_unselect_filename

void
gtk_file_chooser_unselect_filename (GtkFileChooser *chooser,
				    const char     *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);

  gtk_file_chooser_unselect_filename_utf8 (chooser, utf8_filename);
  g_free (utf8_filename);
}

#undef gtk_file_chooser_get_filenames

GSList *
gtk_file_chooser_get_filenames (GtkFileChooser *chooser)
{
  GSList *list = gtk_file_chooser_get_filenames_utf8 (chooser);
  GSList *rover = list;
  
  while (rover)
    {
      gchar *tem = (gchar *) rover->data;
      rover->data = g_locale_from_utf8 ((gchar *) rover->data, -1, NULL, NULL, NULL);
      g_free (tem);
      rover = rover->next;
    }

  return list;
}

#undef gtk_file_chooser_set_current_folder

gboolean
gtk_file_chooser_set_current_folder (GtkFileChooser *chooser,
				     const gchar    *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
  gboolean retval = gtk_file_chooser_set_current_folder_utf8 (chooser, utf8_filename);

  g_free (utf8_filename);

  return retval;
}

#undef gtk_file_chooser_get_current_folder

gchar *
gtk_file_chooser_get_current_folder (GtkFileChooser *chooser)
{
  gchar *utf8_folder = gtk_file_chooser_get_current_folder_utf8 (chooser);
  gchar *retval = g_locale_from_utf8 (utf8_folder, -1, NULL, NULL, NULL);

  g_free (utf8_folder);

  return retval;
}

#undef gtk_file_chooser_get_preview_filename

char *
gtk_file_chooser_get_preview_filename (GtkFileChooser *chooser)
{
  char *utf8_filename = gtk_file_chooser_get_preview_filename_utf8 (chooser);
  char *retval = g_locale_from_utf8 (utf8_filename, -1, NULL, NULL, NULL);

  g_free (utf8_filename);

  return retval;
}

#undef gtk_file_chooser_add_shortcut_folder

gboolean
gtk_file_chooser_add_shortcut_folder (GtkFileChooser    *chooser,
				      const char        *folder,
				      GError           **error)
{
  char *utf8_folder = g_locale_to_utf8 (folder, -1, NULL, NULL, NULL);
  gboolean retval =
    gtk_file_chooser_add_shortcut_folder_utf8 (chooser, utf8_folder, error);

  g_free (utf8_folder);

  return retval;
}

#undef gtk_file_chooser_remove_shortcut_folder

gboolean
gtk_file_chooser_remove_shortcut_folder (GtkFileChooser    *chooser,
					 const char        *folder,
					 GError           **error)
{
  char *utf8_folder = g_locale_to_utf8 (folder, -1, NULL, NULL, NULL);
  gboolean retval =
    gtk_file_chooser_remove_shortcut_folder_utf8 (chooser, utf8_folder, error);

  g_free (utf8_folder);

  return retval;
}

#undef gtk_file_chooser_list_shortcut_folders

GSList *
gtk_file_chooser_list_shortcut_folders (GtkFileChooser *chooser)
{
  GSList *list = gtk_file_chooser_list_shortcut_folders_utf8 (chooser);
  GSList *rover = list;
  
  while (rover)
    {
      gchar *tem = (gchar *) rover->data;
      rover->data = g_locale_from_utf8 ((gchar *) rover->data, -1, NULL, NULL, NULL);
      g_free (tem);
      rover = rover->next;
    }

  return list;
}

#endif

#define __GTK_FILE_CHOOSER_C__
#include "gtkaliasdef.c"
