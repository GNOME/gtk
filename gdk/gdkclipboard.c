/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2014 Red Hat, Inc.
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
#include <gdk/gdkclipboardprivate.h>


typedef struct _GdkClipboardPrivate GdkClipboardPrivate;

struct _GdkClipboardPrivate
{
  GdkClipboardContent content;
  gchar **content_types;
};

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GdkClipboard, gdk_clipboard, G_TYPE_OBJECT)

static void
gdk_clipboard_init (GdkClipboard *clipboard)
{
}

static void
gdk_clipboard_finalize (GObject *object)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (object);
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  g_strfreev (priv->content_types);

  G_OBJECT_CLASS (gdk_clipboard_parent_class)->finalize (object);

}

static void
gdk_clipboard_class_init (GdkClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_clipboard_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkClipboardClass, changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

void
_gdk_clipboard_set_available_content (GdkClipboard        *clipboard,
                                      GdkClipboardContent  content,
                                      const gchar         **content_types)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  priv->content = content;
  g_strfreev (priv->content_types);
  priv->content_types = g_strdupv ((gchar **)content_types);
  /* FIXME: should we automatically set text / image content types ? */

  g_signal_emit (clipboard, signals[CHANGED], 0);
}

GdkClipboardContent
_gdk_clipboard_get_available_content (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);
  
  return priv->content;
}

const gchar **
gdk_clipboard_get_content_types (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv;

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);
  
  priv = gdk_clipboard_get_instance_private (clipboard);

  return (const gchar **)priv->content_types;
}

static gboolean
strv_contains (const gchar **strv, const gchar *s)
{
  gint i;

  for (i = 0; strv[i]; i++)
    {
      if (g_strcmp0 (strv[i], s) == 0)
        return TRUE;
    }

  return FALSE;
}

gboolean
gdk_clipboard_data_available (GdkClipboard *clipboard,
                              const gchar  *content_type)
{
  GdkClipboardPrivate *priv;

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);
  
  priv = gdk_clipboard_get_instance_private (clipboard);

  return priv->content_types != NULL &&
         strv_contains ((const gchar **)priv->content_types, content_type);
}

void
gdk_clipboard_get_text_async (GdkClipboard        *clipboard,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->get_text_async (clipboard, cancellable, callback, user_data);
}

gchar *
gdk_clipboard_get_text_finish (GdkClipboard  *clipboard,
                               GAsyncResult  *res,
                               GError       **error)
{
  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return GDK_CLIPBOARD_GET_CLASS (clipboard)->get_text_finish (clipboard, res, error);
}

void
gdk_clipboard_set_text (GdkClipboard *clipboard,
                        const gchar  *text)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->set_text (clipboard, text);
}

gboolean
gdk_clipboard_text_available (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv;

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

  priv = gdk_clipboard_get_instance_private (clipboard);

  return ((priv->content & TEXT_CONTENT) != 0);
}

void
gdk_clipboard_get_image_async (GdkClipboard        *clipboard,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->get_image_async (clipboard, cancellable, callback, user_data);
}

GdkPixbuf *
gdk_clipboard_get_image_finish (GdkClipboard  *clipboard,
                                GAsyncResult  *res,
                                GError       **error)
{
  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return GDK_CLIPBOARD_GET_CLASS (clipboard)->get_image_finish (clipboard, res, error);
}

void
gdk_clipboard_set_image (GdkClipboard *clipboard,
                         GdkPixbuf    *pixbuf)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->set_image (clipboard, pixbuf);
}

gboolean
gdk_clipboard_image_available (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv;

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

  priv = gdk_clipboard_get_instance_private (clipboard);

  return (priv->content & IMAGE_CONTENT) != 0;
}

void
gdk_clipboard_get_data_async (GdkClipboard        *clipboard,
                              const gchar         *content_type,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->get_data_async (clipboard, content_type, cancellable, callback, user_data);
}

GInputStream *
gdk_clipboard_get_data_finish (GdkClipboard  *clipboard,
                               GAsyncResult  *res,
                               GError       **error)
{
  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return GDK_CLIPBOARD_GET_CLASS (clipboard)->get_data_finish (clipboard, res, error);
}

void
gdk_clipboard_set_data (GdkClipboard          *clipboard,
                        const gchar          **content_types,
                        GdkClipboardProvider   callback,
                        gpointer               user_data,
                        GDestroyNotify         destroy)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->set_data (clipboard, content_types, callback, user_data, destroy);
}

void
gdk_clipboard_clear (GdkClipboard *clipboard)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->clear (clipboard);
}

typedef struct {
  GAsyncReadyCallback callback;
  gpointer            user_data;
} GetBytesData;

static void
get_bytes (GObject      *object,
           GAsyncResult *res,
           gpointer      user_data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (object);
  GetBytesData *data = user_data;
  GError *error;
  GBytes *bytes;
  GInputStream *istream;
  GOutputStream *ostream;

  error = NULL; 
  bytes = NULL;

  istream = gdk_clipboard_get_data_finish (clipboard, res, &error);
  if (istream)
    {
      ostream = g_memory_output_stream_new_resizable ();
      g_output_stream_splice (ostream, istream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, NULL, NULL);
      bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (ostream));
      g_object_unref (ostream);
      g_object_unref (istream);
    }

  g_object_set_data (G_OBJECT (res), "error", error);
  g_object_set_data (G_OBJECT (res), "bytes", bytes);

  data->callback (object, res, data->user_data);

  g_free (data);
}

void
gdk_clipboard_get_bytes_async (GdkClipboard        *clipboard,
                               const gchar         *content_type,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GetBytesData *data;

  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  data = g_new (GetBytesData, 1);
  data->callback = callback;
  data->user_data = user_data;

  gdk_clipboard_get_data_async (clipboard, content_type, cancellable, get_bytes, data);
}

GBytes *
gdk_clipboard_get_bytes_finish (GdkClipboard  *clipboard,
                                GAsyncResult  *res,
                                GError       **error)
{
  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);
  GError *inner;
  GBytes *bytes;

  inner = g_object_get_data (G_OBJECT (res), "error");
  bytes = g_object_get_data (G_OBJECT (res), "bytes");

  if (inner)
    g_propagate_error (error, inner);

  return bytes;
}

static void
set_bytes (GdkClipboard  *clipboard,
           const gchar   *content_type,
           GOutputStream *stream,
           gpointer       user_data)
{
  GBytes *bytes = user_data;
  GBytes *rest;
  gssize written;

  g_bytes_ref (bytes);

  while (TRUE)
    {
      written = g_output_stream_write_bytes (stream, bytes, NULL, NULL);
      if (written == g_bytes_get_size (bytes) || written == -1)
        break;
      rest = g_bytes_new_from_bytes (bytes, written, g_bytes_get_size (bytes) - written);
      g_bytes_unref (bytes);
      bytes = rest;
    }

  g_bytes_unref (bytes);
}

void
gdk_clipboard_set_bytes (GdkClipboard *clipboard,
                         GBytes       *bytes,
                         const gchar  *content_type)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  const gchar *ctypes[2];

  ctypes[0] = content_type;
  ctypes[1] = NULL;

  /* create stream from bytes */
  gdk_clipboard_set_data (clipboard, ctypes, set_bytes, g_bytes_ref (bytes), (GDestroyNotify) g_bytes_unref);
}

