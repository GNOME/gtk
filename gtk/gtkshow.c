/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2008  Jaap Haitsma <jaap@haitsma.org>
 *
 * All rights reserved.
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

#include <gdk/gdk.h>

#include "gtkshow.h"
#include "gtkwindowprivate.h"
#include "gtkmessagedialog.h"
#include "gtkintl.h"

typedef struct {
  GtkWindow *parent;
  GAppLaunchContext *context;
  char *uri;
  GTask *task;
} GtkShowUriData;

static void
gtk_show_uri_data_free (GtkShowUriData *data)
{
  if (data->parent)
    gtk_window_unexport_handle (data->parent);
  g_clear_object (&data->parent);
  g_clear_object (&data->context);
  g_free (data->uri);
  g_clear_object (&data->task);
  g_free (data);
}

static void
launch_uri_done (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GtkShowUriData *data = user_data;
  GError *error = NULL;

  if (g_app_info_launch_default_for_uri_finish (result, &error))
    g_task_return_boolean (data->task, TRUE);
  else
    g_task_return_error (data->task, error);

  gtk_show_uri_data_free (data);
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle,
                        gpointer    user_data)
{
  GtkShowUriData *data = user_data;

  if (handle)
    g_app_launch_context_setenv (data->context, "PARENT_WINDOW_ID", handle);

  g_app_info_launch_default_for_uri_async (data->uri,
                                           data->context,
                                           g_task_get_cancellable (data->task),
                                           launch_uri_done,
                                           data);
}

/**
 * gtk_show_uri_full:
 * @parent: (nullable): parent window
 * @uri: the uri to show
 * @timestamp: timestamp from the event that triggered this call, or %GDK_CURRENT_TIME
 * @cancellable: (nullable): a #GCancellable to cancel the launch
 * @callback: (scope async): a callback to call when the action is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function launches the default application for showing
 * a given uri.
 *
 * The uri must be of a form understood by GIO (i.e. you need
 * to install gvfs to get support for uri schemes such as http://
 * or ftp://, as only local files are handled by GIO itself).
 * Typical examples are
 * - `file:///home/gnome/pict.jpg`
 * - `http://www.gnome.org`
 * - `mailto:me@gnome.org`
 *
 * The @callback will be called when the launch is completed.
 * It should call gtk_show_uri_finish() to obtain the result.
 *
 * This is the recommended call to be used as it passes information
 * necessary for sandbox helpers to parent their dialogs properly.
 */
void
gtk_show_uri_full (GtkWindow           *parent,
                   const char          *uri,
                   guint32              timestamp,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  GtkShowUriData *data;
  GdkAppLaunchContext *context;
  GdkDisplay *display;

  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (uri != NULL);

  if (parent)
    display = gtk_widget_get_display (GTK_WIDGET (parent));
  else
    display = gdk_display_get_default ();

  context = gdk_display_get_app_launch_context (display);
  gdk_app_launch_context_set_timestamp (context, timestamp);

  data = g_new0 (GtkShowUriData, 1);
  data->parent = parent ? g_object_ref (parent) : NULL;
  data->context = G_APP_LAUNCH_CONTEXT (context);
  data->uri = g_strdup (uri);
  data->task = g_task_new (parent, cancellable, callback, user_data);
  g_task_set_source_tag (data->task, gtk_show_uri);

  if (parent)
    gtk_window_export_handle (parent, window_handle_exported, data);
  else
    window_handle_exported (parent, NULL, data);
}

/**
 * gtk_show_uri_full_finish:
 * @parent: the #GtkWindow passed to gtk_show_uri()
 * @result: #GASyncResult that was passed to @callback
 * @error: return location for an error
 *
 * Finished the gtk_show_uri() call and returns the result
 * of the operation.
 *
 * Returns: %TRUE if the URI was shown successfully.
 *     Otherwise, %FALSE is returned and @error is set
 */
gboolean
gtk_show_uri_full_finish (GtkWindow     *parent,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, parent), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_show_uri, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
show_uri_done (GObject      *object,
               GAsyncResult *result,
               gpointer      data)
{
  GtkWindow *parent = GTK_WINDOW (object);
  GError *error = NULL;

  if (!gtk_show_uri_full_finish (parent, result, &error))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (parent,
                                       GTK_DIALOG_DESTROY_WITH_PARENT |
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "%s", _("Could not show link"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s", error->message);

      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_window_present (GTK_WINDOW (dialog));
      G_GNUC_END_IGNORE_DEPRECATIONS
    }
}

/**
 * gtk_show_uri:
 * @parent: (allow-none): parent window
 * @uri: the uri to show
 * @timestamp: timestamp from the event that triggered this call, or %GDK_CURRENT_TIME
 *
 * This function launches the default application for showing
 * a given uri, or shows an error dialog if that fails.
 *
 * The uri must be of a form understood by GIO (i.e. you need
 * to install gvfs to get support for uri schemes such as http://
 * or ftp://, as only local files are handled by GIO itself).
 * Typical examples are
 * - `file:///home/gnome/pict.jpg`
 * - `http://www.gnome.org`
 * - `mailto:me@gnome.org`
 */
void
gtk_show_uri (GtkWindow  *parent,
              const char *uri,
              guint32     timestamp)
{
  gtk_show_uri_full (parent, uri, timestamp, NULL, show_uri_done, NULL);
}
