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

#include "gdkcontentserializer.h"

#include "gdkcontentformats.h"
#include "gdkpixbuf.h"
#include "gdktextureprivate.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>


/**
 * SECTION:gdkcontentserializer
 * @Short_description: Serialize content for transfer
 * @Title: GdkContentSerializer
 * @See_also: #GdkContentDeserializer, #GdkContentProvider
 *
 * A GdkContentSerializer is used to serialize content for inter-application
 * data transfers.
 */

typedef struct _Serializer Serializer;

struct _Serializer 
{
  const char *                    mime_type; /* interned */
  GType                           type;
  GdkContentSerializeFunc         serialize;
  gpointer                        data;
  GDestroyNotify                  notify;
};

GQueue serializers = G_QUEUE_INIT;

static void init (void);

#define GDK_CONTENT_SERIALIZER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_SERIALIZER, GdkContentSerializerClass))
#define GDK_IS_CONTENT_SERIALIZER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_SERIALIZER))
#define GDK_CONTENT_SERIALIZER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_SERIALIZER, GdkContentSerializerClass))

typedef struct _GdkContentSerializerClass GdkContentSerializerClass;

struct _GdkContentSerializer
{
  GObject parent_instance;

  const char *mime_type; /* interned */
  GValue value;
  GOutputStream *stream;
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

struct _GdkContentSerializerClass
{
  GObjectClass parent_class;
};

static gpointer
gdk_content_serializer_async_result_get_user_data (GAsyncResult *res)
{
  return GDK_CONTENT_SERIALIZER (res)->callback_data;
}

static GObject *
gdk_content_serializer_async_result_get_source_object (GAsyncResult *res)
{
  return NULL;
}

static void
gdk_content_serializer_async_result_iface_init (GAsyncResultIface *iface)
{
  iface->get_user_data = gdk_content_serializer_async_result_get_user_data;
  iface->get_source_object = gdk_content_serializer_async_result_get_source_object;
}

G_DEFINE_TYPE_WITH_CODE (GdkContentSerializer, gdk_content_serializer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, gdk_content_serializer_async_result_iface_init))

static void
gdk_content_serializer_finalize (GObject *object)
{
  GdkContentSerializer *serializer = GDK_CONTENT_SERIALIZER (object);

  g_value_unset (&serializer->value);
  g_clear_object (&serializer->stream);
  g_clear_object (&serializer->cancellable);
  g_clear_error (&serializer->error);

  if (serializer->task_notify)
    serializer->task_notify (serializer->task_data);

  G_OBJECT_CLASS (gdk_content_serializer_parent_class)->finalize (object);
}

static void
gdk_content_serializer_class_init (GdkContentSerializerClass *content_serializer_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (content_serializer_class);

  object_class->finalize = gdk_content_serializer_finalize;
}

static void
gdk_content_serializer_init (GdkContentSerializer *content_serializer)
{
}

static void
gdk_content_serializer_run (const char              *mime_type,
                            const GValue            *value,
                            GOutputStream           *stream,
                            int                      priority,
                            GCancellable            *cancellable,
                            GdkContentSerializeFunc  serialize_func,
                            gpointer                 user_data,
                            GAsyncReadyCallback      callback,
                            gpointer                 callback_data)
{
  GdkContentSerializer *serializer;

  serializer = g_object_new (GDK_TYPE_CONTENT_SERIALIZER, NULL);

  serializer->mime_type = mime_type;
  g_value_init (&serializer->value, G_VALUE_TYPE (value));
  g_value_copy (value, &serializer->value);
  serializer->stream = g_object_ref (stream);
  serializer->priority = priority;
  if (cancellable)
    serializer->cancellable = g_object_ref (cancellable);
  serializer->user_data = user_data;
  serializer->callback = callback;
  serializer->callback_data = callback_data;

  serialize_func (serializer);
}

const char *
gdk_content_serializer_get_mime_type (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->mime_type;
}

GType
gdk_content_serializer_get_gtype (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), G_TYPE_INVALID);

  return G_VALUE_TYPE (&serializer->value);
}

