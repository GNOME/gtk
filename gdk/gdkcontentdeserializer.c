/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Benjamin Otte
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

#include <gio/gio.h>

#include "gdkcontentdeserializer.h"

#include "gdkcontentformats.h"
#include "gdktexture.h"

#include <gdk-pixbuf/gdk-pixbuf.h>


/**
 * SECTION:gdkcontentdeserializer
 * @Short_description: Deserialize content for transfer
 * @Title: GdkContentSerializer
 * @See_also: #GdkContentDeserializer
 *
 * A GdkContentDeserializer is used to deserialize content for inter-application
 * data transfers.
 */

typedef struct _Deserializer Deserializer;

struct _Deserializer 
{
  const char *                    mime_type; /* interned */
  GType                           type;
  GdkContentDeserializeFunc       deserialize;
  gpointer                        data;
  GDestroyNotify                  notify;
};

GQueue deserializers = G_QUEUE_INIT;

static void init (void);

#define GDK_CONTENT_DESERIALIZER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_DESERIALIZER, GdkContentDeserializerClass))
#define GDK_IS_CONTENT_DESERIALIZER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_DESERIALIZER))
#define GDK_CONTENT_DESERIALIZER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_DESERIALIZER, GdkContentDeserializerClass))

typedef struct _GdkContentDeserializerClass GdkContentDeserializerClass;

struct _GdkContentDeserializer
{
  GObject parent_instance;

  const char *mime_type; /* interned */
  GValue value;
  GInputStream *stream;
  int priority;
  GCancellable *cancellable;
  gpointer user_data;
  GAsyncReadyCallback callback;
  gpointer callback_data;

  gpointer task_data;
  GDestroyNotify task_notify;

  GError *error;
  gboolean returned;
};

struct _GdkContentDeserializerClass
{
  GObjectClass parent_class;
};

static gpointer
gdk_content_deserializer_async_result_get_user_data (GAsyncResult *res)
{
  return GDK_CONTENT_DESERIALIZER (res)->callback_data;
}

static GObject *
gdk_content_deserializer_async_result_get_source_object (GAsyncResult *res)
{
  return NULL;
}

static void
gdk_content_deserializer_async_result_iface_init (GAsyncResultIface *iface)
{
  iface->get_user_data = gdk_content_deserializer_async_result_get_user_data;
  iface->get_source_object = gdk_content_deserializer_async_result_get_source_object;
}

G_DEFINE_TYPE_WITH_CODE (GdkContentDeserializer, gdk_content_deserializer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, gdk_content_deserializer_async_result_iface_init))

static void
gdk_content_deserializer_finalize (GObject *object)
{
  GdkContentDeserializer *deserializer = GDK_CONTENT_DESERIALIZER (object);

  g_value_unset (&deserializer->value);
  g_clear_object (&deserializer->stream);
  g_clear_object (&deserializer->cancellable);
  g_clear_error (&deserializer->error);

  if (deserializer->task_notify)
    deserializer->task_notify (deserializer->task_data);

  G_OBJECT_CLASS (gdk_content_deserializer_parent_class)->finalize (object);
}

static void
gdk_content_deserializer_class_init (GdkContentDeserializerClass *content_deserializer_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (content_deserializer_class);

  object_class->finalize = gdk_content_deserializer_finalize;
}

static void
gdk_content_deserializer_init (GdkContentDeserializer *content_deserializer)
{
}

static void
gdk_content_deserializer_run (const char                *mime_type,
                              GType                      type,
                              GInputStream              *stream,
                              int                        priority,
                              GCancellable              *cancellable,
                              GdkContentDeserializeFunc  deserialize_func,
                              gpointer                   user_data,
                              GAsyncReadyCallback        callback,
                              gpointer                   callback_data)
{
  GdkContentDeserializer *deserializer;

  deserializer = g_object_new (GDK_TYPE_CONTENT_DESERIALIZER, NULL);

  deserializer->mime_type = mime_type;
  g_value_init (&deserializer->value, type);
  deserializer->stream = g_object_ref (stream);
  deserializer->priority = priority;
  if (cancellable)
    deserializer->cancellable = g_object_ref (cancellable);
  deserializer->user_data = user_data;
  deserializer->callback = callback;
  deserializer->callback_data = callback_data;

  deserialize_func (deserializer);
}

