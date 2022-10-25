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

#include "gtkinfodialog.h"

#include "gtkbutton.h"
#include "gtkmessagedialog.h"
#include <glib/gi18n-lib.h>

/**
 * GtkInfoDialog:
 *
 * A `GtkInfoDialog` object collects the arguments that
 * are needed to present a info chooser dialog to the
 * user, such as a title for the dialog and whether it
 * should be modal.
 *
 * The dialog is shown with the [function@Gtk.InfoDialog.present]
 * function. This API follows the GIO async pattern, and the
 * result can be obtained by calling
 * [function@Gtk.InfoDialog.present_finish].
 *
 * `GtkInfoDialog was added in GTK 4.10.
 */

/* {{{ GObject implementation */

struct _GtkInfoDialog
{
  GObject parent_instance;

  char *heading;
  char *body;
  char **buttons;

  unsigned int modal : 1;
  unsigned int heading_use_markup: 1;
  unsigned int body_use_markup: 1;
};

enum
{
  PROP_MODAL = 1,
  PROP_HEADING,
  PROP_HEADING_USE_MARKUP,
  PROP_BODY,
  PROP_BODY_USE_MARKUP,
  PROP_BUTTONS,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkInfoDialog, gtk_info_dialog, G_TYPE_OBJECT)

static void
gtk_info_dialog_init (GtkInfoDialog *self)
{
  self->modal = TRUE;
}

static void
gtk_info_dialog_finalize (GObject *object)
{
  GtkInfoDialog *self = GTK_INFO_DIALOG (object);

  g_free (self->heading);
  g_free (self->body);
  g_strfreev (self->buttons);

  G_OBJECT_CLASS (gtk_info_dialog_parent_class)->finalize (object);
}

