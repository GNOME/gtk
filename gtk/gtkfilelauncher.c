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

#include "gtkfilelauncher.h"

#include "gtkdialogerror.h"
#include "gopenuriportal.h"
#include "deprecated/gtkshow.h"
#include <glib/gi18n-lib.h>

/**
 * GtkFileLauncher:
 *
 * A `GtkFileLauncher` object collects the arguments that are needed to open a uri
 * with an application.
 *
 * Depending on system configuration, user preferences and available APIs, this
 * may or may not show an app chooser dialog or launch the default application
 * right away.
 *
 * The operation is started with the [method@Gtk.FileLauncher.launch] function.
 * This API follows the GIO async pattern, and the result can be obtained by
 * calling [method@Gtk.FileLauncher.launch_finish].
 *
 * Since: 4.10
 */

/* {{{ GObject implementation */

struct _GtkFileLauncher
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GtkFileLauncher, gtk_file_launcher, G_TYPE_OBJECT)

static void
gtk_file_launcher_init (GtkFileLauncher *self)
{
}

static void
gtk_file_launcher_finalize (GObject *object)
{
  //GtkFileLauncher *self = GTK_FILE_LAUNCHER (object);

  G_OBJECT_CLASS (gtk_file_launcher_parent_class)->finalize (object);
}

static void
gtk_file_launcher_class_init (GtkFileLauncherClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_file_launcher_finalize;
}

/* }}} */
/* {{{ API: Constructor */

/**
 * gtk_file_launcher_new:
 *
 * Creates a new `GtkFileLauncher` object.
 *
 * Returns: the new `GtkFileLauncher`
 *
 * Since: 4.10
 */
GtkFileLauncher *
gtk_file_launcher_new (void)
{
  return g_object_new (GTK_TYPE_FILE_LAUNCHER, NULL);
}

 /* }}} */
/* {{{ Async implementation */

#ifndef G_OS_WIN32
static void
open_done (GObject      *source,
           GAsyncResult *result,
           gpointer      data)
{
  GTask *task = G_TASK (data);
  GError *error = NULL;

  if (!g_openuri_portal_open_finish (result, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}
#endif

static void
show_folder_done (GObject      *source,
                  GAsyncResult *result,
                  gpointer      data)
{
  GDBusConnection *bus = G_DBUS_CONNECTION (source);
  GTask *task = G_TASK (data);
  GVariant *res;
  GError *error = NULL;

  res = g_dbus_connection_call_finish (bus, result, &error);
  if (res)
    g_variant_unref (res);

  if (error)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED, "Cancelled by user");
      else
        g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED, "%s", error->message);
      g_error_free (error);
    }
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}

#define FILE_MANAGER_DBUS_NAME "org.freedesktop.FileManager1"
#define FILE_MANAGER_DBUS_IFACE "org.freedesktop.FileManager1"
#define FILE_MANAGER_DBUS_PATH "/org/freedesktop/FileManager1"

static void
show_folder (GtkWindow           *parent,
             const char          *uri,
             GCancellable        *cancellable,
             GAsyncReadyCallback  callback,
             gpointer             user_data)
{
  GDBusConnection *bus;
  GVariantBuilder uris_builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_STRING_ARRAY);

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  if (!bus)
    {
      g_task_return_new_error (G_TASK (user_data),
                               GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                               "Session bus not available");
      g_object_unref (G_TASK (user_data));
      return;
    }

  g_variant_builder_add (&uris_builder, "s", uri);

  g_dbus_connection_call (bus,
                          FILE_MANAGER_DBUS_NAME,
                          FILE_MANAGER_DBUS_PATH,
                          FILE_MANAGER_DBUS_IFACE,
                          "ShowFolders",
                          g_variant_new ("(ass)", &uris_builder, ""),
                          NULL,   /* ignore returned type */
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          show_folder_done,
                          user_data);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
show_uri_done (GObject      *source,
               GAsyncResult *result,
               gpointer      data)
{
  GtkWindow *parent = GTK_WINDOW (source);
  GTask *task = G_TASK (data);
  GError *error = NULL;

  if (!gtk_show_uri_full_finish (parent, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED, "Cancelled by user");
      else
        g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED, "%s", error->message);
      g_error_free (error);
    }
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}
G_GNUC_END_IGNORE_DEPRECATIONS

 /* }}} */
