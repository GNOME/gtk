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

#include "gdkandroidinit-private.h"

#include "gdkandroidutils-private.h"
#include "gdkandroiddevice-private.h"

#include "gdkandroidcontentfile-private.h"

#define GDK_ANDROID_CONTENT_CHECK_EXCEPTION(eclass, domain, code)                            \
  if ((*env)->IsInstanceOf (env,                                                             \
                            exception,                                                       \
                            gdk_android_get_java_cache ()->j_exceptions.eclass##_exception)) \
    {                                                                                        \
      *err = g_error_new ((domain), (code), "%s", cmsg);                                     \
      goto fin;                                                                              \
    }

gboolean
gdk_android_content_file_has_exception (JNIEnv *env, GError **err)
{
  (*env)->PushLocalFrame (env, 2);

  jthrowable exception = (*env)->ExceptionOccurred (env);
  if (exception)
    {
      (*env)->ExceptionClear (env);

      if (err)
        {
          jstring msg = (*env)->CallObjectMethod (env, exception,
                                                  gdk_android_get_java_cache ()->j_throwable.get_message);
          gchar *cmsg = gdk_android_java_to_utf8 (msg, NULL);

          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (eof,
                                               G_IO_ERROR,
                                               G_IO_ERROR_BROKEN_PIPE)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (not_found,
                                               G_IO_ERROR,
                                               G_IO_ERROR_NOT_FOUND)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (access_denied,
                                               G_IO_ERROR,
                                               G_IO_ERROR_PERMISSION_DENIED)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (not_empty,
                                               G_IO_ERROR,
                                               G_IO_ERROR_NOT_EMPTY)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (exists,
                                               G_IO_ERROR,
                                               G_IO_ERROR_EXISTS)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (loop,
                                               G_IO_ERROR,
                                               G_IO_ERROR_PERMISSION_DENIED)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (no_file,
                                               G_IO_ERROR,
                                               G_IO_ERROR_WOULD_RECURSE)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (not_dir,
                                               G_IO_ERROR,
                                               G_IO_ERROR_NOT_DIRECTORY)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (malformed_uri,
                                               G_IO_ERROR,
                                               G_IO_ERROR_INVALID_FILENAME)
          GDK_ANDROID_CONTENT_CHECK_EXCEPTION (channel_closed,
                                               G_IO_ERROR,
                                               G_IO_ERROR_CLOSED)

          *err = g_error_new (G_IO_ERROR, G_IO_ERROR_UNKNOWN, "%s", cmsg);
fin:
          g_free (cmsg);
        }
      (*env)->PopLocalFrame (env, NULL);
      return TRUE;
    }
  (*env)->PopLocalFrame (env, NULL);
  return FALSE;
}


#define GDK_ANDROID_JAVA_STREAM_CACHE_BUFFER_SIZE 4096

typedef struct _GdkAndroidJavaFileInputStreamClass
{
  GFileInputStreamClass parent_class;
} GdkAndroidJavaFileInputStreamClass;

typedef struct _GdkAndroidJavaFileInputStream
{
  GFileInputStream parent_instance;

  jbyteArray cached_buffer;
  jobject stream;
} GdkAndroidJavaFileInputStream;

G_DEFINE_TYPE (GdkAndroidJavaFileInputStream,
               gdk_android_java_file_input_stream,
               G_TYPE_FILE_INPUT_STREAM)

static void
gdk_android_java_file_input_stream_finalize (GObject *object)
{
  GdkAndroidJavaFileInputStream *self = (GdkAndroidJavaFileInputStream *)object;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  (*env)->DeleteGlobalRef (env, self->cached_buffer);
  (*env)->DeleteGlobalRef (env, self->stream);

  gdk_android_drop_thread_env (&guard);

  G_OBJECT_CLASS (gdk_android_java_file_input_stream_parent_class)->finalize (object);
}

static gboolean
gdk_android_java_file_input_stream_close (GInputStream *stream,
                                          GCancellable *cancellable,
                                          GError **error)
{
  GdkAndroidJavaFileInputStream *self = (GdkAndroidJavaFileInputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  (*env)->CallVoidMethod (env, self->stream,
                          gdk_android_get_java_cache ()->j_istream.close);

  gboolean ret = !gdk_android_content_file_has_exception (env, error);
  gdk_android_drop_thread_env (&guard);
  return ret;
}

static gssize
gdk_android_java_file_input_stream_read (GInputStream *stream,
                                         void *buffer, gsize count,
                                         GCancellable *cancellable,
                                         GError **error)
{
  GdkAndroidJavaFileInputStream *self = (GdkAndroidJavaFileInputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;
  (*env)->PushLocalFrame (env, 1);

  gssize total = 0;
  while (total < count)
    {
      jsize n_bytes = MIN (GDK_ANDROID_JAVA_STREAM_CACHE_BUFFER_SIZE, count - total); // !
      jint len = (*env)->CallIntMethod (env, self->stream,
                                        gdk_android_get_java_cache ()->j_istream.read,
                                        self->cached_buffer,
                                        0, n_bytes);

      if (gdk_android_content_file_has_exception (env, error))
        {
          (*env)->PopLocalFrame (env, NULL);
          gdk_android_drop_thread_env (&guard);
          return -1;
        }

      if (len == -1 || len == 0) // im not sure about len == 0
        goto exit;

      if (len > 0)
        (*env)->GetByteArrayRegion (env, self->cached_buffer, 0, len, &((jbyte *)buffer)[total]);
      total += len;
    }

exit:
  (*env)->PopLocalFrame (env, NULL);
  gdk_android_drop_thread_env (&guard);
  return total;
}

static gssize
gdk_android_java_file_input_stream_skip (GInputStream *stream,
                                         gsize count,
                                         GCancellable *cancellable,
                                         GError **error)
{
  GdkAndroidJavaFileInputStream *self = (GdkAndroidJavaFileInputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  jlong len = (*env)->CallLongMethod (env, self->stream,
                                      gdk_android_get_java_cache ()->j_istream.skip,
                                      count);

  gssize ret = gdk_android_content_file_has_exception (env, error) ? -1 : len;

  gdk_android_drop_thread_env (&guard);
  return ret;
}


static gboolean
gdk_android_java_file_input_stream_can_seek (GFileInputStream *stream)
{
  // is this always the case?
  return TRUE;
}

static gboolean
gdk_android_java_file_input_stream_seek (GFileInputStream *stream,
                                         goffset off, GSeekType type,
                                         GCancellable *cancellable,
                                         GError **error)
{
  GdkAndroidJavaFileInputStream *self = (GdkAndroidJavaFileInputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;
  (*env)->PushLocalFrame (env, 2);

  jobject channel = (*env)->CallObjectMethod (env, self->stream,
                                              gdk_android_get_java_cache ()->j_file_istream.get_channel);
  jlong new_position = 0;
  switch (type)
    {
      case G_SEEK_CUR:
        {
          jlong current = (*env)->CallLongMethod (env, channel,
                                                  gdk_android_get_java_cache ()->j_file_channel.get_position);
          new_position = current + off;
        }
        break;
      case G_SEEK_SET:
        new_position = off;
        break;
      case G_SEEK_END:
        {
          jlong size = (*env)->CallLongMethod (env, channel,
                                               gdk_android_get_java_cache ()->j_file_channel.get_size);
          new_position = size + off;
        }
        break;
      default:
        g_critical ("Encountered unknown seek type: %d", type);
    }

  (*env)->CallObjectMethod (env, channel,
                            gdk_android_get_java_cache ()->j_file_channel.set_position,
                            new_position);

  gboolean ret = !gdk_android_content_file_has_exception (env, error);
  (*env)->PopLocalFrame (env, NULL);
  gdk_android_drop_thread_env (&guard);
  return ret;
}

static goffset
gdk_android_java_file_input_stream_tell (GFileInputStream *stream)
{

  GdkAndroidJavaFileInputStream *self = (GdkAndroidJavaFileInputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;
  (*env)->PushLocalFrame (env, 1);

  jobject channel = (*env)->CallObjectMethod (env, self->stream,
                                              gdk_android_get_java_cache ()->j_file_istream.get_channel);
  jlong position = (*env)->CallLongMethod (env, channel,
                                           gdk_android_get_java_cache ()->j_file_channel.get_position);

  goffset ret = gdk_android_content_file_has_exception (env, NULL) ? -1 : position;
  (*env)->PopLocalFrame (env, NULL);
  gdk_android_drop_thread_env (&guard);
  return ret;
}

static void
gdk_android_java_file_input_stream_class_init (GdkAndroidJavaFileInputStreamClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GInputStreamClass *stream_class = (GInputStreamClass *)klass;
  GFileInputStreamClass *file_stream_class = (GFileInputStreamClass *)klass;

  object_class->finalize = gdk_android_java_file_input_stream_finalize;
  stream_class->close_fn = gdk_android_java_file_input_stream_close;
  stream_class->read_fn = gdk_android_java_file_input_stream_read;
  stream_class->skip = gdk_android_java_file_input_stream_skip;
  file_stream_class->can_seek = gdk_android_java_file_input_stream_can_seek;
  file_stream_class->seek = gdk_android_java_file_input_stream_seek;
  file_stream_class->tell = gdk_android_java_file_input_stream_tell;
}

static void
gdk_android_java_file_input_stream_init (GdkAndroidJavaFileInputStream *self)
{
  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  jbyteArray buffer = (*env)->NewByteArray (env, GDK_ANDROID_JAVA_STREAM_CACHE_BUFFER_SIZE);
  self->cached_buffer = (*env)->NewGlobalRef (env, buffer);
  (*env)->DeleteLocalRef (env, buffer);

  gdk_android_drop_thread_env (&guard);
}

GFileInputStream *
gdk_android_java_file_input_stream_wrap (JNIEnv *env, jobject file_input_stream)
{
  GdkAndroidJavaFileInputStream *self = g_object_new (GDK_TYPE_ANDROID_JAVA_FILE_INPUT_STREAM, NULL);
  self->stream = (*env)->NewGlobalRef (env, file_input_stream);
  return (GFileInputStream *)self;
}


typedef struct _GdkAndroidJavaFileOutputStreamClass
{
  GFileOutputStreamClass parent_class;
} GdkAndroidJavaFileOutputStreamClass;

typedef struct _GdkAndroidJavaFileOutputStream
{
  GFileOutputStream parent_instance;

  jbyteArray cached_buffer;
  jobject stream;
} GdkAndroidJavaFileOutputStream;

G_DEFINE_TYPE (GdkAndroidJavaFileOutputStream,
               gdk_android_java_file_output_stream,
               G_TYPE_FILE_OUTPUT_STREAM)

static void
gdk_android_java_file_output_stream_finalize (GObject *object)
{
  GdkAndroidJavaFileInputStream *self = (GdkAndroidJavaFileInputStream *)object;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  (*env)->DeleteGlobalRef (env, self->cached_buffer);
  (*env)->DeleteGlobalRef (env, self->stream);

  gdk_android_drop_thread_env (&guard);

  G_OBJECT_CLASS (gdk_android_java_file_output_stream_parent_class)->finalize (object);
}

static gboolean
gdk_android_java_file_output_stream_close (GOutputStream *stream,
                                           GCancellable *cancellable,
                                           GError **error)
{
  GdkAndroidJavaFileOutputStream *self = (GdkAndroidJavaFileOutputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  (*env)->CallVoidMethod (env, self->stream,
                          gdk_android_get_java_cache ()->j_ostream.close);

  gboolean ret = !gdk_android_content_file_has_exception (env, error);
  gdk_android_drop_thread_env (&guard);
  return ret;
}

static gboolean
gdk_android_java_file_output_stream_flush (GOutputStream *stream,
                                           GCancellable *cancellable,
                                           GError **error)
{
  GdkAndroidJavaFileOutputStream *self = (GdkAndroidJavaFileOutputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  (*env)->CallVoidMethod (env, self->stream,
                          gdk_android_get_java_cache ()->j_ostream.flush);

  gboolean ret = !gdk_android_content_file_has_exception (env, error);
  gdk_android_drop_thread_env (&guard);
  return ret;
}

static gssize
gdk_android_java_file_output_stream_write (GOutputStream *stream,
                                           const void* buffer, gsize count,
                                           GCancellable *cancellable,
                                           GError **error)
{
  GdkAndroidJavaFileOutputStream *self = (GdkAndroidJavaFileOutputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  gssize total = 0;
  while (total < count)
    {
      jsize n_bytes = MIN (GDK_ANDROID_JAVA_STREAM_CACHE_BUFFER_SIZE, count - total); // !
      (*env)->SetByteArrayRegion (env, self->cached_buffer, 0, n_bytes, &((jbyte *)buffer)[total]);
      (*env)->CallVoidMethod (env, self->stream,
                              gdk_android_get_java_cache ()->j_ostream.write,
                              self->cached_buffer,
                              0, n_bytes);

      if (gdk_android_content_file_has_exception (env, error))
        {
          gdk_android_drop_thread_env (&guard);
          return -1;
        }

      total += n_bytes;
    }

  gdk_android_drop_thread_env (&guard);
  return total;
}

static gboolean
gdk_android_java_file_output_stream_can_seek (GFileOutputStream *stream)
{
  // is this always the case?
  return TRUE;
}

static gboolean
gdk_android_java_file_output_stream_can_truncate (GFileOutputStream *stream)
{
  // is this always the case?
  return TRUE;
}

static gboolean
gdk_android_java_file_output_stream_seek (GFileOutputStream *stream,
                                          goffset off, GSeekType type,
                                          GCancellable *cancellable,
                                          GError **error)
{
  GdkAndroidJavaFileOutputStream *self = (GdkAndroidJavaFileOutputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;
  (*env)->PushLocalFrame (env, 2);

  jobject channel = (*env)->CallObjectMethod (env, self->stream,
                                              gdk_android_get_java_cache ()->j_file_ostream.get_channel);
  jlong new_position = 0;
  switch (type)
    {
      case G_SEEK_CUR:
        {
          jlong current = (*env)->CallLongMethod (env, channel,
                                                  gdk_android_get_java_cache ()->j_file_channel.get_position);
          new_position = current + off;
        }
      break;
      case G_SEEK_SET:
        new_position = off;
      break;
      case G_SEEK_END:
        {
          jlong size = (*env)->CallLongMethod (env, channel,
                                               gdk_android_get_java_cache ()->j_file_channel.get_size);
          new_position = size + off;
        }
      break;
      default:
        g_critical ("Encountered unknown seek type: %d", type);
    }

  (*env)->CallObjectMethod (env, channel,
                            gdk_android_get_java_cache ()->j_file_channel.set_position,
                            new_position);

  gboolean ret = !gdk_android_content_file_has_exception (env, error);
  (*env)->PopLocalFrame (env, NULL);
  gdk_android_drop_thread_env (&guard);
  return ret;
}

static goffset
gdk_android_java_file_output_stream_tell (GFileOutputStream *stream)
{
  GdkAndroidJavaFileOutputStream *self = (GdkAndroidJavaFileOutputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;
  (*env)->PushLocalFrame (env, 1);

  jobject channel = (*env)->CallObjectMethod (env, self->stream,
                                              gdk_android_get_java_cache ()->j_file_istream.get_channel);
  jlong position = (*env)->CallLongMethod (env, channel,
                                           gdk_android_get_java_cache ()->j_file_channel.get_position);

  goffset ret = gdk_android_content_file_has_exception (env, NULL) ? -1 : position;
  (*env)->PopLocalFrame (env, NULL);
  gdk_android_drop_thread_env (&guard);
  return ret;
}

static gboolean
gdk_android_java_file_output_stream_truncate (GFileOutputStream *stream,
                                              goffset size,
                                              GCancellable *cancellable,
                                              GError **error)
{
  GdkAndroidJavaFileOutputStream *self = (GdkAndroidJavaFileOutputStream *)stream;

  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;
  (*env)->PushLocalFrame (env, 2);

  jobject channel = (*env)->CallObjectMethod (env, self->stream,
                                              gdk_android_get_java_cache ()->j_file_ostream.get_channel);
  (*env)->CallObjectMethod (env, channel,
                            gdk_android_get_java_cache ()->j_file_channel.truncate,
                            size);

  gboolean ret = !gdk_android_content_file_has_exception (env, error);
  (*env)->PopLocalFrame (env, NULL);
  gdk_android_drop_thread_env (&guard);
  return ret;
}

static void
gdk_android_java_file_output_stream_class_init (GdkAndroidJavaFileOutputStreamClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GOutputStreamClass *stream_class = (GOutputStreamClass *)klass;
  GFileOutputStreamClass *file_stream_class = (GFileOutputStreamClass *)klass;

  object_class->finalize = gdk_android_java_file_output_stream_finalize;
  stream_class->close_fn = gdk_android_java_file_output_stream_close;
  stream_class->flush = gdk_android_java_file_output_stream_flush;
  stream_class->write_fn = gdk_android_java_file_output_stream_write;
  file_stream_class->can_seek = gdk_android_java_file_output_stream_can_seek;
  file_stream_class->can_truncate = gdk_android_java_file_output_stream_can_truncate;
  file_stream_class->seek = gdk_android_java_file_output_stream_seek;
  file_stream_class->tell = gdk_android_java_file_output_stream_tell;
  file_stream_class->truncate_fn = gdk_android_java_file_output_stream_truncate;
}

static void
gdk_android_java_file_output_stream_init (GdkAndroidJavaFileOutputStream *self)
{
  GdkAndroidThreadGuard guard = gdk_android_get_thread_env ();
  JNIEnv *env = guard.env;

  jbyteArray buffer = (*env)->NewByteArray (env, GDK_ANDROID_JAVA_STREAM_CACHE_BUFFER_SIZE);
  self->cached_buffer = (*env)->NewGlobalRef (env, buffer);
  (*env)->DeleteLocalRef (env, buffer);

  gdk_android_drop_thread_env (&guard);
}

GFileOutputStream *
gdk_android_java_file_output_stream_wrap (JNIEnv *env, jobject file_output_stream)
{
  GdkAndroidJavaFileInputStream *self = g_object_new (GDK_TYPE_ANDROID_JAVA_FILE_OUTPUT_STREAM, NULL);
  self->stream = (*env)->NewGlobalRef (env, file_output_stream);
  return (GFileOutputStream *)self;
}


enum
{
  GDK_ANDROID_CONTENT_PROJECTION_DOCUMENT_ID = 0,
  GDK_ANDROID_CONTENT_PROJECTION_DISPLAY_NAME,
  GDK_ANDROID_CONTENT_PROJECTION_FLAGS,
  GDK_ANDROID_CONTENT_PROJECTION_ICON,
  GDK_ANDROID_CONTENT_PROJECTION_LAST_MODIFIED,
  GDK_ANDROID_CONTENT_PROJECTION_MIME_TYPE,
  GDK_ANDROID_CONTENT_PROJECTION_SIZE,
  GDK_ANDROID_CONTENT_PROJECTION_SUMMARY
};

#define GDK_ANDROID_FLAG_TO_INFO(attr, flag)                               \
  if (g_file_attribute_matcher_matches (matcher, (attr)))                  \
    g_file_info_set_attribute_boolean (info,                               \
                                       G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, \
                                       (qflags & gdk_android_get_java_cache ()->a_documents_contract_document.flag_##flag) != 0);

static GFileInfo *
gdk_android_content_file_fileinfo_from_cursor (JNIEnv *env,
                                               const gchar *attributes,
                                               jobject context,
                                               jobject cursor,
                                               jobject uri)
{
  GFileInfo *info = g_file_info_new ();
  GFileAttributeMatcher *matcher = g_file_attribute_matcher_new (attributes);
  (*env)->PushLocalFrame (env, 5);

  jobject filename = (*env)->CallObjectMethod (env, cursor,
                                               gdk_android_get_java_cache ()->a_cursor.get_string,
                                               GDK_ANDROID_CONTENT_PROJECTION_DISPLAY_NAME);
  gchar *filename_str = gdk_android_java_to_utf8 (filename, NULL);

  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME))
    g_file_info_set_display_name (info, filename_str);
  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_NAME))
    g_file_info_set_name (info, filename_str);
  g_free (filename_str);

  gint qflags = (*env)->CallIntMethod (env, cursor,
                                       gdk_android_get_java_cache ()->a_cursor.get_int,
                                       GDK_ANDROID_CONTENT_PROJECTION_FLAGS);

  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)
   && !(*env)->CallBooleanMethod (env, cursor,
                                  gdk_android_get_java_cache ()->a_cursor.is_null,
                                  GDK_ANDROID_CONTENT_PROJECTION_MIME_TYPE))
    {
      jobject jmime = (*env)->CallObjectMethod (env, cursor,
                                                gdk_android_get_java_cache ()->a_cursor.get_string,
                                                GDK_ANDROID_CONTENT_PROJECTION_MIME_TYPE);
      gchar *mime = gdk_android_java_to_utf8 (jmime, NULL);
      gchar *content = g_content_type_from_mime_type (mime);
      g_file_info_set_content_type (info, content);
      g_free (content);
      g_free (mime);
    }

  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION)
      && !(*env)->CallBooleanMethod (env, cursor,
                                     gdk_android_get_java_cache ()->a_cursor.is_null,
                                     GDK_ANDROID_CONTENT_PROJECTION_SUMMARY))
    {
      jobject jdesc = (*env)->CallObjectMethod (env, cursor,
                                                gdk_android_get_java_cache ()->a_cursor.get_string,
                                                GDK_ANDROID_CONTENT_PROJECTION_SUMMARY);
      gchar *desc = gdk_android_java_to_utf8 (jdesc, NULL);
      g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION, desc);
      g_free (desc);
    }

  /*if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_ICON))
    {
      // not implemented
    }*/

  // I'm going to make the assumption that I can always read the document
  g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ, TRUE);
  GDK_ANDROID_FLAG_TO_INFO (G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, supports_write)

  GDK_ANDROID_FLAG_TO_INFO (G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, supports_delete)
  GDK_ANDROID_FLAG_TO_INFO (G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME, supports_rename)
  GDK_ANDROID_FLAG_TO_INFO (G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL, virtual_document)

  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_SIZE)
      && !(*env)->CallBooleanMethod (env, cursor,
                                     gdk_android_get_java_cache ()->a_cursor.is_null,
                                     GDK_ANDROID_CONTENT_PROJECTION_SIZE))
    {
      jlong size = (*env)->CallLongMethod (env, cursor,
                                           gdk_android_get_java_cache ()->a_cursor.get_long,
                                           GDK_ANDROID_CONTENT_PROJECTION_SIZE);
      g_file_info_set_size (info, size);
    }

  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_TIME_MODIFIED)
      && !(*env)->CallBooleanMethod (env, cursor,
                                     gdk_android_get_java_cache ()->a_cursor.is_null,
                                     GDK_ANDROID_CONTENT_PROJECTION_LAST_MODIFIED))
    {
      jlong time = (*env)->CallLongMethod (env, cursor,
                                           gdk_android_get_java_cache ()->a_cursor.get_long,
                                           GDK_ANDROID_CONTENT_PROJECTION_LAST_MODIFIED);
      GDateTime *date = g_date_time_new_from_unix_utc_usec (time * 1000);
      g_file_info_set_modification_date_time (info, date);
      g_date_time_unref (date);
    }

  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_TYPE))
    {
      if ((*env)->CallStaticBooleanMethod (env,
                                           gdk_android_get_java_cache ()->a_documents_contract.klass,
                                           gdk_android_get_java_cache ()->a_documents_contract.is_tree,
                                           uri))
        g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
      else if ((*env)->CallStaticBooleanMethod (env,
                                                gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                gdk_android_get_java_cache ()->a_documents_contract.is_document,
                                                context, uri))
        g_file_info_set_file_type (info, G_FILE_TYPE_REGULAR);
      else
        g_file_info_set_file_type (info, G_FILE_TYPE_UNKNOWN);
    }

  (*env)->PopLocalFrame (env, NULL);
  g_file_attribute_matcher_unref (matcher);

  return info;
}

