/*
 * Copyright (c) 2025 Florian "sp1rit" <sp1rit@disroot.org>
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

#include <glib.h>
#include <gmodule.h>

#include <gdk/android/gdkandroid.h>
#include <gdk/android/gdkandroidinit-private.h>
#include <gdk/android/gdkandroidutils-private.h>

#include <android/log.h>
#include <android/looper.h>

static void
gdk_android_runtime_print_handler (const char *message)
{
  __android_log_print (ANDROID_LOG_INFO, "print", "%s", message);
}
static void
gdk_android_runtime_printerr_handler (const char *message)
{
  __android_log_print (ANDROID_LOG_WARN, "print", "%s", message);
}

static int
gdk_android_runtime_glib_log_level_to_android (GLogLevelFlags log_level)
{
  if (log_level & G_LOG_LEVEL_ERROR)
    return ANDROID_LOG_FATAL;
  else if (log_level & G_LOG_LEVEL_CRITICAL)
    return ANDROID_LOG_ERROR;
  else if (log_level & G_LOG_LEVEL_WARNING)
    return ANDROID_LOG_WARN;
  else if (log_level & G_LOG_LEVEL_MESSAGE)
    return ANDROID_LOG_INFO;
  else if (log_level & G_LOG_LEVEL_INFO)
    return ANDROID_LOG_INFO;
  else if (log_level & G_LOG_LEVEL_DEBUG)
    return ANDROID_LOG_DEBUG;
  else
    return ANDROID_LOG_WARN;
}

static void
gdk_android_runtime_log_handler (const gchar            *log_domain,
                                 GLogLevelFlags          log_level,
                                 const char             *message,
                                 G_GNUC_UNUSED gpointer  user_data)
{
  __android_log_print (gdk_android_runtime_glib_log_level_to_android (log_level),
                       log_domain,
                       "%s", message);
}

static GLogWriterOutput
gdk_android_runtime_structured_log_handler (GLogLevelFlags          log_level,
                                            const GLogField        *fields,
                                            gsize                   n_fields,
                                            G_GNUC_UNUSED gpointer  user_data)
{
  const gchar *domain = NULL;
  const gchar *message = NULL;

  for (guint i = 0; (!domain || !message) && i < n_fields; i++)
    {
      const GLogField *field = &fields[i];
      if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
        domain = field->value;
      else if (g_strcmp0 (field->key, "MESSAGE") == 0)
        message = field->value;
    }

  if (!domain)
    domain = "**";

  if (!message)
    message = "(empty)";

  int rc = __android_log_print (gdk_android_runtime_glib_log_level_to_android (log_level),
                                domain,
                                "%s", message);
  return rc == 0 ? G_LOG_WRITER_HANDLED : G_LOG_WRITER_UNHANDLED;
}

typedef int (*main_entrypoint)(int argc, char **argv, char **envp);
typedef struct
{
  JavaVM *vm;
  main_entrypoint application_entrypoint;
  pthread_barrier_t application_available;
  gboolean available_check_completed;
  gint exitfd;
} GdkAndroidRuntimeData;

static gboolean
gdk_android_runtime_application_available_tester (GdkAndroidRuntimeData *data)
{
  if (g_application_get_default ())
    {
      data->available_check_completed = TRUE;
      pthread_barrier_wait (&data->application_available);
      return G_SOURCE_REMOVE;
    }
  return G_SOURCE_CONTINUE;
}

static int
gdk_android_runtime_exit_looper_cb (int fd, int events, GThread *thread)
{
  close (fd);
  gint ret = GPOINTER_TO_INT (g_thread_join (thread));
  gdk_android_finalize ();
  // TODO: figure out if it's possible to differentiate between a process
  //  spawned by forking off of zygote and a new process (this usually happens
  //  when using wrap.sh). Processes spawned of zygote have to call _exit, as
  //  it's impossible to exit them cleanly. Newly spawned processes seem to cope
  //  with exit, as long as it is on the main thread. See
  //  https://android.googlesource.com/platform/frameworks/base/+/b5bd3c2/core/java/com/android/internal/os/RuntimeInit.java#376
  exit (ret);
}

static void *
gdk_android_runtime_gtk_thread (GdkAndroidRuntimeData *data)
{
  __android_log_print (ANDROID_LOG_DEBUG,
                       "GTK Runtime",
                       "Reached GTK Thread");

  JNIEnv *env;
  JavaVMAttachArgs jargs = {
      .version = JNI_VERSION_1_6,
      .name = "GTK Thread",
      .group = NULL
  };
  int rc = (*data->vm)->AttachCurrentThread (data->vm, &env, &jargs);
  if (rc != JNI_OK)
    {
      __android_log_print (ANDROID_LOG_ERROR,
                           "GTK Runtime",
                           "Unable to attach thread to JVM: Error %d", rc);
      gint exitfd = data->exitfd;
      g_free (data);
      // see the close() below for more details
      close (exitfd);
      pthread_exit (GINT_TO_POINTER (-1));
    }

  data->available_check_completed = FALSE;
  g_idle_add_full (G_PRIORITY_LOW,
                   (GSourceFunc)gdk_android_runtime_application_available_tester,
                   data,
                   NULL);

  char *argv[] = { "android-gtk", NULL };
  __android_log_print (ANDROID_LOG_DEBUG,
                       "GTK Runtime",
                       "Calling main()");
  int ret = data->application_entrypoint (1, argv, environ);
  // Ideally, this should not be reached as a g_application_hold is active. But in cases where
  // g_application_quit was called, main shall return. We'll just clean up a bit and then exit
  // the process to have the OS give us a "clean" process.
  __android_log_print (ANDROID_LOG_WARN,
                       "GTK Runtime",
                       "main() returned with %d", ret);

  if (!data->available_check_completed)
    {
      __android_log_print (ANDROID_LOG_ERROR,
                           "GTK Runtime",
                           "GLib eventloop never ran. This is not supposed to happen!");
      pthread_barrier_wait (&data->application_available);
    }

  rc = (*data->vm)->DetachCurrentThread (data->vm);
  if (rc != JNI_OK)
    {
      __android_log_print (ANDROID_LOG_ERROR,
                           "GTK Runtime",
                           "Unable to detach thread to JVM: Error %d", rc);
      ret = -1;
    }
  gint exitfd = data->exitfd;
  g_free (data);

  // If this is reached, we could either attempt to restart the GTK thread, potentially resulting
  // in an infinite loop in cases where g_application_run immediately exists (like when
  // g_application_quit has been called) or just exit the process to let the OS handle cleanup &&
  // reinitalization at a later date.
  // We can only call exit on the main thread (someone probably registered non threadsafe exit
  // handlers). By closing this exitfd, we signal the looper to join the gtk thread.
  close (exitfd);
  return GINT_TO_POINTER (ret);
}

static gchar *
gdk_android_runtime_path_of_dir (JNIEnv *env, jobject dir)
{
  jclass file = (*env)->FindClass (env, "java/io/File");
  jmethodID get_path = (*env)->GetMethodID (env, file, "getAbsolutePath", "()Ljava/lang/String;");
  jobject path = (*env)->CallObjectMethod (env, dir, get_path);
  return gdk_android_java_to_utf8 (path, NULL);
}

void g_set_user_dirs (const char *first_dir_type, ...) G_GNUC_NULL_TERMINATED;

static GThread *gtk_thread_s = NULL;

static void
_gdk_android_application_start_runtime (JNIEnv  *env,
                                        jobject  thiz,
                                        jstring  application_library)
{
  jclass looper_class = (*env)->FindClass (env, "android/os/Looper");
  jmethodID get_main_looper = (*env)->GetStaticMethodID (env, looper_class, "getMainLooper", "()Landroid/os/Looper;");
  jmethodID is_current_thread = (*env)->GetMethodID (env, looper_class, "isCurrentThread", "()Z");
  jobject looper = (*env)->CallStaticObjectMethod (env, looper_class, get_main_looper);
  if (!(*env)->CallBooleanMethod (env, looper, is_current_thread))
    {
      jclass illegal_state_exception = (*env)->FindClass (env, "java/lang/IllegalStateException");
      (*env)->ThrowNew (env,
                        illegal_state_exception,
                        "RuntimeApplication.startRuntime called on non-main thread. This is forbidden!");
      return;
    }

  if (gtk_thread_s)
    return;

  __android_log_print (ANDROID_LOG_DEBUG,
                       "GTK Runtime",
                       "Starting GTK Android runtime");

  g_set_print_handler (gdk_android_runtime_print_handler);
  g_set_printerr_handler (gdk_android_runtime_printerr_handler);
  g_log_set_default_handler (gdk_android_runtime_log_handler, NULL);
  g_log_set_writer_func (gdk_android_runtime_structured_log_handler, NULL, NULL);

  // This is *really* questionable, as thiz isn't actually an activity.
  // I've updated the code to handle this case, but it should be replaced
  // once glib gains java/android support.
  gdk_android_set_latest_activity (env, thiz);

  jclass ctx = (*env)->FindClass (env, "android/content/Context");
  jmethodID get_files_dir = (*env)->GetMethodID (env, ctx, "getFilesDir", "()Ljava/io/File;");
  jobject files_dir = (*env)->CallObjectMethod (env, thiz, get_files_dir);
  gchar *files_path = gdk_android_runtime_path_of_dir (env, files_dir);
  gchar *configdir = g_build_filename (files_path, "etc", NULL);
  gchar *datadir = g_build_filename (files_path, "share", NULL);
  g_free (files_path);
  gchar *config_dirs[] = { configdir, NULL };
  gchar *data_dirs[] = { datadir, NULL };

  jmethodID get_ext_files_dir = (*env)->GetMethodID (env, ctx, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
  jobject ext_files_dir = (*env)->CallObjectMethod (env, thiz, get_ext_files_dir, NULL);
  gchar *ext_files_path = gdk_android_runtime_path_of_dir (env, ext_files_dir);
  gchar *userconfigdir = g_build_filename (ext_files_path, "etc", NULL);
  gchar *userdatadir = g_build_filename (ext_files_path, "share", NULL);
  g_free (ext_files_path);

  g_set_user_dirs ("XDG_CONFIG_DIRS", config_dirs,
                   "XDG_DATA_DIRS", data_dirs,
                   "XDG_CONFIG_HOME", userconfigdir,
                   "XDG_DATA_HOME", userdatadir,
                   NULL);

  g_free (configdir);
  g_free (datadir);
  g_free (userconfigdir);
  g_free (userdatadir);

  GdkAndroidRuntimeData *data = g_new (GdkAndroidRuntimeData, 1);
  gint rc = (*env)->GetJavaVM(env, &data->vm);
  g_assert(rc == JNI_OK);

  jclass link_error_class = (*env)->FindClass (env, "java/lang/UnsatisfiedLinkError");

  gchar *application_library_str = gdk_android_java_to_utf8 (application_library, NULL);
  GError *err = NULL;
  GModule *application = g_module_open_full (application_library_str, 0, &err);
  if (err)
    {
      gchar *errmsg = g_strdup_printf ("Unable to open library \"%s\": %s",
                                       application_library_str,
                                       err->message);
      (*env)->ThrowNew (env, link_error_class, errmsg);
      g_free (errmsg);

      g_error_free (err);
      g_free (data);
      return;
  }
  g_free (application_library_str);

  if (!g_module_symbol (application, "main", (gpointer *)&data->application_entrypoint))
    {
      gchar *errmsg = g_strdup_printf ("Unable to find entrypoint \"main\" in application library \"%s\". "
                                       "Ensure that the library is correct and the function is visible. "
                                       "Perhaps you need to prefix it with G_MODULE_EXPORT?",
                                       g_module_name (application));
      (*env)->ThrowNew (env, link_error_class, errmsg);
      g_free (errmsg);

      g_error_free (err);
      g_module_close (application);
      g_free (data);
      return;
    }
  g_module_make_resident (application);

  gint pipefd[2];
  rc = pipe (pipefd);
  g_assert (rc >= 0);
  data->exitfd = pipefd[1];

  pthread_barrier_init (&data->application_available, NULL, 2);

  gtk_thread_s = g_thread_new ("GTK Thread", (GThreadFunc)gdk_android_runtime_gtk_thread, data);

  rc = ALooper_addFd (ALooper_forThread(),
                      pipefd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_HANGUP,
                      (ALooper_callbackFunc)gdk_android_runtime_exit_looper_cb, gtk_thread_s);
  g_assert (rc >= 0);

  pthread_barrier_wait (&data->application_available);
}


#define RUNTIME_APPLICATION_CLASS "org/gtk/android/RuntimeApplication"
static const JNINativeMethod
gdk_android_runtime_application_natives[] = {
  { .name = "startRuntime", .signature = "(Ljava/lang/String;)V", .fnPtr = _gdk_android_application_start_runtime }
};

JNIEXPORT jint JNICALL
JNI_OnLoad (JavaVM *vm, gpointer reserved)
{
  static gboolean initialized = FALSE;
  if (initialized)
    return JNI_VERSION_1_6;

  JNIEnv *env;
  if ((*vm)->GetEnv (vm, (gpointer *)&env, JNI_VERSION_1_6) != JNI_OK)
    return JNI_ERR;

  jclass runtime_application_class = (*env)->FindClass (env, RUNTIME_APPLICATION_CLASS);
  if (runtime_application_class)
    {
      (*env)->RegisterNatives (env,
                               runtime_application_class,
                               gdk_android_runtime_application_natives,
                               G_N_ELEMENTS (gdk_android_runtime_application_natives));

      jclass class_class = (*env)->FindClass (env, "java/lang/Class");
      jmethodID get_class_loader = (*env)->GetMethodID (env, class_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
      jobject class_loader = (*env)->CallObjectMethod (env, runtime_application_class, get_class_loader);

      if (!gdk_android_initialize (env, class_loader, NULL))
          return JNI_ERR;
    }
  else
    {
      __android_log_print (ANDROID_LOG_INFO,
                           "GTK Runtime",
                           "Did not find \"" RUNTIME_APPLICATION_CLASS "\" in class path, skipping runtime initialization.");
      (*env)->ExceptionClear (env);
    }

  initialized = TRUE;
  return JNI_VERSION_1_6;
}
