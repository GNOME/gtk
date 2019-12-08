#include "config.h"

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "gfiletransferportal.h"

static GDBusProxy *
ensure_file_transfer_portal (void)
{
  static GDBusProxy *proxy = NULL;

  if (proxy == NULL)
    {
      GError *error = NULL;

      proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                             | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS
                                             | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                             NULL,
                                             "org.freedesktop.portal.Documents",
                                             "/org/freedesktop/portal/documents",
                                             "org.freedesktop.portal.FileTransfer",
                                             NULL, &error);

      if (error)
        {
          g_warning ("Failed to get proxy: %s", error->message);
          g_error_free (error);
        }
    }

  if (proxy)
    {
      char *owner = g_dbus_proxy_get_name_owner (proxy);

      if (owner)
        {
          g_free (owner);
          return proxy;
        }
    }

  return NULL;
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
  GVariantBuilder options;

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
                                            char         **out_key,
                                            GError       **error)
{
  char *key;
  GDBusProxy *proxy;

  key = g_object_get_data (G_OBJECT (result), "key");

  if (g_task_propagate_boolean (G_TASK (result), error))
    {
      *out_key = g_strdup (key);
      return TRUE;
    }

  proxy = ensure_file_transfer_portal ();

  g_dbus_proxy_call (proxy, "StopTransfer",
                     g_variant_new ("(s)", key),
                     0, -1, NULL, NULL, NULL);

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
