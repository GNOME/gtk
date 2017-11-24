/* GDK - The GIMP Drawing Kit
 *
 * Copyright (C) 2017 Benjamin Otte <otte@gnome.org>
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

#include "gdkcontentdeserializer.h"
#include "gdkcontentformats.h"
#include "gdkcontentproviderimpl.h"
#include "gdkcontentproviderprivate.h"
#include "gdkdisplay.h"
#include "gdkintl.h"
#include "gdkpipeiostreamprivate.h"

typedef struct _GdkClipboardPrivate GdkClipboardPrivate;

struct _GdkClipboardPrivate
{
  GdkDisplay *display;
  GdkContentFormats *formats;
  GdkContentProvider *content;

  guint local : 1;
};

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_FORMATS,
  PROP_LOCAL,
  PROP_CONTENT,
  N_PROPERTIES
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };
static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GdkClipboard, gdk_clipboard, G_TYPE_OBJECT)

static void
gdk_clipboard_set_property (GObject      *gobject,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (gobject);
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      priv->display = g_value_get_object (value);
      g_assert (priv->display != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_clipboard_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (gobject);
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;

    case PROP_FORMATS:
      g_value_set_boxed (value, priv->formats);
      break;

    case PROP_CONTENT:
      g_value_set_object (value, priv->content);
      break;

    case PROP_LOCAL:
      g_value_set_boolean (value, priv->local);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_clipboard_finalize (GObject *object)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (object);
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  g_clear_pointer (&priv->formats, gdk_content_formats_unref);

  G_OBJECT_CLASS (gdk_clipboard_parent_class)->finalize (object);
}

static void
gdk_clipboard_read_local_write_done (GObject      *clipboard,
                                     GAsyncResult *result,
                                     gpointer      stream)
{
  /* we don't care about the error, we just want to clean up */
  gdk_clipboard_write_finish (GDK_CLIPBOARD (clipboard), result, NULL);

  /* XXX: Do we need to close_async() here? */
  g_output_stream_close (stream, NULL, NULL);

  g_object_unref (stream);
}

static void
gdk_clipboard_read_local_async (GdkClipboard        *clipboard,
                                GdkContentFormats   *formats,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);
  GdkContentFormats *content_formats;
  const char *mime_type;
  GTask *task;

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_clipboard_read_local_async);

  if (priv->content == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                     _("Cannot read from empty clipboard."));
      g_object_unref (task);
      return;
    }

  content_formats = gdk_content_provider_ref_formats (priv->content);

  if (!gdk_content_formats_match (content_formats, formats, NULL, &mime_type)
      || mime_type == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("No compatible formats to transfer clipboard contents."));
    }
  else
    {
      GOutputStream *output_stream;
      GIOStream *stream;
      
      stream = gdk_pipe_io_stream_new ();
      output_stream = g_io_stream_get_output_stream (stream);
      gdk_clipboard_write_async (clipboard,
                                 mime_type,
                                 output_stream,
                                 io_priority,
                                 cancellable,
                                 gdk_clipboard_read_local_write_done,
                                 g_object_ref (output_stream));
      g_task_set_task_data (task, (gpointer) mime_type, NULL);
      g_task_return_pointer (task, g_object_ref (g_io_stream_get_input_stream (stream)), g_object_unref);

      g_object_unref (stream);
    }

  gdk_content_formats_unref (content_formats);
  g_object_unref (task);
}

