/*
 * Copyright (c) 2024 Florian "sp1rit" <sp1rit@disroot.org>
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

#include <glib/gi18n.h>

#include "gdkcontentproviderprivate.h"

#include "gdkandroidinit-private.h"
#include "gdkandroidutils-private.h"
#include "gdkandroidcontentfile-private.h"

#include "gdkandroidclipboard-private.h"

void
_gdk_android_clipboard_on_clipboard_changed (JNIEnv *env, jobject this)
{
  GdkAndroidClipboard *self = (GdkAndroidClipboard *) (*env)->GetLongField (env, this,
                                                                            gdk_android_get_java_cache ()->clipboard_provider_change_listener.native_ptr);
  g_return_if_fail (GDK_IS_ANDROID_CLIPBOARD (self));

  gdk_android_clipboard_update_remote_formats (self);
}

typedef struct _GdkAndroidClipboardClass
{
  GdkClipboardClass parent_class;
} GdkAndroidClipboardClass;

G_DEFINE_TYPE (GdkAndroidClipboard, gdk_android_clipboard, GDK_TYPE_CLIPBOARD)

static void
gdk_android_clipboard_finalize (GObject *object)
{
  GdkAndroidClipboard *self = (GdkAndroidClipboard *) object;

  JNIEnv *env = gdk_android_get_env ();
  (*env)->SetLongField (env, self->listener,
                        gdk_android_get_java_cache ()->clipboard_provider_change_listener.native_ptr,
                        0);
  (*env)->CallVoidMethod (env, self->manager,
                          gdk_android_get_java_cache ()->a_clipboard_manager.remove_change_listener,
                          self->listener);
  (*env)->DeleteGlobalRef (env, self->listener);
  (*env)->DeleteGlobalRef (env, self->manager);

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_object_unref (self->cancellable);
    }
  G_OBJECT_CLASS (gdk_android_clipboard_parent_class)->finalize (object);
}


// The entire gdk_android_clipboard_provider_write_or_serialize_* family
// of functions should be removed once (and if) GDK provides unified
// handling of Clipboard/DnD content (likely via the Gdk.ContentProvider).

static void
gdk_android_clipboard_provider_write_or_serialize_write_done (GObject      *content,
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

static void
gdk_android_clipboard_provider_write_or_serialize_serialize_done (GObject      *content,
                                                                  GAsyncResult *result,
                                                                  gpointer      task)
{
  GError *error = NULL;

  if (gdk_content_serialize_finish (result, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);

  g_object_unref (task);
}

static void
gdk_android_clipboard_provider_write_or_serialize_async (GdkContentProvider  *provider,
                                                         const char          *mime_type,
                                                         GOutputStream       *stream,
                                                         int                  io_priority,
                                                         GCancellable        *cancellable,
                                                         GAsyncReadyCallback  callback,
                                                         gpointer             user_data)
{
  GdkContentFormats *formats, *mime_formats;
  GTask *task;
  GType gtype;

  g_return_if_fail (GDK_IS_CONTENT_PROVIDER (provider));
  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (mime_type == g_intern_string (mime_type));
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_android_clipboard_provider_write_or_serialize_async);

  formats = gdk_content_provider_ref_formats (provider);
  if (gdk_content_formats_contain_mime_type (formats, mime_type))
    {
      gdk_content_provider_write_mime_type_async (provider,
                                                  mime_type,
                                                  stream,
                                                  io_priority,
                                                  cancellable,
                                                  gdk_android_clipboard_provider_write_or_serialize_write_done,
                                                  task);
      gdk_content_formats_unref (formats);
      return;
    }

  mime_formats = gdk_content_formats_new ((const char *[2]) { mime_type, NULL }, 1);
  mime_formats = gdk_content_formats_union_serialize_gtypes (mime_formats);
  gtype = gdk_content_formats_match_gtype (formats, mime_formats);
  if (gtype != G_TYPE_INVALID)
    {
      GValue value = G_VALUE_INIT;
      GError *error = NULL;

      g_assert (gtype != G_TYPE_INVALID);

      g_value_init (&value, gtype);
      if (gdk_content_provider_get_value (provider, &value, &error))
        {
          gdk_content_serialize_async (stream,
                                       mime_type,
                                       &value,
                                       io_priority,
                                       cancellable,
                                       gdk_android_clipboard_provider_write_or_serialize_serialize_done,
                                       g_object_ref (task));
        }
      else
        {
          g_task_return_error (task, error);
        }

      g_value_unset (&value);
    }
  else
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible formats to transfer contents of content provider."));
    }

  gdk_content_formats_unref (mime_formats);
  gdk_content_formats_unref (formats);
  g_object_unref (task);
}

static gboolean
gdk_android_clipboard_provider_write_or_serialize_finish (GdkContentProvider *provider,
                                                          GAsyncResult       *result,
                                                          GError            **error)
{
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_android_clipboard_provider_write_or_serialize_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}


typedef struct {
  JNIEnv *env;
  jobject context;
  GMemoryOutputStream *uris;
  GMemoryOutputStream *html;
  GMemoryOutputStream *text;
  guint pending;
} GdkAndroidClipboardWriteMgr;

static void
gdk_android_clipboard_write_mgr_free (GdkAndroidClipboardWriteMgr *self)
{
  (*self->env)->DeleteGlobalRef (self->env, self->context);
  if (self->uris)
    g_object_unref (self->uris);
  if (self->html)
    g_object_unref (self->html);
  if (self->text)
    g_object_unref (self->text);
  g_free (self);
}

static void
gdk_android_clipboard_provider_write_cb (GdkContentProvider *provider, GAsyncResult *res, GTask *task)
{
  GdkAndroidClipboardWriteMgr *mgr = g_task_get_task_data (task);
  gint pending = --mgr->pending;

  GError *err = NULL;
  if (!gdk_android_clipboard_provider_write_or_serialize_finish(provider, res, &err))
    {
      g_critical ("Failed to retrieve clipboard data: %s", err->message);
      g_error_free (err);
    }

  if (pending)
    return;

  JNIEnv *env = mgr->env;
  (*env)->PushLocalFrame (env, 3);
  jobject clipdata = NULL;

  jobject resolver = (*env)->CallObjectMethod (env, mgr->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);
  jstring text = mgr->text ? gdk_android_utf8n_to_java (g_memory_output_stream_get_data (mgr->text),
                                                        g_memory_output_stream_get_data_size (mgr->text)) : NULL;

  if (mgr->uris)
    {
      const gchar *buf = g_memory_output_stream_get_data (mgr->uris);
      gsize len = g_memory_output_stream_get_data_size (mgr->uris);
      gsize prev = 0;
      for (gsize i = 0; i < len; i++)
        {
          if (buf[i] == '\r' && (i+1) < len && buf[i+1] == '\n' && prev <= i && buf[prev] != '#')
            {
              jstring uri_string = gdk_android_utf8n_to_java (&buf[prev], i - prev);
              jobject uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_uri.klass,
                                                            gdk_android_get_java_cache ()->a_uri.parse,
                                                            uri_string);
              if (clipdata)
                {
                  jobject item = (*env)->NewObject (env, gdk_android_get_java_cache ()->a_clipdata_item.klass,
                                                    gdk_android_get_java_cache ()->a_clipdata_item.constructor_uri,
                                                    uri);
                  (*env)->CallVoidMethod (env, clipdata,
                                          gdk_android_get_java_cache ()->a_clipdata.add_item,
                                          resolver, item);
                }
              else
                {
                  clipdata = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_clipdata.klass,
                                                             gdk_android_get_java_cache ()->a_clipdata.new_uri,
                                                             resolver, text ? text : uri_string, uri);
                }
              prev = i+2;
            }
        }
    }

  if (mgr->html && text)
    {
      jstring html = gdk_android_utf8n_to_java (g_memory_output_stream_get_data (mgr->html),
                                                g_memory_output_stream_get_size (mgr->html));
      if (clipdata)
        {
          jobject item = (*env)->NewObject (env, gdk_android_get_java_cache ()->a_clipdata_item.klass,
                                            gdk_android_get_java_cache ()->a_clipdata_item.constructor_html,
                                            text, html);
          (*env)->CallVoidMethod (env, clipdata,
                                  gdk_android_get_java_cache ()->a_clipdata.add_item,
                                  resolver, item);
        }
      else
        {
          clipdata = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_clipdata.klass,
                                                     gdk_android_get_java_cache ()->a_clipdata.new_html,
                                                     text, text, html);
        }
    }
  else if (text)
    {
      if (clipdata)
        {
          jobject item = (*env)->NewObject (env, gdk_android_get_java_cache ()->a_clipdata_item.klass,
                                            gdk_android_get_java_cache ()->a_clipdata_item.constructor_text,
                                            text);
          (*env)->CallVoidMethod (env, clipdata,
                                  gdk_android_get_java_cache ()->a_clipdata.add_item,
                                  resolver, item);

        }
      else
        {
          clipdata = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_clipdata.klass,
                                                     gdk_android_get_java_cache ()->a_clipdata.new_plain_text,
                                                     text, text);
        }
    }

  if (clipdata)
    g_task_return_pointer (task,
                           (*env)->NewGlobalRef(env, clipdata),
                           gdk_android_utils_unref_jobject);
  else
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "No compatible clipboard transfer format found (currently only plaintext & URIs is supported)");
  g_object_unref (task);
  (*env)->PopLocalFrame (env, NULL);
}

void
gdk_android_clipboard_clipdata_from_provider_async (GdkContentProvider *provider,
                                                    GdkContentFormats  *formats,
                                                    jobject             context,
                                                    GCancellable       *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer            user_data)
{
  GTask *task = g_task_new (provider, cancellable, callback, user_data);

  JNIEnv *env = gdk_android_get_env();

  GdkAndroidClipboardWriteMgr *mgr = g_new0 (GdkAndroidClipboardWriteMgr, 1);
  mgr->env = env;
  mgr->context = (*env)->NewGlobalRef (env, context);
  g_task_set_task_data(task, mgr, (GDestroyNotify)gdk_android_clipboard_write_mgr_free);

  gsize n_mimes;
  const gchar *const *mimes = gdk_content_formats_get_mime_types (formats, &n_mimes);
  for (gsize i = 0; i < n_mimes; i++) {
    if (!mgr->text &&
        (g_strcmp0 (mimes[i], "text/plain;charset=utf-8") == 0 ||
         g_strcmp0 (mimes[i], "text/plain") == 0)
    ) {
      mgr->text = (GMemoryOutputStream *)g_memory_output_stream_new_resizable ();
      ++mgr->pending;
      gdk_android_clipboard_provider_write_or_serialize_async(provider,
                                                              mimes[i],
                                                              (GOutputStream *)mgr->text,
                                                              G_PRIORITY_HIGH,
                                                              cancellable,
                                                              (GAsyncReadyCallback)gdk_android_clipboard_provider_write_cb,
                                                              task);
    } else if (!mgr->html && g_strcmp0 (mimes[i], "text/html") == 0) {
      mgr->html = (GMemoryOutputStream *)g_memory_output_stream_new_resizable ();
      ++mgr->pending;
      gdk_android_clipboard_provider_write_or_serialize_async(provider,
                                                              mimes[i],
                                                              (GOutputStream *)mgr->html,
                                                              G_PRIORITY_HIGH,
                                                              cancellable,
                                                              (GAsyncReadyCallback)gdk_android_clipboard_provider_write_cb,
                                                              task);
    } else if (!mgr->uris && g_strcmp0 (mimes[i], "text/uri-list") == 0) {
      mgr->uris = (GMemoryOutputStream *)g_memory_output_stream_new_resizable ();
      ++mgr->pending;
      gdk_android_clipboard_provider_write_or_serialize_async(provider,
                                                              mimes[i],
                                                              (GOutputStream *)mgr->uris,
                                                              G_PRIORITY_HIGH,
                                                              cancellable,
                                                              (GAsyncReadyCallback)gdk_android_clipboard_provider_write_cb,
                                                              task);
    }
  }

  if (n_mimes == 0) {
    (*env)->PushLocalFrame (env, 1);
    jobject clipdata = (*env)->NewObject (env, gdk_android_get_java_cache ()->clipboard_internal_clipdata.klass,
                                          gdk_android_get_java_cache ()->clipboard_internal_clipdata.constructor);
    g_task_return_pointer (task,
                           (*env)->NewGlobalRef(env, clipdata),
                           gdk_android_utils_unref_jobject);
    (*env)->PopLocalFrame (env, NULL);
    g_object_unref (task);
  }
}

jobject
gdk_android_clipboard_clipdata_from_provider_finish (GdkContentProvider *provider,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  g_return_val_if_fail (g_task_is_valid (result, provider), NULL);
  jobject ret = g_task_propagate_pointer (G_TASK (result), error);
  if (ret) {
    JNIEnv *env = gdk_android_get_env ();
    jobject global = ret;
    ret = (*env)->NewLocalRef (env, global);
    (*env)->DeleteGlobalRef (env, NULL);
  }
  return ret;
}

static void
gdk_android_clipboard_from_provider_cb (GdkContentProvider  *provider,
                                        GAsyncResult        *res,
                                        GdkAndroidClipboard *self)
{
  JNIEnv *env = gdk_android_get_env ();
  GError *err = NULL;
  (*env)->PushLocalFrame (env, 1);
  jobject clipdata = gdk_android_clipboard_clipdata_from_provider_finish (provider, res, &err);
  if (!clipdata)
    {
      g_critical ("Failed producing clipdata: %s", err->message);
      g_error_free (err);
      goto exit;
    }
  (*env)->CallVoidMethod (env, self->manager,
                          gdk_android_get_java_cache ()->a_clipboard_manager.set_primary_clip,
                          clipdata);
  if (gdk_android_check_exception (&err))
    {
      g_critical ("Failed to set clipboard: %s", err->message);
      g_error_free (err);
    }
exit:
  (*env)->PopLocalFrame (env, NULL);
  g_object_unref (self);
}

static void
gdk_android_clipboard_send_to_remote (GdkAndroidClipboard *self,
                                      GdkContentFormats   *formats,
                                      GdkContentProvider  *content)
{
  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_object_unref (self->cancellable);
    }
  self->cancellable = g_cancellable_new ();

  gdk_android_clipboard_clipdata_from_provider_async (content,
                                                      formats,
                                                      gdk_android_get_activity (),
                                                      self->cancellable,
                                                      (GAsyncReadyCallback)gdk_android_clipboard_from_provider_cb,
                                                      g_object_ref (self));
}

static gboolean
gdk_android_clipboard_claim (GdkClipboard       *clipboard,
                             GdkContentFormats  *formats,
                             gboolean            local,
                             GdkContentProvider *content)
{
  GdkAndroidClipboard *self = (GdkAndroidClipboard *) clipboard;

  gboolean ret = GDK_CLIPBOARD_CLASS (gdk_android_clipboard_parent_class)->claim (clipboard, formats, local, content);

  if (local)
    gdk_android_clipboard_send_to_remote (self, formats, content);

  return ret;
}

static void
gdk_android_clipboard_store_async (GdkClipboard       *clipboard,
                                   int                 io_priority,
                                   GCancellable       *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer            user_data)
{
  g_debug ("store async called");
}

static gboolean
gdk_android_clipboard_store_finish (GdkClipboard *clipboard,
                                    GAsyncResult *result,
                                    GError      **error)
{
  g_debug ("store finish");
  return FALSE;
}

static gboolean
gdk_android_clipboard_string_to_task_result (GTask       *task,
                                             const gchar *mimetype,
                                             jstring      string)
{
  if (string == NULL)
    return FALSE;
  gsize len;
  gchar *content = gdk_android_java_to_utf8 (string, &len);
  GInputStream *stream = g_memory_input_stream_new_from_data (content, len, g_free);
  g_task_set_task_data (task, g_strdup (mimetype), g_free);
  g_task_return_pointer (task, stream, g_object_unref);
  return TRUE;
}

static void
gdk_android_clipboard_url_to_task_result (GTask  *task,
                                          jstring mimetype,
                                          jobject resolver,
                                          jobject uri)
{
  GError *error = NULL;
  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 4);
  jobject descriptor = (*env)->CallObjectMethod (env, resolver,
                                                 gdk_android_get_java_cache ()->a_content_resolver.open_typed_asset_fd,
                                                 uri, mimetype, NULL, NULL);
  if (gdk_android_content_file_has_exception (env, &error))
    {
      g_task_return_error (task, error);
      (*env)->PopLocalFrame (env, NULL);
      return;
    }
  jobject istream = (*env)->CallObjectMethod (env, descriptor,
                                              gdk_android_get_java_cache ()->a_asset_fd.create_istream);
  if (gdk_android_content_file_has_exception (env, &error))
    {
      g_task_return_error (task, error);
      (*env)->PopLocalFrame (env, NULL);
      return;
    }
  GFileInputStream *stream = gdk_android_java_file_input_stream_wrap (env, istream);
  g_task_set_task_data (task, gdk_android_java_to_utf8 (mimetype, NULL), g_free);
  g_task_return_pointer (task, stream, g_object_unref);
  (*env)->PopLocalFrame (env, NULL);
}

void
gdk_android_clipdata_read_async (GTask              *task,
                                 jobject             clipdata,
                                 GdkContentFormats  *formats)
{
  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 5);

  jobject resolver = (*env)->CallObjectMethod (env, gdk_android_get_activity (),
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);

  gsize n_mimes;
  const gchar *const *mimes = gdk_content_formats_get_mime_types (formats, &n_mimes);

  jint n_items = (*env)->CallIntMethod (env, clipdata,
                                        gdk_android_get_java_cache ()->a_clipdata.get_item_count);
  for (jint i = 0; i < n_items; i++)
    {
      jobject item = (*env)->CallObjectMethod (env, clipdata,
                                               gdk_android_get_java_cache ()->a_clipdata.get_item, i);
      for (gsize j = 0; j < n_mimes; j++)
        {
          if (g_strcmp0 (mimes[j], "text/plain;charset=utf-8") == 0 ||
              g_strcmp0 (mimes[j], "text/plain") == 0)
            {
              jobject text = (*env)->CallObjectMethod (env, item,
                                                       gdk_android_get_java_cache ()->a_clipdata_item.coerce_to_text,
                                                       gdk_android_get_activity ());
              if (!text)
                break;
              jstring text_str = (*env)->CallObjectMethod (env, text,
                                                           gdk_android_get_java_cache ()->j_object.to_string);
              gdk_android_clipboard_string_to_task_result (task,
                                                           "text/plain;charset=utf-8",
                                                           text_str);
              goto cleanup;
            }
          else if (g_strcmp0 (mimes[j], "text/html") == 0)
            {
              jstring html = (*env)->CallObjectMethod (env, item,
                                                       gdk_android_get_java_cache ()->a_clipdata_item.get_html,
                                                       gdk_android_get_activity ());
              if (!gdk_android_clipboard_string_to_task_result (task,
                                                                "text/html;charset=utf-8",
                                                                html))
                break;
              goto cleanup;
            }
          else
            {
              jobject uri = (*env)->CallObjectMethod (env, item,
                                                      gdk_android_get_java_cache ()->a_clipdata_item.get_uri);
              if (uri)
                {
                  jstring type = (*env)->CallObjectMethod (env, resolver,
                                                           gdk_android_get_java_cache ()->a_content_resolver.get_type,
                                                           uri);
                  jstring mime = (*env)->NewStringUTF (env, mimes[j]); // I'm making the assumption, that
                                                                       // mimetypes won't contain high codepoints.
                  if ((*env)->CallBooleanMethod (env, mime,
                                                 gdk_android_get_java_cache ()->j_object.equals,
                                                 type))
                    {
                      gdk_android_clipboard_url_to_task_result (task,
                                                                type,
                                                                resolver,
                                                                uri);
                      goto cleanup;
                    }
                  (*env)->DeleteLocalRef (env, mime);
                  (*env)->DeleteLocalRef (env, uri);
                }
            }
        }

      (*env)->DeleteLocalRef (env, item);
    }

  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s",
                           _("No compatible transfer format found"));

cleanup:
  (*env)->PopLocalFrame (env, NULL);
}

static void
gdk_android_clipboard_read_async (GdkClipboard       *clipboard,
                                  GdkContentFormats  *formats,
                                  int                 io_priority,
                                  GCancellable       *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer            user_data)
{
  GdkAndroidClipboard *self = (GdkAndroidClipboard *) clipboard;

  GTask *task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_android_clipboard_read_async);
  g_task_set_priority (task, io_priority);

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 1);

  jobject clip = (*env)->CallObjectMethod (env, self->manager,
                                           gdk_android_get_java_cache ()->a_clipboard_manager.get_primary_clip);
  if (!clip)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_PERMISSION_DENIED,
                               "Unable to access clipboard data");
      goto cleanup;
    }

  gdk_android_clipdata_read_async (task, clip, formats);

cleanup:
  (*env)->PopLocalFrame (env, NULL);
  g_clear_object (&task);
}

GInputStream *
gdk_android_clipdata_read_finish (GAsyncResult *result,
                                  const char  **out_mime_type,
                                  GError      **error)
{
  GTask *task = (GTask *) result;
  if (out_mime_type != NULL)
    *out_mime_type = g_task_get_task_data (task);
  return g_task_propagate_pointer (task, error);
}

static GInputStream *
gdk_android_clipboard_read_finish (GdkClipboard *clipboard,
                                   GAsyncResult *result,
                                   const char  **out_mime_type,
                                   GError      **error)
{
  return gdk_android_clipdata_read_finish (result, out_mime_type, error);
}

static void
gdk_android_clipboard_class_init (GdkAndroidClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (klass);

  object_class->finalize = gdk_android_clipboard_finalize;
  clipboard_class->claim = gdk_android_clipboard_claim;
  clipboard_class->store_async = gdk_android_clipboard_store_async;
  clipboard_class->store_finish = gdk_android_clipboard_store_finish;
  clipboard_class->read_async = gdk_android_clipboard_read_async;
  clipboard_class->read_finish = gdk_android_clipboard_read_finish;
}

static void
gdk_android_clipboard_init (GdkAndroidClipboard *self)
{
  self->cancellable = NULL;

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 2);
  jobject clipboard_mgr = (*env)->CallObjectMethod (env, gdk_android_get_activity (),
                                                    gdk_android_get_java_cache ()->a_context.get_system_service,
                                                    gdk_android_get_java_cache ()->a_context.clipboard_service);
  self->manager = (*env)->NewGlobalRef (env, clipboard_mgr);

  jobject listener = (*env)->NewObject (env, gdk_android_get_java_cache ()->clipboard_provider_change_listener.klass,
                                        gdk_android_get_java_cache ()->clipboard_provider_change_listener.constructor,
                                        (jlong) self);
  self->listener = (*env)->NewGlobalRef (env, listener);
  (*env)->CallVoidMethod (env, self->manager,
                          gdk_android_get_java_cache ()->a_clipboard_manager.add_change_listener,
                          self->listener);
  (*env)->PopLocalFrame (env, NULL);

  gdk_android_clipboard_update_remote_formats (self);
}

GdkClipboard *
gdk_android_clipboard_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_ANDROID_CLIPBOARD, "display", display, NULL);
}

GdkContentFormats *
gdk_android_clipboard_description_to_formats (jobject clipdesc)
{
  GdkContentFormatsBuilder *builder = gdk_content_formats_builder_new ();
  if (clipdesc != NULL)
    {
      JNIEnv *env = gdk_android_get_env ();
      (*env)->PushLocalFrame (env, 1);

      jint n_mimes = (*env)->CallIntMethod (env, clipdesc,
                                            gdk_android_get_java_cache ()->a_clip_desc.get_mime_type_count);
      for (jint i = 0; i < n_mimes; i++)
        {
          jstring mime = (*env)->CallObjectMethod (env, clipdesc,
                                                   gdk_android_get_java_cache ()->a_clip_desc.get_mime_type, i);
          const char *utf = (*env)->GetStringUTFChars (env, mime, NULL); // this breaks on high codepoints
          if (g_strcmp0(utf, "text/plain") == 0) {
            // Consider all Android text/plain as UTF-8 (it comes in UTF-16, but we'll convert it)
            gdk_content_formats_builder_add_mime_type (builder, "text/plain;charset=utf-8");
          }
          gdk_content_formats_builder_add_mime_type (builder, utf);
          (*env)->ReleaseStringUTFChars (env, mime, utf);

          (*env)->DeleteLocalRef (env, mime);
        }

      (*env)->PopLocalFrame (env, NULL);
    }

  return gdk_content_formats_builder_free_to_formats (builder);
}

void
gdk_android_clipboard_update_remote_formats (GdkAndroidClipboard *self)
{
  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 1);
  jobject desc = (*env)->CallObjectMethod (env, self->manager,
                                           gdk_android_get_java_cache ()->a_clipboard_manager.get_clip_desc);
  GdkContentFormats *formats = gdk_android_clipboard_description_to_formats(desc);
  (*env)->PopLocalFrame (env, NULL);
  gdk_clipboard_claim_remote ((GdkClipboard *) self, formats);
  gdk_content_formats_unref (formats);
}