typedef struct _GdkAndroidContentFileEnumeratorClass
{
  GFileEnumeratorClass parent_class;
} GdkAndroidContentFileEnumeratorClass;

typedef struct _GdkAndroidContentFileEnumerator
{
  GFileEnumerator parent_instance;

  gchar *attributes;
  jobject context;
  jobject cursor;
  jobject parent_uri;
} GdkAndroidContentFileEnumerator;

#define GDK_TYPE_ANDROID_CONTENT_FILE_ENUMERATOR (gdk_android_content_file_enumerator_get_type ())
GType gdk_android_content_file_enumerator_get_type (void);

G_DEFINE_TYPE (GdkAndroidContentFileEnumerator,
               gdk_android_content_file_enumerator,
               G_TYPE_FILE_ENUMERATOR)

static void
gdk_android_content_file_enumerator_finalize (GObject *object)
{
  GdkAndroidContentFileEnumerator *self = (GdkAndroidContentFileEnumerator *)object;
  g_free (self->attributes);
  JNIEnv *env = gdk_android_get_env();
  jobject cursor = self->cursor;
  (*env)->DeleteGlobalRef (env, self->parent_uri);
  (*env)->DeleteGlobalRef (env, self->context);
  G_OBJECT_CLASS (gdk_android_content_file_enumerator_parent_class)->finalize (object);
  // parent.finalize is calling close, so we have to wait before freeing cursor
  (*env)->DeleteGlobalRef (env, cursor);
}

