/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechooserdialog.c: File selector dialog
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
#include "gtkdialogprivate.h"
#include "gtklabel.h"
#include "gtkfilechooserentry.h"

#include <stdarg.h>


/**
 * SECTION:gtkfilechooserdialog
 * @Short_description: A file chooser dialog, suitable for “File/Open” or “File/Save” commands
 * @Title: GtkFileChooserDialog
 * @See_also: #GtkFileChooser, #GtkDialog, GtkFileChooserNative
 *
 * #GtkFileChooserDialog is a dialog box suitable for use with
 * “File/Open” or “File/Save as” commands.  This widget works by
 * putting a #GtkFileChooserWidget inside a #GtkDialog.  It exposes
 * the #GtkFileChooser interface, so you can use all of the
 * #GtkFileChooser functions on the file chooser dialog as well as
 * those for #GtkDialog.
 *
 * Note that #GtkFileChooserDialog does not have any methods of its
 * own.  Instead, you should use the functions that work on a
 * #GtkFileChooser.
 *
 * If you want to integrate well with the platform you should use the
 * #GtkFileChooserNative API, which will use a platform-specific
 * dialog if available and fall back to GtkFileChooserDialog
 * otherwise.
 *
 * ## Typical usage ## {#gtkfilechooser-typical-usage}
 *
 * In the simplest of cases, you can the following code to use
 * #GtkFileChooserDialog to select a file for opening:
 *
 * |[
 * GtkWidget *dialog;
 * GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
 * gint res;
 *
 * dialog = gtk_file_chooser_dialog_new ("Open File",
 *                                       parent_window,
 *                                       action,
 *                                       _("_Cancel"),
 *                                       GTK_RESPONSE_CANCEL,
 *                                       _("_Open"),
 *                                       GTK_RESPONSE_ACCEPT,
 *                                       NULL);
 *
 * res = gtk_dialog_run (GTK_DIALOG (dialog));
 * if (res == GTK_RESPONSE_ACCEPT)
 *   {
 *     char *filename;
 *     GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
 *     filename = gtk_file_chooser_get_filename (chooser);
 *     open_file (filename);
 *     g_free (filename);
 *   }
 *
 * gtk_widget_destroy (dialog);
 * ]|
 *
 * To use a dialog for saving, you can use this:
 *
 * |[
 * GtkWidget *dialog;
 * GtkFileChooser *chooser;
 * GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
 * gint res;
 *
 * dialog = gtk_file_chooser_dialog_new ("Save File",
 *                                       parent_window,
 *                                       action,
 *                                       _("_Cancel"),
 *                                       GTK_RESPONSE_CANCEL,
 *                                       _("_Save"),
 *                                       GTK_RESPONSE_ACCEPT,
 *                                       NULL);
 * chooser = GTK_FILE_CHOOSER (dialog);
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
 * res = gtk_dialog_run (GTK_DIALOG (dialog));
 * if (res == GTK_RESPONSE_ACCEPT)
 *   {
 *     char *filename;
 *
 *     filename = gtk_file_chooser_get_filename (chooser);
 *     save_to_file (filename);
 *     g_free (filename);
 *   }
 *
 * gtk_widget_destroy (dialog);
 * ]|
 *
 * ## Setting up a file chooser dialog ## {#gtkfilechooserdialog-setting-up}
 *
 * There are various cases in which you may need to use a #GtkFileChooserDialog:
 *
 * - To select a file for opening. Use #GTK_FILE_CHOOSER_ACTION_OPEN.
 *
 * - To save a file for the first time. Use #GTK_FILE_CHOOSER_ACTION_SAVE,
 *   and suggest a name such as “Untitled” with gtk_file_chooser_set_current_name().
 *
 * - To save a file under a different name. Use #GTK_FILE_CHOOSER_ACTION_SAVE,
 *   and set the existing filename with gtk_file_chooser_set_filename().
 *
 * - To choose a folder instead of a file. Use #GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER.
 *
 * Note that old versions of the file chooser’s documentation suggested
 * using gtk_file_chooser_set_current_folder() in various
 * situations, with the intention of letting the application
 * suggest a reasonable default folder.  This is no longer
 * considered to be a good policy, as now the file chooser is
 * able to make good suggestions on its own.  In general, you
 * should only cause the file chooser to show a specific folder
 * when it is appropriate to use gtk_file_chooser_set_filename(),
 * i.e. when you are doing a Save As command and you already
 * have a file saved somewhere.

 * ## Response Codes ## {#gtkfilechooserdialog-responses}
 *
 * #GtkFileChooserDialog inherits from #GtkDialog, so buttons that
 * go in its action area have response codes such as
 * #GTK_RESPONSE_ACCEPT and #GTK_RESPONSE_CANCEL.  For example, you
 * could call gtk_file_chooser_dialog_new() as follows:
 *
 * |[
 * GtkWidget *dialog;
 * GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
 *
 * dialog = gtk_file_chooser_dialog_new ("Open File",
 *                                       parent_window,
 *                                       action,
 *                                       _("_Cancel"),
 *                                       GTK_RESPONSE_CANCEL,
 *                                       _("_Open"),
 *                                       GTK_RESPONSE_ACCEPT,
 *                                       NULL);
 * ]|
 *
 * This will create buttons for “Cancel” and “Open” that use stock
 * response identifiers from #GtkResponseType.  For most dialog
 * boxes you can use your own custom response codes rather than the
 * ones in #GtkResponseType, but #GtkFileChooserDialog assumes that
 * its “accept”-type action, e.g. an “Open” or “Save” button,
 * will have one of the following response codes:
 *
 * - #GTK_RESPONSE_ACCEPT
 * - #GTK_RESPONSE_OK
 * - #GTK_RESPONSE_YES
 * - #GTK_RESPONSE_APPLY
 *
 * This is because #GtkFileChooserDialog must intercept responses
 * and switch to folders if appropriate, rather than letting the
 * dialog terminate — the implementation uses these known
 * response codes to know which responses can be blocked if
 * appropriate.
 *
 * To summarize, make sure you use a
 * [stock response code][gtkfilechooserdialog-responses]
 * when you use #GtkFileChooserDialog to ensure proper operation.
 */


