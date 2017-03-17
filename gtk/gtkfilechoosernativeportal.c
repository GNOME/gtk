/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechoosernativeportal.c: Portal File selector dialog
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
#include "gtkmain.h"
#include "gtkinvisible.h"
#include "gtkfilechooserentry.h"
#include "gtkfilefilterprivate.h"
#include "gtkwindowprivate.h"

typedef struct {
  GtkFileChooserNative *self;

  GtkWidget *grab_widget;

  GDBusConnection *connection;
  char *portal_handle;
  guint portal_response_signal_id;
  gboolean modal;

  gboolean hidden;

  const char *method_name;

  GtkWindow *exported_window;
} FilechooserPortalData;


static void
filechooser_portal_data_free (FilechooserPortalData *data)
{
  if (data->portal_response_signal_id != 0)
    g_dbus_connection_signal_unsubscribe (data->connection,
                                          data->portal_response_signal_id);

  g_object_unref (data->connection);

  if (data->grab_widget)
    {
      gtk_grab_remove (data->grab_widget);
      gtk_widget_destroy (data->grab_widget);
    }

  g_clear_object (&data->self);

  if (data->exported_window)
    gtk_window_unexport_handle (data->exported_window);

  g_free (data->portal_handle);

  g_free (data);
}

static void
response_cb (GDBusConnection  *connection,
             const gchar      *sender_name,
             const gchar      *object_path,
             const gchar      *interface_name,
             const gchar      *signal_name,
             GVariant         *parameters,
             gpointer          user_data)
{
  GtkFileChooserNative *self = user_data;
  FilechooserPortalData *data = self->mode_data;
  guint32 portal_response;
  int gtk_response;
  const char **uris;
  int i;
  GVariant *response_data;
  GVariant *choices = NULL;

  g_variant_get (parameters, "(u@a{sv})", &portal_response, &response_data);
  g_variant_lookup (response_data, "uris", "^a&s", &uris);

  choices = g_variant_lookup_value (response_data, "choices", G_VARIANT_TYPE ("a(ss)"));
  if (choices)
    {
      for (i = 0; i < g_variant_n_children (choices); i++)
        {
          const char *id;
          const char *selected;
          g_variant_get_child (choices, i, "(&s&s)", &id, &selected);
          gtk_file_chooser_set_choice (GTK_FILE_CHOOSER (self), id, selected);
        }
      g_variant_unref (choices);
    }

  g_slist_free_full (self->custom_files, g_object_unref);
  self->custom_files = NULL;
  for (i = 0; uris[i]; i++)
    self->custom_files = g_slist_prepend (self->custom_files, g_file_new_for_uri (uris[i]));

  switch (portal_response)
    {
    case 0:
      gtk_response = GTK_RESPONSE_ACCEPT;
      break;
    case 1:
      gtk_response = GTK_RESPONSE_CANCEL;
      break;
    case 2:
    default:
      gtk_response = GTK_RESPONSE_DELETE_EVENT;
      break;
    }

  filechooser_portal_data_free (data);
  self->mode_data = NULL;

  _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (self), gtk_response);
}