static gboolean
gdk_android_content_file_enumerator_close (GFileEnumerator *enumerator,
                                           GCancellable    *cancellable,
                                           GError         **error)
{
  GdkAndroidContentFileEnumerator *self = (GdkAndroidContentFileEnumerator *)enumerator;
  JNIEnv *env = gdk_android_get_env();
  (*env)->CallVoidMethod (env, self->cursor,
                          gdk_android_get_java_cache ()->a_cursor.close);
  return TRUE;
}


static GFileInfo *
gdk_android_content_file_enumerator_next_file (GFileEnumerator *enumerator,
                                               GCancellable    *cancellable,
                                               GError         **error)
{
  GdkAndroidContentFileEnumerator *self = (GdkAndroidContentFileEnumerator *)enumerator;
  JNIEnv *env = gdk_android_get_env();
  if (!(*env)->CallBooleanMethod (env, self->cursor,
                                  gdk_android_get_java_cache ()->a_cursor.move_to_next))
    return NULL;

  (*env)->PushLocalFrame (env, 2);

  jobject document_id = (*env)->CallObjectMethod (env, self->cursor,
                                                  gdk_android_get_java_cache ()->a_cursor.get_string,
                                                  GDK_ANDROID_CONTENT_PROJECTION_DOCUMENT_ID);
  jobject uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                gdk_android_get_java_cache ()->a_documents_contract.build_document_from_tree,
                                                self->parent_uri, document_id);

  GFileInfo *info = gdk_android_content_file_fileinfo_from_cursor (env,
                                                                   self->attributes,
                                                                   self->context,
                                                                   self->cursor,
                                                                   uri);

  (*env)->PopLocalFrame (env, NULL);
  return info;
}