static GInputStream *
gdk_clipboard_read_local_finish (GdkClipboard  *clipboard,
                                 const char   **out_mime_type,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, clipboard), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_clipboard_read_local_async, NULL);

  if (out_mime_type)
    *out_mime_type = g_task_get_task_data (G_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gdk_clipboard_class_init (GdkClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gdk_clipboard_get_property;
  object_class->set_property = gdk_clipboard_set_property;
  object_class->finalize = gdk_clipboard_finalize;

  class->read_async = gdk_clipboard_read_local_async;
  class->read_finish = gdk_clipboard_read_local_finish;

  /**
   * GdkClipboard:display:
   *
   * The #GdkDisplay that the clipboard belongs to.
   *
   * Since: 3.94
   */
  properties[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         "Display",
                         "Display owning this clipboard",
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkClipboard:formats:
   *
   * The possible formats that the clipboard can provide its data in.
   *
   * Since: 3.94
   */
  properties[PROP_FORMATS] =
    g_param_spec_boxed ("formats",
                        "Formats",
                        "The possible formats for data",
                        GDK_TYPE_CONTENT_FORMATS,
                        G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkClipboard:local:
   *
   * %TRUE if the contents of the clipboard are owned by this process.
   *
   * Since: 3.94
   */
  properties[PROP_LOCAL] =
    g_param_spec_boolean ("local",
                          "Local",
                          "If the contents are owned by this process",
                          TRUE,
                          G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkClipboard:content:
   *
   * The #GdkContentProvider or %NULL if the clipboard is empty or contents are
   * provided otherwise.
   *
   * Since: 3.94
   */
  properties[PROP_CONTENT] =
    g_param_spec_object ("content",
                         "Content",
                         "Provider of the clipboard's content",
                         GDK_TYPE_CONTENT_PROVIDER,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkClipboardClass, changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
gdk_clipboard_init (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  priv->formats = gdk_content_formats_new (NULL, 0);
  priv->local = TRUE;
}

/**
 * gdk_clipboard_get_display:
 * @clipboard: a #GdkClipboard
 *
 * Gets the #GdkDisplay that the clipboard was created for.
 *
 * Returns: (transfer none): a #GdkDisplay
 **/
GdkDisplay *
gdk_clipboard_get_display (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return priv->display;
}

/**
 * gdk_clipboard_get_formats:
 * @clipboard: a #GdkClipboard
 *
 * Gets the formats that the clipboard can provide its current contents in.
 *
 * Returns: (transfer none): The formats of the clipboard
 **/
GdkContentFormats *
gdk_clipboard_get_formats (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return priv->formats;
}

/**
 * gdk_clipboard_is_local:
 * @clipboard: a #GdkClipboard
 *
 * Returns if the clipboard is local. A clipboard is consideredlocal if it was
 * last claimed by the running application.
 *
 * Note that gdk_clipboard_get_content() may return %NULL even on a local
 * clipboard. In this case the clipboard is empty.
 *
 * Returns: %TRUE if the clipboard is local
 **/
gboolean
gdk_clipboard_is_local (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

  return priv->local;
}

/**
 * gdk_clipboard_get_content:
 * @clipboard: a #GdkClipboard
 *
 * Returns the #GdkContentProvider currently set on @clipboard. If the
 * @clipboard is empty or its contents are not owned by the current process,
 * %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): The content of a clipboard or %NULL
 *     if the clipboard does not maintain any content.
 **/
GdkContentProvider *
gdk_clipboard_get_content (GdkClipboard *clipboard)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

  return priv->content;
}

static void
gdk_clipboard_read_internal (GdkClipboard        *clipboard,
                             GdkContentFormats   *formats,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  if (priv->local)
    {
      return gdk_clipboard_read_local_async (clipboard,
                                             formats,
                                             io_priority,
                                             cancellable,
                                             callback,
                                             user_data);
    }
  else
    {
      return GDK_CLIPBOARD_GET_CLASS (clipboard)->read_async (clipboard,
                                                              formats,
                                                              io_priority,
                                                              cancellable,
                                                              callback,
                                                              user_data);
    }
}

/**
 * gdk_clipboard_read_async:
 * @clipboard: a #GdkClipboard
 * @mime_types: a %NULL-terminated array of mime types to choose from
 * @io_priority: the [I/O priority][io-priority]
 * of the request. 
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously requests an input stream to read the @clipboard's
 * contents from. When the operation is finished @callback will be called. 
 * You can then call gdk_clipboard_read_finish() to get the result of the 
 * operation.
 *
 * The clipboard will choose the most suitable mime type from the given list
 * to fulfill the request, preferring the ones listed first. 
 **/
void
gdk_clipboard_read_async (GdkClipboard        *clipboard,
                          const char         **mime_types,
                          int                  io_priority,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  GdkContentFormats *formats;

  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (mime_types != NULL && mime_types[0] != NULL);
  g_return_if_fail (callback != NULL);

  formats = gdk_content_formats_new (mime_types, g_strv_length ((char **) mime_types));

  gdk_clipboard_read_internal (clipboard, formats, io_priority, cancellable, callback, user_data);

  gdk_content_formats_unref (formats);
}

/**
 * gdk_clipboard_read_finish:
 * @clipboard: a #GdkClipboard
 * @out_mime_type: (out) (allow-none) (transfer none): pointer to store
 *     the chosen mime type in or %NULL
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 *
 * Finishes an asynchronous clipboard read started with gdk_clipboard_read_async().
 *
 * Returns: (transfer full): a #GInputStream or %NULL on error.
 **/
GInputStream *
gdk_clipboard_read_finish (GdkClipboard  *clipboard,
                           const char   **out_mime_type,
                           GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* don't check priv->local here because it might have changed while the
   * read was ongoing */
  if (g_async_result_is_tagged (result, gdk_clipboard_read_local_async))
    {
      return gdk_clipboard_read_local_finish (clipboard, out_mime_type, result, error);
    }
  else
    {
      return GDK_CLIPBOARD_GET_CLASS (clipboard)->read_finish (clipboard, out_mime_type, result, error);
    }
}

static void
gdk_clipboard_read_value_done (GObject      *source,
                               GAsyncResult *result,
                               gpointer      data)
{
  GTask *task = data;
  GError *error = NULL;
  GValue *value;

  value = g_task_get_task_data (task);

  if (!gdk_content_deserialize_finish (result, value, &error))
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, value, NULL);

  g_object_unref (task);
}

static void
gdk_clipboard_read_value_got_stream (GObject      *source,
                                     GAsyncResult *result,
                                     gpointer      data)
{
  GInputStream *stream;
  GError *error = NULL;
  GTask *task = data;
  const char *mime_type;

  stream = gdk_clipboard_read_finish (GDK_CLIPBOARD (source), &mime_type, result, &error);
  if (stream == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  gdk_content_deserialize_async (stream,
                                 mime_type,
                                 G_VALUE_TYPE (g_task_get_task_data (task)),
                                 g_task_get_priority (task),
                                 g_task_get_cancellable (task),
                                 gdk_clipboard_read_value_done,
                                 task);
  g_object_unref (stream);
}

static void
free_value (gpointer value)
{
  g_value_unset (value);
  g_slice_free (GValue, value);
}

static void
gdk_clipboard_read_value_internal (GdkClipboard        *clipboard,
                                   GType                type,
                                   gpointer             source_tag,
                                   int                  io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);
  GdkContentFormatsBuilder *builder;
  GdkContentFormats *formats;
  GValue *value;
  GTask *task;
 
  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, source_tag);
  value = g_slice_new0 (GValue);
  g_value_init (value, type);
  g_task_set_task_data (task, value, free_value);

  if (priv->local)
    {
      GError *error = NULL;

      if (priv->content == NULL)
        {
          g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                   _("Cannot read from empty clipboard."));
          g_object_unref (task);
          return;
        }

      if (gdk_content_provider_get_value (priv->content, value, &error))
        {
          g_task_return_pointer (task, value, NULL);
          g_object_unref (task);
          return;
        }
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        {
          g_task_return_error (task, error);
          g_object_unref (task);
          return;
        }
      else
        {
          /* fall through to regular stream transfer */
          g_clear_error (&error);
        }
    }

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_gtype (builder, type);
  formats = gdk_content_formats_builder_free (builder);
  formats = gdk_content_formats_union_deserialize_mime_types (formats);

  gdk_clipboard_read_internal (clipboard,
                               formats,
                               io_priority,
                               cancellable,
                               gdk_clipboard_read_value_got_stream,
                               task);

  gdk_content_formats_unref (formats);
}

/**
 * gdk_clipboard_read_value_async:
 * @clipboard: a #GdkClipboard
 * @type: a #GType to read
 * @io_priority: the [I/O priority][io-priority]
 *     of the request. 
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously request the @clipboard contents converted to the given
 * @type. When the operation is finished @callback will be called. 
 * You can then call gdk_clipboard_read_value_finish() to get the resulting
 * #GValue.
 *
 * For local clipboard contents that are available in the given #GType, the
 * value will be copied directly. Otherwise, GDK will try to use
 * gdk_content_deserialize_async() to convert the clipboard's data.
 **/
void
gdk_clipboard_read_value_async (GdkClipboard        *clipboard,
                                GType                type,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  gdk_clipboard_read_value_internal (clipboard,
                                     type,
                                     gdk_clipboard_read_value_async,
                                     io_priority,
                                     cancellable,
                                     callback,
                                     user_data);
}

/**
 * gdk_clipboard_read_value_finish:
 * @clipboard: a #GdkClipboard
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 *
 * Finishes an asynchronous clipboard read started with
 * gdk_clipboard_read_value_async().
 *
 * Returns: (transfer none): a #GValue containing the result.
 **/
const GValue *
gdk_clipboard_read_value_finish (GdkClipboard  *clipboard,
                                 GAsyncResult  *res,
                                 GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, clipboard), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (res)) == gdk_clipboard_read_value_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}