static void
send_close (FilechooserPortalData *data)
{
  GDBusMessage *message;
  GError *error = NULL;

  message = g_dbus_message_new_method_call ("org.freedesktop.portal.Desktop",
                                            "/org/freedesktop/portal/desktop",
                                            "org.freedesktop.portal.FileChooser",
                                            "Close");
  g_dbus_message_set_body (message,
                           g_variant_new ("(o)", data->portal_handle));

  if (!g_dbus_connection_send_message (data->connection,
                                       message,
                                       G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                       NULL, &error))
    {
      g_warning ("unable to send FileChooser Close message: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (message);
}

static void
open_file_msg_cb (GObject *source_object,
                  GAsyncResult *res,
                  gpointer user_data)
{
  FilechooserPortalData *data = user_data;
  GtkFileChooserNative *self = data->self;
  GDBusMessage *reply;
  GError *error = NULL;

  reply = g_dbus_connection_send_message_with_reply_finish (data->connection, res, &error);

  if (reply && g_dbus_message_to_gerror (reply, &error))
    g_clear_object (&reply);

  if (reply == NULL)
    {
      if (!data->hidden)
        _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (self), GTK_RESPONSE_DELETE_EVENT);
      g_warning ("Can't open portal file chooser: %s", error->message);
      g_error_free (error);
      filechooser_portal_data_free (data);
      self->mode_data = NULL;
      return;
    }

  g_variant_get_child (g_dbus_message_get_body (reply), 0, "o",
                       &data->portal_handle);

  if (data->hidden)
    {
      /* The dialog was hidden before we got the handle, close it now */
      send_close (data);
      filechooser_portal_data_free (data);
      self->mode_data = NULL;
    }
  else
    {
      data->portal_response_signal_id =
        g_dbus_connection_signal_subscribe (data->connection,
                                            "org.freedesktop.portal.Desktop",
                                            "org.freedesktop.portal.Request",
                                            "Response",
                                            data->portal_handle,
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                            response_cb,
                                            self, NULL);
    }

  g_object_unref (reply);
}

static GVariant *
get_filters (GtkFileChooser *self)
{
  GSList *list, *l;
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sa(us))"));
  list = gtk_file_chooser_list_filters (self);
  for (l = list; l; l = l->next)
    {
      GtkFileFilter *filter = l->data;
      g_variant_builder_add (&builder, "@(sa(us))", gtk_file_filter_to_gvariant (filter));
    }
  g_slist_free (list);

  return g_variant_builder_end (&builder);
}

static GVariant *
gtk_file_chooser_native_choice_to_variant (GtkFileChooserNativeChoice *choice)
{
  GVariantBuilder choices;
  int i;

  g_variant_builder_init (&choices, G_VARIANT_TYPE ("a(ss)"));
  if (choice->options)
    {
      for (i = 0; choice->options[i]; i++)
        g_variant_builder_add (&choices, "(&s&s)", choice->options[i], choice->option_labels[i]);
    }

  return g_variant_new ("(&s&s@a(ss)&s)",
                        choice->id,
                        choice->label,
                        g_variant_builder_end (&choices),
                        choice->selected ? choice->selected : "");
}

static GVariant *
serialize_choices (GtkFileChooserNative *self)
{
  GVariantBuilder builder;
  GSList *l;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssa(ss)s)"));
  for (l = self->choices; l; l = l->next)
    {
      GtkFileChooserNativeChoice *choice = l->data;

      g_variant_builder_add (&builder, "@(ssa(ss)s)",
                             gtk_file_chooser_native_choice_to_variant (choice));
    }

  return g_variant_builder_end (&builder);
}

