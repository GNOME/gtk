/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechoosernative.c: Native File selector dialog
 * Copyright (C) 2015, Red Hat, Inc.
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

#include "gtkfilechoosernativeprivate.h"
#include "gtknativedialogprivate.h"

#include "gtkprivate.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserwidget.h"
#include "gtkfilechooserwidgetprivate.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooserembed.h"
#include "gtkfilesystem.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtktogglebutton.h"
#include "gtkstylecontext.h"
#include "gtkheaderbar.h"
#include "gtklabel.h"
#include "gtkfilechooserentry.h"
#include "gtkfilefilterprivate.h"
#ifdef GDK_WINDOWING_QUARTZ
#include <gdk/quartz/gdkquartz.h>
#endif

/**
 * SECTION:gtkfilechoosernative
 * @Short_description: A native file chooser dialog, suitable for “File/Open” or “File/Save” commands
 * @Title: GtkFileChooserNative
 * @See_also: #GtkFileChooser, #GtkNativeDialog, #GtkFileChooserDialog
 *
 * #GtkFileChooserNative is an abstraction of a dialog box suitable
 * for use with “File/Open” or “File/Save as” commands. By default, this
 * just uses a #GtkFileChooserDialog to implement the actual dialog.
 * However, on certain platforms, such as Windows and macOS, the native platform
 * file chooser is used instead. When the application is running in a
 * sandboxed environment without direct filesystem access (such as Flatpak),
 * #GtkFileChooserNative may call the proper APIs (portals) to let the user
 * choose a file and make it available to the application.
 *
 * While the API of #GtkFileChooserNative closely mirrors #GtkFileChooserDialog, the main
 * difference is that there is no access to any #GtkWindow or #GtkWidget for the dialog.
 * This is required, as there may not be one in the case of a platform native dialog.
 * Showing, hiding and running the dialog is handled by the #GtkNativeDialog functions.
 *
 * ## Typical usage ## {#gtkfilechoosernative-typical-usage}
 *
 * In the simplest of cases, you can the following code to use
 * #GtkFileChooserDialog to select a file for opening:
 *
 * |[
 * GtkFileChooserNative *native;
 * GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
 * gint res;
 *
 * native = gtk_file_chooser_native_new ("Open File",
 *                                       parent_window,
 *                                       action,
 *                                       "_Open",
 *                                       "_Cancel");
 *
 * res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));
 * if (res == GTK_RESPONSE_ACCEPT)
 *   {
 *     char *filename;
 *     GtkFileChooser *chooser = GTK_FILE_CHOOSER (native);
 *     filename = gtk_file_chooser_get_filename (chooser);
 *     open_file (filename);
 *     g_free (filename);
 *   }
 *
 * g_object_unref (native);
 * ]|
 *
 * To use a dialog for saving, you can use this:
 *
 * |[
 * GtkFileChooserNative *native;
 * GtkFileChooser *chooser;
 * GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
 * gint res;
 *
 * native = gtk_file_chooser_native_new ("Save File",
 *                                       parent_window,
 *                                       action,
 *                                       "_Save",
 *                                       "_Cancel");
 * chooser = GTK_FILE_CHOOSER (native);
 *
 * gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);
 *
 * if (user_edited_a_new_document)
 *   gtk_file_chooser_set_current_name (chooser,
 *                                      _("Untitled document"));
 * else
 *   gtk_file_chooser_set_filename (chooser,
 *                                  existing_filename);
 *
 * res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));
 * if (res == GTK_RESPONSE_ACCEPT)
 *   {
 *     char *filename;
 *
 *     filename = gtk_file_chooser_get_filename (chooser);
 *     save_to_file (filename);
 *     g_free (filename);
 *   }
 *
 * g_object_unref (native);
 * ]|
 *
 * For more information on how to best set up a file dialog, see #GtkFileChooserDialog.
 *
 * ## Response Codes ## {#gtkfilechooserdialognative-responses}
 *
 * #GtkFileChooserNative inherits from #GtkNativeDialog, which means it
 * will return #GTK_RESPONSE_ACCEPT if the user accepted, and
 * #GTK_RESPONSE_CANCEL if he pressed cancel. It can also return
 * #GTK_RESPONSE_DELETE_EVENT if the window was unexpectedly closed.
 *
 * ## Differences from #GtkFileChooserDialog ##  {#gtkfilechooserdialognative-differences}
 *
 * There are a few things in the GtkFileChooser API that are not
 * possible to use with #GtkFileChooserNative, as such use would
 * prohibit the use of a native dialog.
 *
 * There is no support for the signals that are emitted when the user
 * navigates in the dialog, including:
 * * #GtkFileChooser::current-folder-changed
 * * #GtkFileChooser::selection-changed
 * * #GtkFileChooser::file-activated
 * * #GtkFileChooser::confirm-overwrite
 *
 * You can also not use the methods that directly control user navigation:
 * * gtk_file_chooser_unselect_filename()
 * * gtk_file_chooser_select_all()
 * * gtk_file_chooser_unselect_all()
 *
 * If you need any of the above you will have to use #GtkFileChooserDialog directly.
 *
 * No operations that change the the dialog work while the dialog is
 * visible. Set all the properties that are required before showing the dialog.
 *
 * ## Win32 details ## {#gtkfilechooserdialognative-win32}
 *
 * On windows the IFileDialog implementation (added in Windows Vista) is
 * used. It supports many of the features that #GtkFileChooserDialog
 * does, but there are some things it does not handle:
 *
 * * Extra widgets added with gtk_file_chooser_set_extra_widget().
 *
 * * Use of custom previews by connecting to #GtkFileChooser::update-preview.
 *
 * * Any #GtkFileFilter added using a mimetype or custom filter.
 *
 * If any of these features are used the regular #GtkFileChooserDialog
 * will be used in place of the native one.
 *
 * ## Portal details ## {#gtkfilechooserdialognative-portal}
 *
 * When the org.freedesktop.portal.FileChooser portal is available on the
 * session bus, it is used to bring up an out-of-process file chooser. Depending
 * on the kind of session the application is running in, this may or may not
 * be a GTK+ file chooser. In this situation, the following things are not
 * supported and will be silently ignored:
 *
 * * Extra widgets added with gtk_file_chooser_set_extra_widget().
 *
 * * Use of custom previews by connecting to #GtkFileChooser::update-preview.
 *
 * * Any #GtkFileFilter added with a custom filter.
 *
 * ## macOS details ## {#gtkfilechooserdialognative-macos}
 *
 * On macOS the NSSavePanel and NSOpenPanel classes are used to provide native
 * file chooser dialogs. Some features provided by #GtkFileChooserDialog are
 * not supported:
 *
 * * Extra widgets added with gtk_file_chooser_set_extra_widget(), unless the
 *   widget is an instance of GtkLabel, in which case the label text will be used
 *   to set the NSSavePanel message instance property.
 *
 * * Use of custom previews by connecting to #GtkFileChooser::update-preview.
 *
 * * Any #GtkFileFilter added with a custom filter.
 *
 * * Shortcut folders.
 */

