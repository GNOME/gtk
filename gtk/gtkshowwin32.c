/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2024-2025 Sergey Bugaev
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

#include "gtkshowwin32.h"
#include <gtk/gtknative.h>
#include <gdk/win32/gdkwin32misc.h>

#include <shellapi.h>
#include <shlobj.h>

typedef struct {
  gunichar2 *uri_or_path;
  gboolean always_ask;
} ShowData;

static void
show_data_free (ShowData *data)
{
  g_free (data->uri_or_path);
  g_slice_free (ShowData, data);
}

static void
show_uri_win32_in_thread (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  ShowData *data = task_data;
  GdkSurface *parent_surface;
  HWND parent_hwnd;
  HMONITOR monitor;
  HRESULT hr;

  if (source_object)
    {
      parent_surface = gtk_native_get_surface (GTK_NATIVE (source_object));
      parent_hwnd = GDK_SURFACE_HWND (parent_surface);
      monitor = MonitorFromWindow (parent_hwnd, MONITOR_DEFAULTTONULL);
    }
  else
    {
      parent_hwnd = 0;
      monitor = (HMONITOR) NULL;
    }

  if (!data->always_ask)
    {
      BOOL res;
      SHELLEXECUTEINFOW shex_info;

      /* Attempt to initialize COM, in the off chance that there
       * are ShellExecute hooks. */
      hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

      memset (&shex_info, 0, sizeof (shex_info));

      shex_info.cbSize = sizeof (shex_info);
      shex_info.fMask = SEE_MASK_NOASYNC | SEE_MASK_HMONITOR;
      shex_info.hwnd = parent_hwnd;
      shex_info.lpVerb = NULL;

      shex_info.lpFile = data->uri_or_path;
      shex_info.lpParameters = NULL;
      shex_info.lpDirectory = NULL;
      shex_info.nShow = SW_SHOWNORMAL;
      shex_info.hMonitor = monitor;

      /* Us passing the monitor derived from the parent window shouldn't
       * break any custom window positioning logic in the app being
       * launched, since the passed monitor is only used as a fallback
       * for apps that use CW_USEDEFAULT. */

      res = ShellExecuteExW (&shex_info);

      /* Un-initialize COM if we have successfully initialized it earlier. */
      if (SUCCEEDED (hr))
        CoUninitialize ();

      if (res)
        g_task_return_boolean (task, TRUE);
      else
        {
          int errsv = GetLastError ();
          gchar *emsg = g_win32_error_message (errsv);

          g_task_return_new_error (task, G_IO_ERROR,
                                   g_io_error_from_win32_error (errsv),
                                   "%s", emsg);
          g_free (emsg);
        }
    }
  else
    {
      OPENASINFO openas_info;

      memset (&openas_info, 0, sizeof (openas_info));

      openas_info.pcszFile = data->uri_or_path;
      openas_info.pcszClass = NULL;
      openas_info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_REGISTER_EXT | OAIF_EXEC;

      hr = SHOpenWithDialog (parent_hwnd, &openas_info);

      if (SUCCEEDED (hr))
        g_task_return_boolean (task, TRUE);
      else
        g_task_return_new_error (task, G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to display Open With dialog: 0x%lx", hr);
    }
}

void
gtk_show_uri_win32 (GtkWindow          *parent,
                    const char         *uri_or_path,
                    gboolean            always_ask,
                    GCancellable       *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer            user_data)
{
  ShowData *show_data;
  GTask *task;
  GError *error = NULL;
  char *owned_path = NULL;

  g_return_if_fail (uri_or_path != NULL);

  task = g_task_new (parent, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_show_uri_win32);

  /* ShellExecute doesn't quite like file:// URLs, so convert
   * those to file paths now. */
  if (g_str_has_prefix (uri_or_path, "file://"))
    {
      GFile *file = g_file_new_for_uri (uri_or_path);
      uri_or_path = owned_path = g_file_get_path (file);
      g_object_unref (file);
    }

  show_data = g_slice_new (ShowData);
  show_data->always_ask = always_ask;
  /* Note: uri_or_path is UTF-8 encoded here. */
  show_data->uri_or_path = g_utf8_to_utf16 (uri_or_path, -1, NULL, NULL, &error);
  g_clear_pointer(&owned_path, g_free);
  if (G_UNLIKELY (error))
    {
      g_task_return_error (task, error);
      g_slice_free (ShowData, show_data);
      g_object_unref (task);
      return;
    }

  g_task_set_task_data (task, show_data, (GDestroyNotify) show_data_free);
  g_task_run_in_thread (task, show_uri_win32_in_thread);
  g_object_unref (task);
}

gboolean
gtk_show_uri_win32_finish (GtkWindow    *parent,
                           GAsyncResult *result,
                           GError      **error)
{
  g_return_val_if_fail (g_task_is_valid (result, parent), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
open_containing_folder_win32_in_thread (GTask        *task,
                                        gpointer      source_object,
                                        gpointer      task_data,
                                        GCancellable *cancellable)
{
  HRESULT hr;
  PIDLIST_ABSOLUTE pidl;
  char *error_message;

  hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (!SUCCEEDED (hr))
    {
      error_message = g_win32_error_message (hr);
      g_task_return_new_error (task, G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to initialize COM: %s", error_message);
      g_free (error_message);
      return;
    }

  pidl = ILCreateFromPathW ((const gunichar2 *) task_data);
  /* Note: this API doesn't use an A/W version split. */
  hr = SHOpenFolderAndSelectItems (pidl, 0, NULL, 0);
  ILFree (pidl);

  if (SUCCEEDED (hr))
    g_task_return_boolean (task, TRUE);
  else
    {
      error_message = g_win32_error_message (hr);
      g_task_return_new_error (task, G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "SHOpenFolderAndSelectItems failed: %s", error_message);
      g_free (error_message);
    }

  CoUninitialize ();
}

void
gtk_open_containing_folder_win32 (const char         *path,
                                  GCancellable       *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer            user_data)
{
  GTask *task;
  gunichar2 *path_utf16;
  GError *error = NULL;

  g_return_if_fail (path != NULL);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_open_containing_folder_win32);

  /* Note: path is UTF-8 encoded here. */
  path_utf16 = g_utf8_to_utf16 (path, -1, NULL, NULL, &error);

  g_task_set_task_data (task, path_utf16, g_free);
  g_task_run_in_thread (task, open_containing_folder_win32_in_thread);
  g_object_unref (task);
}

gboolean
gtk_open_containing_folder_win32_finish (GAsyncResult *result,
                                         GError      **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