static void
gdk_android_content_file_enumerator_class_init (GdkAndroidContentFileEnumeratorClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GFileEnumeratorClass *enumerator_class = (GFileEnumeratorClass *)klass;

  object_class->finalize = gdk_android_content_file_enumerator_finalize;
  enumerator_class->close_fn = gdk_android_content_file_enumerator_close;
  enumerator_class->next_file = gdk_android_content_file_enumerator_next_file;
}

static void
gdk_android_content_file_enumerator_init (GdkAndroidContentFileEnumerator *self)
{}

static GFileEnumerator *
gdk_android_content_file_enumerator_create (const gchar *attributes,
                                            jobject      context,
                                            jobject      cursor,
                                            jobject      parent_uri)
{
  JNIEnv *env = gdk_android_get_env();
  GdkAndroidContentFileEnumerator *self = g_object_new (GDK_TYPE_ANDROID_CONTENT_FILE_ENUMERATOR, NULL);
  self->attributes = g_strdup (attributes);
  self->context = (*env)->NewGlobalRef (env, context);
  self->cursor = (*env)->NewGlobalRef (env, cursor);
  self->parent_uri = (*env)->NewGlobalRef (env, parent_uri);
  return (GFileEnumerator *)self;
}

/// begin ContentFile

struct _GdkAndroidContentFileClass
{
  GObjectClass parent_class;
};

struct _GdkAndroidContentFile
{
  GObject parent_instance;
  jobjectArray query_projection;
  jobjectArray full_projection;

  jobject context;

  jobject uri;
  jstring child_name; // if this is set, uri refers to the parent of the file
};

/**
 * GdkAndroidContentFile:
 *
 * Adapted [iface@Gio.File] interface to interact with `content://` URIs
 * from the
 * [ContentProvider](https://developer.android.com/guide/topics/providers/content-provider-basics)
 * system of Android.
 *
 * As the
 * [SAF/DocumentProvider](https://developer.android.com/guide/topics/providers/document-provider)
 * interface is more restrictive than is commonly expected from a
 * "normal" filesystem, some methods do not work.
 *
 * Since: 4.18
 */
static void gdk_android_content_file_iface_init (GFileIface *iface);
G_DEFINE_TYPE_WITH_CODE (GdkAndroidContentFile, gdk_android_content_file, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_FILE, gdk_android_content_file_iface_init))

static void
gdk_android_content_file_finalize (GObject *object)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)object;
  JNIEnv *env = gdk_android_get_env ();
  if (self->child_name)
    (*env)->DeleteGlobalRef (env, self->child_name);
  if (self->uri)
    (*env)->DeleteGlobalRef (env, self->uri);
  (*env)->DeleteGlobalRef (env, self->context);
  (*env)->DeleteGlobalRef (env, self->query_projection);
  (*env)->DeleteGlobalRef (env, self->full_projection);
  G_OBJECT_CLASS (gdk_android_content_file_parent_class)->finalize (object);
}