/**
 * gdk_clipboard_read_pixbuf_async:
 * @clipboard: a #GdkClipboard
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously request the @clipboard contents converted to a #GdkPixbuf.
 * When the operation is finished @callback will be called. You can then
 * call gdk_clipboard_read_pixbuf_finish() to get the result.
 *
 * This is a simple wrapper around gdk_clipboard_read_value_async(). Use
 * that function or gdk_clipboard_read_async() directly if you need more
 * control over the operation.
 **/
void
gdk_clipboard_read_pixbuf_async (GdkClipboard        *clipboard,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  gdk_clipboard_read_value_internal (clipboard,
                                     GDK_TYPE_PIXBUF,
                                     gdk_clipboard_read_pixbuf_async,
                                     G_PRIORITY_DEFAULT,
                                     cancellable,
                                     callback,
                                     user_data);
}

/**
 * gdk_clipboard_read_pixbuf_finish:
 * @clipboard: a #GdkClipboard
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 *
 * Finishes an asynchronous clipboard read started with
 * gdk_clipboard_read_pixbuf_async().
 *
 * Returns: (transfer full) (nullable): a new #GdkPixbuf or %NULL on error.
 **/
GdkPixbuf *
gdk_clipboard_read_pixbuf_finish (GdkClipboard  *clipboard,
                                  GAsyncResult  *res,
                                  GError       **error)
{
  const GValue *value;

  g_return_val_if_fail (g_task_is_valid (res, clipboard), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (res)) == gdk_clipboard_read_pixbuf_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  value = g_task_propagate_pointer (G_TASK (res), error);
  if (!value)
    return NULL;
  
  return g_value_dup_object (value);
}

