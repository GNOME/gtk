/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechoosernativeandroid.c: Android native filepicker
 * Copyright (C) 2024, Florian "sp1rit" <sp1rit@disroot.org>
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

#include "gtkfilefilterprivate.h"
#include "gtkfilechoosernativeprivate.h"
#include "gtknativedialogprivate.h"

#include "deprecated/gtkdialog.h"
#include "gtknative.h"

#include "android/gdkandroidcontentfile.h"
#include "android/gdkandroidtoplevel.h"

#include "android/gdkandroidinit-private.h"
#include "android/gdkandroidutils-private.h"

typedef struct {
  GCancellable *cancellable;
} GtkFileChooserNativeAndroidData;

static void
gtk_file_chooser_native_android_result (GdkAndroidToplevel *toplevel, GAsyncResult *res, GtkFileChooserNative *self)
{
  JNIEnv *env = gdk_android_get_env();
  (*env)->PushLocalFrame (env, 2);

  jint ret;
  jobject result;
  GError *err = NULL;
  if (!gdk_android_toplevel_launch_activity_for_result_finish (toplevel, res, &ret, &result, &err) ||
      ret != gdk_android_get_java_cache ()->a_activity.result_ok)
    {
      _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (self), GTK_RESPONSE_CANCEL);
      if (err)
        g_error_free (err);
      goto exit;
    }

  jobject data = (*env)->CallObjectMethod (env, result,
                                           gdk_android_get_java_cache ()->a_intent.get_data);
  if (data)
    {
      GFile *file = gdk_android_content_file_from_uri (data);
      self->custom_files = g_slist_prepend (self->custom_files, file);
    }
  else
    {
      jobject clipdata = (*env)->CallObjectMethod (env, result,
                                                   gdk_android_get_java_cache ()->a_intent.get_clipdata);

      jint n_items = (*env)->CallIntMethod (env, clipdata,
                                            gdk_android_get_java_cache ()->a_clipdata.get_item_count);
      for (jint i = 0; i < n_items; i++)
        {
          jobject item = (*env)->CallObjectMethod (env, clipdata,
                                                   gdk_android_get_java_cache ()->a_clipdata.get_item, i);
          jobject uri = (*env)->CallObjectMethod (env, item,
                                                  gdk_android_get_java_cache ()->a_clipdata_item.get_uri);
          if (uri)
            {
              GFile *file = gdk_android_content_file_from_uri (uri);
              self->custom_files = g_slist_prepend (self->custom_files, file);
            }
          else
            g_warning ("A file returned from document picker did not have a document attached");
          (*env)->DeleteLocalRef (env, uri);
          (*env)->DeleteLocalRef (env, item);
        }
    }

  self->custom_files = g_slist_reverse (self->custom_files);
  _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (self), GTK_RESPONSE_ACCEPT);
exit:
  (*env)->PopLocalFrame (env, NULL);

  g_object_unref (((GtkFileChooserNativeAndroidData *)self->mode_data)->cancellable);
  g_clear_pointer (&self->mode_data, g_free);
  g_object_unref (self);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

gboolean
gtk_file_chooser_native_android_show (GtkFileChooserNative *self)
{
  GtkWindow *transient_for = gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (self));
  if (!transient_for)
    {
      g_critical ("Android filepicker needs to be a transient dialog!");
      return FALSE;
    }

  GtkFileChooserNativeAndroidData *data = g_new(GtkFileChooserNativeAndroidData, 1);
  data->cancellable = g_cancellable_new();

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 8);

  GtkFileChooserAction action = gtk_file_chooser_get_action ((GtkFileChooser *)self);
  jstring jaction;
  switch (action)
    {
      case GTK_FILE_CHOOSER_ACTION_SAVE:
        jaction = gdk_android_get_java_cache ()->a_intent.action_create_document;
        break;
      case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
        jaction = gdk_android_get_java_cache ()->a_intent.action_open_document_tree;
        break;
      case GTK_FILE_CHOOSER_ACTION_OPEN:
      default:
        jaction = gdk_android_get_java_cache ()->a_intent.action_open_document;
        break;
    }

  jobject intent = (*env)->NewObject (env, gdk_android_get_java_cache ()->a_intent.klass,
                                      gdk_android_get_java_cache ()->a_intent.constructor_action,
                                      jaction);

  (*env)->CallObjectMethod (env, intent,
                            gdk_android_get_java_cache ()->a_intent.put_extra_bool,
                            gdk_android_get_java_cache ()->a_intent.extra_allow_multiple,
                            gtk_file_chooser_get_select_multiple ((GtkFileChooser *)self->dialog));

  if (action != GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      GListModel *filters = gtk_file_chooser_get_filters ((GtkFileChooser *)self);
      guint n_filters = g_list_model_get_n_items (filters);
      if (n_filters > 0)
        {
          jobject list = (*env)->NewObject (env, gdk_android_get_java_cache ()->j_arraylist.klass,
                                            gdk_android_get_java_cache ()->j_arraylist.constructor);

          for (guint i = 0; i < n_filters; i++)
            {
              GtkFileFilter *filter = g_list_model_get_item (filters, i);
              _gtk_file_filter_store_types_in_list (filter, list);
              g_object_unref (filter);
            }

          jobject any = (*env)->NewStringUTF (env, "*/*");
          (*env)->CallObjectMethod (env, intent,
                                    gdk_android_get_java_cache ()->a_intent.set_type,
                                    any);

          jobjectArray mime_filters = (*env)->CallObjectMethod (env, list,
                                                                gdk_android_get_java_cache ()->j_list.to_array,
                                                                (*env)->NewObjectArray (env,
                                                                                        0,
                                                                                        gdk_android_get_java_cache ()->j_string.klass,
                                                                                        NULL));

          (*env)->CallObjectMethod (env, intent,
                                    gdk_android_get_java_cache ()->a_intent.put_extra_string_array,
                                    gdk_android_get_java_cache ()->a_intent.extra_mimetypes,
                                    mime_filters);
        }
      else
        {
          jobject any = (*env)->NewStringUTF (env, "*/*");
          (*env)->CallObjectMethod (env, intent,
                                    gdk_android_get_java_cache ()->a_intent.set_type,
                                    any);
        }
    }

  GdkAndroidToplevel* parent = (GdkAndroidToplevel *)gtk_native_get_surface (GTK_NATIVE (transient_for));
  gdk_android_toplevel_launch_activity_for_result_async (parent,
                                                         intent,
                                                         data->cancellable,
                                                         (GAsyncReadyCallback)gtk_file_chooser_native_android_result,
                                                         g_object_ref (self));
  self->mode_data = data;

  (*env)->PopLocalFrame (env, NULL);
  return TRUE;
}

G_GNUC_END_IGNORE_DEPRECATIONS

void
gtk_file_chooser_native_android_hide (GtkFileChooserNative *self)
{
  GtkFileChooserNativeAndroidData *data = self->mode_data;

  /* This is always set while dialog visible */
  g_assert (data != NULL);

  g_cancellable_cancel (data->cancellable);
}
