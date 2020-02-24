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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gtkfilechooser.h"
#include "gtkfilechooserprivate.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"


/**
 * SECTION:gtkfilechooser
 * @Short_description: File chooser interface used by GtkFileChooserWidget and GtkFileChooserDialog
 * @Title: GtkFileChooser
 * @See_also: #GtkFileChooserDialog, #GtkFileChooserWidget, #GtkFileChooserButton
 *
 * #GtkFileChooser is an interface that can be implemented by file
 * selection widgets.  In GTK+, the main objects that implement this
 * interface are #GtkFileChooserWidget, #GtkFileChooserDialog, and
 * #GtkFileChooserButton.  You do not need to write an object that
 * implements the #GtkFileChooser interface unless you are trying to
 * adapt an existing file selector to expose a standard programming
 * interface.
 *
 * #GtkFileChooser allows for shortcuts to various places in the filesystem.
 * In the default implementation these are displayed in the left pane. It
 * may be a bit confusing at first that these shortcuts come from various
 * sources and in various flavours, so lets explain the terminology here:
 *
 * - Bookmarks: are created by the user, by dragging folders from the
 *   right pane to the left pane, or by using the “Add”. Bookmarks
 *   can be renamed and deleted by the user.
 *
 * - Shortcuts: can be provided by the application. For example, a Paint
 *   program may want to add a shortcut for a Clipart folder. Shortcuts
 *   cannot be modified by the user.
 *
 * - Volumes: are provided by the underlying filesystem abstraction. They are
 *   the “roots” of the filesystem.
 *
 * # File Names and Encodings
 *
 * When the user is finished selecting files in a
 * #GtkFileChooser, your program can get the selected filenames as
 * #GFiles.
 *
 * # Adding options 
 *
 * You can add extra widgets to a file chooser to provide options
 * that are not present in the default design, by using
 * gtk_file_chooser_add_choice(). Each choice has an identifier and
 * a user visible label; additionally, each choice can have multiple
 * options. If a choice has no option, it will be rendered as a
 * check button with the given label; if a choice has options, it will
 * be rendered as a combo box.
 */


typedef GtkFileChooserIface GtkFileChooserInterface;
G_DEFINE_INTERFACE (GtkFileChooser, gtk_file_chooser, G_TYPE_OBJECT);

static void
gtk_file_chooser_default_init (GtkFileChooserInterface *iface)
{
  GType iface_type = G_TYPE_FROM_INTERFACE (iface);

  /**
   * GtkFileChooser::current-folder-changed:
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
   */
  g_signal_new (I_("current-folder-changed"),
                iface_type,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkFileChooserIface, current_folder_changed),
                NULL, NULL,
                NULL,
                G_TYPE_NONE, 0);

  /**
   * GtkFileChooser::selection-changed:
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
   */
  g_signal_new (I_("selection-changed"),
                iface_type,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkFileChooserIface, selection_changed),
                NULL, NULL,
                NULL,
                G_TYPE_NONE, 0);

  /**
   * GtkFileChooser::file-activated:
   * @chooser: the object which received the signal.
   *
   * This signal is emitted when the user "activates" a file in the file
   * chooser.  This can happen by double-clicking on a file in the file list, or
   * by pressing `Enter`.
   *
   * Normally you do not need to connect to this signal.  It is used internally
   * by #GtkFileChooserDialog to know when to activate the default button in the
   * dialog.
   *
   * See also: gtk_file_chooser_get_file(), gtk_file_chooser_get_files()
   */
  g_signal_new (I_("file-activated"),
                iface_type,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkFileChooserIface, file_activated),
                NULL, NULL,
                NULL,
                G_TYPE_NONE, 0);

  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("action",
                                                          P_("Action"),
                                                          P_("The type of operation that the file selector is performing"),
                                                          GTK_TYPE_FILE_CHOOSER_ACTION,
                                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                                          GTK_PARAM_READWRITE));
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("filter",
                                                            P_("Filter"),
                                                            P_("The current filter for selecting which files are displayed"),
                                                            GTK_TYPE_FILE_FILTER,
                                                            GTK_PARAM_READWRITE));
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("select-multiple",
                                                             P_("Select Multiple"),
                                                             P_("Whether to allow multiple files to be selected"),
                                                             FALSE,
                                                             GTK_PARAM_READWRITE));

  /**
   * GtkFileChooser:create-folders:
   * 
   * Whether a file chooser not in %GTK_FILE_CHOOSER_ACTION_OPEN mode
   * will offer the user to create new folders.
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("create-folders",
                                                             P_("Allow folder creation"),
                                                             P_("Whether a file chooser not in open mode "
                                                                "will offer the user to create new folders."),
                                                             TRUE,
                                                             GTK_PARAM_READWRITE));
}

/**
 * gtk_file_chooser_error_quark:
 *
 * Registers an error quark for #GtkFileChooser if necessary.
 * 
 * Returns: The error quark used for #GtkFileChooser errors.
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
 * Returns: the action that the file selector is performing
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
 * gtk_file_chooser_set_select_multiple:
 * @chooser: a #GtkFileChooser
 * @select_multiple: %TRUE if multiple files can be selected.
 * 
 * Sets whether multiple files can be selected in the file selector.  This is
 * only relevant if the action is set to be %GTK_FILE_CHOOSER_ACTION_OPEN or
 * %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER.
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
 * Returns: %TRUE if multiple files can be selected.
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
 * gtk_file_chooser_set_create_folders:
 * @chooser: a #GtkFileChooser
 * @create_folders: %TRUE if the Create Folder button should be displayed
 * 
 * Sets whether file choser will offer to create new folders.
 * This is only relevant if the action is not set to be 
 * %GTK_FILE_CHOOSER_ACTION_OPEN.
 **/
