/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkfiledialog.h"

#include "gtk/gtkchoice.h"
#include "gtkfilechooserdialog.h"
#include <glib/gi18n-lib.h>

/**
 * GtkFileDialog:
 *
 * A `GtkFileDialog` object collects the arguments that
 * are needed to present a file chooser dialog to the
 * user, such as a title for the dialog and whether it
 * should be modal.
 *
 * The dialog is shown with the [function@Gtk.FileDialog.choose_rgba]
 * function. This API follows the GIO async pattern, and the
 * result can be obtained by calling
 * [function@Gtk.FileDialog.choose_rgba_finish].
 *
 * See [class@Gtk.FileDialogButton] for a convenient control
 * that uses `GtkFileDialog` and presents the results.
 *
 * `GtkFileDialog was added in GTK 4.10.
 */
/* {{{ GObject implementation */

struct _GtkFileDialog
{
  GObject parent_instance;

  char *title;
  unsigned int modal : 1;
  unsigned int select_multiple : 1;
  unsigned int create_folders : 1;

  GListModel *filters;
  GListModel *shortcuts;
  GListModel *choices;
};

enum
{
  PROP_TITLE = 1,
  PROP_MODAL,
  PROP_SELECT_MULTIPLE,
  PROP_CREATE_FOLDERS,
  PROP_FILTERS,
  PROP_SHORTCUTS,
  PROP_CHOICES,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkFileDialog, gtk_file_dialog, G_TYPE_OBJECT)

static void
gtk_file_dialog_init (GtkFileDialog *self)
{
  self->title = g_strdup (_("Pick a File"));
  self->modal = TRUE;
  self->create_folders = TRUE;
}

static void
gtk_file_dialog_finalize (GObject *object)
{
  GtkFileDialog *self = GTK_FILE_DIALOG (object);

  g_free (self->title);
  g_clear_object (&self->filters);
  g_clear_object (&self->shortcuts);
  g_clear_object (&self->choices);

  G_OBJECT_CLASS (gtk_file_dialog_parent_class)->finalize (object);
}

