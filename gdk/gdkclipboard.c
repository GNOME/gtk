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


/**
 * SECTION:gdkclipboard
 * @Short_description: A clipboard class
 * @Title: GdkClipboard
 *
 * The #GdkClipboard object represent a clipboard of data that
 * is shared between different processes or between different widgets
 * in the same process. GDK backends are responsible for implementing
 * the #GdkClipboard API using their platforms data exchange mechanisms.
 *
 * GDK supports two different clipboards, which are historically named
 * CLIPBOARD and PRIMARY. To obtain these clipboards, use
 * gdk_display_get_clipboard() and gdk_display_get_primary(). The
 * CLIPBOARD is supposed to be controlled explicitly by the user (e.g.
 * using the common Ctrl-X/Ctrl-V shortcuts). The PRIMARY clipboard
 * is supposed to always correspond to 'the current selection', which
 * is a somewhat fuzzy concept.  On platforms that don't support this
 * distinction, GDK will provide a fallback implementation for the
 * PRIMARY clipboard that only allows local data exchange inside the
 * application.
 *
 * A GdkClipboard can hold data in different formats and types. Content
 * types are represented as strings with mime types, such as "application/pdf".
 *
 * The most important data types, text and images, are supported explicitly
 * with the functions gdk_clipboard_set_text(), gdk_clipboard_get_text_async(),
 * gdk_clipboard_text_available() for text and gdk_clipboard_set_image(),
 * gdk_clipboard_get_image_async() and gdk_clipboard_image_available()
 * for images.
 *
 * To store other content in a GdkClipboard, use gdk_clipboard_set_bytes()
 * or gdk_clipboard_set_data() and their corresponding getter functions.
 *
 * GdkClipboard was introduced in GTK+ 3.14.
 */

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
  const gchar *text_content_types[] = { "text/plain", NULL };
  const gchar *image_content_types[] = { "image/png", NULL };

  if (content_types == NULL)
    {
      if (content == TEXT_CONTENT)
        content_types = text_content_types;
      else if (content == IMAGE_CONTENT)
        content_types = image_content_types;
    }

  priv->content = content;
  g_strfreev (priv->content_types);
  priv->content_types = g_strdupv ((gchar **)content_types);

  g_signal_emit (clipboard, signals[CHANGED], 0);
}

GdkClipboardContent
_gdk_clipboard_get_available_content (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);
  
  return priv->content;
}

/**
 * gdk_clipboard_get_content_types:
 * @clipboard: a #GdkClipboard
 *
 * Gets the content types for which the clipboard can currently
 * provide content.
 *
 * Note that text and image data is not represented by content types.
 * Instead, use gdk_clipboard_text_available() and
 * gdk_clipboard_image_available() to check for text or image
 * content.
 *
 * Returns: (transfer none): Content type of the current content
 *
 * Since: 3.14
 */
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

/**
 * gdk_clipboard_data_available:
 * @clipboard: a #GdkClipboard
 * @content_type: the content type to check for
 *
 * Returns whether the clipboard can currently provide content
 * of the given type.
 *
 * Note that text and image data is not represented by content types.
 * Instead, use gdk_clipboard_text_available() and
 * gdk_clipboard_image_available() to check for text or image
 * content.
 *
 * Returns: (transfer none): %TRUE if content of the given type is available
 *
 * Since: 3.14
 */
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

/**
 * gdk_clipboard_get_text_async:
 * @clipboard: a #GdkClipboard
 * @cancellable: (allow-none): a #GCancellable
 * @callback: the callback to call when the text is available
 * @user_data: data that gets passed to @callback
 *
 * Retrieves the text content of the clipboard. This may involve
 * inter-process communication with the current owner of the system
 * clipboard. Therefore, it is implemented as an asynchronous
 * operation. 
 *
 * When the asynchronous operation is completed, @callback is called.
 * It should call gdk_clipboard_get_text_finish() to retrieve the text.
 *
 * Since: 3.14
 */
void
gdk_clipboard_get_text_async (GdkClipboard        *clipboard,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->get_text_async (clipboard, cancellable, callback, user_data);
}

