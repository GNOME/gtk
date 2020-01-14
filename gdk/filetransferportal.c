/*
 * Copyright (C) 2018 Matthias Clasen
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

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gio/gio.h>

#ifdef G_OS_UNIX

#include <gio/gunixfdlist.h>

#ifndef O_PATH
#define O_PATH 0
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#else
#define HAVE_O_CLOEXEC 1
#endif

#include "filetransferportalprivate.h"

static GDBusProxy *file_transfer_proxy = NULL;
static gboolean done;
static guint timeout_id;

static void
got_proxy (GObject *source,
           GAsyncResult *result,
           gpointer data)
{
  GError *error = NULL;

  file_transfer_proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
  if (!file_transfer_proxy)
    {
      g_message ("failed to get file transfer portal proxy: %s", error->message);
      g_error_free (error);
    }

  done = TRUE;
}

static gboolean
give_up_on_proxy (gpointer data)
{
  GCancellable *cancellable = data;

  g_cancellable_cancel (cancellable);

  timeout_id = 0;
  done = TRUE;

  g_main_context_wakeup (NULL);

  return G_SOURCE_REMOVE;
}

static GDBusProxy *
ensure_file_transfer_portal (void)
{
  if (file_transfer_proxy == NULL)
    {
      GError *error = NULL;
      GCancellable *cancellable;

      cancellable = g_cancellable_new ();

      g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS
                                | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                NULL,
                                "org.freedesktop.portal.Documents",
                                "/org/freedesktop/portal/documents",
                                "org.freedesktop.portal.FileTransfer",
                                cancellable,
                                got_proxy,
                                NULL);

      timeout_id = g_timeout_add_full (1000, 0, give_up_on_proxy, cancellable, g_object_unref);

      while (!done)
        g_main_context_iteration (NULL, TRUE);

      if (timeout_id)
        g_source_remove (timeout_id);

      if (error)
        {
          g_debug ("Failed to get proxy: %s", error->message);
          g_error_free (error);
        }
    }

  if (file_transfer_proxy)
    {
      char *owner = g_dbus_proxy_get_name_owner (file_transfer_proxy);

      if (owner)
        {
          g_free (owner);
          return file_transfer_proxy;
        }
    }

  return NULL;
}

gboolean
file_transfer_portal_available (void)
{
  gboolean available;

  ensure_file_transfer_portal ();

  available = file_transfer_proxy != NULL;

  g_clear_object (&file_transfer_proxy);

  return available;
}

typedef struct {
  GTask *task;
  const char **files;
  int len;
  int start;
} AddFileData;

static void add_files (GDBusProxy  *proxy,
                       AddFileData *afd);

static void
add_files_done (GObject      *object,
                GAsyncResult *result,
                gpointer      data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);
  AddFileData *afd = data;
  GError *error = NULL;
  GVariant *ret;

  ret = g_dbus_proxy_call_with_unix_fd_list_finish (proxy, NULL, result, &error);
  if (ret == NULL)
    {
      g_task_return_error (afd->task, error);
      g_object_unref (afd->task);
      g_free (afd);
      return;
    }

  g_variant_unref (ret);

  if (afd->start >= afd->len)
    {
      g_task_return_boolean (afd->task, TRUE);
      g_object_unref (afd->task);
      g_free (afd);
      return;
    }

  add_files (proxy, afd);
}

/* We call AddFiles in chunks of 16 to avoid running into
 * the per-message fd limit of the bus.
 */
static void
add_files (GDBusProxy  *proxy,
           AddFileData *afd)
{
  GUnixFDList *fd_list;
  GVariantBuilder fds;
  int i;
  char *key;

  g_variant_builder_init (&fds, G_VARIANT_TYPE ("ah"));
  fd_list = g_unix_fd_list_new ();

  for (i = 0; afd->files[afd->start + i]; i++)
    {
      int fd;
      int fd_in;
      GError *error = NULL;

      if (i == 16)
        break;

      fd = open (afd->files[afd->start + i], O_PATH | O_CLOEXEC);
      if (fd == -1)
        {
          g_task_return_new_error (afd->task, G_IO_ERROR, g_io_error_from_errno (errno),
                                   "Failed to open %s", afd->files[afd->start + i]);
          g_object_unref (afd->task);
          g_free (afd);
          g_object_unref (fd_list);
          return;
         }

#ifndef HAVE_O_CLOEXEC
      fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif
      fd_in = g_unix_fd_list_append (fd_list, fd, &error);
      close (fd);

      if (fd_in == -1)
        {
          g_task_return_error (afd->task, error);
          g_object_unref (afd->task);
          g_free (afd);
          g_object_unref (fd_list);
          return;
        }

      g_variant_builder_add (&fds, "h", fd_in);
    }

   afd->start += 16;

  key = (char *)g_object_get_data (G_OBJECT (afd->task), "key");

  g_dbus_proxy_call_with_unix_fd_list (proxy,
                                       "AddFiles",
                                       g_variant_new ("(sah)", key, &fds),
                                       0, -1,
                                       fd_list,
                                       NULL,
                                       add_files_done, afd);

  g_object_unref (fd_list);
}