static void
gtk_file_dialog_get_property (GObject      *object,
                              unsigned int  property_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  GtkFileDialog *self = GTK_FILE_DIALOG (object);

  switch (property_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_MODAL:
      g_value_set_boolean (value, self->modal);
      break;

    case PROP_SELECT_MULTIPLE:
      g_value_set_boolean (value, self->select_multiple);
      break;

    case PROP_CREATE_FOLDERS:
      g_value_set_boolean (value, self->create_folders);
      break;

    case PROP_FILTERS:
      g_value_set_object (value, self->filters);
      break;

    case PROP_SHORTCUTS:
      g_value_set_object (value, self->shortcuts);
      break;

    case PROP_CHOICES:
      g_value_set_object (value, self->choices);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_file_dialog_set_property (GObject      *object,
                              unsigned int  prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkFileDialog *self = GTK_FILE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_file_dialog_set_title (self, g_value_get_string (value));
      break;

    case PROP_MODAL:
      gtk_file_dialog_set_modal (self, g_value_get_boolean (value));
      break;

    case PROP_SELECT_MULTIPLE:
      gtk_file_dialog_set_select_multiple (self, g_value_get_boolean (value));
      break;

    case PROP_CREATE_FOLDERS:
      gtk_file_dialog_set_create_folders (self, g_value_get_boolean (value));
      break;

    case PROP_FILTERS:
      gtk_file_dialog_set_filters (self, g_value_get_object (value));
      break;

    case PROP_SHORTCUTS:
      gtk_file_dialog_set_shortcuts (self, g_value_get_object (value));
      break;

    case PROP_CHOICES:
      gtk_file_dialog_set_choices (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_dialog_class_init (GtkFileDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_file_dialog_finalize;
  object_class->get_property = gtk_file_dialog_get_property;
  object_class->set_property = gtk_file_dialog_set_property;

  /**
   * GtkFileDialog:title: (attributes org.gtk.Property.get=gtk_file_dialog_get_title org.gtk.Property.set=gtk_color_dialog_set_title)
   *
   * A title that may be shown on the file chooser
   * dialog that is presented by [function@Gtk.FileDialog.choose_rgba].
   *
   * Since: 4.10
   */
  properties[PROP_TITLE] =
      g_param_spec_string ("title", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:modal: (attributes org.gtk.Property.get=gtk_file_dialog_get_modal org.gtk.Property.set=gtk_color_dialog_set_modal)
   *
   * Whether the file chooser dialog is modal.
   *
   * Since: 4.10
   */
  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:select-multiple: (attributes org.gtk.Property.get=gtk_file_dialog_get_select_multiple org.gtk.Property.set=gtk_color_dialog_set_select_multiple)
   *
   * Whether the file chooser dialog allows to select more than one file.
   *
   * Since: 4.10
   */
  properties[PROP_SELECT_MULTIPLE] =
      g_param_spec_boolean ("select-multiple", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFileDialog:create-folders: (attributes org.gtk.Property.get=gtk_file_dialog_get_create_folders org.gtk.Property.set=gtk_color_dialog_set_create_folders)
   *
   * Whether the file chooser dialog will allow to create new folders.
   *
   * Since: 4.10
   */
  properties[PROP_CREATE_FOLDERS] =
      g_param_spec_boolean ("create-folders", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_FILTERS] =
      g_param_spec_object ("filters", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHORTCUTS] =
      g_param_spec_object ("shortcuts", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_CHOICES] =
      g_param_spec_object ("choices", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ Utilities */

static void
file_chooser_set_filters (GtkFileChooser *chooser,
                          GListModel     *filters)
{
  if (!filters)
    return;

  for (unsigned int i = 0; i < g_list_model_get_n_items (filters); i++)
    {
      GtkFileFilter *filter = g_list_model_get_item (filters, i);
      gtk_file_chooser_add_filter (chooser, filter);
      g_object_unref (filter);
    }
}

static void
file_chooser_set_shortcuts (GtkFileChooser *chooser,
                            GListModel     *shortcuts)
{
  if (!shortcuts)
    return;

  for (unsigned int i = 0; i < g_list_model_get_n_items (shortcuts); i++)
    {
      GFile *shortcut = g_list_model_get_item (shortcuts, i);
      gtk_file_chooser_add_shortcut_folder (chooser, shortcut, NULL);
      g_object_unref (shortcut);
    }
}

static void
file_chooser_set_choices (GtkFileChooser *chooser,
                          GListModel     *choices)
{
  if (!choices)
    return;

  for (unsigned int i = 0; i < g_list_model_get_n_items (choices); i++)
    {
      GtkChoice *choice = g_list_model_get_item (choices, i);
      char *id;
      const char *label;
      char **options;
      const char * const *option_labels;

      label = gtk_choice_get_label (choice);
      option_labels = gtk_choice_get_options (choice);

      id = g_strdup_printf ("choice %u", i);

      if (option_labels)
        {
          GStrvBuilder *strv = g_strv_builder_new ();

          for (unsigned int j = 0; option_labels[j]; j++)
            {
              char *option = g_strdup_printf ("option %u", j);
              g_strv_builder_add (strv, option);
              g_free (option);
            }

          options = g_strv_builder_end (strv);
        }
      else
        options = NULL;

      gtk_file_chooser_add_choice (chooser, id, label, (const char **)options, (const char **)option_labels);

      g_free (id);
      g_strfreev (options);

      g_object_unref (choice);
    }
}

static char **
file_chooser_get_options (GtkFileChooser *chooser,
                          GListModel     *choices)
{
  GStrvBuilder *strv;

  if (!choices)
    return NULL;

  strv = g_strv_builder_new ();

  for (unsigned int i = 0; i < g_list_model_get_n_items (choices); i++)
    {
      GtkChoice *choice = g_list_model_get_item (choices, i);
      char *id;
      const char *option;
      unsigned int pos;
      const char *option_label;

      id = g_strdup_printf ("choice %u", i);

      option = gtk_file_chooser_get_choice (chooser, id);

      pos = (unsigned int) g_ascii_strtoull (option + strlen ("option "), NULL, 10);

      option_label = gtk_choice_get_options (choice)[pos];

      g_strv_builder_add (strv, option_label);

      g_free (id);
      g_object_unref (choice);
    }

  return g_strv_builder_end (strv);
}

/* }}} */
/* {{{ Public API */
/* {{{ Constructor */

/**
 * gtk_file_dialog_new:
 *
 * Creates a new `GtkFileDialog` object.
 *
 * Returns: the new `GtkFileDialog`
 *
 * Since: 4.10
 */
GtkFileDialog *
gtk_file_dialog_new (void)
{
  return g_object_new (GTK_TYPE_FILE_DIALOG, NULL);
}

/* }}} */
/* {{{ Getters and setters */

/**
 * gtk_file_dialog_get_title:
 * @self: a `GtkFileDialog`
 *
 * Returns the title that will be shown on the
 * file chooser dialog.
 *
 * Returns: the title
 *
 * Since: 4.10
 */
const char *
gtk_file_dialog_get_title (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->title;
}

/**
 * gtk_file_dialog_set_title:
 * @self: a `GtkFileDialog`
 * @title: the new title
 *
 * Sets the title that will be shown on the
 * file chooser dialog.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_title (GtkFileDialog *self,
                           const char    *title)
{
  char *new_title;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (title != NULL);

  if (g_strcmp0 (self->title, title) == 0)
    return;

  new_title = g_strdup (title);
  g_free (self->title);
  self->title = new_title;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * gtk_file_dialog_get_modal:
 * @self: a `GtkFileDialog`
 *
 * Returns whether the file chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Returns: `TRUE` if the file chooser dialog is modal
 *
 * Since: 4.10
 */
gboolean
gtk_file_dialog_get_modal (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), TRUE);

  return self->modal;
}

/**
 * gtk_file_dialog_set_modal:
 * @self: a `GtkFileDialog`
 * @modal: the new value
 *
 * Sets whether the file chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_modal (GtkFileDialog *self,
                           gboolean       modal)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  if (self->modal == modal)
    return;

  self->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODAL]);
}

/**
 * gtk_file_dialog_get_select_multiple:
 * @self: a `GtkFileDialog`
 *
 * Returns whether the file chooser dialog
 * allows to select multiple files.
 *
 * Returns: `TRUE` if the file chooser dialog allows multi-selection
 *
 * Since: 4.10
 */
gboolean
gtk_file_dialog_get_select_multiple (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), FALSE);

  return self->select_multiple;
}

/**
 * gtk_file_dialog_set_select_multiple:
 * @self: a `GtkFileDialog`
 * @modal: the new value
 *
 * Sets whether the file chooser dialog
 * allows to select multiple files.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_select_multiple (GtkFileDialog *self,
                                     gboolean       select_multiple)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  if (self->select_multiple == select_multiple)
    return;

  self->select_multiple = select_multiple;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECT_MULTIPLE]);
}

/**
 * gtk_file_dialog_get_create_folders:
 * @self: a `GtkFileDialog`
 *
 * Returns whether the file chooser dialog
 * allows to create folders.
 *
 * Returns: `TRUE` if the file chooser dialog allows folder creation
 *
 * Since: 4.10
 */
gboolean
gtk_file_dialog_get_create_folders (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), FALSE);

  return self->create_folders;
}

/**
 * gtk_file_dialog_set_create_folders:
 * @self: a `GtkFileDialog`
 * @modal: the new value
 *
 * Sets whether the file chooser dialog
 * allows to create folders.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_set_create_folders (GtkFileDialog *self,
                                    gboolean       create_folders)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  if (self->create_folders == create_folders)
    return;

  self->create_folders = create_folders;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CREATE_FOLDERS]);
}

void
gtk_file_dialog_set_filters (GtkFileDialog   *self,
                             GListModel      *filters)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (G_IS_LIST_MODEL (filters));
  g_return_if_fail (g_list_model_get_item_type (G_LIST_MODEL (filters)) == GTK_TYPE_FILE_FILTER);

  if (!g_set_object (&self->filters, filters))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILTERS]);
}

GListModel *
gtk_file_dialog_get_filters (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->filters;
}

void
gtk_file_dialog_set_shortcuts (GtkFileDialog   *self,
                               GListModel      *shortcuts)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (G_IS_LIST_MODEL (shortcuts));
  g_return_if_fail (g_list_model_get_item_type (G_LIST_MODEL (shortcuts)) == G_TYPE_FILE);

  if (!g_set_object (&self->shortcuts, shortcuts))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHORTCUTS]);
}

GListModel *
gtk_file_dialog_get_shortcuts (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->shortcuts;
}

void
gtk_file_dialog_set_choices (GtkFileDialog   *self,
                             GListModel      *choices)
{
  g_return_if_fail (GTK_IS_FILE_DIALOG (self));
  g_return_if_fail (G_IS_LIST_MODEL (choices));
  g_return_if_fail (g_list_model_get_item_type (G_LIST_MODEL (choices)) == GTK_TYPE_CHOICE);

  if (!g_set_object (&self->choices, choices))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHOICES]);
}

GListModel *
gtk_file_dialog_get_choices (GtkFileDialog *self)
{
  g_return_val_if_fail (GTK_IS_FILE_DIALOG (self), NULL);

  return self->choices;
}

/* }}} */
/* {{{ Async API */

typedef struct
{
  GListModel *files;
  char **options;
} FileResult;

static void response_cb (GTask *task,
                         int    response);

static void
cancelled_cb (GCancellable *cancellable,
              GTask        *task)
{
  response_cb (task, GTK_RESPONSE_CANCEL);
}

static void
response_cb (GTask *task,
             int    response)
{
  GCancellable *cancellable;

  cancellable = g_task_get_cancellable (task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, task);

  if (response == GTK_RESPONSE_OK)
    {
      GtkFileDialog *self;
      GtkFileChooserDialog *window;
      FileResult file_result;

      self = GTK_FILE_DIALOG (g_task_get_source_object (task));
      window = GTK_FILE_CHOOSER_DIALOG (g_task_get_task_data (task));

      file_result.files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (window));
      file_result.options = file_chooser_get_options (GTK_FILE_CHOOSER (window),
                                                      gtk_file_dialog_get_choices (self));

      g_task_return_pointer (task, &file_result, NULL);
    }
  else
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");

  g_object_unref (task);
}

static void
dialog_response (GtkDialog *dialog,
                 int        response,
                 GTask     *task)
{
  response_cb (task, response);
}

static GtkFileChooserDialog *
create_file_chooser_dialog (GtkFileDialog        *self,
                            GtkWindow            *parent,
                            GtkFileChooserAction  action)
{
  GtkWidget *window;
  const char *accept[] = {
    N_("_Open"), N_("_Save"), N_("_Select")
  };

  window = gtk_file_chooser_dialog_new (self->title, parent,
                                        action,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _(accept[action]), GTK_RESPONSE_OK,
                                        NULL);
  gtk_window_set_modal (GTK_WINDOW (window), self->modal);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (window), self->select_multiple);
  gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER (window), self->create_folders);

  file_chooser_set_filters (GTK_FILE_CHOOSER (window), self->filters);
  file_chooser_set_shortcuts (GTK_FILE_CHOOSER (window), self->shortcuts);
  file_chooser_set_choices (GTK_FILE_CHOOSER (window), self->choices);

  return GTK_FILE_CHOOSER_DIALOG (window);
}

