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
 * #GtkFileChooser, your program can get the selected names
 * either as filenames or as URIs.  For URIs, the normal escaping
 * rules are applied if the URI contains non-ASCII characters.
 * However, filenames are always returned in
 * the character set specified by the
 * `G_FILENAME_ENCODING` environment variable.
 * Please see the GLib documentation for more details about this
 * variable.
 *
 * This means that while you can pass the result of
 * gtk_file_chooser_get_filename() to open() or fopen(),
 * you may not be able to directly set it as the text of a
 * #GtkLabel widget unless you convert it first to UTF-8,
 * which all GTK+ widgets expect. You should use g_filename_to_utf8()
 * to convert filenames into strings that can be passed to GTK+
 * widgets.
 *
 * # Adding a Preview Widget
 *
 * You can add a custom preview widget to a file chooser and then
 * get notification about when the preview needs to be updated.
 * To install a preview widget, use
 * gtk_file_chooser_set_preview_widget().  Then, connect to the
 * #GtkFileChooser::update-preview signal to get notified when
 * you need to update the contents of the preview.
 *
 * Your callback should use
 * gtk_file_chooser_get_preview_filename() to see what needs
 * previewing.  Once you have generated the preview for the
 * corresponding file, you must call
 * gtk_file_chooser_set_preview_widget_active() with a boolean
 * flag that indicates whether your callback could successfully
 * generate a preview.
 *
 * ## Example: Using a Preview Widget ## {#gtkfilechooser-preview}
 * |[<!-- language="C" -->
 * {
 *   GtkImage *preview;
 *
 *   ...
 *
 *   preview = gtk_image_new ();
 *
 *   gtk_file_chooser_set_preview_widget (my_file_chooser, preview);
 *   g_signal_connect (my_file_chooser, "update-preview",
 * 		    G_CALLBACK (update_preview_cb), preview);
 * }
 *
 * static void
 * update_preview_cb (GtkFileChooser *file_chooser, gpointer data)
 * {
 *   GtkWidget *preview;
 *   char *filename;
 *   GdkPixbuf *pixbuf;
 *   gboolean have_preview;
 *
 *   preview = GTK_WIDGET (data);
 *   filename = gtk_file_chooser_get_preview_filename (file_chooser);
 *
 *   pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 128, 128, NULL);
 *   have_preview = (pixbuf != NULL);
 *   g_free (filename);
 *
 *   gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
 *   if (pixbuf)
 *     g_object_unref (pixbuf);
 *
 *   gtk_file_chooser_set_preview_widget_active (file_chooser, have_preview);
 * }
 * ]|
 *
 * # Adding Extra Widgets
 *
 * You can add extra widgets to a file chooser to provide options
 * that are not present in the default design.  For example, you
 * can add a toggle button to give the user the option to open a
 * file in read-only mode.  You can use
 * gtk_file_chooser_set_extra_widget() to insert additional
 * widgets in a file chooser.
 *
 * An example for adding extra widgets:
 * |[<!-- language="C" -->
 *
 *   GtkWidget *toggle;
 *
 *   ...
 *
 *   toggle = gtk_check_button_new_with_label ("Open file read-only");
 *   gtk_widget_show (toggle);
 *   gtk_file_chooser_set_extra_widget (my_file_chooser, toggle);
 * }
 * ]|
 *
 * If you want to set more than one extra widget in the file
 * chooser, you can a container such as a #GtkBox or a #GtkGrid
 * and include your widgets in it.  Then, set the container as
 * the whole extra widget.
 */