static void
start_session_done (GObject      *object,
                    GAsyncResult *result,
                    gpointer      data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);
  AddFileData *afd = data;
  GError *error = NULL;
  GVariant *ret;
  const char *key;

  ret = g_dbus_proxy_call_finish (proxy, result, &error);
  if (ret == NULL)
    {
      g_task_return_error (afd->task, error);
      g_object_unref (afd->task);
      g_free (afd);
      return;
    }

  g_variant_get (ret, "(&s)", &key);

  g_object_set_data_full (G_OBJECT (afd->task), "key", g_strdup (key), g_free);

  g_variant_unref (ret);

  add_files (proxy, afd);
}

void
file_transfer_portal_register_files (const char          **files,
                                     gboolean              writable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              data)
{
  GTask *task;
  GDBusProxy *proxy;
  AddFileData *afd;
  GVariantBuilder options;

  task = g_task_new (NULL, NULL, callback, data);

  proxy = ensure_file_transfer_portal ();

  if (proxy == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               "No portal found");
      g_object_unref (task);
      return;
    }

  afd = g_new (AddFileData, 1);
  afd->task = task;
  afd->files = files;
  afd->len = g_strv_length ((char **)files);
  afd->start = 0;

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "writable", g_variant_new_boolean (writable));
  g_variant_builder_add (&options, "{sv}", "autostop", g_variant_new_boolean (TRUE));

  g_dbus_proxy_call (proxy, "StartTransfer",
                     g_variant_new ("(a{sv})", &options),
                     0, -1, NULL, start_session_done, afd);
}

gboolean
file_transfer_portal_register_files_finish (GAsyncResult  *result,
                                            char         **key,
                                            GError       **error)
{
  if (g_task_propagate_boolean (G_TASK (result), error))
    {
      *key = g_strdup (g_object_get_data (G_OBJECT (result), "key"));
      return TRUE;
    }

  return FALSE;
}

static void
retrieve_files_done (GObject      *object,
                     GAsyncResult *result,
                     gpointer      data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);
  GTask *task = data;
  GError *error = NULL;
  GVariant *ret;
  char **files;

  ret = g_dbus_proxy_call_finish (proxy, result, &error);
  if (ret == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_variant_get (ret, "(^a&s)", &files);

  g_object_set_data_full (G_OBJECT (task), "files", g_strdupv (files), (GDestroyNotify)g_strfreev);

  g_variant_unref (ret);

  g_task_return_boolean (task, TRUE);
}

void
file_transfer_portal_retrieve_files (const char          *key,
                                     GAsyncReadyCallback  callback,
                                     gpointer             data)
{
  GDBusProxy *proxy;
  GTask *task;
  GVariantBuilder options;

  task = g_task_new (NULL, NULL, callback, data);

  proxy = ensure_file_transfer_portal ();

  if (proxy == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               "No portal found");
      g_object_unref (task);
      return;
    }

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call (proxy,
                     "RetrieveFiles",
                     g_variant_new ("(sa{sv})", key, &options),
                     0, -1, NULL,
                     retrieve_files_done, task);
}

gboolean
file_transfer_portal_retrieve_files_finish (GAsyncResult   *result,
                                            char         ***files,
                                            GError        **error)
{
  if (g_task_propagate_boolean (G_TASK (result), error))
    {
      *files = g_strdupv (g_object_get_data (G_OBJECT (result), "files"));
      return TRUE;
    }

  return FALSE;
}

#endif /* G_OS_UNIX */