static void
gdk_android_content_file_class_init (GdkAndroidContentFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gdk_android_content_file_finalize;
}

static void
gdk_android_content_file_init (GdkAndroidContentFile *self)
{
  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 2);

  jobjectArray query_projection = (*env)->NewObjectArray (env, 2,
                                                          gdk_android_get_java_cache ()->j_string.klass,
                                                          NULL);
  (*env)->SetObjectArrayElement (env, query_projection, GDK_ANDROID_CONTENT_PROJECTION_DOCUMENT_ID,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_document_id);
  (*env)->SetObjectArrayElement (env, query_projection, GDK_ANDROID_CONTENT_PROJECTION_DISPLAY_NAME,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_display_name);
  self->query_projection = (*env)->NewGlobalRef (env, query_projection);

  jobjectArray full_projection = (*env)->NewObjectArray (env, 8,
                                                         gdk_android_get_java_cache ()->j_string.klass,
                                                         NULL);
  (*env)->SetObjectArrayElement (env, full_projection, GDK_ANDROID_CONTENT_PROJECTION_DOCUMENT_ID,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_document_id);
  (*env)->SetObjectArrayElement (env, full_projection, GDK_ANDROID_CONTENT_PROJECTION_DISPLAY_NAME,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_display_name);
  (*env)->SetObjectArrayElement (env, full_projection, GDK_ANDROID_CONTENT_PROJECTION_FLAGS,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_flags);
  (*env)->SetObjectArrayElement (env, full_projection, GDK_ANDROID_CONTENT_PROJECTION_ICON,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_icon);
  (*env)->SetObjectArrayElement (env, full_projection, GDK_ANDROID_CONTENT_PROJECTION_LAST_MODIFIED,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_last_modified);
  (*env)->SetObjectArrayElement (env, full_projection, GDK_ANDROID_CONTENT_PROJECTION_MIME_TYPE,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_mime_type);
  (*env)->SetObjectArrayElement (env, full_projection, GDK_ANDROID_CONTENT_PROJECTION_SIZE,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_size);
  (*env)->SetObjectArrayElement (env, full_projection, GDK_ANDROID_CONTENT_PROJECTION_SUMMARY,
                                 gdk_android_get_java_cache ()->a_documents_contract_document.column_summary);
  self->full_projection = (*env)->NewGlobalRef (env, full_projection);

  (*env)->PopLocalFrame (env, NULL);
}

static gboolean
gdk_android_content_file_make_valid (GdkAndroidContentFile *self, GError **error)
{
  if (self->child_name == NULL)
    return TRUE;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 7);
  jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);

  jobject parent_document_id = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                               gdk_android_get_java_cache ()->a_documents_contract.get_document_id,
                                                               self->uri);
  jobject children_uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                         gdk_android_get_java_cache ()->a_documents_contract.build_children_from_tree,
                                                         self->uri, parent_document_id);

  jobject cursor = (*env)->CallObjectMethod (env, resolver,
                                             gdk_android_get_java_cache ()->a_content_resolver.query,
                                             children_uri,
                                             self->query_projection, NULL, NULL, NULL);
  if (gdk_android_content_file_has_exception (env, error))
    {
      (*env)->PopLocalFrame (env, NULL);
      return FALSE;
    }

  while ((*env)->CallBooleanMethod (env, cursor,
                                      gdk_android_get_java_cache ()->a_cursor.move_to_next))
    {
      jobject filename = (*env)->CallObjectMethod (env, cursor,
                                                   gdk_android_get_java_cache ()->a_cursor.get_string,
                                                   GDK_ANDROID_CONTENT_PROJECTION_DISPLAY_NAME);
      if ((*env)->CallBooleanMethod (env, self->child_name,
                                     gdk_android_get_java_cache ()->j_object.equals,
                                     filename))
        {
          jobject document_id = (*env)->CallObjectMethod (env, cursor,
                                                          gdk_android_get_java_cache ()->a_cursor.get_string,
                                                          GDK_ANDROID_CONTENT_PROJECTION_DOCUMENT_ID);
          jobject uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                        gdk_android_get_java_cache ()->a_documents_contract.build_document_from_tree,
                                                        self->uri, document_id);

          (*env)->DeleteGlobalRef (env, self->uri);
          self->uri = (*env)->NewGlobalRef (env, uri);

          (*env)->DeleteGlobalRef (env, self->child_name);
          self->child_name = NULL;

          (*env)->CallVoidMethod (env, cursor,
                                  gdk_android_get_java_cache ()->a_cursor.close);
          (*env)->PopLocalFrame (env, NULL);
          return TRUE;
        }

      (*env)->DeleteLocalRef (env, filename);
    }

  (*env)->CallVoidMethod (env, cursor,
                          gdk_android_get_java_cache ()->a_cursor.close);
  (*env)->PopLocalFrame (env, NULL);

  if (error)
    {
      gchar *child_name = gdk_android_java_to_utf8 (self->child_name, NULL);
      *error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "File \"%s\" was not found in directory", child_name);
      g_free (child_name);
    }
  return FALSE;
}

static jobject
gdk_android_content_file_open_descriptor (GdkAndroidContentFile *self, jstring mode, GCancellable *cancellable, GError **error)
{
  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 3);

  jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);
  jobject descriptor = (*env)->CallObjectMethod (env, resolver,
                                                 gdk_android_get_java_cache ()->a_content_resolver.open_asset_fd,
                                                 self->uri, mode, NULL);

  if (gdk_android_content_file_has_exception (env, error))
    {
      (*env)->PopLocalFrame (env, NULL);
      return NULL;
    }

  return (*env)->PopLocalFrame (env, descriptor);
}

static GFileOutputStream *
gdk_android_content_file_append_to (GFile *file,
                                    GFileCreateFlags flags,
                                    GCancellable *cancellable,
                                    GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (!gdk_android_content_file_make_valid (self, error))
    return NULL;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 2);

  jobject fd = gdk_android_content_file_open_descriptor (self,
                                                         gdk_android_get_java_cache ()->a_asset_fd.mode_append,
                                                         NULL,
                                                         error);
  if (!fd)
    goto err;

  jobject ostream = (*env)->CallObjectMethod (env, fd,
                                              gdk_android_get_java_cache ()->a_asset_fd.create_ostream);
  if (gdk_android_content_file_has_exception (env, error))
    goto err;

  GFileOutputStream *stream = gdk_android_java_file_output_stream_wrap (env, ostream);
  (*env)->PopLocalFrame (env, NULL);
  return stream;
err:
  (*env)->PopLocalFrame (env, NULL);
  return NULL;
}

