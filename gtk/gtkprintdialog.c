/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2023 Red Hat, Inc.
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

#include "gtkprintdialog.h"

#include "gtkwindowprivate.h"
#include "gtkdialogerror.h"
#include "gtkprivate.h"
#include "deprecated/gtkdialog.h"

#include "print/gtkprinter.h"

#ifdef HAVE_GIO_UNIX
#include <gio/gunixfdlist.h>
#include <gio/gfiledescriptorbased.h>
#include "print/gtkprintjob.h"
#include "print/gtkprinterprivate.h"
#include "print/gtkprintunixdialog.h"
#endif

#include <glib/gi18n-lib.h>

/**
 * GtkPrintDialog:
 *
 * A `GtkPrintDialog` object collects the arguments that
 * are needed to present a print dialog to the user, such
 * as a title for the dialog and whether it should be modal.
 *
 * The dialog is shown with the [method@Gtk.PrintDialog.prepare_print]
 * function. The actual printing can be done with [method@Gtk.PrintDialog.print_stream]
 * or [method@Gtk.PrintDialog.print_file]. These APIs follows the GIO async pattern,
 * and the results can be obtained by calling the corresponding finish methods.
 *
 * Since: 4.14
 */
/* {{{ GObject implementation */

struct _GtkPrintDialog
{
  GObject parent_instance;

  GtkPrintSettings *print_settings;
  GtkPageSetup *default_page_setup;

  GtkPrinter *printer;

  GDBusProxy *portal;

  GtkWindow *exported_window;

  char *portal_handle;
  unsigned int token;
  unsigned int response_signal_id;

  char *accept_label;
  char *title;

  unsigned int modal : 1;
};

enum
{
  PROP_ACCEPT_LABEL = 1,
  PROP_DEFAULT_PAGE_SETUP,
  PROP_MODAL,
  PROP_PRINT_SETTINGS,
  PROP_TITLE,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkPrintDialog, gtk_print_dialog, G_TYPE_OBJECT)

static void
gtk_print_dialog_init (GtkPrintDialog *self)
{
  self->modal = TRUE;
}

static void
gtk_print_dialog_finalize (GObject *object)
{
  GtkPrintDialog *self = GTK_PRINT_DIALOG (object);

  g_clear_object (&self->printer);
  g_clear_object (&self->portal);
  g_free (self->portal_handle);
  g_clear_object (&self->exported_window);
  g_clear_object (&self->print_settings);
  g_clear_object (&self->default_page_setup);
  g_free (self->accept_label);
  g_free (self->title);

  G_OBJECT_CLASS (gtk_print_dialog_parent_class)->finalize (object);
}

