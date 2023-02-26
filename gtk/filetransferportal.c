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

typedef struct {
  GTask *task;
  char **files;
  int len;
  int start;
} AddFileData;

static void
free_add_file_data (gpointer data)
{
  AddFileData *afd = data;

  g_object_unref (afd->task);
  g_free (afd->files);
  g_free (afd);
}

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
      free_add_file_data (afd);
      return;
    }

  g_variant_unref (ret);

  if (afd->start >= afd->len)
    {
      g_task_return_boolean (afd->task, TRUE);
      free_add_file_data (afd);
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
  GVariantBuilder fds, options;
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
          free_add_file_data (afd);
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
          free_add_file_data (afd);
          g_object_unref (fd_list);
          return;
        }

      g_variant_builder_add (&fds, "h", fd_in);
    }

   afd->start += 16;

  key = (char *)g_object_get_data (G_OBJECT (afd->task), "key");

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call_with_unix_fd_list (proxy,
                                       "AddFiles",
                                       g_variant_new ("(saha{sv})", key, &fds, &options),
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
      free_add_file_data (afd);
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
  AddFileData *afd;
  GVariantBuilder options;

  task = g_task_new (NULL, NULL, callback, data);

  if (file_transfer_proxy == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               "No portal found");
      g_object_unref (task);
      return;
    }

  afd = g_new (AddFileData, 1);
  afd->task = task;
  afd->files = g_strdupv ((char **)files);
  afd->len = g_strv_length (afd->files);
  afd->start = 0;

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "writable", g_variant_new_boolean (writable));
  g_variant_builder_add (&options, "{sv}", "autostop", g_variant_new_boolean (TRUE));

  g_dbus_proxy_call (file_transfer_proxy, "StartTransfer",
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

char *
file_transfer_portal_register_files_sync (const char **files,
                                          gboolean     writable,
                                          GError     **error)
{
  const char *value;
  char *key;
  GUnixFDList *fd_list;
  GVariantBuilder fds, options;
  int i;
  GVariant *ret;

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  ret = g_dbus_proxy_call_sync (file_transfer_proxy,
                                "StartTransfer",
                                g_variant_new ("(a{sv})", &options),
                                0,
                                -1,
                                NULL,
                                error);
  if (ret == NULL)
    return NULL;

  g_variant_get (ret, "(&s)", &value);
  key = g_strdup (value);
  g_variant_unref (ret);

  fd_list = NULL;

  for (i = 0; files[i]; i++)
    {
      int fd;
      int fd_in;

      if (fd_list == NULL)
        {
          g_variant_builder_init (&fds, G_VARIANT_TYPE ("ah"));
          fd_list = g_unix_fd_list_new ();
        }

      fd = open (files[i], O_PATH | O_CLOEXEC);
      if (fd == -1)
        {
          g_set_error (error,
                       G_IO_ERROR, g_io_error_from_errno (errno),
                       "Failed to open %s", files[i]);
          g_variant_builder_clear (&fds);
          g_object_unref (fd_list);
          g_free (key);
          return NULL;
         }

#ifndef HAVE_O_CLOEXEC
      fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif
      fd_in = g_unix_fd_list_append (fd_list, fd, error);
      close (fd);

      if (fd_in == -1)
        {
          g_variant_builder_clear (&fds);
          g_object_unref (fd_list);
          g_free (key);
          return NULL;
        }

      g_variant_builder_add (&fds, "h", fd_in);

      if ((i + 1) % 16 == 0 || files[i + 1] == NULL)
        {
          g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
          ret = g_dbus_proxy_call_with_unix_fd_list_sync (file_transfer_proxy,
                                                          "AddFiles",
                                                          g_variant_new ("(saha{sv})",
                                                                         key,
                                                                         &fds,
                                                                         &options),
                                                          0,
                                                          -1,
                                                          fd_list,
                                                          NULL,
                                                          NULL,
                                                          error);
          g_clear_object (&fd_list);

          if (ret == NULL)
            {
              g_free (key);
              return NULL;
            }

          g_variant_unref (ret);
        }
    }

  return key;
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
  GTask *task;
  GVariantBuilder options;

  task = g_task_new (NULL, NULL, callback, data);

  if (file_transfer_proxy == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               "No portal found");
      g_object_unref (task);
      return;
    }

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call (file_transfer_proxy,
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

char **
file_transfer_portal_retrieve_files_sync (const char  *key,
                                          GError     **error)
{
  GVariantBuilder options;
  GVariant *ret;
  char **files = NULL;

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  ret = g_dbus_proxy_call_sync (file_transfer_proxy,
                                "RetrieveFiles",
                                g_variant_new ("(sa{sv})", key, &options),
                                0, -1, NULL,
                                error);
  if (ret)
    {
      const char **value;
      g_variant_get (ret, "(^a&s)", &value);
      files = g_strdupv ((char **)value);
      g_variant_unref (ret);
    }

  return files;
}

static void
connection_closed (GDBusConnection *connection,
                   gboolean         remote_peer_vanished,
                   GError          *error)
{
  g_clear_object (&file_transfer_proxy);
}

static void
finish_registration (void)
{
  /* Free the singleton when the connection closes, important for test */
  g_signal_connect (g_dbus_proxy_get_connection (G_DBUS_PROXY (file_transfer_proxy)),
                    "closed", G_CALLBACK (connection_closed), NULL);
}

static gboolean
proxy_has_owner (GDBusProxy *proxy)
{
  char *owner;

  owner = g_dbus_proxy_get_name_owner (proxy);
  if (owner)
    {
      g_free (owner);
      return TRUE;
    }

  return FALSE;
}

void
file_transfer_portal_register (void)
{
  static gboolean called;

  if (!called)
    {
      called = TRUE;

      file_transfer_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS
                                | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                NULL,
                                "org.freedesktop.portal.Documents",
                                "/org/freedesktop/portal/documents",
                                "org.freedesktop.portal.FileTransfer",
                                NULL,
                                NULL);

      if (file_transfer_proxy && !proxy_has_owner (file_transfer_proxy))
        g_clear_object (&file_transfer_proxy);

      if (file_transfer_proxy)
        finish_registration ();
    }
}

gboolean
file_transfer_portal_supported (void)
{
  file_transfer_portal_register ();

  return file_transfer_proxy != NULL;
}

#endif /* G_OS_UNIX */