/* {{{ Async API */

/**
 * gtk_file_launcher_launch:
 * @self: a `GtkFileLauncher`
 * @parent: (nullable): the parent `GtkWindow`
 * @file: (nullable): the `GFile` to open
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * Launch an application to open the file.
 *
 * This may present an app chooser dialog to the user.
 *
 * The @callback will be called when the operation is completed.
 * It should call [method@Gtk.FileLauncher.launch_finish] to obtain
 * the result.
 *
 * Since: 4.10
 */
void
gtk_file_launcher_launch (GtkFileLauncher     *self,
                          GtkWindow           *parent,
                          GFile               *file,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_LAUNCHER (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_file_launcher_launch);

#ifndef G_OS_WIN32
  if (g_openuri_portal_is_available ())
    {
      g_openuri_portal_open_async (file, FALSE, parent, cancellable, open_done, task);
    }
  else
#endif
    {
      char *uri = g_file_get_uri (file);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_show_uri_full (parent, uri, GDK_CURRENT_TIME, cancellable, show_uri_done, task);
G_GNUC_END_IGNORE_DEPRECATIONS

      g_free (uri);
    }
}

/**
 * gtk_file_launcher_launch_finish:
 * @self: a `GtkFileLauncher`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] or [enum@Gio.Error] error
 *
 * Finishes the [method@Gtk.FileLauncher.launch] call and
 * returns the result.
 *
 * Returns: `TRUE` if an application was launched,
 *     or `FALSE` and @error is set
 *
 * Since: 4.10
 */
gboolean
gtk_file_launcher_launch_finish (GtkFileLauncher  *self,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  g_return_val_if_fail (GTK_IS_FILE_LAUNCHER (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_file_launcher_launch, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * gtk_file_launcher_open_containing_folder:
 * @self: a `GtkFileLauncher`
 * @parent: (nullable): the parent `GtkWindow`
 * @file: (nullable): the `GFile` to open
 * @cancellable: (nullable): a `GCancellable` to cancel the operation
 * @callback: (scope async): a callback to call when the operation is complete
 * @user_data: (closure callback): data to pass to @callback
 *
 * Launch a file manager to show the file in its parent directory.
 *
 * This is only supported native files. It will fail if @file
 * is e.g. a http:// uri.
 *
 * The @callback will be called when the operation is completed.
 * It should call [method@Gtk.FileLauncher.open_containing_folder_finish]
 * to obtain the result.
 *
 * Since: 4.10
 */
void
gtk_file_launcher_open_containing_folder (GtkFileLauncher     *self,
                                          GtkWindow           *parent,
                                          GFile               *file,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (GTK_IS_FILE_LAUNCHER (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_source_tag (task, gtk_file_launcher_open_containing_folder);

  if (!g_file_is_native (file))
    {
      g_task_return_new_error (task,
                               GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                               "Operation not supported on non-native files");
      return;
    }

#ifndef G_OS_WIN32
  if (g_openuri_portal_is_available ())
    {
      g_openuri_portal_open_async (file, TRUE, parent, cancellable, open_done, task);
    }
  else
#endif
    {
      char *uri = g_file_get_uri (file);

      show_folder (parent, uri, cancellable, show_folder_done, task);

      g_free (uri);
    }
}

/**
 * gtk_file_launcher_open_containing_folder_finish:
 * @self: a `GtkFileLauncher`
 * @result: a `GAsyncResult`
 * @error: return location for a [enum@Gtk.DialogError] or [enum@Gio.Error] error
 *
 * Finishes the [method@Gtk.FileLauncher.open_containing_folder]
 * call and returns the result.
 *
 * Returns: `TRUE` if an application was launched,
 *     or `FALSE` and @error is set
 *
 * Since: 4.10
 */
gboolean
gtk_file_launcher_open_containing_folder_finish (GtkFileLauncher  *self,
                                                 GAsyncResult     *result,
                                                 GError          **error)
{
  g_return_val_if_fail (GTK_IS_FILE_LAUNCHER (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_file_launcher_open_containing_folder, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/* }}} */
/* vim:set foldmethod=marker expandtab: */