enum {
  MODE_FALLBACK,
  MODE_WIN32,
  MODE_QUARTZ,
  MODE_PORTAL,
};

enum {
  PROP_0,
  PROP_ACCEPT_LABEL,
  PROP_CANCEL_LABEL,
  LAST_ARG,
};

static GParamSpec *native_props[LAST_ARG] = { NULL, };

static void    _gtk_file_chooser_native_iface_init   (GtkFileChooserIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserNative, gtk_file_chooser_native, GTK_TYPE_NATIVE_DIALOG,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
                                                _gtk_file_chooser_native_iface_init))


/**
 * gtk_file_chooser_native_get_accept_label:
 * @self: a #GtFileChooserNative
 *
 * Retrieves the custom label text for the accept button.
 *
 * Returns: (nullable): The custom label, or %NULL for the default. This string
 * is owned by GTK+ and should not be modified or freed
 *
 * Since: 3.20
 **/
const char *
gtk_file_chooser_native_get_accept_label (GtkFileChooserNative *self)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_NATIVE (self), NULL);

  return self->accept_label;
}

/**
 * gtk_file_chooser_native_set_accept_label:
 * @self: a #GtFileChooserNative
 * @accept_label: (allow-none): custom label or %NULL for the default
 *
 * Sets the custom label text for the accept button.
 *
 * If characters in @label are preceded by an underscore, they are underlined.
 * If you need a literal underscore character in a label, use “__” (two
 * underscores). The first underlined character represents a keyboard
 * accelerator called a mnemonic.
 * Pressing Alt and that key activates the button.
 *
 * Since: 3.20
 **/