void
gtk_file_chooser_set_create_folders (GtkFileChooser *chooser,
                                     gboolean        create_folders)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "create-folders", create_folders, NULL);
}

/**
 * gtk_file_chooser_get_create_folders:
 * @chooser: a #GtkFileChooser
 * 
 * Gets whether file choser will offer to create new folders.
 * See gtk_file_chooser_set_create_folders().
 * 
 * Returns: %TRUE if the Create Folder button should be displayed.
 **/
gboolean
gtk_file_chooser_get_create_folders (GtkFileChooser *chooser)
{
  gboolean create_folders;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "create-folders", &create_folders, NULL);

  return create_folders;
}

/**
 * gtk_file_chooser_set_current_name:
 * @chooser: a #GtkFileChooser
 * @name: (type filename): the filename to use, as a UTF-8 string
 * 
 * Sets the current name in the file selector, as if entered
 * by the user. Note that the name passed in here is a UTF-8
 * string rather than a filename. This function is meant for
 * such uses as a suggested name in a “Save As...” dialog.  You can
 * pass “Untitled.doc” or a similarly suitable suggestion for the @name.
 *
 * If you want to preselect a particular existing file, you should use
 * gtk_file_chooser_set_file() instead.
 *
 * Please see the documentation for those functions for an example of using
 * gtk_file_chooser_set_current_name() as well.
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
 * gtk_file_chooser_get_current_name:
 * @chooser: a #GtkFileChooser
 *
 * Gets the current name in the file selector, as entered by the user in the
 * text entry for “Name”.
 *
 * This is meant to be used in save dialogs, to get the currently typed filename
 * when the file itself does not exist yet.
 *
 * Returns: The raw text from the file chooser’s “Name” entry.  Free this with
 * g_free().  Note that this string is not a full pathname or URI; it is
 * whatever the contents of the entry are.  Note also that this string is in
 * UTF-8 encoding, which is not necessarily the system’s encoding for filenames.
 **/
gchar *
gtk_file_chooser_get_current_name (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_current_name (chooser);
}

/**
 * gtk_file_chooser_select_all:
 * @chooser: a #GtkFileChooser
 * 
 * Selects all the files in the current folder of a file chooser.
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
 **/
void
gtk_file_chooser_unselect_all (GtkFileChooser *chooser)
{

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_all (chooser);
}

/**
 * gtk_file_chooser_set_current_folder:
 * @chooser: a #GtkFileChooser
 * @file: the #GFile for the new folder
 * @error: location to store error, or %NULL.
 *
 * Sets the current folder for @chooser from a #GFile.
 *
 * Returns: %TRUE if the folder could be changed successfully, %FALSE
 * otherwise.
 **/
gboolean
gtk_file_chooser_set_current_folder (GtkFileChooser  *chooser,
                                     GFile           *file,
                                     GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->set_current_folder (chooser, file, error);
}

/**
 * gtk_file_chooser_get_current_folder:
 * @chooser: a #GtkFileChooser
 *
 * Gets the current folder of @chooser as #GFile.
 *
 * Returns: (transfer full): the #GFile for the current folder.
 */
GFile *
gtk_file_chooser_get_current_folder (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_current_folder (chooser);
}

/**
 * gtk_file_chooser_select_file:
 * @chooser: a #GtkFileChooser
 * @file: the file to select
 * @error: (allow-none): location to store error, or %NULL
 * 
 * Selects the file referred to by @file.
 *
 * Returns: Not useful.
 **/
