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

#include "gdkcontentprovider.h"

#include "gdkcontentformats.h"
#include "gdkintl.h"
#include "gdkcontentproviderimpl.h"

#define GDK_TYPE_CONTENT_PROVIDER_VALUE            (gdk_content_provider_value_get_type ())
#define GDK_CONTENT_PROVIDER_VALUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CONTENT_PROVIDER_VALUE, GdkContentProviderValue))
#define GDK_IS_CONTENT_PROVIDER_VALUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CONTENT_PROVIDER_VALUE))
#define GDK_CONTENT_PROVIDER_VALUE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_PROVIDER_VALUE, GdkContentProviderValueClass))
#define GDK_IS_CONTENT_PROVIDER_VALUE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_PROVIDER_VALUE))
#define GDK_CONTENT_PROVIDER_VALUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_PROVIDER_VALUE, GdkContentProviderValueClass))

typedef struct _GdkContentProviderValue GdkContentProviderValue;
typedef struct _GdkContentProviderValueClass GdkContentProviderValueClass;

struct _GdkContentProviderValue
{
  GdkContentProvider parent;

  GValue value;
};

struct _GdkContentProviderValueClass
{
  GdkContentProviderClass parent_class;
};

GType gdk_content_provider_value_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkContentProviderValue, gdk_content_provider_value, GDK_TYPE_CONTENT_PROVIDER)

static void
gdk_content_provider_value_finalize (GObject *object)
{
  GdkContentProviderValue *content = GDK_CONTENT_PROVIDER_VALUE (object);

  g_value_unset (&content->value);

  G_OBJECT_CLASS (gdk_content_provider_value_parent_class)->finalize (object);
}

static GdkContentFormats *
gdk_content_provider_value_ref_formats (GdkContentProvider *provider)
{
  GdkContentProviderValue *content = GDK_CONTENT_PROVIDER_VALUE (provider);

  return gdk_content_formats_new_for_gtype (G_VALUE_TYPE (&content->value));
}

static gboolean
gdk_content_provider_value_get_value (GdkContentProvider  *provider,
                                      GValue              *value,
                                      GError             **error)
{
  GdkContentProviderValue *content = GDK_CONTENT_PROVIDER_VALUE (provider);

  if (G_VALUE_HOLDS (value, G_VALUE_TYPE (&content->value)))
    {
      g_value_copy (&content->value, value);
      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (gdk_content_provider_value_parent_class)->get_value (provider, value, error);
}

static void
gdk_content_provider_value_class_init (GdkContentProviderValueClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  object_class->finalize = gdk_content_provider_value_finalize;

  provider_class->ref_formats = gdk_content_provider_value_ref_formats;
  provider_class->get_value = gdk_content_provider_value_get_value;
}

static void
gdk_content_provider_value_init (GdkContentProviderValue *content)
{
}

/**
 * gdk_content_provider_new_for_value:
 * @value: a #GValue
 *
 * Create a content provider that provides the given @value.
 *
 * Returns: a new #GdkContentProvider
 **/
GdkContentProvider *
gdk_content_provider_new_for_value (const GValue *value)
{
  GdkContentProviderValue *content;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  content = g_object_new (GDK_TYPE_CONTENT_PROVIDER_VALUE, NULL);
  g_value_init (&content->value, G_VALUE_TYPE (value));
  g_value_copy (value, &content->value);
  
  return GDK_CONTENT_PROVIDER (content);
}

#define GDK_TYPE_CONTENT_PROVIDER_BYTES            (gdk_content_provider_bytes_get_type ())
#define GDK_CONTENT_PROVIDER_BYTES(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CONTENT_PROVIDER_BYTES, GdkContentProviderBytes))
#define GDK_IS_CONTENT_PROVIDER_BYTES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CONTENT_PROVIDER_BYTES))
#define GDK_CONTENT_PROVIDER_BYTES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_PROVIDER_BYTES, GdkContentProviderBytesClass))
#define GDK_IS_CONTENT_PROVIDER_BYTES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_PROVIDER_BYTES))
#define GDK_CONTENT_PROVIDER_BYTES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_PROVIDER_BYTES, GdkContentProviderBytesClass))

typedef struct _GdkContentProviderBytes GdkContentProviderBytes;
typedef struct _GdkContentProviderBytesClass GdkContentProviderBytesClass;

struct _GdkContentProviderBytes
{
  GdkContentProvider parent;

  /* interned */const char *mime_type;
  GBytes *bytes;
};

struct _GdkContentProviderBytesClass
{
  GdkContentProviderClass parent_class;
};

GType gdk_content_provider_bytes_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkContentProviderBytes, gdk_content_provider_bytes, GDK_TYPE_CONTENT_PROVIDER)