void
gtk_file_chooser_native_set_accept_label (GtkFileChooserNative *self,
                                          const char           *accept_label)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_NATIVE (self));

  g_free (self->accept_label);
  self->accept_label = g_strdup (accept_label);

  g_object_notify_by_pspec (G_OBJECT (self), native_props[PROP_ACCEPT_LABEL]);
}

/**
 * gtk_file_chooser_native_get_cancel_label:
 * @self: a #GtFileChooserNative
 *
 * Retrieves the custom label text for the cancel button.
 *
 * Returns: (nullable): The custom label, or %NULL for the default. This string
 * is owned by GTK+ and should not be modified or freed
 *
 * Since: 3.20
 **/
const char *
gtk_file_chooser_native_get_cancel_label (GtkFileChooserNative *self)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_NATIVE (self), NULL);

  return self->cancel_label;
}

/**
 * gtk_file_chooser_native_set_cancel_label:
 * @self: a #GtFileChooserNative
 * @cancel_label: (allow-none): custom label or %NULL for the default
 *
 * Sets the custom label text for the cancel button.
 *
 * If characters in @label are preceded by an underscore, they are underlined.
 * If you need a literal underscore character in a label, use “__” (two
 * underscores). The first underlined character represents a keyboard
 * accelerator called a mnemonic.
 * Pressing Alt and that key activates the button.
 *
 * Since: 3.20
 **/
void
gtk_file_chooser_native_set_cancel_label (GtkFileChooserNative *self,
                                         const char           *cancel_label)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_NATIVE (self));

  g_free (self->cancel_label);
  self->cancel_label = g_strdup (cancel_label);

  g_object_notify_by_pspec (G_OBJECT (self), native_props[PROP_CANCEL_LABEL]);
}

static GtkFileChooserNativeChoice *
find_choice (GtkFileChooserNative *self,
             const char           *id)
{
  GSList *l;

  for (l = self->choices; l; l = l->next)
    {
      GtkFileChooserNativeChoice *choice = l->data;

      if (strcmp (choice->id, id) == 0)
        return choice;
    }

  return NULL;
}

static void
gtk_file_chooser_native_choice_free (GtkFileChooserNativeChoice *choice)
{
  g_free (choice->id);
  g_free (choice->label);
  g_strfreev (choice->options);
  g_strfreev (choice->option_labels);
  g_free (choice->selected);
  g_free (choice);
}

static void
gtk_file_chooser_native_add_choice (GtkFileChooser  *chooser,
                                    const char      *id,
                                    const char      *label,
                                    const char     **options,
                                    const char     **option_labels)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (chooser);
  GtkFileChooserNativeChoice *choice = find_choice (self, id);

  if (choice != NULL)
    {
      g_warning ("Choice with id %s already added to %s %p", id, G_OBJECT_TYPE_NAME (self), self);
      return;
    }

  g_assert ((options == NULL && option_labels == NULL) ||
            g_strv_length ((char **)options) == g_strv_length ((char **)option_labels));

  choice = g_new0 (GtkFileChooserNativeChoice, 1);
  choice->id = g_strdup (id);
  choice->label = g_strdup (label);
  choice->options = g_strdupv ((char **)options);
  choice->option_labels = g_strdupv ((char **)option_labels);

  self->choices = g_slist_append (self->choices, choice);

  gtk_file_chooser_add_choice (GTK_FILE_CHOOSER (self->dialog),
                               id, label, options, option_labels);
}

static void
gtk_file_chooser_native_remove_choice (GtkFileChooser *chooser,
                                       const char     *id)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (chooser);
  GtkFileChooserNativeChoice *choice = find_choice (self, id);

  if (choice == NULL)
    {
      g_warning ("No choice with id %s found in %s %p", id, G_OBJECT_TYPE_NAME (self), self);
      return;
    }

  self->choices = g_slist_remove (self->choices, choice);

  gtk_file_chooser_native_choice_free (choice);

  gtk_file_chooser_remove_choice (GTK_FILE_CHOOSER (self->dialog), id);
}

