/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2008  Jaap Haitsma <jaap@haitsma.org>
 *
 * All rights reserved.
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

#include <gdk/gdk.h>

#include "gtkshow.h"
#include "gtkwindowprivate.h"
#include "gtkalertdialog.h"
#include <glib/gi18n-lib.h>

#ifdef G_OS_WIN32
#include "gtkshowwin32.h"
#endif

#ifdef GDK_WINDOWING_ANDROID
#include "gtknative.h"

#include "android/gdkandroidinit-private.h"
#include "android/gdkandroidutils-private.h"
#include "android/gdkandroidtoplevel-private.h"
#endif // GDK_WINDOWING_ANDROID

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#if defined (G_OS_WIN32)
static void
show_win32_done (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GTask *task = user_data;
  GError *error = NULL;

  if (gtk_show_uri_win32_finish (GTK_WINDOW (source), result, &error))
    g_task_return_boolean (task, TRUE);
  else
     g_task_return_error (task, error);

  g_object_unref (task);
}

#elif defined (GDK_WINDOWING_ANDROID)
static gboolean
gtk_show_uri_android (const gchar         *uri,
                      GdkAndroidToplevel  *toplevel,
                      GError             **error)
{
  JNIEnv *env = gdk_android_get_env ();

  (*env)->PushLocalFrame (env, 8);

  jobject juri = (*env)->CallStaticObjectMethod (env, gdk_android_get_java_cache ()->a_uri.klass,
                                                 gdk_android_get_java_cache ()->a_uri.parse,
                                                 gdk_android_utf8_to_java (uri));
  if (gdk_android_check_exception (error))
    {
      (*env)->PopLocalFrame (env, NULL);
      return FALSE;
    }

  jobject intent = (*env)->NewObject (env, gdk_android_get_java_cache ()->a_intent.klass,
                                      gdk_android_get_java_cache ()->a_intent.constructor_action,
                                      gdk_android_get_java_cache ()->a_intent.action_view);
  (*env)->CallObjectMethod (env, intent,
                            gdk_android_get_java_cache ()->a_intent.set_data_norm,
                            juri);
  (*env)->CallObjectMethod (env, intent,
                            gdk_android_get_java_cache ()->a_intent.add_flags,
                            gdk_android_get_java_cache ()->a_intent.flag_grant_read_perm);

  jobject bundle = (*env)->NewObject (env, gdk_android_get_java_cache ()->a_bundle.klass,
                                      gdk_android_get_java_cache ()->a_bundle.constructor);
  (*env)->CallVoidMethod (env, bundle,
                          gdk_android_get_java_cache ()->a_bundle.put_binder,
                          gdk_android_get_java_cache ()->a_intent.extra_customtabs_session,
                          NULL);
  (*env)->CallObjectMethod (env, intent,
                            gdk_android_get_java_cache ()->a_intent.put_extras_from_bundle,
                            bundle);

  // TODO: there should probably be a mechanism for defining an accent color, as this right now uses
  //       gdk_android_toplevel_get_bars_color which returns the default GTK light/dark background color,
  //       but that wouldn't typically be seen as "accent".
  (*env)->CallObjectMethod (env, intent,
                            gdk_android_get_java_cache ()->a_intent.put_extra_int,
                            gdk_android_get_java_cache ()->a_intent.extra_customtabs_toolbar_color,
                            gdk_android_utils_color_to_android (gdk_android_toplevel_get_bars_color (toplevel)));

  gboolean launched = gdk_android_toplevel_launch_activity (toplevel, intent, error);
  (*env)->PopLocalFrame (env, NULL);
  return launched;
}
#else
typedef struct {
  GtkWindow *parent;
  char *handle;
  GAppLaunchContext *context;
  char *uri;
  GTask *task;
} GtkShowUriData;

static void
gtk_show_uri_data_free (GtkShowUriData *data)
{
  if (data->parent && data->handle)
    gtk_window_unexport_handle (data->parent, data->handle);
  g_clear_object (&data->parent);
  g_free (data->handle);
  g_clear_object (&data->context);
  g_free (data->uri);
  g_clear_object (&data->task);
  g_free (data);
}

static void
launch_uri_done (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GtkShowUriData *data = user_data;
  GError *error = NULL;

  if (g_app_info_launch_default_for_uri_finish (result, &error))
    g_task_return_boolean (data->task, TRUE);
  else
    g_task_return_error (data->task, error);

  gtk_show_uri_data_free (data);
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle,
                        gpointer    user_data)
{
  GtkShowUriData *data = user_data;

  if (handle)
    {
      g_app_launch_context_setenv (data->context, "PARENT_WINDOW_ID", handle);
      data->handle = g_strdup (handle);
    }

  g_app_info_launch_default_for_uri_async (data->uri,
                                           data->context,
                                           g_task_get_cancellable (data->task),
                                           launch_uri_done,
                                           data);
}