static gboolean
gdk_android_content_file_copy (GFile *file,
                               GFile *destination,
                               GFileCopyFlags flags,
                               GCancellable *cancellable,
                               GFileProgressCallback callback,
                               gpointer callback_data,
                               GError **error)
{
  if (!GDK_IS_ANDROID_CONTENT_FILE(file) || !GDK_IS_ANDROID_CONTENT_FILE(destination))
    return FALSE;
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  GdkAndroidContentFile *dest = (GdkAndroidContentFile *)destination;
  if (!gdk_android_content_file_make_valid (self, error))
    return FALSE;

  JNIEnv *env = gdk_android_get_env();
  if (dest->child_name)
    {
      (*env)->PushLocalFrame (env, 3);

      jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                                   gdk_android_get_java_cache ()->a_context.get_content_resolver);
      jobject uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                    gdk_android_get_java_cache ()->a_documents_contract.copy_document,
                                                    resolver, self->uri, dest->uri);
      uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                            gdk_android_get_java_cache ()->a_documents_contract.rename_document,
                                            uri, dest->child_name);

      (*env)->DeleteGlobalRef (env, dest->uri);
      dest->uri = (*env)->NewGlobalRef (env, uri);
      (*env)->DeleteGlobalRef (env, dest->child_name);
      dest->child_name = NULL;

      (*env)->PopLocalFrame (env, NULL);
      return TRUE;
    }
  else if (flags & G_FILE_COPY_OVERWRITE)
    {
      GFileInputStream *istream = g_file_read (file, cancellable, error);
      if (!istream)
        return FALSE;
      GFileOutputStream *ostream = g_file_replace (file,
                                                   NULL,
                                                   FALSE,
                                                   G_FILE_CREATE_REPLACE_DESTINATION,
                                                   cancellable,
                                                   error);
      if (!ostream)
        {
          g_object_unref (istream);
          return FALSE;
        }

      if (g_output_stream_splice ((GOutputStream *)ostream,
                                  (GInputStream *)istream,
                                  G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                  cancellable,
                                  error) < 0)
        {
          g_object_unref (istream);
          g_object_unref (ostream);
          return FALSE;
        }

      g_object_unref (istream);
      g_object_unref (ostream);
      return TRUE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS, "Copy destination already exist");
      return FALSE;
    }
}

static GFileOutputStream *
gdk_android_content_file_create (GFile *file,
                                 GFileCreateFlags flags,
                                 GCancellable *cancellable,
                                 GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (!self->child_name)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS, "File already exists");
      return NULL;
    }

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 3);

  jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);
  jstring mime = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->j_urlconnection.klass,
                                                 gdk_android_get_java_cache ()->j_urlconnection.guess_content_type_for_name,
                                                 self->child_name);
  if (!mime)
    mime = gdk_android_get_java_cache ()->j_urlconnection.mime_binary_data;
  jobject uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                gdk_android_get_java_cache ()->a_documents_contract.create_document,
                                                resolver, self->uri, mime, self->child_name);

  if (gdk_android_content_file_has_exception (env, error))
    {
      (*env)->PopLocalFrame (env, NULL);
      return NULL;
    }

  (*env)->DeleteGlobalRef (env, self->uri);
  self->uri = (*env)->NewGlobalRef (env, uri);
  (*env)->DeleteGlobalRef (env, self->child_name);
  self->child_name = NULL;

  (*env)->PopLocalFrame (env, NULL);

  return g_file_replace (file, NULL, FALSE, flags, cancellable, error);
}

static gboolean
gdk_android_content_file_delete_file (GFile *file,
                                      GCancellable *cancellable,
                                      GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (!gdk_android_content_file_make_valid (self, error))
    return FALSE;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 1);
  jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);
  jboolean success = (*env)->CallStaticBooleanMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                      gdk_android_get_java_cache ()->a_documents_contract.delete_document,
                                                      resolver, self->uri);
  if (gdk_android_content_file_has_exception (env, error))
    {
      (*env)->PopLocalFrame (env, NULL);
      return FALSE;
    }

  (*env)->PopLocalFrame (env, NULL);
  return success;
}

static GFile *
gdk_android_content_file_dup (GFile *file)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  JNIEnv *env = gdk_android_get_env();

  GdkAndroidContentFile *copy = g_object_new (GDK_TYPE_ANDROID_CONTENT_FILE, NULL);
  copy->context = (*env)->NewGlobalRef (env, self->context);
  copy->uri = (*env)->NewGlobalRef (env, self->uri);
  copy->child_name = self->child_name ? (*env)->NewGlobalRef (env, self->child_name) : NULL;
  return (GFile *)copy;
}

static GFileEnumerator *
gdk_android_content_file_enumerate_children (GFile *file,
                                             const gchar *attributes,
                                             GFileQueryInfoFlags flags,
                                             GCancellable *cancellable,
                                             GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (!gdk_android_content_file_make_valid (self, error))
    return NULL;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 4);
  jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);

  jobject parent_document_id = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                               gdk_android_get_java_cache ()->a_documents_contract.get_document_id,
                                                               self->uri);

  jobject children_uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                         gdk_android_get_java_cache ()->a_documents_contract.build_children_from_tree,
                                                         self->uri, parent_document_id);

  jobject cursor = (*env)->CallObjectMethod (env, resolver,
                                             gdk_android_get_java_cache ()->a_content_resolver.query,
                                             children_uri,
                                             self->full_projection, NULL, NULL, NULL);
  if (gdk_android_content_file_has_exception (env, error))
    {
      (*env)->PopLocalFrame (env, NULL);
      return FALSE;
    }

  GFileEnumerator *enumerator = gdk_android_content_file_enumerator_create (attributes,
                                                                            self->context,
                                                                            cursor,
                                                                            self->uri);
  (*env)->PopLocalFrame (env, NULL);
  return enumerator;
}

static gboolean
gdk_android_content_file_equal (GFile *lhsf, GFile *rhsf)
{
  if (lhsf == rhsf)
    return TRUE;
  if (!GDK_IS_ANDROID_CONTENT_FILE (lhsf) || !GDK_IS_ANDROID_CONTENT_FILE (rhsf))
    return FALSE;

  GdkAndroidContentFile *lhs = (GdkAndroidContentFile *)lhsf;
  GdkAndroidContentFile *rhs = (GdkAndroidContentFile *)rhsf;
  gdk_android_content_file_make_valid (lhs, NULL);
  gdk_android_content_file_make_valid (rhs, NULL);

  JNIEnv *env = gdk_android_get_env();

  if (!(*env)->CallBooleanMethod (env, lhs->uri,
                                  gdk_android_get_java_cache ()->j_object.equals,
                                  rhs->uri))
    return FALSE;

  if (lhs->child_name == NULL && rhs->child_name == NULL)
    return TRUE;

  // as above, at least one is not NULL, so if one is NULL they can't be equal
  if (lhs->child_name == NULL || rhs->child_name == NULL)
    return FALSE;

  return (*env)->CallBooleanMethod (env, lhs->child_name,
                                    gdk_android_get_java_cache ()->j_object.equals,
                                    rhs->child_name);
}

static gchar *
gdk_android_content_file_get_basename (GFile *file)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (self->child_name)
    return gdk_android_java_to_utf8 (self->child_name, NULL);

  GFileInfo *info = g_file_query_info (file,
                                       G_FILE_ATTRIBUTE_STANDARD_NAME,
                                       G_FILE_QUERY_INFO_NONE,
                                       NULL, NULL);
  g_return_val_if_fail (info, NULL);
  gchar *basename = g_strdup (g_file_info_get_name (info));
  g_object_unref (info);
  return basename;
}

static GFile *
gdk_android_content_file_get_child_for_displayname (GFile *file,
                                                    const gchar *display_name,
                                                    GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (!gdk_android_content_file_make_valid (self, error))
    return NULL;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 1);

  GdkAndroidContentFile *child = g_object_new (GDK_TYPE_ANDROID_CONTENT_FILE, NULL);
  child->context = (*env)->NewGlobalRef (env, self->context);
  child->uri = (*env)->NewGlobalRef (env, self->uri);

  jobject child_name = gdk_android_utf8_to_java (display_name);
  child->child_name = (*env)->NewGlobalRef (env, child_name);

  // If the file already exists, normalize. (what about error condition?)
  gdk_android_content_file_make_valid (child, NULL);

  (*env)->PopLocalFrame (env, NULL);
  return (GFile *)child;
}