static void
gtk_file_chooser_native_set_choice (GtkFileChooser *chooser,
                                    const char     *id,
                                    const char     *selected)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (chooser);
  GtkFileChooserNativeChoice *choice = find_choice (self, id);

  if (choice == NULL)
    {
      g_warning ("No choice with id %s found in %s %p", id, G_OBJECT_TYPE_NAME (self), self);
      return;
    }

  if ((choice->options && !g_strv_contains ((const char *const*)choice->options, selected)) ||
      (!choice->options && !g_str_equal (selected, "true") && !g_str_equal (selected, "false")))
    {
      g_warning ("Not a valid option for %s: %s", id, selected);
      return;
    }

  g_free (choice->selected);
  choice->selected = g_strdup (selected);

  gtk_file_chooser_set_choice (GTK_FILE_CHOOSER (self->dialog), id, selected);
}

static const char *
gtk_file_chooser_native_get_choice (GtkFileChooser *chooser,
                                    const char     *id)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (chooser);
  GtkFileChooserNativeChoice *choice = find_choice (self, id);

  if (choice == NULL)
    {
      g_warning ("No choice with id %s found in %s %p", id, G_OBJECT_TYPE_NAME (self), self);
      return NULL;
    }

  if (self->mode == MODE_FALLBACK)
    return gtk_file_chooser_get_choice (GTK_FILE_CHOOSER (self->dialog), id);

  return choice->selected;
}

static void
gtk_file_chooser_native_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)

{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (object);

  switch (prop_id)
    {
    case PROP_ACCEPT_LABEL:
      gtk_file_chooser_native_set_accept_label (self, g_value_get_string (value));
      break;

    case PROP_CANCEL_LABEL:
      gtk_file_chooser_native_set_cancel_label (self, g_value_get_string (value));
      break;

    case GTK_FILE_CHOOSER_PROP_FILTER:
      self->current_filter = g_value_get_object (value);
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (self->dialog), self->current_filter);
      g_object_notify (G_OBJECT (self), "filter");
      break;

    default:
      g_object_set_property (G_OBJECT (self->dialog), pspec->name, value);
      break;
    }
}

static void
gtk_file_chooser_native_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (object);

  switch (prop_id)
    {
    case PROP_ACCEPT_LABEL:
      g_value_set_string (value, self->accept_label);
      break;

    case PROP_CANCEL_LABEL:
      g_value_set_string (value, self->cancel_label);
      break;

    case GTK_FILE_CHOOSER_PROP_FILTER:
      self->current_filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (self->dialog));
      g_value_set_object (value, self->current_filter);
      break;

    default:
      g_object_get_property (G_OBJECT (self->dialog), pspec->name, value);
      break;
    }
}

static void
gtk_file_chooser_native_finalize (GObject *object)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (object);

  g_clear_pointer (&self->current_name, g_free);
  g_clear_object (&self->current_file);
  g_clear_object (&self->current_folder);

  g_clear_pointer (&self->accept_label, g_free);
  g_clear_pointer (&self->cancel_label, g_free);
  gtk_widget_destroy (self->dialog);

  g_slist_free_full (self->custom_files, g_object_unref);
  g_slist_free_full (self->choices, (GDestroyNotify)gtk_file_chooser_native_choice_free);

  G_OBJECT_CLASS (gtk_file_chooser_native_parent_class)->finalize (object);
}

static gint
override_delete_handler (GtkDialog *dialog,
                         GdkEventAny *event,
                         gpointer data)
{
  return TRUE; /* Do not destroy */
}