struct _GtkFileChooserDialogPrivate
{
  GtkWidget *widget;

  GtkSizeGroup *buttons;

  /* for use with GtkFileChooserEmbed */
  gboolean response_requested;
  gboolean search_setup;
  gboolean has_entry;
};

static void     gtk_file_chooser_dialog_set_property (GObject               *object,
                                                      guint                  prop_id,
                                                      const GValue          *value,
                                                      GParamSpec            *pspec);
static void     gtk_file_chooser_dialog_get_property (GObject               *object,
                                                      guint                  prop_id,
                                                      GValue                *value,
                                                      GParamSpec            *pspec);
static void     gtk_file_chooser_dialog_notify       (GObject               *object,
                                                      GParamSpec            *pspec);

static void     gtk_file_chooser_dialog_map          (GtkWidget             *widget);
static void     gtk_file_chooser_dialog_unmap        (GtkWidget             *widget);
static void     gtk_file_chooser_dialog_size_allocate (GtkWidget             *widget,
                                                       GtkAllocation         *allocation);
static void     file_chooser_widget_file_activated   (GtkFileChooser        *chooser,
                                                      GtkFileChooserDialog  *dialog);
static void     file_chooser_widget_default_size_changed (GtkWidget            *widget,
                                                          GtkFileChooserDialog *dialog);
static void     file_chooser_widget_response_requested (GtkWidget            *widget,
                                                        GtkFileChooserDialog *dialog);
static void     file_chooser_widget_selection_changed (GtkWidget            *widget,
                                                        GtkFileChooserDialog *dialog);

static void response_cb (GtkDialog *dialog,
                         gint       response_id);

static void setup_save_entry (GtkFileChooserDialog *dialog);

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserDialog, gtk_file_chooser_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (GtkFileChooserDialog)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
                                                _gtk_file_chooser_delegate_iface_init))