const GValue *
gdk_content_serializer_get_value (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return &serializer->value;
}

GOutputStream *
gdk_content_serializer_get_output_stream (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->stream;
}

int
gdk_content_serializer_get_priority (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), G_PRIORITY_DEFAULT);

  return serializer->priority;
}

GCancellable *
gdk_content_serializer_get_cancellable (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->cancellable;
}

gpointer
gdk_content_serializer_get_user_data (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->user_data;
}

void
gdk_content_serializer_set_task_data (GdkContentSerializer   *serializer,
                                      gpointer                data,
                                      GDestroyNotify          notify)
{
  g_return_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer));

  if (serializer->task_notify)
    serializer->task_notify (serializer->task_data);

  serializer->task_data = data;
  serializer->task_notify = notify;
}

gpointer
gdk_content_serializer_get_task_data (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->task_data;
}

static gboolean
gdk_content_serializer_emit_callback (gpointer data)
{
  GdkContentSerializer *serializer = data;

  if (serializer->callback)
    {
      serializer->callback (NULL, G_ASYNC_RESULT (serializer), serializer->callback_data);
    }

  return G_SOURCE_REMOVE;
}

void
gdk_content_serializer_return_success (GdkContentSerializer *serializer)
{
  g_return_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer));
  g_return_if_fail (!serializer->returned);

  serializer->returned = TRUE;
  g_idle_add_full (serializer->priority,
                   gdk_content_serializer_emit_callback,
                   serializer,
                   g_object_unref);
  /* NB: the idle will destroy our reference */
}

void
gdk_content_serializer_return_error (GdkContentSerializer *serializer,
                                     GError               *error)
{
  g_return_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer));
  g_return_if_fail (!serializer->returned);
  g_return_if_fail (error != NULL);

  serializer->error = error;
  /* FIXME: naming */
  gdk_content_serializer_return_success (serializer);
}

void
gdk_content_register_serializer (GType                    type,
                                 const char              *mime_type,
                                 GdkContentSerializeFunc  serialize,
                                 gpointer                 data,
                                 GDestroyNotify           notify)
{
  Serializer *serializer;

  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (serialize != NULL);

  init ();

  serializer = g_slice_new0 (Serializer);

  serializer->mime_type = g_intern_string (mime_type);
  serializer->type = type;
  serializer->serialize = serialize;
  serializer->data = data;
  serializer->notify = notify;

  g_queue_push_tail (&serializers, serializer);
}

static Serializer *
lookup_serializer (const char *mime_type,
                   GType       type)
{
  GList *l;

  g_return_val_if_fail (mime_type != NULL, NULL);

  init ();

  mime_type = g_intern_string (mime_type);

  for (l = g_queue_peek_head_link (&serializers); l; l = l->next)
    {
      Serializer *serializer = l->data;

      if (serializer->mime_type == mime_type &&
          serializer->type == type)
        return serializer;
    }
  
  return NULL;
}

GdkContentFormats *
gdk_content_formats_union_serialize_gtypes (GdkContentFormats *formats)
{
  GdkContentFormatsBuilder *builder;
  GList *l;

  g_return_val_if_fail (formats != NULL, NULL);

  init ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, formats);

  for (l = g_queue_peek_head_link (&serializers); l; l = l->next)
    {
      Serializer *serializer = l->data;

      if (gdk_content_formats_contain_mime_type (formats, serializer->mime_type))
        gdk_content_formats_builder_add_gtype (builder, serializer->type);
    }

  gdk_content_formats_unref (formats);

  return gdk_content_formats_builder_free (builder);
}

GdkContentFormats *
gdk_content_formats_union_serialize_mime_types (GdkContentFormats *formats)
{
  GdkContentFormatsBuilder *builder;
  GList *l;

  g_return_val_if_fail (formats != NULL, NULL);

  init ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, formats);

  for (l = g_queue_peek_head_link (&serializers); l; l = l->next)
    {
      Serializer *serializer = l->data;

      if (gdk_content_formats_contain_gtype (formats, serializer->type))
        gdk_content_formats_builder_add_mime_type (builder, serializer->mime_type);
    }

  gdk_content_formats_unref (formats);

  return gdk_content_formats_builder_free (builder);
}