/**
 * gdk_clipboard_read_text_async:
 * @clipboard: a #GdkClipboard
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously request the @clipboard contents converted to a string.
 * When the operation is finished @callback will be called. You can then
 * call gdk_clipboard_read_text_finish() to get the result.
 *
 * This is a simple wrapper around gdk_clipboard_read_value_async(). Use
 * that function or gdk_clipboard_read_async() directly if you need more
 * control over the operation.
 **/
void
gdk_clipboard_read_text_async (GdkClipboard        *clipboard,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  gdk_clipboard_read_value_internal (clipboard,
                                     G_TYPE_STRING,
                                     gdk_clipboard_read_text_async,
                                     G_PRIORITY_DEFAULT,
                                     cancellable,
                                     callback,
                                     user_data);
}

/**
 * gdk_clipboard_read_text_finish:
 * @clipboard: a #GdkClipboard
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 *
 * Finishes an asynchronous clipboard read started with
 * gdk_clipboard_read_text_async().
 *
 * Returns: (transfer full) (nullable): a new string or %NULL on error.
 **/
char *
gdk_clipboard_read_text_finish (GdkClipboard  *clipboard,
                                GAsyncResult  *res,
                                GError       **error)
{
  const GValue *value;

  g_return_val_if_fail (g_task_is_valid (res, clipboard), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (res)) == gdk_clipboard_read_text_async, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  value = g_task_propagate_pointer (G_TASK (res), error);
  if (!value)
    return NULL;
  
  return g_value_dup_string (value);
}

GdkClipboard *
gdk_clipboard_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_CLIPBOARD,
                       "display", display,
                       NULL);
}

static void
gdk_clipboard_write_done (GObject      *content,
                          GAsyncResult *result,
                          gpointer      task)
{
  GError *error = NULL;

  if (gdk_content_provider_write_mime_type_finish (GDK_CONTENT_PROVIDER (content), result, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);

  g_object_unref (task);
}

void
gdk_clipboard_write_async (GdkClipboard        *clipboard,
                           const char          *mime_type,
                           GOutputStream       *stream,
                           int                  io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);
  GdkContentFormats *formats;
  GTask *task;

  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (priv->local);
  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (mime_type == g_intern_string (mime_type));
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_clipboard_write_async);

  if (priv->content == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                               _("Cannot read from empty clipboard."));
      g_object_unref (task);
      return;
    }

  formats = gdk_content_provider_ref_formats (priv->content);
  if (gdk_content_formats_contain_mime_type (formats, mime_type))
    {
      gdk_content_provider_write_mime_type_async (priv->content,
                                                  mime_type,
                                                  stream,
                                                  io_priority,
                                                  cancellable,
                                                  gdk_clipboard_write_done,
                                                  task);
      gdk_content_formats_unref (formats);
      return;
    }

  g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("FIXME: Implement serializing."));
  gdk_content_formats_unref (formats);
  g_object_unref (task);
  return;
}

gboolean
gdk_clipboard_write_finish (GdkClipboard  *clipboard,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, clipboard), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_clipboard_write_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error); 
}

static void
gdk_clipboard_content_changed_cb (GdkContentProvider *provider,
                                  GdkClipboard       *clipboard);

