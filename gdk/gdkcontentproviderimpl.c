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

#include <gobject/gvaluecollector.h>

#include "gdkcontentformats.h"
#include "gdkcontentserializer.h"
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

/**
 * gdk_content_provider_new_typed:
 * @type: Type of value to follow
 * ...: value
 *
 * Create a content provider that provides the value of the given
 * @type.
 *
 * The value is provided using G_VALUE_COLLECT(), so the same rules
 * apply as when calling g_object_new() or g_object_set().
 *
 * Returns: a new #GdkContentProvider
 **/
GdkContentProvider *
gdk_content_provider_new_typed (GType type,
                                ...)
{
  GdkContentProviderValue *content;
  va_list args;
  char *error;

  content = g_object_new (GDK_TYPE_CONTENT_PROVIDER_VALUE, NULL);

  va_start (args, type);
  G_VALUE_COLLECT_INIT (&content->value, type, args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);
      g_free (error);
      /* we purposely leak the value here, it might not be
       * in a sane state if an error condition occoured
       */
    }
  va_end (args);

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

#define GDK_TYPE_CONTENT_PROVIDER_CALLBACK            (gdk_content_provider_callback_get_type ())
#define GDK_CONTENT_PROVIDER_CALLBACK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CONTENT_PROVIDER_CALLBACK, GdkContentProviderCallback))

typedef struct _GdkContentProviderCallback GdkContentProviderCallback;
typedef struct _GdkContentProviderCallbackClass GdkContentProviderCallbackClass;

struct _GdkContentProviderCallback
{
  GdkContentProvider parent;

  GType type;
  GdkContentProviderGetValueFunc func;
  gpointer data;
  GDestroyNotify notify;
};

struct _GdkContentProviderCallbackClass
{
  GdkContentProviderClass parent_class;
};

GType gdk_content_provider_callback_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkContentProviderCallback, gdk_content_provider_callback, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gdk_content_provider_callback_ref_formats (GdkContentProvider *provider)
{
  GdkContentProviderCallback *callback = GDK_CONTENT_PROVIDER_CALLBACK (provider);

  return gdk_content_formats_new_for_gtype (callback->type);
}

static gboolean
gdk_content_provider_callback_get_value (GdkContentProvider  *provider,
                                         GValue              *value,
                                         GError             **error)
{
  GdkContentProviderCallback *callback = GDK_CONTENT_PROVIDER_CALLBACK (provider);

  if (G_VALUE_HOLDS (value, callback->type) && callback->func != NULL)
    {
      callback->func (value, callback->data);
      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (gdk_content_provider_callback_parent_class)->get_value (provider, value, error);
}

static void
gdk_content_provider_callback_dispose (GObject *gobject)
{
  GdkContentProviderCallback *self = GDK_CONTENT_PROVIDER_CALLBACK (gobject);

  if (self->notify != NULL)
    self->notify (self->data);

  self->func = NULL;
  self->data = NULL;
  self->notify = NULL;

  G_OBJECT_CLASS (gdk_content_provider_callback_parent_class)->dispose (gobject);
}

static void
gdk_content_provider_callback_class_init (GdkContentProviderCallbackClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = gdk_content_provider_callback_dispose;

  provider_class->ref_formats = gdk_content_provider_callback_ref_formats;
  provider_class->get_value = gdk_content_provider_callback_get_value;
}

static void
gdk_content_provider_callback_init (GdkContentProviderCallback *content)
{
}

/**
 * gdk_content_provider_new_for_callback: (constructor)
 * @type: the type that the callback provides
 * @func: (not nullable): callback to populate a #GValue
 * @data: (closure): data that gets passed to @func
 * @notify: a function to be called to free @data when the content provider
 *   goes away
 *
 * Create a content provider that provides data that is provided via a callback.
 *
 * Returns: a new #GdkContentProvider
 **/
GdkContentProvider *
gdk_content_provider_new_with_callback (GType                          type,
                                        GdkContentProviderGetValueFunc func,
                                        gpointer                       data,
                                        GDestroyNotify                 notify)
{
  GdkContentProviderCallback *content;

  g_return_val_if_fail (func != NULL, NULL);

  content = g_object_new (GDK_TYPE_CONTENT_PROVIDER_CALLBACK, NULL);
  content->type = type;
  content->func = func;
  content->data = data;
  content->notify = notify;
  
  return GDK_CONTENT_PROVIDER (content);
}

#define GDK_TYPE_CONTENT_PROVIDER_CALLBACK2            (gdk_content_provider_callback2_get_type ())
#define GDK_CONTENT_PROVIDER_CALLBACK2(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CONTENT_PROVIDER_CALLBACK2, GdkContentProviderCallback2))

typedef struct _GdkContentProviderCallback2 GdkContentProviderCallback2;
typedef struct _GdkContentProviderCallback2Class GdkContentProviderCallback2Class;

struct _GdkContentProviderCallback2
{
  GdkContentProvider parent;

  GdkContentFormats *formats;
  GdkContentProviderGetBytesFunc func;
  gpointer data;
  GDestroyNotify notify;
};

struct _GdkContentProviderCallback2Class
{
  GdkContentProviderClass parent_class;
};

GType gdk_content_provider_callback2_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkContentProviderCallback2, gdk_content_provider_callback2, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gdk_content_provider_callback2_ref_formats (GdkContentProvider *provider)
{
  GdkContentProviderCallback2 *callback = GDK_CONTENT_PROVIDER_CALLBACK2 (provider);

  return gdk_content_formats_ref (callback->formats);
}

static void
gdk_content_provider_callback2_write_mime_type_done (GObject      *stream,
                                                     GAsyncResult *result,
                                                     gpointer      task)
{
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (G_OUTPUT_STREAM (stream), result, NULL, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}

static void
gdk_content_provider_callback2_write_mime_type_async (GdkContentProvider     *provider,
                                                      const char             *mime_type,
                                                      GOutputStream          *stream,
                                                      int                     io_priority,
                                                      GCancellable           *cancellable,
                                                      GAsyncReadyCallback     callback,
                                                      gpointer                user_data)
{
  GdkContentProviderCallback2 *content = GDK_CONTENT_PROVIDER_CALLBACK2 (provider);
  GTask *task;
  GBytes *bytes;

  task = g_task_new (content, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_content_provider_callback2_write_mime_type_async);

  if (!gdk_content_formats_contain_mime_type (content->formats, mime_type))
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("Cannot provide contents as “%s”"), mime_type);
      g_object_unref (task);
      return;
    }

  bytes = content->func (mime_type, content->data);
  if (!bytes)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("Failed to get contents as “%s”"), mime_type);
      g_object_unref (task);
      return;
    }

  g_object_set_data_full (G_OBJECT (task), "bytes", bytes, (GDestroyNotify)g_bytes_unref);

  g_output_stream_write_all_async (stream,
                                   g_bytes_get_data (bytes, NULL),
                                   g_bytes_get_size (bytes),
                                   io_priority,
                                   cancellable,
                                   gdk_content_provider_callback2_write_mime_type_done,
                                   task);
}