static void
gtk_file_chooser_dialog_class_init (GtkFileChooserDialogClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->set_property = gtk_file_chooser_dialog_set_property;
  gobject_class->get_property = gtk_file_chooser_dialog_get_property;
  gobject_class->notify = gtk_file_chooser_dialog_notify;

  widget_class->map = gtk_file_chooser_dialog_map;
  widget_class->unmap = gtk_file_chooser_dialog_unmap;
  widget_class->size_allocate = gtk_file_chooser_dialog_size_allocate;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_FILE_CHOOSER);

  _gtk_file_chooser_install_properties (gobject_class);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/ui/gtkfilechooserdialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserDialog, widget);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserDialog, buttons);
  gtk_widget_class_bind_template_callback (widget_class, response_cb);
  gtk_widget_class_bind_template_callback (widget_class, file_chooser_widget_file_activated);
  gtk_widget_class_bind_template_callback (widget_class, file_chooser_widget_default_size_changed);
  gtk_widget_class_bind_template_callback (widget_class, file_chooser_widget_response_requested);
  gtk_widget_class_bind_template_callback (widget_class, file_chooser_widget_selection_changed);
}

static void
gtk_file_chooser_dialog_init (GtkFileChooserDialog *dialog)
{
  dialog->priv = gtk_file_chooser_dialog_get_instance_private (dialog);
  dialog->priv->response_requested = FALSE;

  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (dialog));

  _gtk_file_chooser_set_delegate (GTK_FILE_CHOOSER (dialog),
                                  GTK_FILE_CHOOSER (dialog->priv->widget));
}

static GtkWidget *
get_accept_action_widget (GtkDialog *dialog,
                          gboolean   sensitive_only)
{
  gint response[] = {
    GTK_RESPONSE_ACCEPT,
    GTK_RESPONSE_OK,
    GTK_RESPONSE_YES,
    GTK_RESPONSE_APPLY
  };
  gint i;
  GtkWidget *widget;

  for (i = 0; i < G_N_ELEMENTS (response); i++)
    {
      widget = gtk_dialog_get_widget_for_response (dialog, response[i]);
      if (widget)
        {
          if (!sensitive_only)
            return widget;

          if (gtk_widget_is_sensitive (widget))
            return widget;
        }
    }

  return NULL;
}

static gboolean
is_stock_accept_response_id (gint response_id)
{
  return (response_id == GTK_RESPONSE_ACCEPT ||
          response_id == GTK_RESPONSE_OK ||
          response_id == GTK_RESPONSE_YES ||
          response_id == GTK_RESPONSE_APPLY);
}

/* Callback used when the user activates a file in the file chooser widget */
static void
file_chooser_widget_file_activated (GtkFileChooser       *chooser,
                                    GtkFileChooserDialog *dialog)
{
  GtkWidget *widget;

  if (gtk_window_activate_default (GTK_WINDOW (dialog)))
    return;

  /* There probably isn't a default widget, so make things easier for the
   * programmer by looking for a reasonable button on our own.
   */
  widget = get_accept_action_widget (GTK_DIALOG (dialog), TRUE);
  if (widget)
    gtk_widget_activate (widget);
}

static void
file_chooser_widget_default_size_changed (GtkWidget            *widget,
                                          GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv;
  gint default_width, default_height;
  GtkRequisition req, widget_req;

  priv = gtk_file_chooser_dialog_get_instance_private (dialog);

  /* Unset any previously set size */
  gtk_widget_set_size_request (GTK_WIDGET (dialog), -1, -1);

  if (gtk_widget_is_drawable (widget))
    {
      /* Force a size request of everything before we start. This will make sure
       * that widget->requisition is meaningful.
       */
      gtk_widget_get_preferred_size (GTK_WIDGET (dialog), &req, NULL);
      gtk_widget_get_preferred_size (widget, &widget_req, NULL);
    }

  _gtk_file_chooser_embed_get_default_size (GTK_FILE_CHOOSER_EMBED (priv->widget),
                                            &default_width, &default_height);

  gtk_window_resize (GTK_WINDOW (dialog), default_width, default_height);
}

static void
file_chooser_widget_selection_changed (GtkWidget            *widget,
                                       GtkFileChooserDialog *dialog)
{
  GtkWidget *button;
  GSList *uris;
  gboolean sensitive;

  button = get_accept_action_widget (GTK_DIALOG (dialog), FALSE);
  if (button == NULL)
    return;

  uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (dialog->priv->widget));
  sensitive = (uris != NULL);
  gtk_widget_set_sensitive (button, sensitive);

  g_slist_free_full (uris, g_free);
}