gboolean
gtk_file_chooser_select_file (GtkFileChooser  *chooser,
                              GFile           *file,
                              GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_file (chooser, file, error);
}

/**
 * gtk_file_chooser_unselect_file:
 * @chooser: a #GtkFileChooser
 * @file: a #GFile
 * 
 * Unselects the file referred to by @file. If the file is not in the current
 * directory, does not exist, or is otherwise not currently selected, does nothing.
 **/
void
gtk_file_chooser_unselect_file (GtkFileChooser *chooser,
                                GFile          *file)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (G_IS_FILE (file));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_file (chooser, file);
}

/**
 * gtk_file_chooser_get_files:
 * @chooser: a #GtkFileChooser
 * 
 * Lists all the selected files and subfolders in the current folder of @chooser
 * as #GFile.
 *
 * Returns: (element-type GFile) (transfer full): a list
 *   containing a #GFile for each selected file and subfolder in the
 *   current folder.  Free the returned list with g_slist_free(), and
 *   the files with g_object_unref().
 **/
GSList *
gtk_file_chooser_get_files (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_files (chooser);
}

/**
 * gtk_file_chooser_set_file:
 * @chooser: a #GtkFileChooser
 * @file: the #GFile to set as current
 * @error: (allow-none): location to store the error, or %NULL to ignore errors.
 *
 * Sets @file as the current filename for the file chooser, by changing
 * to the file’s parent folder and actually selecting the file in list.  If
 * the @chooser is in %GTK_FILE_CHOOSER_ACTION_SAVE mode, the file’s base name
 * will also appear in the dialog’s file name entry.
 *
 * If the file name isn’t in the current folder of @chooser, then the current
 * folder of @chooser will be changed to the folder containing @filename. This
 * is equivalent to a sequence of gtk_file_chooser_unselect_all() followed by
 * gtk_file_chooser_select_filename().
 *
 * Note that the file must exist, or nothing will be done except
 * for the directory change.
 *
 * If you are implementing a save dialog,
 * you should use this function if you already have a file name to which the
 * user may save; for example, when the user opens an existing file and then
 * does Save As...  If you don’t have
 * a file name already — for example, if the user just created a new
 * file and is saving it for the first time, do not call this function.
 * Instead, use something similar to this:
 * |[<!-- language="C" -->
 * if (document_is_new)
 *   {
 *     // the user just created a new document
 *     gtk_file_chooser_set_current_folder (chooser, default_file_for_saving);
 *     gtk_file_chooser_set_current_name (chooser, "Untitled document");
 *   }
 * else
 *   {
 *     // the user edited an existing document
 *     gtk_file_chooser_set_file (chooser, existing_file);
 *   }
 * ]|
 *
 * Returns: Not useful.
 **/
gboolean
gtk_file_chooser_set_file (GtkFileChooser  *chooser,
                           GFile           *file,
                           GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  gtk_file_chooser_unselect_all (chooser);
  return gtk_file_chooser_select_file (chooser, file, error);
}

/**
 * gtk_file_chooser_get_file:
 * @chooser: a #GtkFileChooser
 *
 * Gets the #GFile for the currently selected file in
 * the file selector. If multiple files are selected,
 * one of the files will be returned at random.
 *
 * If the file chooser is in folder mode, this function returns the selected
 * folder.
 *
 * Returns: (transfer full): a selected #GFile. You own the returned file;
 *     use g_object_unref() to release it.
 **/
GFile *
gtk_file_chooser_get_file (GtkFileChooser *chooser)
{
  GSList *list;
  GFile *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  list = gtk_file_chooser_get_files (chooser);
  if (list)
    {
      result = list->data;
      list = g_slist_delete_link (list, list);

      g_slist_free_full (list, g_object_unref);
    }

  return result;
}

/*< private >
 * _gtk_file_chooser_get_file_system:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the #GtkFileSystem of @chooser; this is an internal
 * implementation detail, used for conversion between paths
 * and filenames and URIs.
 * 
 * Returns: the file system for @chooser.
 **/
GtkFileSystem *
_gtk_file_chooser_get_file_system (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_file_system (chooser);
}

/**
 * gtk_file_chooser_add_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @folder: a #GFile for the folder to add
 * @error: location to store error, or %NULL
 * 
 * Adds a folder to be displayed with the shortcut folders in a file chooser.
 * 
 * Returns: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.
 **/
gboolean
gtk_file_chooser_add_shortcut_folder (GtkFileChooser  *chooser,
                                      GFile           *folder,
                                      GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (folder), FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_shortcut_folder (chooser, folder, error);
}