typedef GtkFileChooserIface GtkFileChooserInterface;
G_DEFINE_INTERFACE (GtkFileChooser, gtk_file_chooser, G_TYPE_OBJECT);

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
   * gtk_file_chooser_set_current_folder_uri(),
   * gtk_file_chooser_get_current_folder_uri().
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
		NULL,
		G_TYPE_NONE, 0);

  /**
   * GtkFileChooser::update-preview:
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
   * must call gtk_file_chooser_set_preview_widget_active() to inform the file
   * chooser about whether the preview was generated successfully or not.
   *
   * Please see the example code in
   * [Using a Preview Widget][gtkfilechooser-preview].
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
   * See also: gtk_file_chooser_get_filename(),
   * gtk_file_chooser_get_filenames(), gtk_file_chooser_get_uri(),
   * gtk_file_chooser_get_uris().
   */
  g_signal_new (I_("file-activated"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, file_activated),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 0);

  /**
   * GtkFileChooser::confirm-overwrite:
   * @chooser: the object which received the signal.
   *
   * This signal gets emitted whenever it is appropriate to present a
   * confirmation dialog when the user has selected a file name that
   * already exists.  The signal only gets emitted when the file
   * chooser is in %GTK_FILE_CHOOSER_ACTION_SAVE mode.
   *
   * Most applications just need to turn on the
   * #GtkFileChooser:do-overwrite-confirmation property (or call the
   * gtk_file_chooser_set_do_overwrite_confirmation() function), and
   * they will automatically get a standard confirmation dialog.
   * Applications which need to customize this behavior should do
   * that, and also connect to the #GtkFileChooser::confirm-overwrite
   * signal.
   *
   * A signal handler for this signal must return a
   * #GtkFileChooserConfirmation value, which indicates the action to
   * take.  If the handler determines that the user wants to select a
   * different filename, it should return
   * %GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN.  If it determines
   * that the user is satisfied with his choice of file name, it
   * should return %GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME.
   * On the other hand, if it determines that the standard confirmation
   * dialog should be used, it should return
   * %GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM. The following example
   * illustrates this.
   *
   * ## Custom confirmation ## {#gtkfilechooser-confirmation}
   *
   * |[<!-- language="C" -->
   * static GtkFileChooserConfirmation
   * confirm_overwrite_callback (GtkFileChooser *chooser, gpointer data)
   * {
   *   char *uri;
   *
   *   uri = gtk_file_chooser_get_uri (chooser);
   *
   *   if (is_uri_read_only (uri))
   *     {
   *       if (user_wants_to_replace_read_only_file (uri))
   *         return GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME;
   *       else
   *         return GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN;
   *     } else
   *       return GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM; // fall back to the default dialog
   * }
   *
   * ...
   *
   * chooser = gtk_file_chooser_dialog_new (...);
   *
   * gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
   * g_signal_connect (chooser, "confirm-overwrite",
   *                   G_CALLBACK (confirm_overwrite_callback), NULL);
   *
   * if (gtk_dialog_run (chooser) == GTK_RESPONSE_ACCEPT)
   *         save_to_file (gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
   *
   * gtk_widget_destroy (chooser);
   * ]|
   *
   * Returns: a #GtkFileChooserConfirmation value that indicates which
   *  action to take after emitting the signal.
   */
  g_signal_new (I_("confirm-overwrite"),
		iface_type,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GtkFileChooserIface, confirm_overwrite),
		confirm_overwrite_accumulator, NULL,
		_gtk_marshal_ENUM__VOID,
		GTK_TYPE_FILE_CHOOSER_CONFIRMATION, 0);
  
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
				       g_param_spec_boolean ("local-only",
							     P_("Local Only"),
							     P_("Whether the selected file(s) should be limited to local file: URLs"),
							     FALSE,
							     GTK_PARAM_READWRITE));
  g_object_interface_install_property (iface,
				       g_param_spec_object ("preview-widget",
							    P_("Preview widget"),
							    P_("Application supplied widget for custom previews."),
							    GTK_TYPE_WIDGET,
							    GTK_PARAM_READWRITE));
  g_object_interface_install_property (iface,
				       g_param_spec_boolean ("preview-widget-active",
							     P_("Preview Widget Active"),
							     P_("Whether the application supplied widget for custom previews should be shown."),
							     TRUE,
							     GTK_PARAM_READWRITE));
  g_object_interface_install_property (iface,
				       g_param_spec_boolean ("use-preview-label",
							     P_("Use Preview Label"),
							     P_("Whether to display a label with the name of the previewed file."),
							     TRUE,
							     GTK_PARAM_READWRITE));
  g_object_interface_install_property (iface,
				       g_param_spec_object ("extra-widget",
							    P_("Extra widget"),
							    P_("Application supplied widget for extra options."),
							    GTK_TYPE_WIDGET,
							    GTK_PARAM_READWRITE));
  g_object_interface_install_property (iface,
				       g_param_spec_boolean ("select-multiple",
							     P_("Select Multiple"),
							     P_("Whether to allow multiple files to be selected"),
							     FALSE,
							     GTK_PARAM_READWRITE));

  g_object_interface_install_property (iface,
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
   */
  g_object_interface_install_property (iface,
				       g_param_spec_boolean ("do-overwrite-confirmation",
							     P_("Do overwrite confirmation"),
							     P_("Whether a file chooser in save mode "
								"will present an overwrite confirmation dialog "
								"if necessary."),
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
 * gtk_file_chooser_set_local_only:
 * @chooser: a #GtkFileChooser
 * @local_only: %TRUE if only local files can be selected
 * 
 * Sets whether only local files can be selected in the
 * file selector. If @local_only is %TRUE (the default is %FALSE),
 * then the selected file or files are guaranteed to be
 * accessible through the operating system’s native file
 * system and therefore the application only
 * needs to worry about the filename functions in
 * #GtkFileChooser, like gtk_file_chooser_get_filename(),
 * rather than the URI functions like
 * gtk_file_chooser_get_uri(),
 *
 * On some systems non-native files may still be
 * available using the native filesystem via a userspace
 * filesystem (FUSE).
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
 * @chooser: a #GtkFileChooser
 * 
 * Gets whether only local files can be selected in the
 * file selector. See gtk_file_chooser_set_local_only()
 * 
 * Returns: %TRUE if only local files can be selected.
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
 * gtk_file_chooser_get_filename:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the filename for the currently selected file in
 * the file selector. The filename is returned as an absolute path. If
 * multiple files are selected, one of the filenames will be returned at
 * random.
 *
 * If the file chooser is in folder mode, this function returns the selected
 * folder.
 * 
 * Returns: (nullable) (type filename): The currently selected filename,
 *  or %NULL if no file is selected, or the selected file can't
 *  be represented with a local filename. Free with g_free().
 **/
gchar *
gtk_file_chooser_get_filename (GtkFileChooser *chooser)
{
  GFile *file;
  gchar *result = NULL;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file = gtk_file_chooser_get_file (chooser);

  if (file)
    {
      result = g_file_get_path (file);
      g_object_unref (file);
    }

  return result;
}

/**
 * gtk_file_chooser_set_filename:
 * @chooser: a #GtkFileChooser
 * @filename: (type filename): the filename to set as current
 * 
 * Sets @filename as the current filename for the file chooser, by changing to
 * the file’s parent folder and actually selecting the file in list; all other
 * files will be unselected.  If the @chooser is in
 * %GTK_FILE_CHOOSER_ACTION_SAVE mode, the file’s base name will also appear in
 * the dialog’s file name entry.
 *
 * Note that the file must exist, or nothing will be done except
 * for the directory change.
 *
 * You should use this function only when implementing a save
 * dialog for which you already have a file name to which
 * the user may save.  For example, when the user opens an existing file and
 * then does Save As... to save a copy or
 * a modified version.  If you don’t have a file name already — for
 * example, if the user just created a new file and is saving it for the first
 * time, do not call this function.  Instead, use something similar to this:
 * |[<!-- language="C" -->
 * if (document_is_new)
 *   {
 *     // the user just created a new document
 *     gtk_file_chooser_set_current_name (chooser, "Untitled document");
 *   }
 * else
 *   {
 *     // the user edited an existing document
 *     gtk_file_chooser_set_filename (chooser, existing_filename);
 *   }
 * ]|
 *
 * In the first case, the file chooser will present the user with useful suggestions
 * as to where to save his new file.  In the second case, the file’s existing location
 * is already known, so the file chooser will use it.
 * 
 * Returns: Not useful.
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
 * @filename: (type filename): the filename to select
 * 
 * Selects a filename. If the file name isn’t in the current
 * folder of @chooser, then the current folder of @chooser will
 * be changed to the folder containing @filename.
 *
 * Returns: Not useful.
 *
 * See also: gtk_file_chooser_set_filename()
 **/
gboolean
gtk_file_chooser_select_filename (GtkFileChooser *chooser,
				  const gchar    *filename)
{
  GFile *file;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  file = g_file_new_for_path (filename);
  result = gtk_file_chooser_select_file (chooser, file, NULL);
  g_object_unref (file);

  return result;
}

/**
 * gtk_file_chooser_unselect_filename:
 * @chooser: a #GtkFileChooser
 * @filename: (type filename): the filename to unselect
 * 
 * Unselects a currently selected filename. If the filename
 * is not in the current directory, does not exist, or
 * is otherwise not currently selected, does nothing.
 **/
void
gtk_file_chooser_unselect_filename (GtkFileChooser *chooser,
				    const char     *filename)
{
  GFile *file;

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (filename != NULL);

  file = g_file_new_for_path (filename);
  gtk_file_chooser_unselect_file (chooser, file);
  g_object_unref (file);
}

/* Converts a list of GFile* to a list of strings using the specified function */
static GSList *
files_to_strings (GSList  *files,
		  gchar * (*convert_func) (GFile *file))
{
  GSList *strings;

  strings = NULL;

  for (; files; files = files->next)
    {
      GFile *file;
      gchar *string;

      file = files->data;
      string = (* convert_func) (file);

      if (string)
	strings = g_slist_prepend (strings, string);
    }

  return g_slist_reverse (strings);
}

static gchar *
file_to_uri_with_native_path (GFile *file)
{
  gchar *result = NULL;
  gchar *native;

  native = g_file_get_path (file);
  if (native)
    {
      result = g_filename_to_uri (native, NULL, NULL); /* NULL-GError */
      g_free (native);
    }

  return result;
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
 * Returns: (element-type filename) (transfer full): a #GSList
 *    containing the filenames of all selected files and subfolders in
 *    the current folder. Free the returned list with g_slist_free(),
 *    and the filenames with g_free().
 **/
GSList *
gtk_file_chooser_get_filenames (GtkFileChooser *chooser)
{
  GSList *files, *result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  files = gtk_file_chooser_get_files (chooser);

  result = files_to_strings (files, g_file_get_path);
  g_slist_free_full (files, g_object_unref);

  return result;
}

/**
 * gtk_file_chooser_set_current_folder:
 * @chooser: a #GtkFileChooser
 * @filename: (type filename): the full path of the new current folder
 * 
 * Sets the current folder for @chooser from a local filename.
 * The user will be shown the full contents of the current folder,
 * plus user interface elements for navigating to other folders.
 *
 * In general, you should not use this function.  See the
 * [section on setting up a file chooser dialog][gtkfilechooserdialog-setting-up]
 * for the rationale behind this.
 *
 * Returns: Not useful.
 **/
gboolean
gtk_file_chooser_set_current_folder (GtkFileChooser *chooser,
				     const gchar    *filename)
{
  GFile *file;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  file = g_file_new_for_path (filename);
  result = gtk_file_chooser_set_current_folder_file (chooser, file, NULL);
  g_object_unref (file);

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
 * (e.g. "/home/username/Documents"), which is not the same
 * as the currently-selected folder if the chooser is in
 * %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER mode
 * (e.g. "/home/username/Documents/selected-folder/".  To get the
 * currently-selected folder in that mode, use gtk_file_chooser_get_uri() as the
 * usual way to get the selection.
 * 
 * Returns: (nullable) (type filename): the full path of the current
 * folder, or %NULL if the current path cannot be represented as a local
 * filename.  Free with g_free().  This function will also return
 * %NULL if the file chooser was unable to load the last folder that
 * was requested from it; for example, as would be for calling
 * gtk_file_chooser_set_current_folder() on a nonexistent folder.
 **/
gchar *
gtk_file_chooser_get_current_folder (GtkFileChooser *chooser)
{
  GFile *file;
  gchar *filename;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file = gtk_file_chooser_get_current_folder_file (chooser);
  if (!file)
    return NULL;

  filename = g_file_get_path (file);
  g_object_unref (file);

  return filename;
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
 * gtk_file_chooser_set_filename() or gtk_file_chooser_set_uri() instead.
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
 * when the file itself does not exist yet.  For example, an application that
 * adds a custom extra widget to the file chooser for “file format” may want to
 * change the extension of the typed filename based on the chosen format, say,
 * from “.jpg” to “.png”.
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
 * Returns: (nullable) (transfer full): The currently selected URI, or %NULL
 *    if no file is selected. If gtk_file_chooser_set_local_only() is set to
 *    %TRUE (the default) a local URI will be returned for any FUSE locations.
 *    Free with g_free()
 **/
gchar *
gtk_file_chooser_get_uri (GtkFileChooser *chooser)
{
  GFile *file;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file = gtk_file_chooser_get_file (chooser);
  if (file)
    {
      if (gtk_file_chooser_get_local_only (chooser))
	  result = file_to_uri_with_native_path (file);
      else 
          result = g_file_get_uri (file);

      g_object_unref (file);
    }

  return result;
}

/**
 * gtk_file_chooser_set_uri:
 * @chooser: a #GtkFileChooser
 * @uri: the URI to set as current
 * 
 * Sets the file referred to by @uri as the current file for the file chooser,
 * by changing to the URI’s parent folder and actually selecting the URI in the
 * list.  If the @chooser is %GTK_FILE_CHOOSER_ACTION_SAVE mode, the URI’s base
 * name will also appear in the dialog’s file name entry.
 *
 * Note that the URI must exist, or nothing will be done except for the 
 * directory change.
 *
 * You should use this function only when implementing a save
 * dialog for which you already have a file name to which
 * the user may save.  For example, when the user opens an existing file and then
 * does Save As... to save a copy or a
 * modified version.  If you don’t have a file name already — for example,
 * if the user just created a new file and is saving it for the first time, do
 * not call this function.  Instead, use something similar to this:
 * |[<!-- language="C" -->
 * if (document_is_new)
 *   {
 *     // the user just created a new document
 *     gtk_file_chooser_set_current_name (chooser, "Untitled document");
 *   }
 * else
 *   {
 *     // the user edited an existing document
 *     gtk_file_chooser_set_uri (chooser, existing_uri);
 *   }
 * ]|
 *
 *
 * In the first case, the file chooser will present the user with useful suggestions
 * as to where to save his new file.  In the second case, the file’s existing location
 * is already known, so the file chooser will use it.
 * 
 * Returns: Not useful.
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
 * Selects the file to by @uri. If the URI doesn’t refer to a
 * file in the current folder of @chooser, then the current folder of
 * @chooser will be changed to the folder containing @filename.
 *
 * Returns: Not useful.
 **/
gboolean
gtk_file_chooser_select_uri (GtkFileChooser *chooser,
			     const char     *uri)
{
  GFile *file;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  file = g_file_new_for_uri (uri);
  result = gtk_file_chooser_select_file (chooser, file, NULL);
  g_object_unref (file);

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
 **/
void
gtk_file_chooser_unselect_uri (GtkFileChooser *chooser,
			       const char     *uri)
{
  GFile *file;

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (uri != NULL);

  file = g_file_new_for_uri (uri);
  gtk_file_chooser_unselect_file (chooser, file);
  g_object_unref (file);
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
 * gtk_file_chooser_get_uris:
 * @chooser: a #GtkFileChooser
 * 
 * Lists all the selected files and subfolders in the current folder of
 * @chooser. The returned names are full absolute URIs.
 *
 * Returns: (element-type utf8) (transfer full): a #GSList containing the URIs of all selected
 *   files and subfolders in the current folder. Free the returned list
 *   with g_slist_free(), and the filenames with g_free().
 **/
GSList *
gtk_file_chooser_get_uris (GtkFileChooser *chooser)
{
  GSList *files, *result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  files = gtk_file_chooser_get_files (chooser);

  if (gtk_file_chooser_get_local_only (chooser))
    result = files_to_strings (files, file_to_uri_with_native_path);
  else
    result = files_to_strings (files, g_file_get_uri);

  g_slist_free_full (files, g_object_unref);

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
 * In general, you should not use this function.  See the
 * [section on setting up a file chooser dialog][gtkfilechooserdialog-setting-up]
 * for the rationale behind this.
 *
 * Returns: %TRUE if the folder could be changed successfully, %FALSE
 * otherwise.
 **/
gboolean
gtk_file_chooser_set_current_folder_uri (GtkFileChooser *chooser,
					 const gchar    *uri)
{
  GFile *file;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  file = g_file_new_for_uri (uri);
  result = gtk_file_chooser_set_current_folder_file (chooser, file, NULL);
  g_object_unref (file);

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
 * (e.g. "file:///home/username/Documents"), which is not the same
 * as the currently-selected folder if the chooser is in
 * %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER mode
 * (e.g. "file:///home/username/Documents/selected-folder/".  To get the
 * currently-selected folder in that mode, use gtk_file_chooser_get_uri() as the
 * usual way to get the selection.
 * 
 * Returns: (nullable) (transfer full): the URI for the current folder.
 * Free with g_free().  This function will also return %NULL if the file chooser
 * was unable to load the last folder that was requested from it; for example,
 * as would be for calling gtk_file_chooser_set_current_folder_uri() on a
 * nonexistent folder.
 */
gchar *
gtk_file_chooser_get_current_folder_uri (GtkFileChooser *chooser)
{
  GFile *file;
  gchar *uri;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file = gtk_file_chooser_get_current_folder_file (chooser);
  if (!file)
    return NULL;

  uri = g_file_get_uri (file);
  g_object_unref (file);

  return uri;
}

/**
 * gtk_file_chooser_set_current_folder_file:
 * @chooser: a #GtkFileChooser
 * @file: the #GFile for the new folder
 * @error: (allow-none): location to store error, or %NULL.
 *
 * Sets the current folder for @chooser from a #GFile.
 * Internal function, see gtk_file_chooser_set_current_folder_uri().
 *
 * Returns: %TRUE if the folder could be changed successfully, %FALSE
 * otherwise.
 **/
gboolean
gtk_file_chooser_set_current_folder_file (GtkFileChooser  *chooser,
                                          GFile           *file,
                                          GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->set_current_folder (chooser, file, error);
}

/**
 * gtk_file_chooser_get_current_folder_file:
 * @chooser: a #GtkFileChooser
 *
 * Gets the current folder of @chooser as #GFile.
 * See gtk_file_chooser_get_current_folder_uri().
 *
 * Returns: (transfer full): the #GFile for the current folder.
 */
GFile *
gtk_file_chooser_get_current_folder_file (GtkFileChooser *chooser)
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
 * Selects the file referred to by @file. An internal function. See
 * _gtk_file_chooser_select_uri().
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
 * as #GFile. An internal function, see gtk_file_chooser_get_uris().
 *
 * Returns: (element-type GFile) (transfer full): a #GSList
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
 *     gtk_file_chooser_set_current_folder_file (chooser, default_file_for_saving);
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

/**
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

/* Preview widget
 */
/**
 * gtk_file_chooser_set_preview_widget:
 * @chooser: a #GtkFileChooser
 * @preview_widget: widget for displaying preview.
 *
 * Sets an application-supplied widget to use to display a custom preview
 * of the currently selected file. To implement a preview, after setting the
 * preview widget, you connect to the #GtkFileChooser::update-preview
 * signal, and call gtk_file_chooser_get_preview_filename() or
 * gtk_file_chooser_get_preview_uri() on each change. If you can
 * display a preview of the new file, update your widget and
 * set the preview active using gtk_file_chooser_set_preview_widget_active().
 * Otherwise, set the preview inactive.
 *
 * When there is no application-supplied preview widget, or the
 * application-supplied preview widget is not active, the file chooser
 * will display no preview at all.
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
 * Returns: (nullable) (transfer none): the current preview widget, or %NULL
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
 * Returns: %TRUE if the preview widget is active for the current filename.
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
 * @use_label: whether to display a label with the name of the previewed file
 * 
 * Sets whether the file chooser should display a label with the name of
 * the file that is being previewed; the default is %TRUE.  Applications that
 * want to draw the whole preview area themselves should set this to %FALSE and
 * display the name themselves in their preview widget.
 *
 * See also: gtk_file_chooser_set_preview_widget()
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
 * Gets whether a label should be drawn with the name of the previewed
 * file.  See gtk_file_chooser_set_use_preview_label().
 * 
 * Returns: %TRUE if the file chooser is set to display a label with the
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
 * gtk_file_chooser_get_preview_file:
 * @chooser: a #GtkFileChooser
 *
 * Gets the #GFile that should be previewed in a custom preview
 * Internal function, see gtk_file_chooser_get_preview_uri().
 *
 * Returns: (nullable) (transfer full): the #GFile for the file to preview,
 *     or %NULL if no file is selected. Free with g_object_unref().
 **/
GFile *
gtk_file_chooser_get_preview_file (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_preview_file (chooser);
}

/**
 * _gtk_file_chooser_add_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @file: file for the folder to add
 * @error: (allow-none): location to store error, or %NULL
 * 
 * Adds a folder to be displayed with the shortcut folders in a file chooser.
 * Internal function, see gtk_file_chooser_add_shortcut_folder().
 * 
 * Returns: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.
 **/
gboolean
_gtk_file_chooser_add_shortcut_folder (GtkFileChooser  *chooser,
				       GFile           *file,
				       GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_shortcut_folder (chooser, file, error);
}

/**
 * _gtk_file_chooser_remove_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @file: file for the folder to remove
 * @error: (allow-none): location to store error, or %NULL
 * 
 * Removes a folder from the shortcut folders in a file chooser.  Internal
 * function, see gtk_file_chooser_remove_shortcut_folder().
 * 
 * Returns: %TRUE if the folder could be removed successfully, %FALSE
 * otherwise.
 **/
gboolean
_gtk_file_chooser_remove_shortcut_folder (GtkFileChooser  *chooser,
					  GFile           *file,
					  GError         **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_shortcut_folder (chooser, file, error);
}

/**
 * gtk_file_chooser_get_preview_filename:
 * @chooser: a #GtkFileChooser
 * 
 * Gets the filename that should be previewed in a custom preview
 * widget. See gtk_file_chooser_set_preview_widget().
 * 
 * Returns: (nullable) (type filename): the filename to preview, or %NULL if
 *  no file is selected, or if the selected file cannot be represented
 *  as a local filename. Free with g_free()
 **/
char *
gtk_file_chooser_get_preview_filename (GtkFileChooser *chooser)
{
  GFile *file;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file = gtk_file_chooser_get_preview_file (chooser);
  if (file)
    {
      result = g_file_get_path (file);
      g_object_unref (file);
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
 * Returns: (nullable) (transfer full): the URI for the file to preview,
 *     or %NULL if no file is selected. Free with g_free().
 **/
char *
gtk_file_chooser_get_preview_uri (GtkFileChooser *chooser)
{
  GFile *file;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file = gtk_file_chooser_get_preview_file (chooser);
  if (file)
    {
      result = g_file_get_uri (file);
      g_object_unref (file);
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
 * Gets the current extra widget; see
 * gtk_file_chooser_set_extra_widget().
 *
 * Returns: (nullable) (transfer none): the current extra widget, or %NULL
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
 * gtk_file_chooser_add_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @folder: (type filename): filename of the folder to add
 * @error: (allow-none): location to store error, or %NULL
 * 
 * Adds a folder to be displayed with the shortcut folders in a file chooser.
 * Note that shortcut folders do not get saved, as they are provided by the
 * application.  For example, you can use this to add a
 * “/usr/share/mydrawprogram/Clipart” folder to the volume list.
 * 
 * Returns: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.  In the latter case, the @error will be set as appropriate.
 **/
gboolean
gtk_file_chooser_add_shortcut_folder (GtkFileChooser    *chooser,
				      const char        *folder,
				      GError           **error)
{
  GFile *file;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (folder != NULL, FALSE);

  file = g_file_new_for_path (folder);
  result = GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_shortcut_folder (chooser, file, error);
  g_object_unref (file);

  return result;
}

/**
 * gtk_file_chooser_remove_shortcut_folder:
 * @chooser: a #GtkFileChooser
 * @folder: (type filename): filename of the folder to remove
 * @error: (allow-none): location to store error, or %NULL
 * 
 * Removes a folder from a file chooser’s list of shortcut folders.
 * 
 * Returns: %TRUE if the operation succeeds, %FALSE otherwise.  
 * In the latter case, the @error will be set as appropriate.
 *
 * See also: gtk_file_chooser_add_shortcut_folder()
 **/
gboolean
gtk_file_chooser_remove_shortcut_folder (GtkFileChooser    *chooser,
					 const char        *folder,
					 GError           **error)
{
  GFile *file;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (folder != NULL, FALSE);

  file = g_file_new_for_path (folder);
  result = GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_shortcut_folder (chooser, file, error);
  g_object_unref (file);

  return result;
}

/**
 * gtk_file_chooser_list_shortcut_folders:
 * @chooser: a #GtkFileChooser
 * 
 * Queries the list of shortcut folders in the file chooser, as set by
 * gtk_file_chooser_add_shortcut_folder().
 *
 * Returns: (nullable) (element-type filename) (transfer full): A list
 * of folder filenames, or %NULL if there are no shortcut folders.
 * Free the returned list with g_slist_free(), and the filenames with
 * g_free().
 **/
GSList *
gtk_file_chooser_list_shortcut_folders (GtkFileChooser *chooser)
{
  GSList *folders;
  GSList *result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  folders = _gtk_file_chooser_list_shortcut_folder_files (chooser);

  result = files_to_strings (folders, g_file_get_path);
  g_slist_free_full (folders, g_object_unref);

  return result;
}

/**
 * gtk_file_chooser_add_shortcut_folder_uri:
 * @chooser: a #GtkFileChooser
 * @uri: URI of the folder to add
 * @error: (allow-none): location to store error, or %NULL
 * 
 * Adds a folder URI to be displayed with the shortcut folders in a file
 * chooser.  Note that shortcut folders do not get saved, as they are provided
 * by the application.  For example, you can use this to add a
 * “file:///usr/share/mydrawprogram/Clipart” folder to the volume list.
 * 
 * Returns: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.  In the latter case, the @error will be set as appropriate.
 **/
gboolean
gtk_file_chooser_add_shortcut_folder_uri (GtkFileChooser    *chooser,
					  const char        *uri,
					  GError           **error)
{
  GFile *file;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  file = g_file_new_for_uri (uri);
  result = GTK_FILE_CHOOSER_GET_IFACE (chooser)->add_shortcut_folder (chooser, file, error);
  g_object_unref (file);

  return result;
}

/**
 * gtk_file_chooser_remove_shortcut_folder_uri:
 * @chooser: a #GtkFileChooser
 * @uri: URI of the folder to remove
 * @error: (allow-none): location to store error, or %NULL
 * 
 * Removes a folder URI from a file chooser’s list of shortcut folders.
 * 
 * Returns: %TRUE if the operation succeeds, %FALSE otherwise.  
 * In the latter case, the @error will be set as appropriate.
 *
 * See also: gtk_file_chooser_add_shortcut_folder_uri()
 **/
gboolean
gtk_file_chooser_remove_shortcut_folder_uri (GtkFileChooser    *chooser,
					     const char        *uri,
					     GError           **error)
{
  GFile *file;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  file = g_file_new_for_uri (uri);
  result = GTK_FILE_CHOOSER_GET_IFACE (chooser)->remove_shortcut_folder (chooser, file, error);
  g_object_unref (file);

  return result;
}

/**
 * gtk_file_chooser_list_shortcut_folder_uris:
 * @chooser: a #GtkFileChooser
 * 
 * Queries the list of shortcut folders in the file chooser, as set by
 * gtk_file_chooser_add_shortcut_folder_uri().
 *
 * Returns: (nullable) (element-type utf8) (transfer full): A list of
 * folder URIs, or %NULL if there are no shortcut folders.  Free the
 * returned list with g_slist_free(), and the URIs with g_free().
 **/
GSList *
gtk_file_chooser_list_shortcut_folder_uris (GtkFileChooser *chooser)
{
  GSList *folders;
  GSList *result;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  folders = _gtk_file_chooser_list_shortcut_folder_files (chooser);

  result = files_to_strings (folders, g_file_get_uri);
  g_slist_free_full (folders, g_object_unref);

  return result;
}

GSList *
_gtk_file_chooser_list_shortcut_folder_files (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->list_shortcut_folders (chooser);
}

/**
 * gtk_file_chooser_set_show_hidden:
 * @chooser: a #GtkFileChooser
 * @show_hidden: %TRUE if hidden files and folders should be displayed.
 * 
 * Sets whether hidden files and folders are displayed in the file selector.  
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
 * Returns: %TRUE if hidden files and folders are displayed.
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
 * Sets whether a file chooser in %GTK_FILE_CHOOSER_ACTION_SAVE mode will present
 * a confirmation dialog if the user types a file name that already exists.  This
 * is %FALSE by default.
 *
 * If set to %TRUE, the @chooser will emit the
 * #GtkFileChooser::confirm-overwrite signal when appropriate.
 *
 * If all you need is the standard confirmation dialog, set this property to %TRUE.
 * You can override the way confirmation is done by actually handling the
 * #GtkFileChooser::confirm-overwrite signal; please refer to its documentation
 * for the details.
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
 * Returns: %TRUE if the file chooser will present a confirmation dialog;
 * %FALSE otherwise.
 **/
gboolean
gtk_file_chooser_get_do_overwrite_confirmation (GtkFileChooser *chooser)
{
  gboolean do_overwrite_confirmation;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "do-overwrite-confirmation", &do_overwrite_confirmation, NULL);

  return do_overwrite_confirmation;
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
 *
 * Compare gtk_file_chooser_set_extra_widget().
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