static void
gtk_file_chooser_native_init (GtkFileChooserNative *self)
{
  /* We always create a File chooser dialog and delegate all properties to it.
   * This way we can reuse that store, plus we always have a dialog we can use
   * in case something makes the native one not work (like the custom widgets) */
  self->dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG, NULL);
  self->cancel_button = gtk_dialog_add_button (GTK_DIALOG (self->dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  self->accept_button = gtk_dialog_add_button (GTK_DIALOG (self->dialog), _("_Open"), GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_default_response (GTK_DIALOG (self->dialog),
                                   GTK_RESPONSE_ACCEPT);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (self->dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_CANCEL,
                                           -1);
G_GNUC_END_IGNORE_DEPRECATIONS

  /* We don't want to destroy on delete event, instead we hide in the response cb */
  g_signal_connect (self->dialog,
                    "delete-event",
                    G_CALLBACK (override_delete_handler),
                    NULL);

  /* This is used, instead of the standard delegate, to ensure that signals are not delegated. */
  g_object_set_qdata (G_OBJECT (self), GTK_FILE_CHOOSER_DELEGATE_QUARK, self->dialog);
}

/**
 * gtk_file_chooser_native_new:
 * @title: (allow-none): Title of the native, or %NULL
 * @parent: (allow-none): Transient parent of the native, or %NULL
 * @action: Open or save mode for the dialog
 * @accept_label: (allow-none): text to go in the accept button, or %NULL for the default
 * @cancel_label: (allow-none): text to go in the cancel button, or %NULL for the default
 *
 * Creates a new #GtkFileChooserNative.
 *
 * Returns: a new #GtkFileChooserNative
 *
 * Since: 3.20
 **/
GtkFileChooserNative *
gtk_file_chooser_native_new (const gchar          *title,
                             GtkWindow            *parent,
                             GtkFileChooserAction  action,
                             const gchar          *accept_label,
                             const gchar          *cancel_label)
{
  GtkFileChooserNative *result;

  result = g_object_new (GTK_TYPE_FILE_CHOOSER_NATIVE,
                         "title", title,
                         "action", action,
                         "transient-for", parent,
                         "accept-label", accept_label,
                         "cancel-label", cancel_label,
                         NULL);

  return result;
}

static void
dialog_response_cb (GtkDialog *dialog,
                    gint response_id,
                    gpointer data)
{
  GtkFileChooserNative *self = data;

  g_signal_handlers_disconnect_by_func (self->dialog, dialog_response_cb, self);
  gtk_widget_hide (self->dialog);

  _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (self), response_id);
}

static void
dialog_update_preview_cb (GtkFileChooser *file_chooser,
                          gpointer data)
{
  g_signal_emit_by_name (data, "update-preview");
}

static void
show_dialog (GtkFileChooserNative *self)
{
  GtkFileChooserAction action;
  const char *accept_label, *cancel_label;

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self->dialog));

  accept_label = self->accept_label;
  if (accept_label == NULL)
    accept_label = (action == GTK_FILE_CHOOSER_ACTION_SAVE) ? _("_Save") :  _("_Open");

  gtk_button_set_label (GTK_BUTTON (self->accept_button), accept_label);

  cancel_label = self->cancel_label;
  if (cancel_label == NULL)
    cancel_label = _("_Cancel");

  gtk_button_set_label (GTK_BUTTON (self->cancel_button), cancel_label);

  gtk_window_set_title (GTK_WINDOW (self->dialog),
                        gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (self)));

  gtk_window_set_transient_for (GTK_WINDOW (self->dialog),
                                gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (self)));

  gtk_window_set_modal (GTK_WINDOW (self->dialog),
                        gtk_native_dialog_get_modal (GTK_NATIVE_DIALOG (self)));

  g_signal_connect (self->dialog,
                    "response",
                    G_CALLBACK (dialog_response_cb),
                    self);

  g_signal_connect (self->dialog,
                    "update-preview",
                    G_CALLBACK (dialog_update_preview_cb),
                    self);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_window_present (GTK_WINDOW (self->dialog));
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
hide_dialog (GtkFileChooserNative *self)
{
  g_signal_handlers_disconnect_by_func (self->dialog, dialog_response_cb, self);
  g_signal_handlers_disconnect_by_func (self->dialog, dialog_update_preview_cb, self);
  gtk_widget_hide (self->dialog);
}

static gboolean
gtk_file_chooser_native_set_current_folder (GtkFileChooser    *chooser,
                                            GFile             *file,
                                            GError           **error)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (chooser);
  gboolean res;

  res = gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (self->dialog),
                                                  file, error);


  if (res)
    {
      g_set_object (&self->current_folder, file);

      g_clear_object (&self->current_file);
    }

  return res;
}

static gboolean
gtk_file_chooser_native_select_file (GtkFileChooser    *chooser,
                                     GFile             *file,
                                     GError           **error)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (chooser);
  gboolean res;

  res = gtk_file_chooser_select_file (GTK_FILE_CHOOSER (self->dialog),
                                      file, error);

  if (res)
    {
      g_set_object (&self->current_file, file);

      g_clear_object (&self->current_folder);
      g_clear_pointer (&self->current_name, g_free);
    }

  return res;
}