static void
gdk_clipboard_claim (GdkClipboard       *clipboard,
                     GdkContentFormats  *formats,
                     gboolean            local,
                     GdkContentProvider *content)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  g_object_freeze_notify (G_OBJECT (clipboard));

  gdk_content_formats_unref (priv->formats);
  gdk_content_formats_ref (formats);
  formats = gdk_content_formats_union_deserialize_gtypes (formats);
  priv->formats = formats;
  g_object_notify_by_pspec (G_OBJECT (clipboard), properties[PROP_FORMATS]);
  if (priv->local != local)
    {
      priv->local = local;
      g_object_notify_by_pspec (G_OBJECT (clipboard), properties[PROP_LOCAL]);
    }

  if (priv->content != content)
    {
      GdkContentProvider *old_content = priv->content;

      priv->content = g_object_ref (content);

      if (old_content)
        {
          g_signal_handlers_disconnect_by_func (old_content,
                                                gdk_clipboard_content_changed_cb,
                                                clipboard);
          gdk_content_provider_detach_clipboard (old_content, clipboard);
          g_object_unref (old_content);
        }
      if (content)
        {
          gdk_content_provider_attach_clipboard (content, clipboard);
          g_signal_connect (content,
                            "content-changed",
                            G_CALLBACK (gdk_clipboard_content_changed_cb),
                            clipboard);
        }

      g_object_notify_by_pspec (G_OBJECT (clipboard), properties[PROP_CONTENT]);
    }

  g_object_thaw_notify (G_OBJECT (clipboard));

  g_signal_emit (clipboard, signals[CHANGED], 0);
}

static void
gdk_clipboard_content_changed_cb (GdkContentProvider *provider,
                                  GdkClipboard       *clipboard)
{
  GdkContentFormats *formats;

  formats = gdk_content_provider_ref_formats (provider);

  gdk_clipboard_claim (clipboard, formats, TRUE, provider);

  gdk_content_formats_unref (formats);
}

void
gdk_clipboard_claim_remote (GdkClipboard      *clipboard,
                            GdkContentFormats *formats)
{
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (formats != NULL);

  gdk_clipboard_claim (clipboard, formats, FALSE, NULL);
}

/**
 * gdk_clipboard_set_content:
 * @clipboard: a #GdkClipboard
 * @provider: (transfer none) (allow-none): the new contents of @clipboard or
 *     %NULL to clear the clipboard
 *
 * Sets a new content provider on @clipboard. The clipboard will claim the
 * #GdkDisplay's resources and advertise these new contents to other
 * applications.
 *
 * If the contents are read by either an external application or the
 * @clipboard's read functions, @clipboard will select the best format to
 * transfer the contents and then request that format from @provider.
 **/
void
gdk_clipboard_set_content (GdkClipboard       *clipboard,
                           GdkContentProvider *provider)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);
  GdkContentFormats *formats;

  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (provider == NULL || GDK_IS_CONTENT_PROVIDER (provider));

  if (provider)
    {
      if (priv->content == provider)
        return;

      formats = gdk_content_provider_ref_formats (provider);
    }
  else
    {
      if (priv->content == NULL && priv->local)
        return;

      formats = gdk_content_formats_new (NULL, 0);
    }

  gdk_clipboard_claim (clipboard, formats, TRUE, provider);

  gdk_content_formats_unref (formats);
}

/**
 * gdk_clipboard_set_text:
 * @clipboard: a #GdkClipboard
 * @text: Text to put into the clipboard
 *
 * Puts the given @text into the clipboard.
 **/
void
gdk_clipboard_set_text (GdkClipboard *clipboard,
                        const char   *text)
{
  GdkContentProvider *provider;
  GValue value = G_VALUE_INIT;

  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, text);
  provider = gdk_content_provider_new_for_value (&value);
  g_value_unset (&value);

  gdk_clipboard_set_content (clipboard, provider);
  g_object_unref (provider);
}

/**
 * gdk_clipboard_set_pixbuf:
 * @clipboard: a #GdkClipboard
 * @pixbuf: a #GdkPixbuf to put into the clipboard
 *
 * Puts the given @pixbuf into the clipboard.
 **/
void
gdk_clipboard_set_pixbuf (GdkClipboard *clipboard,
                          GdkPixbuf    *pixbuf)
{
  GdkContentProvider *provider;
  GValue value = G_VALUE_INIT;

  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  g_value_init (&value, GDK_TYPE_PIXBUF);
  g_value_set_object (&value, pixbuf);
  provider = gdk_content_provider_new_for_value (&value);
  g_value_unset (&value);

  gdk_clipboard_set_content (clipboard, provider);
  g_object_unref (provider);
}