static void
gtk_info_dialog_get_property (GObject      *object,
                              unsigned int  property_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  GtkInfoDialog *self = GTK_INFO_DIALOG (object);

  switch (property_id)
    {
    case PROP_MODAL:
      g_value_set_boolean (value, self->modal);
      break;

    case PROP_HEADING:
      g_value_set_string (value, self->heading);
      break;

    case PROP_HEADING_USE_MARKUP:
      g_value_set_boolean (value, self->heading_use_markup);
      break;

    case PROP_BODY:
      g_value_set_string (value, self->body);
      break;

    case PROP_BODY_USE_MARKUP:
      g_value_set_boolean (value, self->body_use_markup);
      break;

    case PROP_BUTTONS:
      g_value_set_boxed (value, self->buttons);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_info_dialog_set_property (GObject      *object,
                              unsigned int  prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkInfoDialog *self = GTK_INFO_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MODAL:
      gtk_info_dialog_set_modal (self, g_value_get_boolean (value));
      break;

    case PROP_HEADING:
      gtk_info_dialog_set_heading (self, g_value_get_string (value));
      break;

    case PROP_HEADING_USE_MARKUP:
      gtk_info_dialog_set_heading_use_markup (self, g_value_get_boolean (value));
      break;

    case PROP_BODY:
      gtk_info_dialog_set_body (self, g_value_get_string (value));
      break;

    case PROP_BODY_USE_MARKUP:
      gtk_info_dialog_set_body_use_markup (self, g_value_get_boolean (value));
      break;

    case PROP_BUTTONS:
      gtk_info_dialog_set_buttons (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_info_dialog_class_init (GtkInfoDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_info_dialog_finalize;
  object_class->get_property = gtk_info_dialog_get_property;
  object_class->set_property = gtk_info_dialog_set_property;

  /**
   * GtkInfoDialog:modal: (attributes org.gtk.Property.get=gtk_info_dialog_get_modal org.gtk.Property.set=gtk_color_dialog_set_modal)
   *
   * Whether the info chooser dialog is modal.
   *
   * Since: 4.10
   */
  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkInfoDialog:heading: (attributes org.gtk.Property.get=gtk_info_dialog_get_heading org.gtk.Property.set=gtk_color_dialog_set_heading)
   *
   * The heading for the dialog that is presented
   * by [function@Gtk.InfoDialog.present].
   *
   * Since: 4.10
   */
  properties[PROP_HEADING] =
      g_param_spec_string ("heading", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkInfoDialog:heading-use-markup: (attributes org.gtk.Property.get=gtk_info_dialog_get_heading_use_markup org.gtk.Property.set=gtk_color_dialog_set_heading_use_markup)
   *
   * Whether the heading uses markup.
   *
   * Since: 4.10
   */
  properties[PROP_HEADING_USE_MARKUP] =
      g_param_spec_boolean ("heading-use-markup", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkInfoDialog:body: (attributes org.gtk.Property.get=gtk_info_dialog_get_body org.gtk.Property.set=gtk_color_dialog_set_body)
   *
   * The body text for the dialog that is presented
   * by [function@Gtk.InfoDialog.present].
   *
   * Since: 4.10
   */
  properties[PROP_BODY] =
      g_param_spec_string ("body", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkInfoDialog:body-use-markup: (attributes org.gtk.Property.get=gtk_info_dialog_get_body_use_markup org.gtk.Property.set=gtk_color_dialog_set_body_use_markup)
   *
   * Whether the body text uses markup.
   *
   * Since: 4.10
   */
  properties[PROP_BODY_USE_MARKUP] =
      g_param_spec_boolean ("body-use-markup", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkInfoDialog:buttons: (attributes org.gtk.Property.get=gtk_info_dialog_get_buttons org.gtk.Property.set=gtk_color_dialog_set_buttons)
   *
   * The labels for buttons to show in the dialog that is presented
   * by [function@Gtk.InfoDialog.present].
   *
   * The labels should be translated and may contain a _ to indicate
   * mnemonic characters.
   *
   * Since: 4.10
   */
  properties[PROP_BUTTONS] =
      g_param_spec_boxed ("buttons", NULL, NULL,
                          G_TYPE_STRV,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ Constructor */

/**
 * gtk_info_dialog_new:
 *
 * Creates a new `GtkInfoDialog` object.
 *
 * Returns: the new `GtkInfoDialog`
 *
 * Since: 4.10
 */
GtkInfoDialog *
gtk_info_dialog_new (void)
{
  return g_object_new (GTK_TYPE_INFO_DIALOG, NULL);
}

/* }}} */
/* {{{ Getters and setters */

/**
 * gtk_info_dialog_get_modal:
 * @self: a `GtkInfoDialog`
 *
 * Returns whether the info chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Returns: `TRUE` if the info chooser dialog is modal
 *
 * Since: 4.10
 */
gboolean
gtk_info_dialog_get_modal (GtkInfoDialog *self)
{
  g_return_val_if_fail (GTK_IS_INFO_DIALOG (self), TRUE);

  return self->modal;
}

/**
 * gtk_info_dialog_set_modal:
 * @self: a `GtkInfoDialog`
 * @modal: the new value
 *
 * Sets whether the info chooser dialog
 * blocks interaction with the parent window
 * while it is presented.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_set_modal (GtkInfoDialog *self,
                           gboolean       modal)
{
  g_return_if_fail (GTK_IS_INFO_DIALOG (self));

  if (self->modal == modal)
    return;

  self->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODAL]);
}

/**
 * gtk_info_dialog_get_heading:
 * @self: a `GtkInfoDialog`
 *
 * Returns the heading that will be shown in the
 * info dialog.
 *
 * Returns: the heading
 *
 * Since: 4.10
 */
const char *
gtk_info_dialog_get_heading (GtkInfoDialog *self)
{
  g_return_val_if_fail (GTK_IS_INFO_DIALOG (self), NULL);

  return self->heading;
}

/**
 * gtk_info_dialog_set_heading:
 * @self: a `GtkInfoDialog`
 * @text: the new heading
 *
 * Sets the heading that will be shown in the
 * info dialog.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_set_heading (GtkInfoDialog *self,
                             const char    *text)
{
  char *new_text;

  g_return_if_fail (GTK_IS_INFO_DIALOG (self));
  g_return_if_fail (text != NULL);

  if (g_strcmp0 (self->heading, text) == 0)
    return;

  new_text = g_strdup (text);
  g_free (self->heading);
  self->heading = new_text;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEADING]);
}

/**
 * gtk_info_dialog_get_heading_use_markup:
 * @self: a `GtkInfoDialog`
 *
 * Returns whether the heading uses markup.
 *
 * Returns: `TRUE` if the heading uses markup
 *
 * Since: 4.10
 */
gboolean
gtk_info_dialog_get_heading_use_markup (GtkInfoDialog *self)
{
  g_return_val_if_fail (GTK_IS_INFO_DIALOG (self), FALSE);

  return self->heading_use_markup;
}

/**
 * gtk_info_dialog_set_heading_use_markup:
 * @self: a `GtkInfoDialog`
 * @use_markup: the new value
 *
 * Sets whether the heading uses markup.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_set_heading_use_markup (GtkInfoDialog *self,
                                        gboolean       use_markup)
{
  g_return_if_fail (GTK_IS_INFO_DIALOG (self));

  if (self->heading_use_markup == use_markup)
    return;

  self->heading_use_markup = use_markup;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEADING_USE_MARKUP]);
}

/**
 * gtk_info_dialog_get_body:
 * @self: a `GtkInfoDialog`
 *
 * Returns the body text that will be shown
 * in the info dialog.
 *
 * Returns: the body text
 *
 * Since: 4.10
 */
const char *
gtk_info_dialog_get_body (GtkInfoDialog *self)
{
  g_return_val_if_fail (GTK_IS_INFO_DIALOG (self), NULL);

  return self->body;
}

/**
 * gtk_info_dialog_set_body:
 * @self: a `GtkInfoDialog`
 * @text: the new text
 *
 * Sets the body text that will be shown
 * in the info dialog.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_set_body (GtkInfoDialog *self,
                          const char    *text)
{
  char *new_text;

  g_return_if_fail (GTK_IS_INFO_DIALOG (self));
  g_return_if_fail (text != NULL);

  if (g_strcmp0 (self->body, text) == 0)
    return;

  new_text = g_strdup (text);
  g_free (self->body);
  self->body = new_text;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BODY]);
}

/**
 * gtk_info_dialog_get_body_use_markup:
 * @self: a `GtkInfoDialog`
 *
 * Returns whether the body text uses markup.
 *
 * Returns: `TRUE` if body text uses markup
 *
 * Since: 4.10
 */
gboolean
gtk_info_dialog_get_body_use_markup (GtkInfoDialog *self)
{
  g_return_val_if_fail (GTK_IS_INFO_DIALOG (self), FALSE);

  return self->body_use_markup;
}

/**
 * gtk_info_dialog_set_body_use_markup:
 * @self: a `GtkInfoDialog`
 * @use_markup: the new value
 *
 * Sets whether the body text uses markup.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_set_body_use_markup (GtkInfoDialog *self,
                                     gboolean       use_markup)
{
  g_return_if_fail (GTK_IS_INFO_DIALOG (self));

  if (self->body_use_markup == use_markup)
    return;

  self->body_use_markup = use_markup;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BODY_USE_MARKUP]);
}

/**
 * gtk_info_dialog_get_buttons:
 * @self: a `GtkInfoDialog`
 *
 * Returns the button labels for the info dialog.
 *
 * Returns: (nullable) (transfer none): the button labels
 *
 * Since: 4.10
 */
const char * const *
gtk_info_dialog_get_buttons (GtkInfoDialog *self)
{
  g_return_val_if_fail (GTK_IS_INFO_DIALOG (self), NULL);

  return (const char * const *) self->buttons;
}

/**
 * gtk_info_dialog_set_buttons:
 * @self: a `GtkInfoDialog`
 * @labels: the new button labels
 *
 * Sets the button labels for the info dialog.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_set_buttons (GtkInfoDialog      *self,
                             const char * const *labels)
{
  g_return_if_fail (GTK_IS_INFO_DIALOG (self));
  g_return_if_fail (labels != NULL);

  g_strfreev (self->buttons);
  self->buttons = g_strdupv ((char **)labels);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BUTTONS]);
}

/* }}} */
/* {{{ Convenience API */

/**
 * gtk_info_dialog_set_heading_markup:
 * @self: a `GtkInfoDialog`
 * @text: the new heading
 *
 * Sets the heading that will be shown in the
 * info dialog, and marks it as using markup.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_set_heading_markup (GtkInfoDialog *self,
                                    const char    *text)
{
  g_object_freeze_notify (G_OBJECT (self));
  gtk_info_dialog_set_heading (self, text);
  gtk_info_dialog_set_heading_use_markup (self, TRUE);
  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_info_dialog_set_body_markup:
 * @self: a `GtkInfoDialog`
 * @text: the new heading
 *
 * Sets the body text that will be shown in the
 * info dialog, and marks it as using markup.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_set_body_markup (GtkInfoDialog *self,
                                 const char    *text)
{
  g_object_freeze_notify (G_OBJECT (self));
  gtk_info_dialog_set_body (self, text);
  gtk_info_dialog_set_body_use_markup (self, TRUE);
  g_object_thaw_notify (G_OBJECT (self));
}

/* }}} */
/* {{{ Async API */

static void response_cb (GTask *task,
                         int    response);

static void
cancelled_cb (GCancellable *cancellable,
              GTask        *task)
{
  response_cb (task, -1);
}

static void
response_cb (GTask *task,
             int    response)
{
  GCancellable *cancellable;

  cancellable = g_task_get_cancellable (task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, task);

  if (response >= 0)
    g_task_return_int (task, response);
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

/**
 * gtk_info_dialog_present:
 * @self: a `GtkInfoDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function presents an info dialog to the user.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [function@Gtk.InfoDialog.present_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_info_dialog_present (GtkInfoDialog       *self,
                         GtkWindow           *parent,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GtkMessageDialog *window;
  GTask *task;

  g_return_if_fail (GTK_IS_INFO_DIALOG (self));

  window = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "transient-for", parent,
                         "destroy-with-parent", TRUE,
                         "modal", self->modal,
                         "text", self->heading,
                         "use-markup", self->heading_use_markup,
                         "secondary-text", self->body,
                         "secondary-use-markup", self->body_use_markup,
                         NULL);

  if (self->buttons)
    {
      for (int i = 0; self->buttons[i]; i++)
        gtk_dialog_add_button (GTK_DIALOG (window), self->buttons[i], i);
    }
  else
    gtk_dialog_add_button (GTK_DIALOG (window), _("Close"), 0);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_info_dialog_present);
  g_task_set_task_data (task, window, (GDestroyNotify) gtk_window_destroy);

  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  g_signal_connect (window, "response", G_CALLBACK (dialog_response), task);

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * gtk_info_dialog_present_finish:
 * @self: a `GtkInfoDialog`
 * @result: a `GAsyncResult`
 * @button: (out caller-allocates): return location for button
 * @error: return location for an error
 *
 * Finishes the [function@Gtk.InfoDialog.present] call
 * and returns the button that was clicked.
 *
 * Returns: `TRUE` if a button was clicked.
 *   Otherwise, `FALSE` is returned and @error is set
 *
 * Since: 4.10
 */
gboolean
gtk_info_dialog_present_finish (GtkInfoDialog  *self,
                                GAsyncResult   *result,
                                int            *button,
                                GError        **error)
{
  *button = (int) g_task_propagate_int (G_TASK (result), error);
  return ! (error && *error);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