static gboolean
gdk_content_provider_callback2_write_mime_type_finish (GdkContentProvider *provider,
                                                       GAsyncResult       *result,
                                                       GError            **error)
{
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_content_provider_callback2_write_mime_type_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gdk_content_provider_callback2_dispose (GObject *gobject)
{
  GdkContentProviderCallback2 *self = GDK_CONTENT_PROVIDER_CALLBACK2 (gobject);

  if (self->notify != NULL)
    self->notify (self->data);

  self->notify = NULL;
  self->data = NULL;
  self->func = NULL;

  G_OBJECT_CLASS (gdk_content_provider_callback2_parent_class)->dispose (gobject);
}

static void
gdk_content_provider_callback2_class_init (GdkContentProviderCallback2Class *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = gdk_content_provider_callback2_dispose;

  provider_class->ref_formats = gdk_content_provider_callback2_ref_formats;
  provider_class->write_mime_type_async = gdk_content_provider_callback2_write_mime_type_async;
  provider_class->write_mime_type_finish = gdk_content_provider_callback2_write_mime_type_finish;
}

static void
gdk_content_provider_callback2_init (GdkContentProviderCallback2 *content)
{
}

/**
 * gdk_content_provider_new_with_formats:
 * @formats: formats to advertise
 * @func: callback to populate a #GValue
 * @data: (closure func): data that gets passed to @func
 * @notify: a function called to free @data when the content provider
 *   goes away
 *
 * Create a content provider that provides data that is provided via a callback.
 *
 * Returns: a new #GdkContentProvider
 **/
GdkContentProvider *
gdk_content_provider_new_with_formats (GdkContentFormats              *formats,
                                       GdkContentProviderGetBytesFunc  func,
                                       gpointer                        data,
                                       GDestroyNotify                  notify)
{
  GdkContentProviderCallback2 *content;
  content = g_object_new (GDK_TYPE_CONTENT_PROVIDER_CALLBACK2, NULL);
  content->formats = gdk_content_formats_union_serialize_mime_types (gdk_content_formats_ref (formats));
  content->func = func;
  content->data = data;
  content->notify = notify;
  
  return GDK_CONTENT_PROVIDER (content);
}