static gboolean
finish_file_op (GtkFileDialog   *self,
                GAsyncResult    *result,
                GListModel     **files,
                char          ***options,
                GError         **error)
{
  FileResult *ret;

  ret = g_task_propagate_pointer (G_TASK (result), error);
  if (ret)
    {
      *files = ret->files;
      if (options)
        *options = ret->options;
       else
         g_strfreev (ret->options);

      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_file_dialog_open:
 * @self: a `GtkFileDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @initial_file: (nullable): the file to select initially
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function initiates a file selection operation by
 * presenting a file chooser dialog to the user.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [function@Gtk.FileDialog.open_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_file_dialog_open (GtkFileDialog       *self,
                      GtkWindow           *parent,
                      GFile               *initial_file,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GtkFileChooserDialog *window;
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  window = create_file_chooser_dialog (self, parent, GTK_FILE_CHOOSER_ACTION_OPEN);

  if (initial_file)
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (window), initial_file, NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_file_dialog_open);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_file_dialog_open_finish:
 * @self: a `GtkFileDialog`
 * @result: a `GAsyncResult`
 * @files: (out caller-allocates): return location for the selected files
 * @options: (out caller-allocates): return location for choices
 * @error: return location for an error
 *
 * Finishes the [function@Gtk.FileDialog.open] call and
 * returns the resulting files as a `GListModel` of `GFiles`.
 *
 * Returns: `TRUE` if a file was selected. Otherwise,
 *   `FALSE` is returned and @error is set
 *
 * Since: 4.10
 */
gboolean
gtk_file_dialog_open_finish (GtkFileDialog   *self,
                             GAsyncResult    *result,
                             GListModel     **files,
                             char          ***options,
                             GError         **error)
{
  return finish_file_op (self, result, files, options, error);
}

void
gtk_file_dialog_select_folder (GtkFileDialog       *self,
                               GtkWindow           *parent,
                               GFile               *current_folder,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GtkFileChooserDialog *window;
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  window = create_file_chooser_dialog (self, parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

  if (current_folder)
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (window), current_folder, NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_file_dialog_select_folder);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

gboolean
gtk_file_dialog_select_folder_finish (GtkFileDialog   *self,
                                      GAsyncResult    *result,
                                      GListModel     **files,
                                      char          ***options,
                                      GError         **error)
{
  return finish_file_op (self, result, files, options, error);
}

void
gtk_file_dialog_save (GtkFileDialog       *self,
                      GtkWindow           *parent,
                      GFile               *current_folder,
                      const char          *current_name,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GtkFileChooserDialog *window;
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_DIALOG (self));

  window = create_file_chooser_dialog (self, parent, GTK_FILE_CHOOSER_ACTION_SAVE);

  if (current_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (window), current_folder, NULL);
  if (current_name)
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (window), current_name);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_file_dialog_save);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

gboolean
gtk_file_dialog_save_finish (GtkFileDialog   *self,
                             GAsyncResult    *result,
                             GListModel     **files,
                             char          ***options,
                             GError         **error)
{
  return finish_file_op (self, result, files, options, error);
}

/* }}} */
/* }}} */

/* vim:set foldmethod=marker expandtab: */