static void
gtk_print_dialog_get_property (GObject      *object,
                               unsigned int  property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GtkPrintDialog *self = GTK_PRINT_DIALOG (object);

  switch (property_id)
    {
    case PROP_ACCEPT_LABEL:
      g_value_set_string (value, self->accept_label);
      break;

    case PROP_DEFAULT_PAGE_SETUP:
      g_value_set_object (value, self->default_page_setup);
      break;

    case PROP_MODAL:
      g_value_set_boolean (value, self->modal);
      break;

    case PROP_PRINT_SETTINGS:
      g_value_set_object (value, self->print_settings);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_print_dialog_set_property (GObject      *object,
                               unsigned int  prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkPrintDialog *self = GTK_PRINT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ACCEPT_LABEL:
      gtk_print_dialog_set_accept_label (self, g_value_get_string (value));
      break;

    case PROP_DEFAULT_PAGE_SETUP:
      gtk_print_dialog_set_default_page_setup (self, g_value_get_object (value));
      break;

    case PROP_MODAL:
      gtk_print_dialog_set_modal (self, g_value_get_boolean (value));
      break;

    case PROP_PRINT_SETTINGS:
      gtk_print_dialog_set_print_settings (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      gtk_print_dialog_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_dialog_class_init (GtkPrintDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_print_dialog_finalize;
  object_class->get_property = gtk_print_dialog_get_property;
  object_class->set_property = gtk_print_dialog_set_property;

  /**
   * GtkPrintDialog:accept-label: (attributes org.gtk.Property.get=gtk_print_dialog_get_accept_label org.gtk.Property.set=gtk_print_dialog_set_accept_label)
   *
   * A label that may be shown on the accept button of a print dialog
   * that is presented by [method@Gtk.PrintDialog.prepare_print].
   *
   * Since: 4.14
   */
  properties[PROP_ACCEPT_LABEL] =
      g_param_spec_string ("accept-label", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPrintDialog:default-page-setup: (attributes org.gtk.Property.get=gtk_print_dialog_get_default_page_setup org.gtk.Property.set=gtk_print_dialog_set_default_page_setup)
   *
   * The default page setup to use.
   *
   * Since: 4.14
   */
  properties[PROP_DEFAULT_PAGE_SETUP] =
      g_param_spec_object ("default-page-setup", NULL, NULL,
                           GTK_TYPE_PAGE_SETUP,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPrintDialog:modal: (attributes org.gtk.Property.get=gtk_print_dialog_get_modal org.gtk.Property.set=gtk_print_dialog_set_modal)
   *
   * Whether the print dialog is modal.
   *
   * Since: 4.14
   */
  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPrintDialog:print-settings: (attributes org.gtk.Property.get=gtk_print_dialog_get_print_settings org.gtk.Property.set=gtk_print_dialog_set_print_settings)
   *
   * The print settings to use.
   *
   * Since: 4.14
   */
  properties[PROP_PRINT_SETTINGS] =
      g_param_spec_object ("print-settings", NULL, NULL,
                           GTK_TYPE_PRINT_SETTINGS,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPrintDialog:title: (attributes org.gtk.Property.get=gtk_print_dialog_get_title org.gtk.Property.set=gtk_print_dialog_set_title)
   *
   * A title that may be shown on the print dialog that is
   * presented by [method@Gtk.PrintDialog.prepare_print].
   *
   * Since: 4.14
   */
  properties[PROP_TITLE] =
      g_param_spec_string ("title", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ API: Constructor */

/**
 * gtk_print_dialog_new:
 *
 * Creates a new `GtkPrintDialog` object.
 *
 * Returns: the new `GtkPrintDialog`
 *
 * Since: 4.14
 */
GtkPrintDialog *
gtk_print_dialog_new (void)
{
  return g_object_new (GTK_TYPE_PRINT_DIALOG, NULL);
}

/* }}} */
/* {{{ API: Getters and setters */

/**
 * gtk_print_dialog_set_title:
 * @self: a `GtkPrintDialog`
 * @title: the new title
 *
 * Sets the title that will be shown on the print dialog.
 *
 * Since: 4.14
 */
void
gtk_print_dialog_set_title (GtkPrintDialog *self,
                            const char     *title)
{
  char *new_title;

  g_return_if_fail (GTK_IS_PRINT_DIALOG (self));
  g_return_if_fail (title != NULL);

  if (g_strcmp0 (self->title, title) == 0)
    return;

  new_title = g_strdup (title);
  g_free (self->title);
  self->title = new_title;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * gtk_print_dialog_get_accept_label:
 * @self: a `GtkPrintDialog`
 *
 * Returns the label that will be shown on the
 * accept button of the print dialog.
 *
 * Returns: the accept label
 *
 * Since: 4.14
 */
const char *
gtk_print_dialog_get_accept_label (GtkPrintDialog *self)
{
  g_return_val_if_fail (GTK_IS_PRINT_DIALOG (self), NULL);

  return self->accept_label;
}

/**
 * gtk_print_dialog_set_accept_label:
 * @self: a `GtkPrintDialog`
 * @accept_label: the new accept label
 *
 * Sets the label that will be shown on the
 * accept button of the print dialog shown for
 * [method@Gtk.PrintDialog.prepare_print].
 *
 * Since: 4.14
 */
void
gtk_print_dialog_set_accept_label (GtkPrintDialog *self,
                                   const char     *accept_label)
{
  char *new_label;

  g_return_if_fail (GTK_IS_PRINT_DIALOG (self));
  g_return_if_fail (accept_label != NULL);

  if (g_strcmp0 (self->accept_label, accept_label) == 0)
    return;

  new_label = g_strdup (accept_label);
  g_free (self->accept_label);
  self->accept_label = new_label;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCEPT_LABEL]);
}

/**
 * gtk_print_dialog_get_modal:
 * @self: a `GtkPrintDialog`
 *
 * Returns whether the print dialog blocks
 * interaction with the parent window while
 * it is presented.
 *
 * Returns: whether the print dialog is modal
 *
 * Since: 4.14
 */
gboolean
gtk_print_dialog_get_modal (GtkPrintDialog *self)
{
  g_return_val_if_fail (GTK_IS_PRINT_DIALOG (self), TRUE);

  return self->modal;
}

/**
 * gtk_print_dialog_set_modal:
 * @self: a `GtkPrintDialog`
 * @modal: the new value
 *
 * Sets whether the print dialog blocks
 * interaction with the parent window while
 * it is presented.
 *
 * Since: 4.14
 */
void
gtk_print_dialog_set_modal (GtkPrintDialog *self,
                            gboolean        modal)
{
  g_return_if_fail (GTK_IS_PRINT_DIALOG (self));

  if (self->modal == modal)
    return;

  self->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODAL]);
}

/**
 * gtk_print_dialog_get_default_page_setup:
 * @self: a `GtkPrintDialog`
 *
 * Returns the default page setup.
 *
 * Returns: (transfer none): the default page setup
 *
 * Since: 4.14
 */
GtkPageSetup *
gtk_print_dialog_get_default_page_setup (GtkPrintDialog *self)
{
  g_return_val_if_fail (GTK_IS_PRINT_DIALOG (self), NULL);

  return self->default_page_setup;
}

/**
 * gtk_print_dialog_set_default_page_setup:
 * @self: a `GtkPrintDialog`
 * @default_page_setup: the new default page setup
 *
 * Set the default page setup for the print dialog.
 *
 * Since: 4.14
 */
void
gtk_print_dialog_set_default_page_setup (GtkPrintDialog *self,
                                         GtkPageSetup   *default_page_setup)
{
  g_return_if_fail (GTK_IS_PRINT_DIALOG (self));
  g_return_if_fail (GTK_IS_PAGE_SETUP (default_page_setup));

  if (g_set_object (&self->default_page_setup, default_page_setup))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_PAGE_SETUP]);
}

/**
 * gtk_print_dialog_get_print_settings:
 * @self: a `GtkPrintDialog`
 *
 * Returns the print settings for the print dialog.
 *
 * Returns: (transfer none): the print settings
 *
 * Since: 4.14
 */
GtkPrintSettings *
gtk_print_dialog_get_print_settings (GtkPrintDialog *self)
{
  g_return_val_if_fail (GTK_IS_PRINT_DIALOG (self), NULL);

  return self->print_settings;
}

/**
 * gtk_print_dialog_set_print_settings:
 * @self: a `GtkPrintDialog`
 * @print_settings: the new print settings
 *
 * Sets the print settings for the print dialog.
 *
 * Since: 4.14
 */
void
gtk_print_dialog_set_print_settings (GtkPrintDialog   *self,
                                     GtkPrintSettings *print_settings)
{
  g_return_if_fail (GTK_IS_PRINT_DIALOG (self));
  g_return_if_fail (GTK_IS_PRINT_SETTINGS (print_settings));

  if (g_set_object (&self->print_settings, print_settings))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRINT_SETTINGS]);
}

/**
 * gtk_print_dialog_get_title:
 * @self: a `GtkPrintDialog`
 *
 * Returns the title that will be shown on the
 * print dialog.
 *
 * Returns: the title
 *
 * Since: 4.14
 */
const char *
gtk_print_dialog_get_title (GtkPrintDialog *self)
{
  g_return_val_if_fail (GTK_IS_PRINT_DIALOG (self), NULL);

  return self->title;
}

/* }}} */
 /* {{{ Async implementation */

#ifdef HAVE_GIO_UNIX

static void
send_close (GTask *task)
{
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  GDBusConnection *connection = g_dbus_proxy_get_connection (self->portal);
  GDBusMessage *message;
  GError *error = NULL;

  if (!self->portal_handle)
    return;

  message = g_dbus_message_new_method_call (PORTAL_BUS_NAME,
                                            self->portal_handle,
                                            PORTAL_REQUEST_INTERFACE,
                                            "Close");

  if (!g_dbus_connection_send_message (connection,
                                       message,
                                       G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                       NULL, &error))
    {
      g_warning ("unable to send PrintDialog Close message: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (message);
}

static gboolean
ensure_portal_proxy (GtkPrintDialog  *self,
                     GError         **error)
{
  if (gdk_display_get_debug_flags (NULL) & GDK_DEBUG_NO_PORTALS)
    return FALSE;

  if (!self->portal)
    self->portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  PORTAL_BUS_NAME,
                                                  PORTAL_OBJECT_PATH,
                                                  PORTAL_PRINT_INTERFACE,
                                                  NULL,
                                                  error);

  return self->portal != NULL;
}

static void cleanup_portal_call_data (GTask *task);

static void
cancelled_cb (GCancellable *cancellable,
              GTask        *task)
{
  send_close (task);
  cleanup_portal_call_data (task);

  g_task_return_new_error (task,
                           GTK_DIALOG_ERROR,
                           GTK_DIALOG_ERROR_CANCELLED,
                           "Cancelled by application");
  g_object_unref (task);
}

static void
cleanup_portal_call_data (GTask *task)
{
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  GDBusConnection *connection = g_dbus_proxy_get_connection (self->portal);
  GCancellable *cancellable;

  cancellable = g_task_get_cancellable (task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, task);

  if (self->response_signal_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (connection, self->response_signal_id);
      self->response_signal_id = 0;
    }

  g_clear_pointer (&self->portal_handle, g_free);
  g_clear_object (&self->exported_window);
}

static void
response_to_task (unsigned int  response,
                  GTask        *task)
{
  switch (response)
    {
    case 0:
      g_task_return_boolean (task, TRUE);
      break;
    case 1:
      g_task_return_new_error (task,
                               GTK_DIALOG_ERROR,
                               GTK_DIALOG_ERROR_DISMISSED,
                               "Dismissed by user");
      break;
    case 2:
    default:
      g_task_return_new_error (task,
                               GTK_DIALOG_ERROR,
                               GTK_DIALOG_ERROR_FAILED,
                               "Operation failed");
      break;
    }
}

static void
prepare_print_response (GDBusConnection *connection,
                        const char      *sender_name,
                        const char      *object_path,
                        const char      *interface_name,
                        const char      *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)
{
  GTask *task = user_data;
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  guint32 response;
  GVariant *options = NULL;

  cleanup_portal_call_data (task);

  g_variant_get (parameters, "(u@a{sv})", &response, &options);

  if (response == 0)
    {
      GVariant *v;
      GtkPrintSettings *settings;
      GtkPageSetup *page_setup;
      unsigned int token;

      v = g_variant_lookup_value (options, "settings", G_VARIANT_TYPE_VARDICT);
      settings = gtk_print_settings_new_from_gvariant (v);
      g_variant_unref (v);

      gtk_print_dialog_set_print_settings (self, settings);
      g_object_unref (settings);

      v = g_variant_lookup_value (options, "page-setup", G_VARIANT_TYPE_VARDICT);
      page_setup = gtk_page_setup_new_from_gvariant (v);
      g_variant_unref (v);

      gtk_print_dialog_set_default_page_setup (self, page_setup);
      g_object_unref (page_setup);

      g_variant_lookup (options, "token", "u", &token);
      self->token = token;
    }

  g_variant_unref (options);

  response_to_task (response, task);
  g_object_unref (task);
}

static void
prepare_print_called (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GTask *task = user_data;
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  GDBusConnection *connection = g_dbus_proxy_get_connection (self->portal);
  GError *error = NULL;
  GVariant *ret;
  char *path;

  ret = g_dbus_proxy_call_finish (self->portal, result, &error);
  if (ret == NULL)
    {
      cleanup_portal_call_data (task);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_variant_get (ret, "(o)", &path);
  if (strcmp (path, self->portal_handle) != 0)
   {
      g_free (self->portal_handle);
      self->portal_handle = g_steal_pointer (&path);

      g_dbus_connection_signal_unsubscribe (connection,
                                            self->response_signal_id);

      self->response_signal_id =
        g_dbus_connection_signal_subscribe (connection,
                                            PORTAL_BUS_NAME,
                                            PORTAL_REQUEST_INTERFACE,
                                            "Response",
                                            self->portal_handle,
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                            prepare_print_response,
                                            self, NULL);

    }

  g_free (path);
  g_variant_unref (ret);
}

static void
prepare_print_window_handle_exported (GtkWindow  *window,
                                      const char *window_handle,
                                      gpointer    user_data)
{
  GTask *task = user_data;
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  GDBusConnection *connection = g_dbus_proxy_get_connection (self->portal);
  char *handle_token;
  GVariant *settings;
  GVariant *setup;
  GVariant *options;
  GVariantBuilder opt_builder;

  self->portal_handle = gtk_get_portal_request_path (connection, &handle_token);

  self->response_signal_id =
    g_dbus_connection_signal_subscribe (connection,
                                        PORTAL_BUS_NAME,
                                        PORTAL_REQUEST_INTERFACE,
                                        "Response",
                                        self->portal_handle,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        prepare_print_response,
                                        task, NULL);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&opt_builder, "{sv}", "handle_token", g_variant_new_string (handle_token));
  if (self->accept_label)
    g_variant_builder_add (&opt_builder, "{sv}", "accept_label", g_variant_new_string (self->accept_label));
  options = g_variant_builder_end (&opt_builder);

  if (self->print_settings)
    settings = gtk_print_settings_to_gvariant (self->print_settings);
  else
    {
      GVariantBuilder builder;
      g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
      settings = g_variant_builder_end (&builder);
    }

  if (self->default_page_setup)
    setup = gtk_page_setup_to_gvariant (self->default_page_setup);
  else
    {
      GtkPageSetup *page_setup = gtk_page_setup_new ();
      setup = gtk_page_setup_to_gvariant (page_setup);
      g_object_unref (page_setup);
    }

  self->token = 0;

  g_dbus_proxy_call (self->portal,
                     "PreparePrint",
                     g_variant_new ("(ss@a{sv}@a{sv}@a{sv})",
                                    window_handle,
                                    self->title ? self->title : "",
                                    settings,
                                    setup,
                                    options),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     prepare_print_called,
                     task);

  g_free (handle_token);
}

static void
print_response (GDBusConnection *connection,
                const char      *sender_name,
                const char      *object_path,
                const char      *interface_name,
                const char      *signal_name,
                GVariant        *parameters,
                gpointer         user_data)
{
  GTask *task = user_data;
  guint32 response;

  cleanup_portal_call_data (task);
  g_variant_get (parameters, "(ua{sv})", &response, NULL);
  response_to_task (response, task);
  g_object_unref (task);
}

static void
print_called (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
  GTask *task = user_data;
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  GDBusConnection *connection = g_dbus_proxy_get_connection (self->portal);
  GError *error = NULL;
  GVariant *ret;
  char *path;

  ret = g_dbus_proxy_call_finish (self->portal, result, &error);
  if (ret == NULL)
    {
      cleanup_portal_call_data (task);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_variant_get (ret, "(o)", &path);
  if (strcmp (path, self->portal_handle) != 0)
   {
      g_free (self->portal_handle);
      self->portal_handle = g_steal_pointer (&path);

      g_dbus_connection_signal_unsubscribe (connection,
                                            self->response_signal_id);

      self->response_signal_id =
        g_dbus_connection_signal_subscribe (connection,
                                            PORTAL_BUS_NAME,
                                            PORTAL_REQUEST_INTERFACE,
                                            "Response",
                                            self->portal_handle,
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                            print_response,
                                            self, NULL);

    }

  g_free (path);
  g_variant_unref (ret);
}

static int
get_content_fd (GObject  *content,
                GError  **error)
{
  if (G_IS_FILE_DESCRIPTOR_BASED (content))
    return g_file_descriptor_based_get_fd (G_FILE_DESCRIPTOR_BASED (content));
  else
    {
      g_set_error (error,
                   GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                   "Not implemented");
      return -1;
    }
}

static void
print_window_handle_exported (GtkWindow  *window,
                              const char *window_handle,
                              gpointer    user_data)
{
  GTask *task = user_data;
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  GDBusConnection *connection = g_dbus_proxy_get_connection (self->portal);
  int fd;
  char *handle_token;
  GVariantBuilder opt_builder;
  GUnixFDList *fd_list;
  int idx;
  GError *error = NULL;

  if (window)
    self->exported_window = g_object_ref (window);

  fd = get_content_fd (g_task_get_task_data (task), &error);
  if (fd == -1)
    {
      cleanup_portal_call_data (task);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  self->portal_handle = gtk_get_portal_request_path (connection, &handle_token);

  self->response_signal_id =
    g_dbus_connection_signal_subscribe (connection,
                                        PORTAL_BUS_NAME,
                                        PORTAL_REQUEST_INTERFACE,
                                        "Response",
                                        self->portal_handle,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        print_response,
                                        task, NULL);

  fd_list = g_unix_fd_list_new ();
  idx = g_unix_fd_list_append (fd_list, fd, NULL);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&opt_builder, "{sv}", "handle_token", g_variant_new_string (handle_token));
  g_variant_builder_add (&opt_builder, "{sv}", "token", g_variant_new_uint32 (self->token));

  g_dbus_proxy_call_with_unix_fd_list (self->portal,
                                       "Print",
                                       g_variant_new ("(ssh@a{sv})",
                                                      window_handle,
                                                      self->title ? self->title : "",
                                                      idx,
                                                      g_variant_builder_end (&opt_builder)),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       fd_list,
                                       NULL,
                                       print_called,
                                       task);
  g_object_unref (fd_list);
  g_free (handle_token);
}

static GtkPrintUnixDialog *
create_print_dialog (GtkPrintDialog *self,
                     GtkWindow      *parent)
{
  GtkPrintUnixDialog *dialog;

  dialog = GTK_PRINT_UNIX_DIALOG (gtk_print_unix_dialog_new (self->title, parent));

  if (self->print_settings)
    gtk_print_unix_dialog_set_settings (dialog, self->print_settings);

  if (self->default_page_setup)
    gtk_print_unix_dialog_set_page_setup (dialog, self->default_page_setup);

  return dialog;
}

static void
response_cb (GtkPrintUnixDialog *window,
             int                 response,
             GTask              *task)
{
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  GCancellable *cancellable = g_task_get_cancellable (task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, task);

  if (response == GTK_RESPONSE_OK)
    {
      gtk_print_dialog_set_print_settings (self, gtk_print_unix_dialog_get_settings (window));
      gtk_print_dialog_set_default_page_setup (self, gtk_print_unix_dialog_get_page_setup (window));
      g_set_object (&self->printer, gtk_print_unix_dialog_get_selected_printer (window));

      g_task_return_boolean (task, TRUE);
    }
  else if (response == GTK_RESPONSE_CLOSE)
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED, "Cancelled by application");
  else if (response == GTK_RESPONSE_CANCEL ||
           response == GTK_RESPONSE_DELETE_EVENT)
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED, "Dismissed by user");
  else
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED, "Unknown failure (%d)", response);

  g_object_unref (task);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
job_complete (GtkPrintJob  *job,
              gpointer      user_data,
              const GError *error)
{
  GTask *task = user_data;
  if (error)
    g_task_return_error (task, g_error_copy (error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
print_file (GtkPrintDialog *self,
            GTask          *task)
{
  int fd;
  GError *error = NULL;

  fd = get_content_fd (g_task_get_task_data (task), &error);
  if (fd == -1)
    {
      cleanup_portal_call_data (task);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  if (self->printer)
    {
      GtkPrintJob *job;

      job = gtk_print_job_new ("My first printjob",
                               self->printer,
                               self->print_settings,
                               self->default_page_setup);
      gtk_print_job_set_source_fd (job, fd, NULL);
      gtk_print_job_send (job, job_complete, task, g_object_unref);
      g_object_unref (job);
    }
  else
    {
      g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED, "No printer selected");
      g_object_unref (task);
    }
}

static void
print_response_cb (GtkPrintUnixDialog *window,
                   int                 response,
                   GTask              *task)
{
  GtkPrintDialog *self = GTK_PRINT_DIALOG (g_task_get_source_object (task));
  GCancellable *cancellable = g_task_get_cancellable (task);

  if (cancellable)
    g_signal_handlers_disconnect_by_func (cancellable, cancelled_cb, task);

  if (response == GTK_RESPONSE_OK)
    {
      gtk_print_dialog_set_print_settings (self, gtk_print_unix_dialog_get_settings (window));
      gtk_print_dialog_set_default_page_setup (self, gtk_print_unix_dialog_get_page_setup (window));
      g_set_object (&self->printer, gtk_print_unix_dialog_get_selected_printer (window));

      print_file (self, g_object_ref (task));
    }
  else if (response == GTK_RESPONSE_CLOSE)
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED, "Cancelled by application");
  else if (response == GTK_RESPONSE_CANCEL ||
           response == GTK_RESPONSE_DELETE_EVENT)
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED, "Dismissed by user");
  else
    g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED, "Unknown failure (%d)", response);

  g_object_unref (task);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
try_to_find_printer (GtkPrintDialog *self)
{
  const char *name;

  if (self->printer)
    return;

  if (!self->print_settings)
    return;

  name = gtk_print_settings_get (self->print_settings, GTK_PRINT_SETTINGS_PRINTER);

  if (!name)
    return;

  self->printer = gtk_printer_find (name);
}

#endif /* HAVE_GIO_UNIX */

/* }}} */
/* {{{ Async API */

/**
 * gtk_print_dialog_prepare_print:
 * @self: a `GtkPrintDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function presents a print dialog to let the
 * user select a printer, and set up print settings
 * and page setup.
 *
 * The @callback will be called when the dialog is dismissed.
 * It should call [method@Gtk.PrintDialog.prepare_print_finish]
 * to obtain the results.
 *
 * One possible use for this method is to have the user select a printer,
 * then show a page setup UI in the application (e.g. to arrange images
 * on a page), then call [method@Gtk.PrintDialog.print_stream] on @self
 * to do the printing without further user interaction.
 *
 * Since: 4.14
 */
void
gtk_print_dialog_prepare_print (GtkPrintDialog       *self,
                                GtkWindow            *parent,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
{
  GTask *task;
  G_GNUC_UNUSED GError *error = NULL;

  g_return_if_fail (GTK_IS_PRINT_DIALOG (self));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (self->response_signal_id == 0);
  g_return_if_fail (self->exported_window == NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_print_dialog_prepare_print);

#ifdef HAVE_GIO_UNIX
  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  if (!ensure_portal_proxy (self, &error))
    {
      GtkPrintUnixDialog *window;

      window = create_print_dialog (self, parent);
      g_signal_connect (window, "response", G_CALLBACK (response_cb), task);
      gtk_window_present (GTK_WINDOW (window));
    }
  else
    {
      if (parent &&
          gtk_widget_is_visible (GTK_WIDGET (parent)) &&
          gtk_window_export_handle (parent, prepare_print_window_handle_exported, task))
        return;
      prepare_print_window_handle_exported (parent, "", task);
    }
#else
  g_task_return_new_error (task,
                           GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                           "GtkPrintDialog is not supported on this platform");
  g_object_unref (task);
#endif
}

/**
 * gtk_print_dialog_prepare_print_finish:
 * @self: a `GtkPrintDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.PrintDialog.prepare_print] call.
 *
 * If the call was successful, the print settings and the
 * default page setup will be updated with the users changes.
 *
 * Returns: Whether the call was successful
 *
 * Since: 4.14
 */
gboolean
gtk_print_dialog_prepare_print_finish (GtkPrintDialog    *self,
                                       GAsyncResult      *result,
                                       GError           **error)
{
  g_return_val_if_fail (GTK_IS_PRINT_DIALOG (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_print_dialog_prepare_print, FALSE);
  g_return_val_if_fail (self->response_signal_id == 0, FALSE);
  g_return_val_if_fail (self->exported_window == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * gtk_print_dialog_print_stream:
 * @self: a `GtkPrintDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @content: the `GInputStream` to print
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function prints content from an input stream.
 *
 * If [method@Gtk.PrintDialog.prepare_print] has not been called
 * on @self before, then this method might present a print dialog.
 * Otherwise, it will attempt to print directly, without user
 * interaction.
 *
 * The @callback will be called when the printing is done.
 * It should call [method@Gtk.PrintDialog.print_stream_finish]
 * to obtain the results.
 *
 * Since: 4.14
 */
void
gtk_print_dialog_print_stream (GtkPrintDialog       *self,
                               GtkWindow            *parent,
                               GInputStream         *content,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
  GTask *task;
  G_GNUC_UNUSED GError *error = NULL;

  g_return_if_fail (GTK_IS_PRINT_DIALOG (self));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (G_IS_INPUT_STREAM (content));
  g_return_if_fail (self->response_signal_id == 0);
  g_return_if_fail (self->exported_window == NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_print_dialog_print_stream);
  g_task_set_task_data (task, g_object_ref (content), g_object_unref);

#ifdef HAVE_GIO_UNIX
  if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  if (!ensure_portal_proxy (self, &error))
    {
      try_to_find_printer (self);

      if (!self->printer)
        {
          GtkPrintUnixDialog *window;

          window = create_print_dialog (self, parent);
          g_signal_connect (window, "response", G_CALLBACK (print_response_cb), task);
          gtk_window_present (GTK_WINDOW (window));
        }
      else
        {
          print_file (self, task);
        }
    }
  else
    {
      if (parent &&
          gtk_widget_is_visible (GTK_WIDGET (parent)) &&
          gtk_window_export_handle (parent, print_window_handle_exported, task))
        return;

      print_window_handle_exported (parent, "", task);
    }
#else
  g_task_return_new_error (task,
                           GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                           "GtkPrintDialog is not supported on this platform");
  g_object_unref (task);
#endif
}

/**
 * gtk_print_dialog_print_stream_finish:
 * @self: a `GtkPrintDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.PrintDialog.print_stream] call and
 * returns the results.
 *
 * Returns: Whether the call was successful
 *
 * Since: 4.14
 */
gboolean
gtk_print_dialog_print_stream_finish (GtkPrintDialog  *self,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  g_return_val_if_fail (GTK_IS_PRINT_DIALOG (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_print_dialog_print_stream, FALSE);
  g_return_val_if_fail (self->response_signal_id == 0, FALSE);
  g_return_val_if_fail (self->exported_window == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * gtk_print_dialog_print_file:
 * @self: a `GtkPrintDialog`
 * @parent: (nullable): the parent `GtkWindow`
 * @file: the `GFile` to print
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function prints a file.
 *
 * If [method@Gtk.PrintDialog.prepare_print] has not been called
 * on @self before, then this method might present a print dialog.
 * Otherwise, it will attempt to print directly, without user
 * interaction.
 *
 * The @callback will be called when the printing is done.
 * It should call [method@Gtk.PrintDialog.print_file_finish]
 * to obtain the results.
 *
 * Since: 4.14
 */
void
gtk_print_dialog_print_file (GtkPrintDialog       *self,
                             GtkWindow            *parent,
                             GFile                *file,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
  GTask *task;
  GFileInputStream *content;
  GError *error = NULL;

  g_return_if_fail (GTK_IS_PRINT_DIALOG (self));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (self->response_signal_id == 0);
  g_return_if_fail (self->exported_window == NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_print_dialog_print_file);

  content = g_file_read (file, NULL, &error);
  if (!content)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

#ifdef HAVE_GIO_UNIX
  g_task_set_task_data (task, content, g_object_unref);

 if (cancellable)
    g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), task);

  if (!ensure_portal_proxy (self, &error))
    {
      try_to_find_printer (self);

      if (!self->printer)
        {
          GtkPrintUnixDialog *window;

          window = create_print_dialog (self, parent);
          g_signal_connect (window, "response", G_CALLBACK (print_response_cb), task);
          gtk_window_present (GTK_WINDOW (window));
        }
      else
        {
          print_file (self, task);
        }
    }
  else
    {
      if (parent &&
          gtk_widget_is_visible (GTK_WIDGET (parent)) &&
          gtk_window_export_handle (parent, print_window_handle_exported, task))
        return;

      print_window_handle_exported (parent, "", task);
    }
#else
  g_task_return_new_error (task,
                           GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                           "GtkPrintDialog is not supported on this platform");
  g_object_unref (task);
#endif
}

/**
 * gtk_print_dialog_print_file_finish:
 * @self: a `GtkPrintDialog`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] error
 *
 * Finishes the [method@Gtk.PrintDialog.print_file] call and
 * returns the results.
 *
 * Returns: Whether the call was successful
 *
 * Since: 4.14
 */
gboolean
gtk_print_dialog_print_file_finish (GtkPrintDialog  *self,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  g_return_val_if_fail (GTK_IS_PRINT_DIALOG (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_print_dialog_print_file, FALSE);
  g_return_val_if_fail (self->response_signal_id == 0, FALSE);
  g_return_val_if_fail (self->exported_window == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