const char *
gdk_content_deserializer_get_mime_type (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->mime_type;
}

GType
gdk_content_deserializer_get_gtype (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), G_TYPE_INVALID);

  return G_VALUE_TYPE (&deserializer->value);
}

GValue *
gdk_content_deserializer_get_value (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return &deserializer->value;
}

GInputStream *
gdk_content_deserializer_get_input_stream (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->stream;
}

int
gdk_content_deserializer_get_priority (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), G_PRIORITY_DEFAULT);

  return deserializer->priority;
}

GCancellable *
gdk_content_deserializer_get_cancellable (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->cancellable;
}

gpointer
gdk_content_deserializer_get_user_data (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->user_data;
}

void
gdk_content_deserializer_set_task_data (GdkContentDeserializer *deserializer,
                                        gpointer                data,
                                        GDestroyNotify          notify)
{
  g_return_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer));

  if (deserializer->task_notify)
    deserializer->task_notify (deserializer->task_data);

  deserializer->task_data = data;
  deserializer->task_notify = notify;
}

gpointer
gdk_content_deserializer_get_task_data (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->task_data;
}

static gboolean
gdk_content_deserializer_emit_callback (gpointer data)
{
  GdkContentDeserializer *deserializer = data;

  if (deserializer->callback)
    {
      deserializer->callback (NULL, G_ASYNC_RESULT (deserializer), deserializer->callback_data);
    }

  return G_SOURCE_REMOVE;
}

void
gdk_content_deserializer_return_success (GdkContentDeserializer *deserializer)
{
  g_return_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer));
  g_return_if_fail (!deserializer->returned);

  deserializer->returned = TRUE;
  g_idle_add_full (deserializer->priority,
                   gdk_content_deserializer_emit_callback,
                   deserializer,
                   g_object_unref);
  /* NB: the idle will destroy our reference */
}

void
gdk_content_deserializer_return_error (GdkContentDeserializer *deserializer,
                                       GError                 *error)
{
  g_return_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer));
  g_return_if_fail (!deserializer->returned);
  g_return_if_fail (error != NULL);

  deserializer->error = error;
  /* FIXME: naming */
  gdk_content_deserializer_return_success (deserializer);
}

void
gdk_content_register_deserializer (const char                *mime_type,
                                   GType                      type,
                                   GdkContentDeserializeFunc  deserialize,
                                   gpointer                   data,
                                   GDestroyNotify             notify)
{
  Deserializer *deserializer;

  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (deserialize != NULL);

  init ();

  deserializer = g_slice_new0 (Deserializer);

  deserializer->mime_type = g_intern_string (mime_type);
  deserializer->type = type;
  deserializer->deserialize = deserialize;
  deserializer->data = data;
  deserializer->notify = notify;

  g_queue_push_tail (&deserializers, deserializer);
}

static Deserializer *
lookup_deserializer (const char *mime_type,
                     GType       type)
{
  GList *l;

  g_return_val_if_fail (mime_type != NULL, NULL);

  init ();

  mime_type = g_intern_string (mime_type);

  for (l = g_queue_peek_head_link (&deserializers); l; l = l->next)
    {
      Deserializer *deserializer = l->data;

      if (deserializer->mime_type == mime_type &&
          deserializer->type == type)
        return deserializer;
    }
  
  return NULL;
}