static void
show_portal_file_chooser (GtkFileChooserNative *self,
                          const char           *parent_window_str)
{
  FilechooserPortalData *data = self->mode_data;
  GDBusMessage *message;
  GVariantBuilder opt_builder;
  gboolean multiple;

  message = g_dbus_message_new_method_call ("org.freedesktop.portal.Desktop",
                                            "/org/freedesktop/portal/desktop",
                                            "org.freedesktop.portal.FileChooser",
                                            data->method_name);

  multiple = gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (self));
  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&opt_builder, "{sv}", "multiple",
                         g_variant_new_boolean (multiple));
  if (self->accept_label)
    g_variant_builder_add (&opt_builder, "{sv}", "accept_label",
                           g_variant_new_string (self->accept_label));
  if (self->cancel_label)
    g_variant_builder_add (&opt_builder, "{sv}", "cancel_label",
                           g_variant_new_string (self->cancel_label));
  g_variant_builder_add (&opt_builder, "{sv}", "modal",
                         g_variant_new_boolean (data->modal));
  g_variant_builder_add (&opt_builder, "{sv}", "filters", get_filters (GTK_FILE_CHOOSER (self)));
  if (GTK_FILE_CHOOSER_NATIVE (self)->current_name)
    g_variant_builder_add (&opt_builder, "{sv}", "current_name",
                           g_variant_new_string (GTK_FILE_CHOOSER_NATIVE (self)->current_name));
  if (GTK_FILE_CHOOSER_NATIVE (self)->current_folder)
    {
      gchar *path;

      path = g_file_get_path (GTK_FILE_CHOOSER_NATIVE (self)->current_folder);
      g_variant_builder_add (&opt_builder, "{sv}", "current_folder",
                             g_variant_new_bytestring (path));
      g_free (path);
    }
  if (GTK_FILE_CHOOSER_NATIVE (self)->current_file)
    {
      gchar *path;

      path = g_file_get_path (GTK_FILE_CHOOSER_NATIVE (self)->current_file);
      g_variant_builder_add (&opt_builder, "{sv}", "current_file",
                             g_variant_new_bytestring (path));
      g_free (path);
    }

  if (GTK_FILE_CHOOSER_NATIVE (self)->choices)
    g_variant_builder_add (&opt_builder, "{sv}", "choices",
                           serialize_choices (GTK_FILE_CHOOSER_NATIVE (self)));

  g_dbus_message_set_body (message,
                           g_variant_new ("(ss@a{sv})",
                                          parent_window_str ? parent_window_str : "",
                                          gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (self)),
                                          g_variant_builder_end (&opt_builder)));

  g_dbus_connection_send_message_with_reply (data->connection,
                                             message,
                                             G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                             G_MAXINT,
                                             NULL,
                                             NULL,
                                             open_file_msg_cb,
                                             data);

  g_object_unref (message);
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle_str,
                        gpointer    user_data)
{
  GtkFileChooserNative *self = user_data;
  FilechooserPortalData *data = self->mode_data;

  if (data->modal)
    {
      GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (window));

      data->grab_widget = gtk_invisible_new_for_screen (screen);
      gtk_grab_add (GTK_WIDGET (data->grab_widget));
    }

  show_portal_file_chooser (self, handle_str);
}

gboolean
gtk_file_chooser_native_portal_show (GtkFileChooserNative *self)
{
  FilechooserPortalData *data;
  GtkWindow *transient_for;
  GDBusConnection *connection;
  GtkFileChooserAction action;
  const char *method_name;

  if (!gtk_should_use_portal ())
    return FALSE;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (connection == NULL)
    return FALSE;

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self));

  if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
    method_name = "OpenFile";
  else if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    method_name = "SaveFile";
  else
    {
      g_warning ("GTK_FILE_CHOOSER_ACTION_%s is not supported by GtkFileChooserNativePortal", action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ? "SELECT_FOLDER" : "CREATE_FOLDER");
      return FALSE;
    }

  data = g_new0 (FilechooserPortalData, 1);
  data->self = g_object_ref (self);
  data->connection = connection;

  data->method_name = method_name;

  if (gtk_native_dialog_get_modal (GTK_NATIVE_DIALOG (self)))
    data->modal = TRUE;

  self->mode_data = data;

  transient_for = gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (self));
  if (transient_for != NULL && gtk_widget_is_visible (GTK_WIDGET (transient_for)))
    {
      if (!gtk_window_export_handle (transient_for,
                                     window_handle_exported,
                                     self))
        {
          g_warning ("Failed to export handle, could not set transient for");
          show_portal_file_chooser (self, NULL);
        }
      else
        {
          data->exported_window = transient_for;
        }
    }
  else
    {
      show_portal_file_chooser (self, NULL);
    }

  return TRUE;
}

void
gtk_file_chooser_native_portal_hide (GtkFileChooserNative *self)
{
  FilechooserPortalData *data = self->mode_data;

  /* This is always set while dialog visible */
  g_assert (data != NULL);

  data->hidden = TRUE;

  if (data->portal_handle)
    {
      send_close (data);
      filechooser_portal_data_free (data);
    }

  self->mode_data = NULL;
}
