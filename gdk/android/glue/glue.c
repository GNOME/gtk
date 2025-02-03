#include <jni.h>
// Produced by "javac -cp ${ANDROID_HOME}/platforms/android-34/android.jar:~/.gradle/caches/modules-2/files-2.1/androidx.annotation/annotation/1.3.0/21f49f5f9b85fc49de712539f79123119740595/annotation-1.3.0.jar:java -h . java/org/gtk/android/GlueLibraryContext.java"
#include <org_gtk_android_GlueLibraryContext.h>

#include <glib.h>
#include <gdk/android/gdkandroid.h>

#include <android/log.h>
#include <android/looper.h>
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "GTK glue", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "GTK glue", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "GTK glue", __VA_ARGS__))

// BEGIN static init routine

static void android_print_handler(const char* message) {
  __android_log_print(ANDROID_LOG_INFO, "print", "%s", message);
}
static void android_printerr_handler(const char* message) {
  __android_log_print(ANDROID_LOG_WARN, "print", "%s", message);
}
static int glib_log_level_to_android(GLogLevelFlags log_level) {
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
static void android_log_handler(const gchar* log_domain, GLogLevelFlags log_level, const char* message, G_GNUC_UNUSED gpointer user_data) {
  __android_log_print(glib_log_level_to_android(log_level), log_domain, "%s", message);
}

static GLogWriterOutput android_structured_log_handler(GLogLevelFlags log_level, const GLogField* fields, gsize n_fields, gpointer user_data) {
  const gchar* domain = NULL;
  const gchar* message = NULL;

  for (guint i = 0; (!domain || !message) && i < n_fields; i++) {
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

  int rc = __android_log_print(glib_log_level_to_android(log_level), domain, "%s", message);
  return rc == 0 ? G_LOG_WRITER_HANDLED : G_LOG_WRITER_UNHANDLED;
}


typedef struct {
  JavaVM* vm;
  jobject class_loader;
  jobject activity;
  pthread_barrier_t application_available;
  gboolean available_check_completed;
  gint exitfd;
} JavaContainer;

static gboolean application_available_tester(JavaContainer* container) {
  if (g_application_get_default()) {
    container->available_check_completed = TRUE;
    pthread_barrier_wait(&container->application_available);
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

static int gtk_exit_looper_cb(int fd, int events, GThread* thread) {
  close(fd);
  gint ret = GPOINTER_TO_INT(g_thread_join(thread));
  // TODO: figure out if it's possible to differentiate between a process
  //  spawned by forking off of zygote and a new process (this usually happens
  //  when using wrap.sh). Processes spawned of zygote have to call _exit, as
  //  it's impossible to exit them cleanly. Newly spawned processes seem to cope
  //  with exit, as long as it is on the main thread. See
  //  https://android.googlesource.com/platform/frameworks/base/+/b5bd3c2/core/java/com/android/internal/os/RuntimeInit.java#376
  exit(ret);
}

int main(int argc, char** argv, char** envp);
static void* gtk_thread(JavaContainer* container) {
  LOGD("REACHED GTK THREAD");
  JNIEnv* env;
  JavaVMAttachArgs jargs = {
      .version = JNI_VERSION_1_6,
      .name = "GTK Thread",
      .group = NULL
  };
  int rc = (*container->vm)->AttachCurrentThread(container->vm, &env, &jargs);
  if (rc != JNI_OK) {
    LOGE("Unable to attach thread to JVM: Error %d", rc);
    pthread_exit((void*)1);
  }

  gdk_android_initialize(env, container->class_loader, container->activity);

  container->available_check_completed = FALSE;
  g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)application_available_tester, container, NULL);

  char* argv[] = { "android-gtk", NULL };
  LOGD("CALLING MAIN");
  int ret = main(1, argv, environ);

  // Ideally, this should not be reached as a g_application_hold is active. But in cases where
  // g_application_quit was called, main shall return. We'll just clean up a bit and then exit
  // the process to have the OS give us a "clean" process.
  LOGW("MAIN RETURNED WITH %d", ret);

  if (!container->available_check_completed) {
    LOGE("GLib eventloop never ran. This is not supposed to happen!");
    pthread_barrier_wait(&container->application_available);
  }

  gdk_android_finalize();

  (*env)->DeleteGlobalRef(env, container->activity);
  (*env)->DeleteGlobalRef(env, container->class_loader);

  rc = (*container->vm)->DetachCurrentThread(container->vm);
  if (rc != JNI_OK) {
    LOGE("Unable to detach thread to JVM: Error %d", rc);
    ret = -1;
  }
  gint exitfd = container->exitfd;
  g_free(container);

  // If this is reached, we could either attempt to restart the GTK thread, potentially resulting
  // in an infinite loop in cases where g_application_run immediately exists (like when
  // g_application_quit has been called) or just exit the process to let the OS handle cleanup &&
  // reinitalization at a later date.
  // We can only call exit on the main thread (someone probably registered non threadsafe exit
  // handlers). By closing this exitfd, we signal the looper to join the gtk thread.
  close(exitfd);
  return GINT_TO_POINTER(ret);
}

static gchar* java_to_utf8(JNIEnv *env, jstring string) {
  jsize len = (*env)->GetStringLength(env, string);
  const jchar* utf16 = (*env)->GetStringChars(env, string, NULL);

  GError *err = NULL;
  gchar* utf8 = g_utf16_to_utf8(utf16, len, NULL, NULL, &err);
  if (err) {
    jclass char_conversion_exception = (*env)->FindClass(env, "java/io/CharConversionException");
    (*env)->ThrowNew(env, char_conversion_exception, err->message);
    g_error_free(err);
  }
  (*env)->ReleaseStringChars(env, string, utf16);
  return utf8;
}
static gchar* path_of_dir(JNIEnv *env, jobject dir) {
  jclass file = (*env)->FindClass(env, "java/io/File");
  jmethodID get_path = (*env)->GetMethodID(env, file, "getAbsolutePath", "()Ljava/lang/String;");
  jobject path = (*env)->CallObjectMethod(env, dir, get_path);
  return java_to_utf8(env, path);
}

void
g_set_user_dirs (const gchar *first_dir_type,
         ...);

static GThread* gtk_thread_s = NULL;
JNIEXPORT void JNICALL Java_org_gtk_android_GlueLibraryContext_runApplication(JNIEnv *env, jclass clazz, jobject activity) {
  jclass looper_class = (*env)->FindClass(env, "android/os/Looper");
  jmethodID get_main_looper = (*env)->GetStaticMethodID(env, looper_class, "getMainLooper", "()Landroid/os/Looper;");
  jmethodID is_current_thread = (*env)->GetMethodID(env, looper_class, "isCurrentThread", "()Z");
  jobject looper = (*env)->CallStaticObjectMethod(env, looper_class, get_main_looper);
  if (!(*env)->CallBooleanMethod(env, looper, is_current_thread)) {
    LOGE("ApplicationLaunchContext.bind called on non-main thread. This is forbidden!");
    return;
  }

  if (gtk_thread_s)
    return;

  LOGD("Reached GTK Android entrypoint");

  g_set_print_handler(android_print_handler);
  g_set_printerr_handler(android_printerr_handler);
  g_log_set_default_handler(android_log_handler, NULL);
  g_log_set_writer_func(android_structured_log_handler, NULL, NULL);

  jclass ctx = (*env)->FindClass(env, "android/content/Context");
  jmethodID get_files_dir = (*env)->GetMethodID(env, ctx, "getFilesDir", "()Ljava/io/File;");
  jobject files_dir = (*env)->CallObjectMethod(env, activity, get_files_dir);
  gchar* files_path = path_of_dir(env, files_dir);
  g_autofree gchar* configdir = g_build_filename(files_path, "etc", NULL);
  g_autofree gchar* datadir = g_build_filename(files_path, "share", NULL);
  g_free(files_path);
  gchar* config_dirs[] = { configdir, NULL };
  gchar* data_dirs[] = { datadir, NULL };

  jmethodID get_ext_files_dir = (*env)->GetMethodID(env, ctx, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
  jobject ext_files_dir = (*env)->CallObjectMethod(env, activity, get_ext_files_dir, NULL);
  gchar* ext_files_path = path_of_dir(env, ext_files_dir);
  g_autofree gchar* userconfigdir = g_build_filename(ext_files_path, "etc", NULL);
  g_autofree gchar* userdatadir = g_build_filename(ext_files_path, "share", NULL);
  g_free(ext_files_path);

  g_set_user_dirs (
      "XDG_CONFIG_DIRS", config_dirs,
      "XDG_DATA_DIRS", data_dirs,
      "XDG_CONFIG_HOME", userconfigdir,
      "XDG_DATA_HOME", userdatadir,
      NULL);

  jclass system_filesystem_class = (*env)->FindClass(env, "org/gtk/android/SystemFilesystem");
  jmethodID write_resources = (*env)->GetStaticMethodID(env, system_filesystem_class, "writeResources", "(Landroid/content/Context;)V");
  (*env)->CallStaticVoidMethod(env, system_filesystem_class, write_resources, activity);

  jclass object_class = (*env)->FindClass(env, "java/lang/Class");
  jmethodID get_classloader = (*env)->GetMethodID(env, object_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
  jobject class_loader = (*env)->CallObjectMethod(env, clazz, get_classloader);

  JavaContainer* container = g_new(JavaContainer, 1);
  gint rc = (*env)->GetJavaVM(env, &container->vm);
  g_assert(rc == JNI_OK);
  container->class_loader = (*env)->NewGlobalRef(env, class_loader);
  container->activity = (*env)->NewGlobalRef(env, activity);

  gint pipefd[2];
  rc = pipe(pipefd);
  g_assert(rc >= 0);
  container->exitfd = pipefd[1];

  pthread_barrier_init(&container->application_available, NULL, 2);

  gtk_thread_s = g_thread_new("GTK Thread", (GThreadFunc)gtk_thread, container);

  rc = ALooper_addFd(ALooper_forThread(),
                     pipefd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_HANGUP,
                     (ALooper_callbackFunc) gtk_exit_looper_cb, gtk_thread_s);
  g_assert(rc >= 0);

  pthread_barrier_wait(&container->application_available);
}
