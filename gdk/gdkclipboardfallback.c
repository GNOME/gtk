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
#include <gdk/gdkclipboardfallback.h>


typedef struct _GdkClipboardFallbackClass GdkClipboardFallbackClass;

struct _GdkClipboardFallback
{
  GdkClipboard parent;

  gchar                *text;
  GdkPixbuf            *pixbuf;
  GdkClipboardProvider  provider;
  gpointer              data;
  GDestroyNotify        destroy;
};

struct _GdkClipboardFallbackClass
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkClipboardFallback, gdk_clipboard_fallback, GDK_TYPE_CLIPBOARD)

static void
clear_data (GdkClipboardFallback *cf)
{
  if (cf->text)
    {
      g_free (cf->text);
      cf->text = NULL;
    }

  if (cf->pixbuf)
    {
      g_object_unref (cf->pixbuf);
      cf->pixbuf = NULL;
    }

  if (cf->provider)
    {
      if (cf->destroy)
        cf->destroy (cf->data);

      cf->provider = NULL;
      cf->data = NULL;
      cf->destroy = NULL;
    }
}

static void
gdk_clipboard_fallback_get_text_async (GdkClipboard        *clipboard,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (clipboard),
                                   callback, user_data,
                                   gdk_clipboard_fallback_get_text_async);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static gchar *
gdk_clipboard_fallback_get_text_finish (GdkClipboard  *clipboard,
                                        GAsyncResult  *res,
                                        GError       **error)
{
  GdkClipboardFallback *cf = GDK_CLIPBOARD_FALLBACK (clipboard);

  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (cf), gdk_clipboard_fallback_get_text_async), NULL);

  if (_gdk_clipboard_get_available_content (clipboard) != TEXT_CONTENT)
    {
      /* FIXME: set error ? */
      return NULL;
    }

  return g_strdup (cf->text);
}

static void
gdk_clipboard_fallback_set_text (GdkClipboard *clipboard,
                                 const gchar  *text)
{
  GdkClipboardFallback *cf = GDK_CLIPBOARD_FALLBACK (clipboard);

  clear_data (cf);
  cf->text = g_strdup (text);
  _gdk_clipboard_set_available_content (clipboard, TEXT_CONTENT, NULL);
}

static void
gdk_clipboard_fallback_get_image_async (GdkClipboard        *clipboard,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (clipboard),
                                   callback, user_data,
                                   gdk_clipboard_fallback_get_image_async);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static GdkPixbuf *
gdk_clipboard_fallback_get_image_finish (GdkClipboard  *clipboard,
                                         GAsyncResult  *res,
                                         GError       **error)
{
  GdkClipboardFallback *cf = GDK_CLIPBOARD_FALLBACK (clipboard);

  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (cf), gdk_clipboard_fallback_get_image_async), NULL);

  if (_gdk_clipboard_get_available_content (clipboard) != IMAGE_CONTENT)
    {
      /* FIXME: set error ? */
      return NULL;
    }

  return g_object_ref (cf->pixbuf);
}

static void
gdk_clipboard_fallback_set_image (GdkClipboard *clipboard,
                                  GdkPixbuf    *pixbuf)
{
  GdkClipboardFallback *cf = GDK_CLIPBOARD_FALLBACK (clipboard);

  clear_data (cf);
  cf->pixbuf = g_object_ref (pixbuf);
  _gdk_clipboard_set_available_content (clipboard, IMAGE_CONTENT, NULL);
}

static void
gdk_clipboard_fallback_get_data_async (GdkClipboard        *clipboard,
                                       const gchar         *content_type,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (clipboard),
                                   callback, user_data,
                                   gdk_clipboard_fallback_get_data_async);
  g_object_set_data (G_OBJECT (res), "ctype", (gpointer) content_type);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

typedef struct
{
  GOutputStream parent;
  GMemoryInputStream *is;
} ReadbackOutputStream;

typedef struct
{
  GOutputStreamClass parent_class;
} ReadbackOutputStreamClass;

G_DEFINE_TYPE (ReadbackOutputStream, readback_output_stream, G_TYPE_OUTPUT_STREAM)

static void
readback_output_stream_init (ReadbackOutputStream *stream)
{
}

static gssize
readback_output_stream_write (GOutputStream  *stream,
                              const void     *buffer,
                              gsize           count,
                              GCancellable   *cancellable,
                              GError        **error)
{
  ReadbackOutputStream *rs = (ReadbackOutputStream *)stream;

  if (count)
    g_memory_input_stream_add_data (rs->is, g_memdup (buffer, count), count, g_free);

  return count;
}