static void
file_chooser_widget_response_requested (GtkWidget            *widget,
                                        GtkFileChooserDialog *dialog)
{
  GtkWidget *button;

  dialog->priv->response_requested = TRUE;

  if (gtk_window_activate_default (GTK_WINDOW (dialog)))
    return;

  /* There probably isn't a default widget, so make things easier for the
   * programmer by looking for a reasonable button on our own.
   */
  button = get_accept_action_widget (GTK_DIALOG (dialog), TRUE);
  if (button)
    {
      gtk_widget_activate (button);
      return;
    }

  dialog->priv->response_requested = FALSE;
}

static void
gtk_file_chooser_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)

{
  GtkFileChooserDialogPrivate *priv;

  priv = gtk_file_chooser_dialog_get_instance_private (GTK_FILE_CHOOSER_DIALOG (object));

  g_object_set_property (G_OBJECT (priv->widget), pspec->name, value);
}

static void
gtk_file_chooser_dialog_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkFileChooserDialogPrivate *priv;

  priv = gtk_file_chooser_dialog_get_instance_private (GTK_FILE_CHOOSER_DIALOG (object));

  g_object_get_property (G_OBJECT (priv->widget), pspec->name, value);
}

static void
gtk_file_chooser_dialog_notify (GObject    *object,
                                GParamSpec *pspec)
{
  if (strcmp (pspec->name, "action") == 0)
    setup_save_entry (GTK_FILE_CHOOSER_DIALOG (object));

  if (G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->notify)
    G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->notify (object, pspec);
}

static void
add_button (GtkWidget *button, gpointer data)
{
  GtkFileChooserDialog *dialog = data;

  if (GTK_IS_BUTTON (button))
    gtk_size_group_add_widget (dialog->priv->buttons, button);
}

static void
setup_search (GtkFileChooserDialog *dialog)
{
  gboolean use_header;

  if (dialog->priv->search_setup)
    return;

  dialog->priv->search_setup = TRUE;

  g_object_get (dialog, "use-header-bar", &use_header, NULL);
  if (use_header)
    {
      GtkWidget *button;
      GtkWidget *image;
      GtkWidget *header;

      button = gtk_toggle_button_new ();
      gtk_widget_set_focus_on_click (button, FALSE);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      image = gtk_image_new_from_icon_name ("edit-find-symbolic", GTK_ICON_SIZE_MENU);
      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
      gtk_style_context_remove_class (gtk_widget_get_style_context (button), "text-button");
      gtk_widget_show (image);
      gtk_widget_show (button);

      header = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      g_object_bind_property (button, "active",
                              dialog->priv->widget, "search-mode",
                              G_BINDING_BIDIRECTIONAL);
      g_object_bind_property (dialog->priv->widget, "subtitle",
                              header, "subtitle",
                              G_BINDING_SYNC_CREATE);

      gtk_container_forall (GTK_CONTAINER (header), add_button, dialog);
    }
}

static void
setup_save_entry (GtkFileChooserDialog *dialog)
{
  gboolean use_header;
  GtkFileChooserAction action;
  gboolean need_entry;
  GtkWidget *header;

  g_object_get (dialog,
                "use-header-bar", &use_header,
                "action", &action,
                NULL);

  if (!use_header)
    return;

  header = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));

  need_entry = action == GTK_FILE_CHOOSER_ACTION_SAVE ||
               action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER;

  if (need_entry && !dialog->priv->has_entry)
    {
      GtkWidget *box;
      GtkWidget *label;
      GtkWidget *entry;

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      label = gtk_label_new_with_mnemonic (_("_Name"));
      entry = _gtk_file_chooser_entry_new (FALSE, FALSE);
      g_object_set (label, "margin-start", 6, "margin-end", 6, NULL);
      g_object_set (entry, "margin-start", 6, "margin-end", 6, NULL);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
      gtk_container_add (GTK_CONTAINER (box), label);
      gtk_container_add (GTK_CONTAINER (box), entry);
      gtk_widget_show_all (box);

      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (header), box);
      gtk_file_chooser_widget_set_save_entry (GTK_FILE_CHOOSER_WIDGET (dialog->priv->widget), entry);
    }
  else if (!need_entry && dialog->priv->has_entry)
    {
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (header), NULL);
      gtk_file_chooser_widget_set_save_entry (GTK_FILE_CHOOSER_WIDGET (dialog->priv->widget), NULL);
    }

  dialog->priv->has_entry = need_entry;
}