static void
gdk_content_provider_bytes_finalize (GObject *object)
{
  GdkContentProviderBytes *content = GDK_CONTENT_PROVIDER_BYTES (object);

  g_bytes_unref (content->bytes);

  G_OBJECT_CLASS (gdk_content_provider_bytes_parent_class)->finalize (object);
}

static GdkContentFormats *
gdk_content_provider_bytes_ref_formats (GdkContentProvider *provider)
{
  GdkContentProviderBytes *content = GDK_CONTENT_PROVIDER_BYTES (provider);
  GdkContentFormatsBuilder *builder;

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_mime_type (builder, content->mime_type);
  return gdk_content_formats_builder_free_to_formats (builder);
}

static void
gdk_content_provider_bytes_write_mime_type_done (GObject      *stream,
                                                 GAsyncResult *result,
                                                 gpointer      task)
{
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (G_OUTPUT_STREAM (stream),
                                         result,
                                         NULL,
                                         &error))
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }

  g_object_unref (task);
}

static void
gdk_content_provider_bytes_write_mime_type_async (GdkContentProvider     *provider,
                                                  const char             *mime_type,
                                                  GOutputStream          *stream,
                                                  int                     io_priority,
                                                  GCancellable           *cancellable,
                                                  GAsyncReadyCallback     callback,
                                                  gpointer                user_data)
{
  GdkContentProviderBytes *content = GDK_CONTENT_PROVIDER_BYTES (provider);
  GTask *task;

  task = g_task_new (content, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_content_provider_bytes_write_mime_type_async);

  if (mime_type != content->mime_type)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("Cannot provide contents as “%s”"), mime_type);
      g_object_unref (task);
      return;
    }

  g_output_stream_write_all_async (stream,
                                   g_bytes_get_data (content->bytes, NULL),
                                   g_bytes_get_size (content->bytes),
                                   io_priority,
                                   cancellable,
                                   gdk_content_provider_bytes_write_mime_type_done,
                                   task);
}

static gboolean
gdk_content_provider_bytes_write_mime_type_finish (GdkContentProvider *provider,
                                                   GAsyncResult       *result,
                                                   GError            **error)
{
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_content_provider_bytes_write_mime_type_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gdk_content_provider_bytes_class_init (GdkContentProviderBytesClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  object_class->finalize = gdk_content_provider_bytes_finalize;

  provider_class->ref_formats = gdk_content_provider_bytes_ref_formats;
  provider_class->write_mime_type_async = gdk_content_provider_bytes_write_mime_type_async;
  provider_class->write_mime_type_finish = gdk_content_provider_bytes_write_mime_type_finish;
}

static void
gdk_content_provider_bytes_init (GdkContentProviderBytes *content)
{
}

/**
 * gdk_content_provider_new_for_bytes:
 * @mime_type: the mime type
 * @bytes: (transfer none): a #GBytes with the data for @mime_type
 *
 * Create a content provider that provides the given @bytes as data for
 * the given @mime_type.
 *
 * Returns: a new #GdkContentProvider
 **/
GdkContentProvider *
gdk_content_provider_new_for_bytes (const char *mime_type,
                                    GBytes     *bytes)
{
  GdkContentProviderBytes *content;

  g_return_val_if_fail (mime_type != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  content = g_object_new (GDK_TYPE_CONTENT_PROVIDER_BYTES, NULL);
  content->mime_type = g_intern_string (mime_type);
  content->bytes = g_bytes_ref (bytes);
  
  return GDK_CONTENT_PROVIDER (content);
}