static GFile *
gdk_android_content_file_get_parent (GFile *file)
{
  // You do not have access to parent directories
  return NULL;
}

static gchar *
gdk_android_content_file_get_path (GFile *file)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  g_return_val_if_fail (gdk_android_content_file_make_valid (self, NULL), NULL);

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 1);
  jstring path_string = (*env)->CallObjectMethod (env, self->uri,
                                                  gdk_android_get_java_cache ()->a_uri.get_path);
  gchar *path = gdk_android_java_to_utf8 (path_string, NULL);
  (*env)->PopLocalFrame (env, NULL);
  return path;
}

static gchar *
gdk_android_content_file_get_uri (GFile *file)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  g_return_val_if_fail (gdk_android_content_file_make_valid (self, NULL), NULL);

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 1);
  jstring uri_string = (*env)->CallObjectMethod (env, self->uri,
                                                 gdk_android_get_java_cache ()->j_object.to_string);
  gchar *uri = gdk_android_java_to_utf8 (uri_string, NULL);
  (*env)->PopLocalFrame (env, NULL);
  return uri;
}

static gchar *
gdk_android_content_file_get_uri_scheme (GFile *file)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  g_return_val_if_fail (gdk_android_content_file_make_valid (self, NULL), NULL);

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 1);
  jstring scheme_string = (*env)->CallObjectMethod (env, self->uri,
                                                 gdk_android_get_java_cache ()->a_uri.get_scheme);
  gchar *scheme = gdk_android_java_to_utf8 (scheme_string, NULL);
  (*env)->PopLocalFrame (env, NULL);
  return scheme;
}

static gboolean
gdk_android_content_file_has_uri_scheme (GFile *file, const gchar *scheme)
{
  gchar* actual_scheme = g_file_get_uri_scheme (file);
  gboolean success = g_strcmp0 (scheme, actual_scheme) == 0;
  g_free (actual_scheme);
  return success;
}

static guint
gdk_android_content_file_hash (GFile *file)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  gdk_android_content_file_make_valid (self, NULL);

  JNIEnv *env = gdk_android_get_env();
  guint hash = (guint)(*env)->CallIntMethod (env, self->uri,
                                             gdk_android_get_java_cache ()->j_object.hash_code);
  if (self->child_name)
    hash ^= (guint)(*env)->CallIntMethod (env, self->child_name,
                                          gdk_android_get_java_cache ()->j_object.hash_code);
  return hash;
}

static gboolean
gdk_android_content_file_is_native (GFile *file)
{
  // Depending on your definition of "native", this might be a lie; but given
  // the problems of dealing with SAF, it's probably better to tell users theat
  // these files are not "native".
  return FALSE;
}

static gboolean
gdk_android_content_file_make_directory (GFile *file,
                                         GCancellable *cancellable,
                                         GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (!self->child_name)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS, "Directory already exists");
      return FALSE;
    }

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 2);

  jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);

  jobject uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                gdk_android_get_java_cache ()->a_documents_contract.create_document,
                                                resolver,
                                                self->uri,
                                                gdk_android_get_java_cache ()->a_documents_contract_document.mime_directory,
                                                self->child_name);

  if (gdk_android_content_file_has_exception (env, error))
    {
      (*env)->PopLocalFrame (env, NULL);
      return FALSE;
    }

  (*env)->DeleteGlobalRef (env, self->uri);
  self->uri = (*env)->NewGlobalRef (env, uri);
  (*env)->DeleteGlobalRef (env, self->child_name);
  self->child_name = NULL;

  (*env)->PopLocalFrame (env, NULL);
  return TRUE;
}

static GFileMonitor *
gdk_android_content_file_monitor_file (GFile *file,
                                       GFileMonitorFlags flags,
                                       GCancellable *cancellable,
                                       GError **error)
{
  if (error)
    *error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unimplemented");
  return NULL;
}

static gboolean
gdk_android_content_file_move (GFile *file,
                               GFile *destination,
                               GFileCopyFlags flags,
                               GCancellable *cancellable,
                               GFileProgressCallback callback,
                               gpointer callback_data,
                               GError **error)
{
  if (error)
    *error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unimplemented");
  return FALSE;
}

static gboolean
gdk_android_content_file_prefix_matches (GFile *prefixf,
                                         GFile *filef)
{
  if (!GDK_IS_ANDROID_CONTENT_FILE(prefixf) || !GDK_IS_ANDROID_CONTENT_FILE(filef))
    return FALSE;
  GdkAndroidContentFile *prefix = (GdkAndroidContentFile *)prefixf;
  GdkAndroidContentFile *file = (GdkAndroidContentFile *)filef;
  gdk_android_content_file_make_valid (prefix, NULL);
  if (prefix->child_name)
    return FALSE; // if prefix does not exist, it cant be a prefix

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 1);
  jobject resolver = (*env)->CallObjectMethod (env, file->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);
  // it doesn't matter whether file has been created or not, given either it or its
  // paren will still have the same prefix.
  jboolean is_child = (*env)->CallStaticBooleanMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                       gdk_android_get_java_cache ()->a_documents_contract.is_child_document,
                                                       resolver,
                                                       prefix->uri, file->uri);
  if (gdk_android_content_file_has_exception (env, NULL))
    {
      (*env)->PopLocalFrame (env, NULL);
      return FALSE;
    }

  (*env)->PopLocalFrame (env, NULL);
  return is_child;
}

static GFileInfo *
gdk_android_content_file_query_info (GFile *file,
                                     const gchar *attributes,
                                     GFileQueryInfoFlags flags,
                                     GCancellable *cancellable,
                                     GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (!gdk_android_content_file_make_valid (self, error))
    return NULL;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 2);
  jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                               gdk_android_get_java_cache ()->a_context.get_content_resolver);

  jobject cursor = (*env)->CallObjectMethod (env, resolver,
                                             gdk_android_get_java_cache ()->a_content_resolver.query,
                                             self->uri,
                                             self->full_projection, NULL, NULL, NULL);
  if (gdk_android_content_file_has_exception (env, error))
    {
      (*env)->PopLocalFrame (env, NULL);
      return FALSE;
    }

  GFileInfo *info = NULL;
  if ((*env)->CallBooleanMethod (env, cursor,
                                    gdk_android_get_java_cache ()->a_cursor.move_to_next))
    info = gdk_android_content_file_fileinfo_from_cursor (env, attributes, self->context, cursor, self->uri);
    // all further entries (should they exist) are ignored
  else
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "File query did not return any results");

  (*env)->CallVoidMethod (env, cursor,
                          gdk_android_get_java_cache ()->a_cursor.close);
  (*env)->PopLocalFrame (env, NULL);
  return info;
}

static GFileInputStream *
gdk_android_content_file_read (GFile *file, GCancellable *cancellable, GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (!gdk_android_content_file_make_valid (self, error))
    return NULL;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 2);

  jobject fd = gdk_android_content_file_open_descriptor (self,
                                                         gdk_android_get_java_cache ()->a_asset_fd.mode_read,
                                                         NULL,
                                                         error);
  if (!fd)
    goto err;

  jobject istream = (*env)->CallObjectMethod (env, fd,
                                              gdk_android_get_java_cache ()->a_asset_fd.create_istream);
  if (gdk_android_content_file_has_exception (env, error))
    goto err;

  GFileInputStream *stream = gdk_android_java_file_input_stream_wrap (env, istream);
  (*env)->PopLocalFrame (env, NULL);
  return stream;
