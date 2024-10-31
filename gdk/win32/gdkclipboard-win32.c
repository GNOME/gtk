/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2017 Red Hat, Inc.
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

#include "gdkclipboardprivate.h"
#include "gdkclipboard-win32.h"
#include "gdkdisplay-win32.h"

#include "gdkdebugprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate-win32.h"
#include "gdkhdataoutputstream-win32.h"
#include "gdk/gdkprivate.h"

#include <string.h>

typedef struct _GdkWin32ClipboardClass GdkWin32ClipboardClass;

typedef struct _RetrievalInfo RetrievalInfo;

struct _GdkWin32Clipboard
{
  GdkClipboard parent;

  /* Taken from the OS, the OS increments it every time
   * clipboard data changes.
   * -1 means that clipboard data that we claim to
   * have access to is actually just an empty set that
   * we made up. Thus any real data from the OS will
   * override anything we make up.
   */
  gint64      sequence_number;
};

struct _GdkWin32ClipboardClass
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkWin32Clipboard, gdk_win32_clipboard, GDK_TYPE_CLIPBOARD)

static GdkContentFormats *
gdk_win32_clipboard_request_contentformats (GdkWin32Clipboard *cb)
{
  BOOL success;
  UINT *w32_formats = NULL;
  UINT w32_formats_len = 0;
  UINT w32_formats_allocated;
  gsize i;
  GdkContentFormatsBuilder *builder;
  GdkContentFormats *formats;
  GdkWin32Clipdrop *clipdrop = gdk_win32_clipboard_get_clipdrop (GDK_CLIPBOARD (cb));
  DWORD error_code;

  SetLastError (0);
  success = GetUpdatedClipboardFormats (NULL, 0, &w32_formats_allocated);
  error_code = GetLastError ();

  if (!success && error_code != ERROR_INSUFFICIENT_BUFFER)
    {
      g_warning ("Initial call to GetUpdatedClipboardFormats() failed with error %lu", error_code);
      return NULL;
    }

  w32_formats = g_new0 (UINT, w32_formats_allocated);

  SetLastError (0);
  success = GetUpdatedClipboardFormats (w32_formats, w32_formats_allocated, &w32_formats_len);
  error_code = GetLastError ();

  if (!success)
    {
      g_warning ("Second call to GetUpdatedClipboardFormats() failed with error %lu", error_code);
      g_free (w32_formats);
      return NULL;
    }

  builder = gdk_content_formats_builder_new ();

  for (i = 0; i < MIN (w32_formats_len, w32_formats_allocated); i++)
    gdk_win32_clipdrop_add_win32_format_to_pairs (clipdrop, w32_formats[i], NULL, builder);

  g_free (w32_formats);
  formats = gdk_content_formats_builder_free_to_formats (builder);

  GDK_NOTE (DND, {
      char *s = gdk_content_formats_to_string (formats);
      g_print ("... %s\n", s);
      g_free (s);
    });

  return formats;
}

void
gdk_win32_clipboard_claim_remote (GdkWin32Clipboard *cb)
{
  GdkContentFormats *formats;

  /* Claim empty first, in case the format request fails */
  formats = gdk_content_formats_new (NULL, 0);
  gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb), formats);
  gdk_content_formats_unref (formats);
  cb->sequence_number = -1;

  formats = gdk_win32_clipboard_request_contentformats (cb);

  if (formats != NULL)
    {
      gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb), formats);
      gdk_content_formats_unref (formats);
      cb->sequence_number = GetClipboardSequenceNumber ();
    }
}

static void
gdk_win32_clipboard_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_win32_clipboard_parent_class)->finalize (object);
}

static gboolean
gdk_win32_clipboard_claim (GdkClipboard       *clipboard,
                           GdkContentFormats  *formats,
                           gboolean            local,
                           GdkContentProvider *content)
{
  if (local)
    _gdk_win32_advertise_clipboard_contentformats (clipboard, NULL, content ? formats : NULL);

  return GDK_CLIPBOARD_CLASS (gdk_win32_clipboard_parent_class)->claim (clipboard, formats, local, content);
}

static void
gdk_win32_clipboard_store_async (GdkClipboard        *clipboard,
                                 int                  io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GdkContentProvider *content;
  GdkContentFormats *formats;
  GTask *store_task;

  store_task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (store_task, io_priority);
  g_task_set_source_tag (store_task, gdk_win32_clipboard_store_async);

  content = gdk_clipboard_get_content (clipboard);

  if (content == NULL)
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("storing empty clipboard: SUCCESS!\n"));
      g_task_return_boolean (store_task, TRUE);
      g_clear_object (&store_task);
      return;
    }

  formats = gdk_content_provider_ref_storable_formats (content);
  formats = gdk_content_formats_union_serialize_mime_types (formats);

  if (!_gdk_win32_store_clipboard_contentformats (clipboard, store_task, formats))
    {
      GDK_NOTE (CLIPBOARD, g_printerr ("clipdrop says there's nothing to store: SUCCESS!\n"));
      g_task_return_boolean (store_task, TRUE);
      g_clear_object (&store_task);

      return;
    }
}

static gboolean
gdk_win32_clipboard_store_finish (GdkClipboard  *clipboard,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, clipboard), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_win32_clipboard_store_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gdk_win32_clipboard_read_async (GdkClipboard        *clipboard,
                                GdkContentFormats   *contentformats,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GTask *task;

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_win32_clipboard_read_async);

  _gdk_win32_retrieve_clipboard_contentformats (clipboard, task, contentformats);

  return;
}

static GInputStream *
gdk_win32_clipboard_read_finish (GdkClipboard  *clipboard,
                                 GAsyncResult  *result,
                                 const char   **out_mime_type,
                                 GError       **error)
{
  GTask *task;
  GInputStream *stream;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (clipboard)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_win32_clipboard_read_async, NULL);

  stream = g_task_propagate_pointer (task, error);

  if (stream == NULL)
    return stream;

  if (out_mime_type)
    *out_mime_type = g_object_get_data (G_OBJECT (stream), "gdk-clipboard-stream-contenttype");

  return stream;
}

static void
gdk_win32_clipboard_class_init (GdkWin32ClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_win32_clipboard_finalize;

  clipboard_class->claim = gdk_win32_clipboard_claim;
  clipboard_class->store_async = gdk_win32_clipboard_store_async;
  clipboard_class->store_finish = gdk_win32_clipboard_store_finish;
  clipboard_class->read_async = gdk_win32_clipboard_read_async;
  clipboard_class->read_finish = gdk_win32_clipboard_read_finish;
}

static void
gdk_win32_clipboard_init (GdkWin32Clipboard *cb)
{
  cb->sequence_number = -1;
}

GdkClipboard *
gdk_win32_clipboard_new (GdkDisplay  *display)
{
  GdkWin32Clipboard *cb;

  cb = g_object_new (GDK_TYPE_WIN32_CLIPBOARD,
                     "display", display,
                     NULL);

  gdk_win32_clipboard_claim_remote (cb);

  return GDK_CLIPBOARD (cb);
}

GdkWin32Clipdrop *
gdk_win32_clipboard_get_clipdrop (GdkClipboard *cb)
{
  return gdk_win32_display_get_clipdrop (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)));
}