static void
ensure_default_response (GtkFileChooserDialog *dialog)
{
  GtkWidget *widget;

  widget = get_accept_action_widget (GTK_DIALOG (dialog), TRUE);
  if (widget)
    gtk_widget_grab_default (widget);
}

static void
gtk_file_chooser_dialog_map (GtkWidget *widget)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);
  GtkFileChooserDialogPrivate *priv = dialog->priv;

  setup_search (dialog);
  setup_save_entry (dialog);
  ensure_default_response (dialog);

  _gtk_file_chooser_embed_initial_focus (GTK_FILE_CHOOSER_EMBED (priv->widget));

  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->map (widget);
}

static void
save_dialog_geometry (GtkFileChooserDialog *dialog)
{
  GtkWindow *window;
  GSettings *settings;
  int old_x, old_y, old_width, old_height;
  int x, y, width, height;

  settings = _gtk_file_chooser_get_settings_for_widget (GTK_WIDGET (dialog));

  window = GTK_WINDOW (dialog);

  gtk_window_get_position (window, &x, &y);
  gtk_window_get_size (window, &width, &height);

  g_settings_get (settings, SETTINGS_KEY_WINDOW_POSITION, "(ii)", &old_x, &old_y);
  if (old_x != x || old_y != y)
    g_settings_set (settings, SETTINGS_KEY_WINDOW_POSITION, "(ii)", x, y);

  g_settings_get (settings, SETTINGS_KEY_WINDOW_SIZE, "(ii)", &old_width, &old_height);
  if (old_width != width || old_height != height)
    g_settings_set (settings, SETTINGS_KEY_WINDOW_SIZE, "(ii)", width, height);

  g_settings_apply (settings);
}

static void
gtk_file_chooser_dialog_unmap (GtkWidget *widget)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);

  save_dialog_geometry (dialog);

  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->unmap (widget);
}

static void
gtk_file_chooser_dialog_size_allocate (GtkWidget     *widget,
                                       GtkAllocation *allocation)
{
  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_is_drawable (widget))
    save_dialog_geometry (GTK_FILE_CHOOSER_DIALOG (widget));
}

/* We do a signal connection here rather than overriding the method in
 * class_init because GtkDialog::response is a RUN_LAST signal.  We want *our*
 * handler to be run *first*, regardless of whether the user installs response
 * handlers of his own.
 */
static void
response_cb (GtkDialog *dialog,
             gint       response_id)
{
  GtkFileChooserDialogPrivate *priv;

  priv = gtk_file_chooser_dialog_get_instance_private (GTK_FILE_CHOOSER_DIALOG (dialog));

  /* Act only on response IDs we recognize */
  if (is_stock_accept_response_id (response_id) &&
      !priv->response_requested &&
      !_gtk_file_chooser_embed_should_respond (GTK_FILE_CHOOSER_EMBED (priv->widget)))
    {
      g_signal_stop_emission_by_name (dialog, "response");
    }

  priv->response_requested = FALSE;
}

static GtkWidget *
gtk_file_chooser_dialog_new_valist (const gchar          *title,
                                    GtkWindow            *parent,
                                    GtkFileChooserAction  action,
                                    const gchar          *first_button_text,
                                    va_list               varargs)
{
  GtkWidget *result;
  const char *button_text = first_button_text;
  gint response_id;

  result = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "title", title,
                         "action", action,
                         NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);

  while (button_text)
    {
      response_id = va_arg (varargs, gint);
      gtk_dialog_add_button (GTK_DIALOG (result), button_text, response_id);
      button_text = va_arg (varargs, const gchar *);
    }

  return result;
}

/**
 * gtk_file_chooser_dialog_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #GtkFileChooserDialog.  This function is analogous to
 * gtk_dialog_new_with_buttons().
 *
 * Returns: a new #GtkFileChooserDialog
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_dialog_new (const gchar          *title,
                             GtkWindow            *parent,
                             GtkFileChooserAction  action,
                             const gchar          *first_button_text,
                             ...)
{
  GtkWidget *result;
  va_list varargs;

  va_start (varargs, first_button_text);
  result = gtk_file_chooser_dialog_new_valist (title, parent, action,
                                               first_button_text,
                                               varargs);
  va_end (varargs);

  return result;
}