err:
  (*env)->PopLocalFrame (env, NULL);
  return NULL;
}

static GFileOutputStream *
gdk_android_content_file_replace (GFile *file,
                                  const gchar *etag,
                                  gboolean make_backup,
                                  GFileCreateFlags flags,
                                  GCancellable *cancellable,
                                  GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;
  if (self->child_name && (flags & G_FILE_CREATE_REPLACE_DESTINATION))
    return g_file_create (file, flags, cancellable, error);

  if (!gdk_android_content_file_make_valid (self, error))
    return NULL;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 2);

  jobject fd = gdk_android_content_file_open_descriptor (self,
                                                         gdk_android_get_java_cache ()->a_asset_fd.mode_overwrite,
                                                         NULL,
                                                         error);
  if (!fd)
    goto err;

  jobject ostream = (*env)->CallObjectMethod (env, fd,
                                              gdk_android_get_java_cache ()->a_asset_fd.create_ostream);
  if (gdk_android_content_file_has_exception (env, error))
    goto err;

  GFileOutputStream *stream = gdk_android_java_file_output_stream_wrap (env, ostream);
  (*env)->PopLocalFrame (env, NULL);
  return stream;
err:
  (*env)->PopLocalFrame (env, NULL);
  return NULL;
}

static GFile *
gdk_android_content_file_resolve_relative_path (GFile *file,
                                                const gchar *relative_path)
{
  while (g_str_has_prefix(relative_path, "./"))
    relative_path = &relative_path[2];
  if (!*relative_path)
    return g_object_ref (file);
  if (strstr(relative_path, G_DIR_SEPARATOR_S))
    return NULL;

  return g_file_get_child_for_display_name (file, relative_path, NULL);
}

static GFile *
gdk_android_content_file_set_display_name (GFile *file,
                                           const gchar *display_name,
                                           GCancellable *cancellable,
                                           GError **error)
{
  GdkAndroidContentFile *self = (GdkAndroidContentFile *)file;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 3);
  jobject new_name = gdk_android_utf8_to_java (display_name);

  if (self->child_name)
    {
      (*env)->DeleteGlobalRef (env, self->child_name);
      self->child_name = (*env)->NewGlobalRef (env, new_name);
    }
  else
    {
      jobject resolver = (*env)->CallObjectMethod (env, self->context,
                                                   gdk_android_get_java_cache ()->a_context.get_content_resolver);

      jobject new_uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_documents_contract.klass,
                                                        gdk_android_get_java_cache ()->a_documents_contract.rename_document,
                                                        resolver, self->uri, new_name);
      if (gdk_android_content_file_has_exception (env, error))
        {
          (*env)->PopLocalFrame (env, NULL);
          return NULL;
        }

      (*env)->DeleteGlobalRef (env, self->uri);
      self->uri = (*env)->NewGlobalRef (env, new_uri);
    }

  (*env)->PopLocalFrame (env, NULL);
  return g_object_ref (file);
}

static void
gdk_android_content_file_iface_init (GFileIface *iface)
{
  iface->append_to = gdk_android_content_file_append_to;
  iface->copy = gdk_android_content_file_copy;
  iface->create = gdk_android_content_file_create;
  iface->delete_file = gdk_android_content_file_delete_file;
  iface->dup = gdk_android_content_file_dup;
  iface->enumerate_children = gdk_android_content_file_enumerate_children;
  iface->equal = gdk_android_content_file_equal;
  iface->get_basename = gdk_android_content_file_get_basename;
  iface->get_child_for_display_name = gdk_android_content_file_get_child_for_displayname;
  iface->get_parse_name = gdk_android_content_file_get_uri;
  iface->get_parent = gdk_android_content_file_get_parent;
  iface->get_path = gdk_android_content_file_get_path;
  iface->get_uri = gdk_android_content_file_get_uri;
  iface->get_uri_scheme = gdk_android_content_file_get_uri_scheme;
  iface->has_uri_scheme = gdk_android_content_file_has_uri_scheme;
  iface->hash = gdk_android_content_file_hash;
  iface->is_native = gdk_android_content_file_is_native;
  iface->make_directory = gdk_android_content_file_make_directory;
  iface->monitor_dir = gdk_android_content_file_monitor_file;
  iface->monitor_file = gdk_android_content_file_monitor_file;
  iface->move = gdk_android_content_file_move;
  iface->prefix_matches = gdk_android_content_file_prefix_matches;
  iface->query_info = gdk_android_content_file_query_info;
  iface->read_fn = gdk_android_content_file_read;
  iface->replace = gdk_android_content_file_replace;
  iface->resolve_relative_path = gdk_android_content_file_resolve_relative_path;
  iface->set_display_name = gdk_android_content_file_set_display_name;
}

/**
 * gdk_android_content_file_from_uri: (skip)
 * @uri: The `content://` URI
 *
 * Create a new [class@Gdk.AndroidContentFile] instance from an Android
 * [URI](https://developer.android.com/reference/android/net/Uri)
 * object.
 *
 * Returns: (transfer full): the newly created file
 *
 * Since: 4.18
 */
GFile *
gdk_android_content_file_from_uri (jobject uri)
{
  JNIEnv *env = gdk_android_get_env();
  g_return_val_if_fail (env != NULL, NULL);
  g_return_val_if_fail ((*env)->IsInstanceOf (env, uri,
                                              gdk_android_get_java_cache ()->a_uri.klass),
                        NULL);

  GdkAndroidContentFile *self = g_object_new (GDK_TYPE_ANDROID_CONTENT_FILE, NULL);
  self->context = (*env)->NewGlobalRef (env, gdk_android_get_activity ());
  self->child_name = NULL;

  if ((*env)->CallStaticBooleanMethod (env, gdk_android_get_java_cache()->a_documents_contract.klass,
                                       gdk_android_get_java_cache()->a_documents_contract.is_document,
                                       self->context, uri))
    {
      self->uri = (*env)->NewGlobalRef (env, uri);
    }
  else if ((*env)->CallStaticBooleanMethod (env, gdk_android_get_java_cache()->a_documents_contract.klass,
                                            gdk_android_get_java_cache()->a_documents_contract.is_tree,
                                            uri))
    {
      jstring document_id = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache()->a_documents_contract.klass,
                                                            gdk_android_get_java_cache()->a_documents_contract.get_tree_document_id,
                                                            uri);
      jobject tree_uri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache()->a_documents_contract.klass,
                                                         gdk_android_get_java_cache()->a_documents_contract.build_document_from_tree,
                                                         uri, document_id);
      self->uri = (*env)->NewGlobalRef (env, tree_uri);
    }
  else
    {
      g_object_unref (self);
      g_return_val_if_reached (NULL);
    }

  return (GFile *)self;
}

/**
 * gdk_android_content_file_get_uri_object: (skip)
 * @self: (transfer none): the content file
 *
 * Get the `content://` URI object that is backing @self.
 *
 * Returns: (nullable): the URI backing @self or %NULL if the file doesn't exist
 *
 * Since: 4.18
 */
jobject
gdk_android_content_file_get_uri_object (GdkAndroidContentFile *self)
{
  g_return_val_if_fail (GDK_IS_ANDROID_CONTENT_FILE (self), NULL);
  if (!gdk_android_content_file_make_valid (self, NULL))
    return NULL;

  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 1);
  jstring norm_uri = (*env)->CallObjectMethod (env, self->uri,
                                               gdk_android_get_java_cache ()->a_uri.normalize);
  return (*env)->PopLocalFrame (env, norm_uri);
}