static void
serialize_not_found (GdkContentSerializer *serializer)
{
  GError *error = g_error_new (G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Could not convert data from %s to %s",
                               g_type_name (gdk_content_serializer_get_gtype (serializer)),
                               gdk_content_serializer_get_mime_type (serializer));
  gdk_content_serializer_return_error (serializer, error);
}

void
gdk_content_serialize_async (GOutputStream       *stream,
                             const char          *mime_type,
                             const GValue        *value,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  Serializer *serializer;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  serializer = lookup_serializer (mime_type, G_VALUE_TYPE (value));

  gdk_content_serializer_run (mime_type,
                              value,
                              stream,
                              io_priority,
                              cancellable,
                              serializer ? serializer->serialize : serialize_not_found,
                              serializer ? serializer->data : NULL,
                              callback,
                              user_data);
}

gboolean
gdk_content_serialize_finish (GAsyncResult  *result,
                              GError       **error)
{
  GdkContentSerializer *serializer;

  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (result), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  serializer = GDK_CONTENT_SERIALIZER (result);

  if (serializer->error)
    {
      if (error)
        *error = g_error_copy (serializer->error);
      return FALSE;
    }

  return TRUE;
}

/*** SERIALIZERS ***/

static void
pixbuf_serializer_finish (GObject      *source,
                          GAsyncResult *res,
                          gpointer      serializer)
{
  GError *error = NULL;

  if (!gdk_pixbuf_save_to_stream_finish (res, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
pixbuf_serializer (GdkContentSerializer *serializer)
{
  const GValue *value;
  GdkPixbuf *pixbuf;
  const char *name;
  
  name = gdk_content_serializer_get_user_data (serializer);
  value = gdk_content_serializer_get_value (serializer);

  if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
    {
      pixbuf = g_value_dup_object (value);
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
    {
      GdkTexture *texture = g_value_get_object (value);
      cairo_surface_t *surface = gdk_texture_download_surface (texture);
      pixbuf = gdk_pixbuf_get_from_surface (surface,
                                            0, 0,
                                            gdk_texture_get_width (texture), gdk_texture_get_height (texture));
      cairo_surface_destroy (surface);
    }
  else
    {
      g_assert_not_reached ();
    }

  gdk_pixbuf_save_to_stream_async (pixbuf,
                                   gdk_content_serializer_get_output_stream (serializer),
                                   name,
				   gdk_content_serializer_get_cancellable (serializer),
                                   pixbuf_serializer_finish,
                                   serializer,
                                   g_str_equal (name, "png") ? "compression" : NULL, "2",
                                   NULL);
  g_object_unref (pixbuf);
}

static void
string_serializer_finish (GObject      *source,
                          GAsyncResult *result,
                          gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
string_serializer (GdkContentSerializer *serializer)
{
  GOutputStream *filter;
  GCharsetConverter *converter;
  GError *error = NULL;
  const char *text;

  converter = g_charset_converter_new (gdk_content_serializer_get_user_data (serializer),
                                       "utf-8",
                                       &error);
  if (converter == NULL)
    {
      gdk_content_serializer_return_error (serializer, error);
      return;
    }
  g_charset_converter_set_use_fallback (converter, TRUE);

  filter = g_converter_output_stream_new (gdk_content_serializer_get_output_stream (serializer),
                                          G_CONVERTER (converter));
  g_object_unref (converter);

  text = g_value_get_string (gdk_content_serializer_get_value (serializer));
  if (text == NULL)
    text = "";

  g_output_stream_write_all_async (filter,
                                   text,
                                   strlen (text) + 1,
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   string_serializer_finish,
                                   serializer);
  g_object_unref (filter);
}

static void
file_serializer_finish (GObject      *source,
                        GAsyncResult *result,
                        gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
file_uri_serializer (GdkContentSerializer *serializer)
{
  GFile *file;
  GString *str;
  const GValue *value;
  char *uri;

  str = g_string_new (NULL);
  value = gdk_content_serializer_get_value (serializer);

  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
      file = g_value_get_object (gdk_content_serializer_get_value (serializer));
      if (file)
        {
          uri = g_file_get_uri (file);
          g_string_append (str, uri);
          g_free (uri);
        }
      else
        {
          g_string_append (str, "# GTK does not crash when copying a NULL GFile!");
        }
      g_string_append (str, "\r\n");
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      GSList *l;
      
      for (l = g_value_get_boxed (value); l; l = l->next)
        {
          uri = g_file_get_uri (l->data);
          g_string_append (str, uri);
          g_free (uri);
          g_string_append (str, "\r\n");
        }
    }

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   str->str,
                                   str->len,
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   file_serializer_finish,
                                   serializer);
  gdk_content_serializer_set_task_data (serializer, g_string_free (str, FALSE), g_free);
}

static void
file_text_serializer (GdkContentSerializer *serializer)
{
  const GValue *value;
  char *path = NULL;

  value = gdk_content_serializer_get_value (serializer);

  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
      GFile *file;

      file = g_value_get_object (value);
      if (file)
        {
          path = g_file_get_path (file);
          if (path == NULL)
            path = g_file_get_uri (file);
        }
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      GString *str;
      GSList *l;
      
      str = g_string_new (NULL);

      for (l = g_value_get_boxed (value); l; l = l->next)
        {
          path = g_file_get_path (l->data);
          if (path == NULL)
            path = g_file_get_uri (l->data);
          g_string_append (str, path);
          g_free (path);
          if (l->next)
            g_string_append (str, " ");
        }
      path = g_string_free (str, FALSE);
    }

  g_assert (path != NULL);

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   path,
                                   strlen (path),
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   file_serializer_finish,
                                   serializer);
  gdk_content_serializer_set_task_data (serializer, path, g_free);
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

      if (!gdk_pixbuf_format_is_writable (fmt))
	continue;

      mimes = gdk_pixbuf_format_get_mime_types (fmt);
      for (m = mimes; *m; m++)
	{
          gdk_content_register_serializer (GDK_TYPE_TEXTURE,
                                           *m,
                                           pixbuf_serializer,
                                           g_strdup (gdk_pixbuf_format_get_name (fmt)),
                                           g_free);
          gdk_content_register_serializer (GDK_TYPE_PIXBUF,
                                           *m,
                                           pixbuf_serializer,
                                           g_strdup (gdk_pixbuf_format_get_name (fmt)),
                                           g_free);
	}
      g_strfreev (mimes);
    }

  g_slist_free (formats);

  gdk_content_register_serializer (G_TYPE_FILE,
                                   "text/uri-list",
                                   file_uri_serializer,
                                   NULL,
                                   NULL);
  gdk_content_register_serializer (G_TYPE_FILE,
                                   "text/plain;charset=utf-8",
                                   file_text_serializer,
                                   NULL,
                                   NULL);
  gdk_content_register_serializer (GDK_TYPE_FILE_LIST,
                                   "text/uri-list",
                                   file_uri_serializer,
                                   NULL,
                                   NULL);
  gdk_content_register_serializer (GDK_TYPE_FILE_LIST,
                                   "text/plain;charset=utf-8",
                                   file_text_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_serializer (G_TYPE_STRING,
                                   "text/plain;charset=utf-8",
                                   string_serializer,
                                   (gpointer) "utf-8",
                                   NULL);
  if (!g_get_charset (&charset))
    {
      char *mime = g_strdup_printf ("text/plain;charset=%s", charset);
      gdk_content_register_serializer (G_TYPE_STRING,
                                       mime,
                                       string_serializer,
                                       mime,
                                       g_free);
    }
  gdk_content_register_serializer (G_TYPE_STRING,
                                   "text/plain",
                                   string_serializer,
                                   (gpointer) "ASCII",
                                   NULL);
}