static void
gtk_file_chooser_native_set_current_name (GtkFileChooser    *chooser,
                                          const gchar       *name)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (chooser);

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (self->dialog), name);

  g_clear_pointer (&self->current_name, g_free);
  self->current_name = g_strdup (name);

  g_clear_object (&self->current_file);
}

static GSList *
gtk_file_chooser_native_get_files (GtkFileChooser *chooser)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (chooser);

  switch (self->mode)
    {
    case MODE_PORTAL:
    case MODE_WIN32:
    case MODE_QUARTZ:
      return g_slist_copy_deep (self->custom_files, (GCopyFunc)g_object_ref, NULL);

    case MODE_FALLBACK:
    default:
      return gtk_file_chooser_get_files (GTK_FILE_CHOOSER (self->dialog));
    }
}

static void
gtk_file_chooser_native_show (GtkNativeDialog *native)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (native);

  self->mode = MODE_FALLBACK;

#ifdef GDK_WINDOWING_WIN32
  if (gtk_file_chooser_native_win32_show (self))
    self->mode = MODE_WIN32;
#endif

#if defined (GDK_WINDOWING_QUARTZ) && \
  MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
    if (gdk_quartz_osx_version() >= GDK_OSX_SNOW_LEOPARD &&
        gtk_file_chooser_native_quartz_show (self))
    self->mode = MODE_QUARTZ;
#endif

  if (self->mode == MODE_FALLBACK &&
      gtk_file_chooser_native_portal_show (self))
    self->mode = MODE_PORTAL;

  if (self->mode == MODE_FALLBACK)
    show_dialog (self);
}

static void
gtk_file_chooser_native_hide (GtkNativeDialog *native)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (native);

  switch (self->mode)
    {
    case MODE_FALLBACK:
      hide_dialog (self);
      break;
    case MODE_WIN32:
#ifdef GDK_WINDOWING_WIN32
      gtk_file_chooser_native_win32_hide (self);
#endif
      break;
    case MODE_QUARTZ:
#if defined (GDK_WINDOWING_QUARTZ) && \
  MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
      if (gdk_quartz_osx_version() >= GDK_OSX_SNOW_LEOPARD)
        gtk_file_chooser_native_quartz_hide (self);
#endif
      break;
    case MODE_PORTAL:
      gtk_file_chooser_native_portal_hide (self);
      break;
    }
}

static void
gtk_file_chooser_native_class_init (GtkFileChooserNativeClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkNativeDialogClass *native_dialog_class = GTK_NATIVE_DIALOG_CLASS (class);

  gobject_class->finalize = gtk_file_chooser_native_finalize;
  gobject_class->set_property = gtk_file_chooser_native_set_property;
  gobject_class->get_property = gtk_file_chooser_native_get_property;

  native_dialog_class->show = gtk_file_chooser_native_show;
  native_dialog_class->hide = gtk_file_chooser_native_hide;

  _gtk_file_chooser_install_properties (gobject_class);

  /**
   * GtkFileChooserNative:accept-label:
   *
   * The text used for the label on the accept button in the dialog, or
   * %NULL to use the default text.
   */
 native_props[PROP_ACCEPT_LABEL] =
      g_param_spec_string ("accept-label",
                           P_("Accept label"),
                           P_("The label on the accept button"),
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkFileChooserNative:cancel-label:
   *
   * The text used for the label on the cancel button in the dialog, or
   * %NULL to use the default text.
   */
  native_props[PROP_CANCEL_LABEL] =
      g_param_spec_string ("cancel-label",
                           P_("Cancel label"),
                           P_("The label on the cancel button"),
                           NULL,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, LAST_ARG, native_props);
}

static void
_gtk_file_chooser_native_iface_init (GtkFileChooserIface *iface)
{
  _gtk_file_chooser_delegate_iface_init (iface);
  iface->select_file = gtk_file_chooser_native_select_file;
  iface->set_current_name = gtk_file_chooser_native_set_current_name;
  iface->set_current_folder = gtk_file_chooser_native_set_current_folder;
  iface->get_files = gtk_file_chooser_native_get_files;
  iface->add_choice = gtk_file_chooser_native_add_choice;
  iface->remove_choice = gtk_file_chooser_native_remove_choice;
  iface->set_choice = gtk_file_chooser_native_set_choice;
  iface->get_choice = gtk_file_chooser_native_get_choice;
}
