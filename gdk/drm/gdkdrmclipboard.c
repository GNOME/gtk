/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gdkprivate.h"

#include "gdkdrmclipboard-private.h"
#include "gdkdrmsurface-private.h"

struct _GdkDrmClipboard
{
  GdkClipboard parent_instance;
};

G_DEFINE_TYPE (GdkDrmClipboard, _gdk_drm_clipboard, GDK_TYPE_CLIPBOARD)

static void
_gdk_drm_clipboard_read_async (GdkClipboard        *clipboard,
                               GdkContentFormats   *formats,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_task_report_new_error (G_OBJECT (clipboard),
                           callback,
                           user_data,
                           _gdk_drm_clipboard_read_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Not supported");
}

static GInputStream *
_gdk_drm_clipboard_read_finish (GdkClipboard  *clipboard,
                                GAsyncResult  *result,
                                const char   **out_mime_type,
                                GError       **error)
{
  if (out_mime_type != NULL)
    *out_mime_type = NULL;

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
_gdk_drm_clipboard_class_init (GdkDrmClipboardClass *klass)
{
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (klass);

  clipboard_class->read_async = _gdk_drm_clipboard_read_async;
  clipboard_class->read_finish = _gdk_drm_clipboard_read_finish;
}

static void
_gdk_drm_clipboard_init (GdkDrmClipboard *self)
{
}

GdkClipboard *
_gdk_drm_clipboard_new (GdkDrmDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DRM_DISPLAY (display), NULL);

  return g_object_new (GDK_TYPE_DRM_CLIPBOARD,
                       "display", display,
                       NULL);
}
