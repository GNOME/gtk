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

static gboolean
gdk_android_glib_context_run_on_main_cb (jobject runnable)
{
  JNIEnv *env = gdk_android_get_env ();
  jclass runnable_class = (*env)->FindClass (env, "java/lang/Runnable");
  jmethodID run = (*env)->GetMethodID (env, runnable_class, "run", "()V");
  (*env)->CallVoidMethod (env, runnable, run);
  (*env)->DeleteLocalRef (env, runnable_class);
  return G_SOURCE_REMOVE;
}
static void
gdk_android_glib_context_run_on_main_data_destroy (jobject runnable)
{
  JNIEnv *env = gdk_android_get_env ();
  (*env)->DeleteGlobalRef (env, runnable);
}
void
_gdk_android_glib_context_run_on_main (JNIEnv *env, jclass klass, jobject runnable)
{
  g_idle_add_full (
      G_PRIORITY_DEFAULT,
      (GSourceFunc) gdk_android_glib_context_run_on_main_cb,
      (*env)->NewGlobalRef (env, runnable),
      (GDestroyNotify) gdk_android_glib_context_run_on_main_data_destroy);
}

jint
gdk_android_utils_color_to_android (const GdkRGBA *rgba)
{
  return ((guchar) (rgba->alpha * 0xff)) << 24 | ((guchar) (rgba->red * 0xff)) << 16 | ((guchar) (rgba->green * 0xff)) << 8 | ((guchar) (rgba->blue * 0xff)) << 0;
}

GdkRectangle
gdk_android_utils_rect_to_gdk (jobject rect)
{
  JNIEnv *env = gdk_android_get_env ();
  jint bottom = (*env)->GetIntField (env, rect, gdk_android_get_java_cache ()->a_rect.bottom);
  jint left = (*env)->GetIntField (env, rect, gdk_android_get_java_cache ()->a_rect.left);
  jint right = (*env)->GetIntField (env, rect, gdk_android_get_java_cache ()->a_rect.right);
  jint top = (*env)->GetIntField (env, rect, gdk_android_get_java_cache ()->a_rect.top);

  return (GdkRectangle){
    .x = MIN (left, right),
    .y = MIN (top, bottom),
    .width = abs (right - left),
    .height = abs (bottom - top)
  };
}

jstring
gdk_android_utf8n_to_java (const gchar *utf8, gssize utf8_len)
{
  if (!utf8)
    return NULL;

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 1);

  glong len;
  GError *err = NULL;
  gunichar2 *utf16 = g_utf8_to_utf16 (utf8, utf8_len, NULL, &len, &err);
  jstring ret;
  if (err)
    {
      (*env)->ThrowNew (env, gdk_android_get_java_cache ()->j_char_conversion_exception.klass, err->message);
      g_error_free (err);
      ret = NULL;
    }
  else
    {
      ret = (*env)->NewString (env, utf16, len);
      g_free (utf16);
    }

  return (*env)->PopLocalFrame (env, ret);
}

jstring
gdk_android_utf8_to_java (const gchar *utf8)
{
  return gdk_android_utf8n_to_java (utf8, -1);
}

gchar *
gdk_android_java_to_utf8 (jstring string, gsize *len)
{
  JNIEnv *env = gdk_android_get_env ();

  jsize jlen = (*env)->GetStringLength (env, string);
  const jchar *utf16 = (*env)->GetStringChars (env, string, NULL);

  GError *err = NULL;
  gchar *utf8 = g_utf16_to_utf8 (utf16, jlen, NULL, (glong *) len, &err);
  if (err)
    {
      (*env)->ThrowNew (env, gdk_android_get_java_cache ()->j_char_conversion_exception.klass, err->message);
      g_error_free (err);
    }
  (*env)->ReleaseStringChars (env, string, utf16);
  return utf8;
}


void
gdk_android_utils_unref_jobject (jobject object)
{
  JNIEnv *env = gdk_android_get_env();
  (*env)->DeleteGlobalRef (env, object);
}

G_DEFINE_QUARK (GDK_ANDROID_ERROR, gdk_android_error)

gboolean
gdk_android_check_exception (GError **error)
{
  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 2);

  jthrowable exception = (*env)->ExceptionOccurred (env);
  if (exception)
    {
      (*env)->ExceptionClear (env);
      if (error)
        {
          jstring msg = (*env)->CallObjectMethod (env, exception,
                                                  gdk_android_get_java_cache ()->j_throwable.get_message);
          gchar *cmsg = gdk_android_java_to_utf8 (msg, NULL);
          g_set_error (error, GDK_ANDROID_ERROR, GDK_ANDROID_JAVA_EXCEPTION, "%s", cmsg);
          g_free (cmsg);
        }

      (*env)->PopLocalFrame (env, NULL);
      return TRUE;
    }

  (*env)->PopLocalFrame (env, NULL);
  return FALSE;
}
