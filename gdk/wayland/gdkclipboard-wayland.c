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
#include "gdkclipboard-wayland.h"

#include "gdkcontentformats.h"
#include "gdkintl.h"
#include "gdk-private.h"

#include <glib-unix.h>
#include <gio/gunixinputstream.h>

typedef struct _GdkWaylandClipboardClass GdkWaylandClipboardClass;

struct _GdkWaylandClipboard
{
  GdkClipboard parent;

  struct wl_data_offer *offer;
  GdkContentFormats *offer_formats;
};

struct _GdkWaylandClipboardClass
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandClipboard, gdk_wayland_clipboard, GDK_TYPE_CLIPBOARD)

static void
gdk_wayland_clipboard_discard_offer (GdkWaylandClipboard *cb)
{
  g_clear_pointer (&cb->offer_formats, gdk_content_formats_unref);
  g_clear_pointer (&cb->offer, (GDestroyNotify) wl_data_offer_destroy);
}

static void
gdk_wayland_clipboard_finalize (GObject *object)
{
  GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (object);

  gdk_wayland_clipboard_discard_offer (cb);
  
  G_OBJECT_CLASS (gdk_wayland_clipboard_parent_class)->finalize (object);
}

static gboolean
gdk_wayland_clipboard_claim (GdkClipboard       *clipboard,
                             GdkContentFormats  *formats,
                             gboolean            local,
                             GdkContentProvider *content)
{
  GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (clipboard);

  if (local)
    {
      /* not handled yet */
      cb->offer = NULL;
    }

  return GDK_CLIPBOARD_CLASS (gdk_wayland_clipboard_parent_class)->claim (clipboard, formats, local, content);
}

static void
gdk_wayland_clipboard_read_async (GdkClipboard        *clipboard,
                                  GdkContentFormats   *formats,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (clipboard);
  GInputStream *stream;
  const char *mime_type;
  int pipe_fd[2];
  GError *error = NULL;
  GTask *task;

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_wayland_clipboard_read_async);

  GDK_NOTE (CLIPBOARD, char *s = gdk_content_formats_to_string (formats);
                       g_printerr ("%p: read for %s\n", cb, s);
                       g_free (s); );
  mime_type = gdk_content_formats_match_mime_type (formats, cb->offer_formats);
  if (mime_type == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      return;
    }
  /* offer formats should be empty if we have no offer */
  g_assert (cb->offer);

  g_task_set_task_data (task, (gpointer) mime_type, NULL);

  if (!g_unix_open_pipe (pipe_fd, FD_CLOEXEC, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  wl_data_offer_receive (cb->offer, mime_type, pipe_fd[1]);
  stream = g_unix_input_stream_new (pipe_fd[0], TRUE);
  close (pipe_fd[1]);
  g_task_return_pointer (task, stream, g_object_unref);
}

static GInputStream *
gdk_wayland_clipboard_read_finish (GdkClipboard  *clipboard,
                                   const char   **out_mime_type,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  GInputStream *stream;
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (clipboard)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_wayland_clipboard_read_async, NULL);

  stream = g_task_propagate_pointer (task, error);

  if (stream)
    {
      if (out_mime_type)
        *out_mime_type = g_task_get_task_data (task);
      g_object_ref (stream);
    }
  else
    {
      if (out_mime_type)
        *out_mime_type = NULL;
    }

  return stream;
}

static void
gdk_wayland_clipboard_class_init (GdkWaylandClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_wayland_clipboard_finalize;

  clipboard_class->claim = gdk_wayland_clipboard_claim;
#if 0
  clipboard_class->store_async = gdk_wayland_clipboard_store_async;
  clipboard_class->store_finish = gdk_wayland_clipboard_store_finish;
#endif
  clipboard_class->read_async = gdk_wayland_clipboard_read_async;
  clipboard_class->read_finish = gdk_wayland_clipboard_read_finish;
}

static void
gdk_wayland_clipboard_init (GdkWaylandClipboard *cb)
{
}

GdkClipboard *
gdk_wayland_clipboard_new (GdkDisplay *display)
{
  GdkWaylandClipboard *cb;

  cb = g_object_new (GDK_TYPE_WAYLAND_CLIPBOARD,
                     "display", display,
                     NULL);

  return GDK_CLIPBOARD (cb);
}

void
gdk_wayland_clipboard_claim_remote (GdkWaylandClipboard  *cb,
                                    struct wl_data_offer *offer,
                                    GdkContentFormats    *formats)
{
  g_return_if_fail (GDK_IS_WAYLAND_CLIPBOARD (cb));

  gdk_wayland_clipboard_discard_offer (cb);

  GDK_NOTE (CLIPBOARD, char *s = gdk_content_formats_to_string (formats);
                       g_printerr ("%p: remote clipboard claim for %s\n", cb, s);
                       g_free (s); );
  cb->offer_formats = formats;
  cb->offer = offer;

  gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb),
                              cb->offer_formats);
}