/**
 * gdk_clipboard_get_text_finish:
 * @clipboard: a #GdkClipboard
 * @res: the #GAsyncResult
 * @error: return location for an error
 *
 * Obtains the result of a gdk_clipboard_get_text_async() call.
 * This function may only be called from a callback passed
 * to gdk_clipboard_get_text_async().
 *
 * If the clipboard does not contain text, %NULL is returned.
 *
 * Returns: (transfer full): the current text content of the clipboard
 *
 * Since: 3.14
 */
gchar *
gdk_clipboard_get_text_finish (GdkClipboard  *clipboard,
                               GAsyncResult  *res,
                               GError       **error)
{
  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return GDK_CLIPBOARD_GET_CLASS (clipboard)->get_text_finish (clipboard, res, error);
}

/**
 * gdk_clipboard_set_text:
 * @clipboard: a #GdkClipboard
 * @text: the text to store in the clipboard
 *
 * Sets the clipboard content to the given text.
 *
 * The clipboard makes a copy of the text and provides it
 * to requestors until the clipboard gets overwritten with
 * new content from this or another process, or until
 * gdk_clipboard_clear() is called.
 *
 * Since: 3.14
 */
void
gdk_clipboard_set_text (GdkClipboard *clipboard,
                        const gchar  *text)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->set_text (clipboard, text);
}

/**
 * gdk_clipboard_text_available:
 * @clipboard:
 *
 * Returns whether the clipboard currently contains text.
 *
 * Returns: %TRUE if the clipboard contains text
 *
 * Since: 3.14
 */
gboolean
gdk_clipboard_text_available (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv;

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

  priv = gdk_clipboard_get_instance_private (clipboard);

  return ((priv->content & TEXT_CONTENT) != 0);
}

/**
 * gdk_clipboard_get_image_async:
 * @clipboard: a #GdkClipboard
 * @cancellable: (allow-none): a #GCancellable
 * @callback: the callback to call when the text is available
 * @user_data: data that gets passed to @callback
 *
 * Retrieves the image content of the clipboard. This may involve
 * inter-process communication with the current owner of the system
 * clipboard. Therefore, it is implemented as an asynchronous
 * operation. 
 *
 * When the asynchronous operation is completed, @callback is called.
 * It should call gdk_clipboard_get_image_finish() to retrieve the text.
 *
 * Since: 3.14
 */
void
gdk_clipboard_get_image_async (GdkClipboard        *clipboard,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->get_image_async (clipboard, cancellable, callback, user_data);
}

/**
 * gdk_clipboard_get_image_finish:
 * @clipboard: a #GdkClipboard
 * @res: the #GAsyncResult
 * @error: return location for an error
 *
 * Obtains the result of a gdk_clipboard_get_image_async() call.
 * This function may only be called from a callback passed
 * to gdk_clipboard_get_image_async().
 *
 * If the clipboard does not contain an image, %NULL is returned.
 *
 * Returns: (transfer full): the current image content of the clipboard
 *
 * Since: 3.14
 */
GdkPixbuf *
gdk_clipboard_get_image_finish (GdkClipboard  *clipboard,
                                GAsyncResult  *res,
                                GError       **error)
{
  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return GDK_CLIPBOARD_GET_CLASS (clipboard)->get_image_finish (clipboard, res, error);
}

/**
 * gdk_clipboard_set_image:
 * @clipboard: a #GdkClipboard
 * @pixbuf: the image to store in the clipboard
 *
 * Sets the clipboard content to the given image.
 *
 * The clipboard takes a reference on @pixbuf and provides it
 * to requestors until the clipboard gets overwritten with
 * new content from this or another process, or until
 * gdk_clipboard_clear() is called.
 *
 * Since: 3.14
 */
void
gdk_clipboard_set_image (GdkClipboard *clipboard,
                         GdkPixbuf    *pixbuf)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CLIPBOARD_GET_CLASS (clipboard)->set_image (clipboard, pixbuf);
}

/**
 * gdk_clipboard_image_available:
 * @clipboard:
 *
 * Returns whether the clipboard currently contains an image.
 *
 * Returns: %TRUE if the clipboard contains an image
 *
 * Since: 3.14
 */
gboolean
gdk_clipboard_image_available (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv;

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

  priv = gdk_clipboard_get_instance_private (clipboard);

  return (priv->content & IMAGE_CONTENT) != 0;
}