#endif

/**
 * gtk_show_uri_full:
 * @parent: (nullable): parent window
 * @uri: the uri to show
 * @timestamp: timestamp from the event that triggered this call, or %GDK_CURRENT_TIME
 * @cancellable: (nullable): a `GCancellable` to cancel the launch
 * @callback: (scope async) (closure user_data): a callback to call when the action is complete
 * @user_data: data to pass to @callback
 *
 * This function launches the default application for showing
 * a given uri.
 *
 * The @callback will be called when the launch is completed.
 *
 * This is the recommended call to be used as it passes information
 * necessary for sandbox helpers to parent their dialogs properly.
 *
 * Deprecated: 4.10: Use [method@Gtk.FileLauncher.launch] or
 *   [method@Gtk.UriLauncher.launch] instead
 */
void
gtk_show_uri_full (GtkWindow           *parent,
                   const char          *uri,
                   guint32              timestamp,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
#if defined (G_OS_WIN32)
  GTask *task;

  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (uri != NULL);

  task = g_task_new (parent, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_show_uri_full);
  gtk_show_uri_win32 (parent, uri, FALSE, cancellable, show_win32_done, task);

#elif defined (GDK_WINDOWING_ANDROID)
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (uri != NULL);

  GTask *task = g_task_new (parent, cancellable, callback, user_data);
  g_task_set_source_tag (task, gtk_show_uri_full);

  GdkAndroidToplevel* toplevel = GDK_ANDROID_TOPLEVEL (gtk_native_get_surface (GTK_NATIVE (parent)));
  GError *err = NULL;
  if (gtk_show_uri_android (uri, toplevel, &err))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, err);
  g_object_unref (task);
#else
  GtkShowUriData *data;
  GdkAppLaunchContext *context;
  GdkDisplay *display;

  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (uri != NULL);

  if (parent)
    display = gtk_widget_get_display (GTK_WIDGET (parent));
  else
    display = gdk_display_get_default ();

  context = gdk_display_get_app_launch_context (display);
  gdk_app_launch_context_set_timestamp (context, timestamp);

  data = g_new0 (GtkShowUriData, 1);
  data->parent = parent ? g_object_ref (parent) : NULL;
  data->context = G_APP_LAUNCH_CONTEXT (context);
  data->uri = g_strdup (uri);
  data->task = g_task_new (parent, cancellable, callback, user_data);
  g_task_set_source_tag (data->task, gtk_show_uri_full);

  if (!parent || !gtk_window_export_handle (parent, window_handle_exported, data))
    window_handle_exported (parent, NULL, data);
#endif
}

/**
 * gtk_show_uri_full_finish:
 * @parent: the `GtkWindow` passed to gtk_show_uri()
 * @result: `GAsyncResult` that was passed to @callback
 * @error: return location for an error
 *
 * Finishes the gtk_show_uri() call and returns the result
 * of the operation.
 *
 * Returns: %TRUE if the URI was shown successfully.
 *   Otherwise, %FALSE is returned and @error is set
 *
 * Deprecated: 4.10: Use [method@Gtk.FileLauncher.launch] or
 *   [method@Gtk.UriLauncher.launch] instead
 */
gboolean
gtk_show_uri_full_finish (GtkWindow     *parent,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, parent), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_show_uri_full, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
show_uri_done (GObject      *object,
               GAsyncResult *result,
               gpointer      data)
{
  GtkWindow *parent = GTK_WINDOW (object);
  GError *error = NULL;

  if (!gtk_show_uri_full_finish (parent, result, &error))
    {
      GtkAlertDialog *dialog;

      dialog = gtk_alert_dialog_new ("%s", _("Could not show link"));
      gtk_alert_dialog_set_detail (dialog, error->message);
      gtk_alert_dialog_show (dialog, parent);
      g_object_unref (dialog);

      g_error_free (error);
    }
}

/**
 * gtk_show_uri:
 * @parent: (nullable): parent window
 * @uri: the uri to show
 * @timestamp: timestamp from the event that triggered this call, or %GDK_CURRENT_TIME
 *
 * This function launches the default application for showing
 * a given uri, or shows an error dialog if that fails.
 *
 * Deprecated: 4.10: Use [method@Gtk.FileLauncher.launch] or
 *   [method@Gtk.UriLauncher.launch] instead
 */
void
gtk_show_uri (GtkWindow  *parent,
              const char *uri,
              guint32     timestamp)
{
  gtk_show_uri_full (parent, uri, timestamp, NULL, show_uri_done, NULL);
}