/**
 * gtk_file_chooser_remove_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @folder: a #GFile for the folder to remove
 * @error: location to store error, or %NULL
 * 
 * Removes a folder from the shortcut folders in a file chooser.
 * 
 * Returns: %TRUE if the folder could be removed successfully, %FALSE
 * otherwise.
 **/
gboolean
gtk_file_chooser_remove_shortcut_folder (GtkFileChooser  *chooser,
                                         GFile           *folder,
                                         GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (folder), FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_shortcut_folder (chooser, folder, error);
}

/**
 * gtk_file_chooser_add_filter:
 * @chooser: a #GtkFileChooser
 * @filter: (transfer full): a #GtkFileFilter
 * 
 * Adds @filter to the list of filters that the user can select between.
 * When a filter is selected, only files that are passed by that
 * filter are displayed. 
 * 
 * Note that the @chooser takes ownership of the filter, so you have to 
 * ref and sink it if you want to keep a reference.
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
 * Returns: (element-type GtkFileFilter) (transfer container): a
 *  #GSList containing the current set of user selectable filters. The
 *  contents of the list are owned by GTK+, but you must free the list
 *  itself with g_slist_free() when you are done with it.
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
 * Returns: (nullable) (transfer none): the current filter, or %NULL
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
 * gtk_file_chooser_list_shortcut_folders:
 * @chooser: a #GtkFileChooser
 * 
 * Queries the list of shortcut folders in the file chooser, as set by
 * gtk_file_chooser_add_shortcut_folder().
 *
 * Returns: (nullable) (element-type Gio.File) (transfer full): A list
 * of folder filenames, or %NULL if there are no shortcut folders.
 */
GSList *
gtk_file_chooser_list_shortcut_folders (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->list_shortcut_folders (chooser);
}

/**
 * gtk_file_chooser_add_choice:
 * @chooser: a #GtkFileChooser
 * @id: id for the added choice
 * @label: user-visible label for the added choice
 * @options: (nullable) (array zero-terminated=1): ids for the options of the choice, or %NULL for a boolean choice
 * @option_labels: (nullable) (array zero-terminated=1): user-visible labels for the options, must be the same length as @options
 *
 * Adds a 'choice' to the file chooser. This is typically implemented
 * as a combobox or, for boolean choices, as a checkbutton. You can select
 * a value using gtk_file_chooser_set_choice() before the dialog is shown,
 * and you can obtain the user-selected value in the ::response signal handler
 * using gtk_file_chooser_get_choice().
 */
void
gtk_file_chooser_add_choice (GtkFileChooser  *chooser,
                             const char      *id,
                             const char      *label,
                             const char     **options,
                             const char     **option_labels)
{
  GtkFileChooserIface *iface = GTK_FILE_CHOOSER_GET_IFACE (chooser);

  if (iface->add_choice)
    iface->add_choice (chooser, id, label, options, option_labels);
}

/**
 * gtk_file_chooser_remove_choice:
 * @chooser: a #GtkFileChooser
 * @id: the ID of the choice to remove
 *
 * Removes a 'choice' that has been added with gtk_file_chooser_add_choice().
 */
void
gtk_file_chooser_remove_choice (GtkFileChooser  *chooser,
                                const char      *id)
{
  GtkFileChooserIface *iface = GTK_FILE_CHOOSER_GET_IFACE (chooser);

  if (iface->remove_choice)
    iface->remove_choice (chooser, id);
}

/**
 * gtk_file_chooser_set_choice:
 * @chooser: a #GtkFileChooser
 * @id: the ID of the choice to set
 * @option: the ID of the option to select
 *
 * Selects an option in a 'choice' that has been added with
 * gtk_file_chooser_add_choice(). For a boolean choice, the
 * possible options are "true" and "false".
 */
void
gtk_file_chooser_set_choice (GtkFileChooser  *chooser,
                             const char      *id,
                             const char      *option)
{
  GtkFileChooserIface *iface = GTK_FILE_CHOOSER_GET_IFACE (chooser);

  if (iface->set_choice)
    iface->set_choice (chooser, id, option);
}

/**
 * gtk_file_chooser_get_choice:
 * @chooser: a #GtkFileChooser
 * @id: the ID of the choice to get
 *
 * Gets the currently selected option in the 'choice' with the given ID.
 *
 * Returns: the ID of the currenly selected option
 */
const char *
gtk_file_chooser_get_choice (GtkFileChooser  *chooser,
                             const char      *id)
{
  GtkFileChooserIface *iface = GTK_FILE_CHOOSER_GET_IFACE (chooser);

  if (iface->get_choice)
    return iface->get_choice (chooser, id);

  return NULL;
}

