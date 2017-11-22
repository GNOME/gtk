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

#include "gdkcontentformats.h"
#include "gdkdisplay.h"

typedef struct _GdkClipboardPrivate GdkClipboardPrivate;

struct _GdkClipboardPrivate
{
  GdkDisplay *display;
  GdkContentFormats *formats;

  guint local : 1;
};

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_FORMATS,
  PROP_LOCAL,
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
gdk_clipboard_real_read_async (GdkClipboard        *clipboard,
                               GdkContentFormats   *formats,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GTask *task;

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_clipboard_read_async);
  g_task_set_task_data (task, gdk_content_formats_ref (formats), (GDestroyNotify) gdk_content_formats_unref);

  g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Reading local content not supported yet.");
  g_object_unref (task);
}

static GInputStream *
gdk_clipboard_real_read_finish (GdkClipboard  *clipboard,
                                const char   **out_mime_type,
                                GAsyncResult  *result,
                                GError       **error)
{
  /* whoop whooop */
  return g_memory_input_stream_new ();
}

static void
gdk_clipboard_class_init (GdkClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gdk_clipboard_get_property;
  object_class->set_property = gdk_clipboard_set_property;
  object_class->finalize = gdk_clipboard_finalize;

  class->read_async = gdk_clipboard_real_read_async;
  class->read_finish = gdk_clipboard_real_read_finish;

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

static void
gdk_clipboard_read_internal (GdkClipboard        *clipboard,
                             GdkContentFormats   *formats,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  return GDK_CLIPBOARD_GET_CLASS (clipboard)->read_async (clipboard,
                                                          formats,
                                                          io_priority,
                                                          cancellable,
                                                          callback,
                                                          user_data);
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

  return GDK_CLIPBOARD_GET_CLASS (clipboard)->read_finish (clipboard, out_mime_type, result, error);
}

GdkClipboard *
gdk_clipboard_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_CLIPBOARD,
                       "display", display,
                       NULL);
}

void
gdk_clipboard_claim_remote (GdkClipboard      *clipboard,
                            GdkContentFormats *formats)
{
  GdkClipboardPrivate *priv = gdk_clipboard_get_instance_private (clipboard);

  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (formats != NULL);

  gdk_content_formats_unref (priv->formats);
  priv->formats = gdk_content_formats_ref (formats);
  g_object_notify_by_pspec (G_OBJECT (clipboard), properties[PROP_FORMATS]);
  if (priv->local)
    {
      priv->local = FALSE;
      g_object_notify_by_pspec (G_OBJECT (clipboard), properties[PROP_LOCAL]);
    }

  g_signal_emit (clipboard, signals[CHANGED], 0);
}