GdkContentFormats *
gdk_content_formats_union_deserialize_gtypes (GdkContentFormats *formats)
{
  GdkContentFormatsBuilder *builder;
  GList *l;

  g_return_val_if_fail (formats != NULL, NULL);

  init ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, formats);

  for (l = g_queue_peek_head_link (&deserializers); l; l = l->next)
    {
      Deserializer *deserializer = l->data;

      if (gdk_content_formats_contain_mime_type (formats, deserializer->mime_type))
        gdk_content_formats_builder_add_gtype (builder, deserializer->type);
    }

  gdk_content_formats_unref (formats);

  return gdk_content_formats_builder_free (builder);
}

GdkContentFormats *
gdk_content_formats_union_deserialize_mime_types (GdkContentFormats *formats)
{
  GdkContentFormatsBuilder *builder;
  GList *l;

  g_return_val_if_fail (formats != NULL, NULL);

  init ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, formats);

  for (l = g_queue_peek_head_link (&deserializers); l; l = l->next)
    {
      Deserializer *deserializer = l->data;

      if (gdk_content_formats_contain_gtype (formats, deserializer->type))
        gdk_content_formats_builder_add_mime_type (builder, deserializer->mime_type);
    }

  gdk_content_formats_unref (formats);

  return gdk_content_formats_builder_free (builder);
}

static void
deserialize_not_found (GdkContentDeserializer *deserializer)
{
  GError *error = g_error_new (G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Could not convert data from %s to %s",
                               gdk_content_deserializer_get_mime_type (deserializer),
                               g_type_name (gdk_content_deserializer_get_gtype (deserializer)));
  gdk_content_deserializer_return_error (deserializer, error);
}

void
gdk_content_deserialize_async (GInputStream        *stream,
                               const char          *mime_type,
                               GType                type,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  Deserializer *deserializer;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  deserializer = lookup_deserializer (mime_type, type);

  gdk_content_deserializer_run (mime_type,
                                type,
                                stream,
                                io_priority,
                                cancellable,
                                deserializer ? deserializer->deserialize : deserialize_not_found,
                                deserializer ? deserializer->data : NULL,
                                callback,
                                user_data);
}

gboolean
gdk_content_deserialize_finish (GAsyncResult  *result,
                                GValue        *value,
                                GError       **error)
{
  GdkContentDeserializer *deserializer;

  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (result), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  deserializer = GDK_CONTENT_DESERIALIZER (result);
  g_return_val_if_fail (G_VALUE_HOLDS (value, G_VALUE_TYPE (&deserializer->value)), FALSE);

  if (deserializer->error)
    {
      if (error)
        *error = g_error_copy (deserializer->error);
      return FALSE;
    }

  g_value_copy (&deserializer->value, value);
  return TRUE;
}

/*** DESERIALIZERS ***/

static void
pixbuf_deserializer_finish (GObject      *source,
                            GAsyncResult *res,
                            gpointer      deserializer)
{
  GdkPixbuf *pixbuf;
  GValue *value;
  GError *error = NULL;

  pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
  if (pixbuf == NULL)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  value = gdk_content_deserializer_get_value (deserializer);
  if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
    {
      g_value_take_object (value, pixbuf);
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
    {
      GdkTexture *texture;
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
      g_value_take_object (value, texture);
    }
  else
    {
      g_assert_not_reached ();
    }
  gdk_content_deserializer_return_success (deserializer);
}

static void
pixbuf_deserializer (GdkContentDeserializer *deserializer)
{
  gdk_pixbuf_new_from_stream_async (gdk_content_deserializer_get_input_stream (deserializer),
				    gdk_content_deserializer_get_cancellable (deserializer),
                                    pixbuf_deserializer_finish,
                                    deserializer);
}

static void
string_deserializer_finish (GObject      *source,
                            GAsyncResult *result,
                            gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }
  else if (written == 0)
    {
      /* Never return NULL, we only return that on error */
      g_value_set_string (gdk_content_deserializer_get_value (deserializer), "");
    }
  else
    {
      g_value_take_string (gdk_content_deserializer_get_value (deserializer),
                           g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (
                               g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (stream)))));
    }
  gdk_content_deserializer_return_success (deserializer);
}