/**
 * gdk_clipboard_get_data_async:
 * @clipboard: a #GdkClipboard
 * @content_type: the type of content to retrieve
 * @cancellable: (allow-none): a #GCancellable
 * @callback: the callback to call when the text is available
 * @user_data: data that gets passed to @callback
 *
 * Retrieves the content of the clipboard with the given content type.
 * This may involve inter-process communication with the current owner
 * of the system clipboard. Therefore, it is implemented as an
 * asynchronous operation. 
 *
 * When the asynchronous operation is completed, @callback is called.
 * It should call gdk_clipboard_get_data_finish() to retrieve the text.
 *
 * Since: 3.14
 */
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

/**
 * gdk_clipboard_get_data_finish:
 * @clipboard: a #GdkClipboard
 * @res: the #GAsyncResult
 * @error: return location for an error
 *
 * Obtains the result of a gdk_clipboard_get_data_async() call.
 * This function may only be called from a callback passed
 * to gdk_clipboard_get_data_async().
 *
 * If the clipboard does not contain content with the type that
 * was passed to gdk_clipboard_get_data_async(), %NULL is returned.
 *
 * Returns: (transfer full): an input stream from which the current
 *     content of the clipboard can be read
 *
 * Since: 3.14
 */
GInputStream *
gdk_clipboard_get_data_finish (GdkClipboard  *clipboard,
                               GAsyncResult  *res,
                               GError       **error)
{
  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return GDK_CLIPBOARD_GET_CLASS (clipboard)->get_data_finish (clipboard, res, error);
}

/**
 * gdk_clipboard_set_data:
 * @clipboard: a #GdkClipboard
 * @content_types: content types that can be provided
 * @callback: the function that will be called to retrieve the content
 * @user_data: user data that gets passed to @callback
 * @destroy: destroy notify for @user_data
 *
 * Sets the clipboard content. The content will be retrieved on demand
 * by the @callback function.
 *
 * Since: 3.14
 */
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

/**
 * gdk_clipboard_clear:
 * @clipboard: a #GdkClipboard
 *
 * Clears the clipboard.
 *
 * If the clipboard is currently holding the system clipboard,
 * this implies that the clipboard will no longer provide content
 * to other processes. If the system clipboard is currently held
 * by another process, this call will drop any cached content,
 * and the next call to get content will retrieve it from the
 * other process again.
 *
 * Since: 3.14
 */
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

/**
 * gdk_clipboard_get_bytes_async:
 * @clipboard: a #GdkClipboard
 * @content_type: the type of content to retrieve
 * @cancellable: (allow-none): a #GCancellable
 * @callback: the callback to call when the text is available
 * @user_data: data that gets passed to @callback
 *
 * Retrieves the content of the clipboard with the given content type
 * as a #GBytes. This may involve inter-process communication with the
 * current owner of the system clipboard. Therefore, it is implemented
 * as an asynchronous operation. 
 *
 * When the asynchronous operation is completed, @callback is called.
 * It should call gdk_clipboard_get_bytes_finish() to retrieve the text.
 *
 * Since: 3.14
 */
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

/**
 * gdk_clipboard_get_bytes_finish:
 * @clipboard: a #GdkClipboard
 * @res: the #GAsyncResult
 * @error: return location for an error
 *
 * Obtains the result of a gdk_clipboard_get_bytes_async() call.
 * This function may only be called from a callback passed
 * to gdk_clipboard_get_bytes_async().
 *
 * If the clipboard does not contain content with the type that
 * was passed to gdk_clipboard_get_bytes_async(), %NULL is returned.
 *
 * Returns: (transfer full): a #GBytes with the current
 *     content of the clipboard
 *
 * Since: 3.14
 */
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

/**
 * gdk_clipboard_set_bytes:
 * @clipboard: a #GdkClipboard
 * @bytes: the content in a #GBytes
 * @content_types: content types that can be provided
 *
 * Sets the clipboard content from a #GBytes.
 *
 * The clipboard takes a reference on @bytes and provides
 * its content to requestors until the clipboard gets
 * overwritten with new content from this or another
 * process, or until gdk_clipboard_clear() is called.
 *
 * Since: 3.14
 */
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