static void
readback_output_stream_finalize (GObject *obj)
{
  ReadbackOutputStream *rs = (ReadbackOutputStream *)obj;

  g_object_unref (rs->is);

  G_OBJECT_CLASS (readback_output_stream_parent_class)->finalize (obj);
}

static void
readback_output_stream_class_init (ReadbackOutputStreamClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GOutputStreamClass *ostream_class = G_OUTPUT_STREAM_CLASS (class);

  object_class->finalize = readback_output_stream_finalize;
  ostream_class->write_fn = readback_output_stream_write;
}

static GOutputStream *
readback_output_stream_new (GMemoryInputStream *is)
{
  ReadbackOutputStream *stream;

  stream = g_object_new (readback_output_stream_get_type (), NULL);
  stream->is = g_object_ref (is);

  return G_OUTPUT_STREAM (stream);
}

static GInputStream *
gdk_clipboard_fallback_get_data_finish (GdkClipboard  *clipboard,
                                        GAsyncResult  *res,
                                        GError       **error)
{
  GdkClipboardFallback *cf = GDK_CLIPBOARD_FALLBACK (clipboard);
  const gchar *ctype;
  GOutputStream *ostream = NULL;
  GInputStream *istream = NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (cf), gdk_clipboard_fallback_get_data_async), NULL);

  ctype = (const gchar *) g_object_get_data (G_OBJECT (res), "ctype");

  if (!gdk_clipboard_data_available (clipboard, ctype))
    {
      /* FIXME: set error ? */
      return NULL;
    }

  istream = g_memory_input_stream_new ();
  ostream = readback_output_stream_new (G_MEMORY_INPUT_STREAM (istream));

  cf->provider (clipboard, ctype, ostream, cf->data);

  g_object_unref (ostream);

  return istream;
}

static void
gdk_clipboard_fallback_set_data (GdkClipboard          *clipboard,
                                 const gchar          **content_types,
                                 GdkClipboardProvider   provider,
                                 gpointer               data,
                                 GDestroyNotify         destroy)
{
  GdkClipboardFallback *cf = GDK_CLIPBOARD_FALLBACK (clipboard);

  clear_data (cf);
  cf->provider = provider;
  cf->data = data;
  cf->destroy = destroy;
  _gdk_clipboard_set_available_content (clipboard, OTHER_CONTENT, content_types);
}

static void
gdk_clipboard_fallback_clear (GdkClipboard *clipboard)
{
  GdkClipboardFallback *cf = GDK_CLIPBOARD_FALLBACK (clipboard);

  clear_data (cf);
  _gdk_clipboard_set_available_content (clipboard, NO_CONTENT, NULL);
}

static void
gdk_clipboard_fallback_init (GdkClipboardFallback *clipboard)
{
}

static void
gdk_clipboard_fallback_finalize (GObject *object)
{
  GdkClipboardFallback *cf = GDK_CLIPBOARD_FALLBACK (object);

  clear_data (cf);

  G_OBJECT_CLASS (gdk_clipboard_fallback_parent_class)->finalize (object);

}

static void
gdk_clipboard_fallback_class_init (GdkClipboardFallbackClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_clipboard_fallback_finalize;

  clipboard_class->get_text_async = gdk_clipboard_fallback_get_text_async;
  clipboard_class->get_text_finish = gdk_clipboard_fallback_get_text_finish;
  clipboard_class->set_text = gdk_clipboard_fallback_set_text;
  clipboard_class->get_image_async = gdk_clipboard_fallback_get_image_async;
  clipboard_class->get_image_finish = gdk_clipboard_fallback_get_image_finish;
  clipboard_class->set_image = gdk_clipboard_fallback_set_image;
  clipboard_class->get_data_async = gdk_clipboard_fallback_get_data_async;
  clipboard_class->get_data_finish = gdk_clipboard_fallback_get_data_finish;
  clipboard_class->set_data = gdk_clipboard_fallback_set_data;
  clipboard_class->clear = gdk_clipboard_fallback_clear;
}

GdkClipboard *
gdk_clipboard_fallback_new (void)
{
  return GDK_CLIPBOARD (g_object_new (GDK_TYPE_CLIPBOARD_FALLBACK, NULL));
}