static void
string_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output, *filter;
  GCharsetConverter *converter;
  GError *error = NULL;

  converter = g_charset_converter_new ("utf-8",
                                       gdk_content_deserializer_get_user_data (deserializer),
                                       &error);
  if (converter == NULL)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }
  g_charset_converter_set_use_fallback (converter, TRUE);

  output = g_memory_output_stream_new_resizable ();
  filter = g_converter_output_stream_new (output, G_CONVERTER (converter));
  g_object_unref (output);
  g_object_unref (converter);

  g_output_stream_splice_async (filter,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                string_deserializer_finish,
                                deserializer);
  g_object_unref (filter);
}

static void
file_uri_deserializer_finish (GObject      *source,
                              GAsyncResult *result,
                              gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;
  GValue *value;
  char *str;
  char **uris;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  /* write terminating NULL */
  if (!g_output_stream_write (stream, "", 1, NULL, &error))
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  str = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (
            g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (stream))));
  uris = g_uri_list_extract_uris (str);
  g_free (str);

  value = gdk_content_deserializer_get_value (deserializer);
  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
      if (uris[0] != NULL)
        g_value_take_object (value, g_file_new_for_uri (uris[0]));
    }
  else
    {
      GSList *l = NULL;
      gsize i;

      for (i = 0; uris[i] != NULL; i++)
        l = g_slist_prepend (l, g_file_new_for_uri (uris[i]));

      g_value_take_boxed (value, g_slist_reverse (l));
    }
  g_strfreev (uris);
}

static void
file_uri_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output;

  output = g_memory_output_stream_new_resizable ();

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                file_uri_deserializer_finish,
                                deserializer);
  g_object_unref (output);
}

static void
init (void)
{
  static gboolean initialized = FALSE;
  GSList *formats, *f;
  const char *charset;

  if (initialized)
    return;

  initialized = TRUE;

  formats = gdk_pixbuf_get_formats ();

  /* Make sure png comes first */
  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;
      gchar *name; 
 
      name = gdk_pixbuf_format_get_name (fmt);
      if (g_str_equal (name, "png"))
	{
	  formats = g_slist_delete_link (formats, f);
	  formats = g_slist_prepend (formats, fmt);

	  g_free (name);

	  break;
	}

      g_free (name);
    }  

  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;
      gchar **mimes, **m;

      mimes = gdk_pixbuf_format_get_mime_types (fmt);
      for (m = mimes; *m; m++)
	{
          gdk_content_register_deserializer (*m,
                                             GDK_TYPE_TEXTURE,
                                             pixbuf_deserializer,
                                             NULL,
                                             NULL);
          gdk_content_register_deserializer (*m,
                                             GDK_TYPE_PIXBUF,
                                             pixbuf_deserializer,
                                             NULL,
                                             NULL);
	}
      g_strfreev (mimes);
    }

  g_slist_free (formats);

  gdk_content_register_deserializer ("text/uri-list",
                                     GDK_TYPE_FILE_LIST,
                                     file_uri_deserializer,
                                     NULL,
                                     NULL);
  gdk_content_register_deserializer ("text/uri-list",
                                     G_TYPE_FILE,
                                     file_uri_deserializer,
                                     NULL,
                                     NULL);

  gdk_content_register_deserializer ("text/plain;charset=utf-8",
                                     G_TYPE_STRING,
                                     string_deserializer,
                                     (gpointer) "utf-8",
                                     NULL);
  if (!g_get_charset (&charset))
    {
      char *mime = g_strdup_printf ("text/plain;charset=%s", charset);
      gdk_content_register_deserializer (mime,
                                         G_TYPE_STRING,
                                         string_deserializer,
                                         mime,
                                         g_free);
    }
  gdk_content_register_deserializer ("text/plain",
                                     G_TYPE_STRING,
                                     string_deserializer,
                                     (gpointer) "ASCII",
                                     NULL);
}

