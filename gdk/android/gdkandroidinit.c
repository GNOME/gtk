/*
 * Copyright (c) 2024-2025 Florian "sp1rit" <sp1rit@disroot.org>
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

#include "config.h"

#include <android/log.h>

#include "gdkandroiddisplay-private.h"
#include "gdkandroidcontentfile-private.h"
#include "gdkandroidclipboard-private.h"
#include "gdkandroidsurface-private.h"
#include "gdkandroidtoplevel-private.h"
#include "gdkandroidutils-private.h"

#include "gdkandroidinit-private.h"

static const JNINativeMethod glib_context_natives[] = {
  { .name = "runOnMain", .signature = "(Ljava/lang/Runnable;)V", .fnPtr = _gdk_android_glib_context_run_on_main }
};

static void _gdk_android_gdk_context_activate (JNIEnv *env, jclass this);
static void _gdk_android_gdk_context_open (JNIEnv *env, jclass this, jobject uri, jstring jhint);
static const JNINativeMethod gdk_context_natives[] = {
  { .name = "activate", .signature = "()V", .fnPtr = _gdk_android_gdk_context_activate },
  { .name = "open", .signature = "(Landroid/net/Uri;Ljava/lang/String;)V", .fnPtr = _gdk_android_gdk_context_open }
};

static const JNINativeMethod clipboard_provider_change_listener_natives[] = {
  { .name = "notifyClipboardChange", .signature = "()V", .fnPtr = _gdk_android_clipboard_on_clipboard_changed }
};

static const JNINativeMethod surface_natives[] = {
  { .name = "bindNative", .signature = "(J)V", .fnPtr = _gdk_android_surface_bind_native },
  { .name = "notifyAttached", .signature = "()V", .fnPtr = _gdk_android_surface_on_attach },
  { .name = "notifyLayoutSurface", .signature = "(IIF)V", .fnPtr = _gdk_android_surface_on_layout_surface },
  { .name = "notifyLayoutPosition", .signature = "(II)V", .fnPtr = _gdk_android_surface_on_layout_position },
  { .name = "notifyDetached", .signature = "()V", .fnPtr = _gdk_android_surface_on_detach },

  { .name = "notifyDNDStartFailed", .signature = "(Lorg/gtk/android/ClipboardProvider$NativeDragIdentifier;)V", .fnPtr = _gdk_android_surface_on_dnd_start_failed },

  { .name = "notifyMotionEvent", .signature = "(ILandroid/view/MotionEvent;)V", .fnPtr = _gdk_android_surface_on_motion_event },
  { .name = "notifyKeyEvent", .signature = "(Landroid/view/KeyEvent;)V", .fnPtr = _gdk_android_surface_on_key_event },
  { .name = "notifyDragEvent", .signature = "(Landroid/view/DragEvent;)Z", .fnPtr = _gdk_android_surface_on_drag_event },

  { .name = "notifyVisibility", .signature = "(Z)V", .fnPtr = _gdk_android_surface_on_visibility_ui_thread },
};
static const JNINativeMethod toplevel_natives[] = {
  { .name = "bindNative", .signature = "(J)V", .fnPtr = _gdk_android_toplevel_bind_native },
  { .name = "notifyConfigurationChange", .signature = "()V", .fnPtr = _gdk_android_toplevel_on_configuration_change },
  { .name = "notifyStateChange", .signature = "(ZZ)V", .fnPtr = _gdk_android_toplevel_on_state_change },
  { .name = "notifyOnBackPress", .signature = "()V", .fnPtr = _gdk_android_toplevel_on_back_press },
  { .name = "notifyDestroy", .signature = "()V", .fnPtr = _gdk_android_toplevel_on_destroy },
  { .name = "notifyActivityResult", .signature = "(IILandroid/content/Intent;)V", .fnPtr = _gdk_android_toplevel_on_activity_result }
};

static void
_gdk_android_gdk_context_activate (JNIEnv *env, jclass this)
{
  g_application_activate (g_application_get_default ());
}
static void
_gdk_android_gdk_context_open (JNIEnv *env, jclass this, jobject uri, jstring jhint)
{
  GApplication *app = g_application_get_default ();
  if (g_application_get_flags (app) & G_APPLICATION_HANDLES_OPEN)
    {
      GFile *file = gdk_android_content_file_from_uri (uri);
      gchar *hint = gdk_android_java_to_utf8 (jhint, NULL);
      g_application_open (app, &file, 1, hint);
      g_free (hint);
      g_object_unref (file);
    }
  else
    {
      g_application_activate (app);
    }
}

static JavaVM *gdk_android_vm = NULL;
static jobject gdk_android_activity = NULL;
static jobject gdk_android_user_classloader = NULL;

static GdkAndroidJavaCache gdk_android_java_cache;

#define POPULATE_REFCACHE_METHOD(cklass, cname, jname, signature) {                                                                 \
    gdk_android_java_cache.a_##cklass.cname = (*env)->GetMethodID (env, gdk_android_java_cache.a_##cklass.klass, jname, signature); \
  }
#define POPULATE_STATIC_REFCACHE_METHOD(cklass, cname, jname, signature) {                                                                \
    gdk_android_java_cache.a_##cklass.cname = (*env)->GetStaticMethodID (env, gdk_android_java_cache.a_##cklass.klass, jname, signature); \
  }
#define POPULATE_REFCACHE_MEMBER(cklass, cname, jname, signature) {                                                                \
    gdk_android_java_cache.a_##cklass.cname = (*env)->GetFieldID (env, gdk_android_java_cache.a_##cklass.klass, jname, signature); \
  }
#define POPULATE_REFCACHE_FIELD(cklass, cname, jname) {                                                                                             \
    jfieldID android_##cklass##_##cname = (*env)->GetStaticFieldID (env, gdk_android_java_cache.a_##cklass.klass, jname, "I");                      \
    gdk_android_java_cache.a_##cklass.cname = (*env)->GetStaticIntField (env, gdk_android_java_cache.a_##cklass.klass, android_##cklass##_##cname); \
  }
#define POPULATE_REFCACHE_ENUM(cklass, cname, jname, signature) {                                                                                      \
    jfieldID android_##cklass##_##cname##_id = (*env)->GetStaticFieldID (env, gdk_android_java_cache.a_##cklass.klass, jname, signature);              \
    jobject android_##cklass##_##cname = (*env)->GetStaticObjectField (env, gdk_android_java_cache.a_##cklass.klass, android_##cklass##_##cname##_id); \
    gdk_android_java_cache.a_##cklass.cname = (*env)->NewGlobalRef (env, android_##cklass##_##cname);                                                  \
    (*env)->DeleteLocalRef (env, android_##cklass##_##cname);                                                                                          \
  }

#define POPULATE_GDK_REFCACHE_METHOD(cklass, cname, jname, signature) {                                                     \
    gdk_android_java_cache.cklass.cname = (*env)->GetMethodID (env, gdk_android_java_cache.cklass.klass, jname, signature); \
  }
#define POPULATE_GDK_REFCACHE_FIELD(cklass, cname, jname) {                                                                       \
    jfieldID cklass##_##cname = (*env)->GetStaticFieldID (env, gdk_android_java_cache.cklass.klass, jname, "I");                  \
    gdk_android_java_cache.cklass.cname = (*env)->GetStaticIntField (env, gdk_android_java_cache.cklass.klass, cklass##_##cname); \
  }
#define POPULATE_GDK_REFCACHE_STRING(cklass, cname, jname) {                                                                           \
    jfieldID cklass##_##cname##_id = (*env)->GetStaticFieldID (env, gdk_android_java_cache.cklass.klass, jname, "Ljava/lang/String;"); \
    jstring cklass##_##cname = (*env)->GetStaticObjectField (env, gdk_android_java_cache.cklass.klass, cklass##_##cname##_id);         \
    gdk_android_java_cache.cklass.cname = (*env)->NewGlobalRef (env, cklass##_##cname);                                                \
  }

#define POPULATE_REFCACHE_CURSOR_TYPE(cname, jname, cssname) {                                                                                          \
    POPULATE_REFCACHE_FIELD (pointericon, cname, jname)                                                                                                 \
    g_hash_table_insert (gdk_android_java_cache.a_pointericon.gdk_type_mapping, cssname, GINT_TO_POINTER (gdk_android_java_cache.a_pointericon.cname)); \
  }

jclass
gdk_android_init_find_class_using_classloader (JNIEnv *env,
                                               jobject class_loader,
                                               const gchar *klass)
{
  (*env)->PushLocalFrame (env, 3);

  jclass cl_class = (*env)->FindClass (env, "java/lang/ClassLoader");
  jmethodID load_class = (*env)->GetMethodID (env, cl_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  jstring class_name = (*env)->NewStringUTF (env, klass);
  jclass jklass = (*env)->CallObjectMethod (env, class_loader, load_class, class_name);

  return (*env)->PopLocalFrame (env, jklass);
}

void
gdk_android_set_latest_activity (JNIEnv *env, jobject activity)
{
  if (gdk_android_activity)
    {
      if ((*env)->IsSameObject (env, gdk_android_activity, activity))
        return;

      if ((*env)->IsInstanceOf (env, gdk_android_activity, gdk_android_get_java_cache ()->toplevel.klass))
        if ((*env)->GetLongField (env, gdk_android_activity, gdk_android_get_java_cache ()->toplevel.native_identifier) == 0) // previously set activity was stil unbound
          (*env)->CallVoidMethod (env, gdk_android_activity, gdk_android_get_java_cache ()->a_activity.finish);
      (*env)->DeleteGlobalRef(env, gdk_android_activity);
    }
  gdk_android_activity = activity ? (*env)->NewGlobalRef(env, activity) : NULL;
}

/**
 * gdk_android_initialize: (skip)
 * @env: the JNI environment for the current thread
 * @application_classloader: the classloader used to resolve GTK classes
 * @activity: (nullable): the android.content.Context object
 *
 * Initializes the android backend.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 * Since: 4.18
 */
gboolean
gdk_android_initialize (JNIEnv *env, jobject application_classloader, jobject activity)
{
  gint rc = (*env)->GetJavaVM (env, &gdk_android_vm);
  if (rc != JNI_OK)
    {
      g_critical ("Failed getting VM from JNI Env");
      return FALSE;
    }
  gdk_android_set_latest_activity (env, activity);
  gdk_android_user_classloader = (*env)->NewGlobalRef (env, application_classloader);

  (*env)->PushLocalFrame (env, 16);

  jclass glib_context_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/GlibContext");
  (*env)->RegisterNatives (env, glib_context_class, glib_context_natives, sizeof glib_context_natives / sizeof (JNINativeMethod));

  jclass gdk_context_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ToplevelActivity$GdkContext");
  (*env)->RegisterNatives (env, gdk_context_class, gdk_context_natives, sizeof gdk_context_natives / sizeof (JNINativeMethod));

  jclass gdk_clipboard_provider_change_listener_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ClipboardProvider$ClipboardChangeListener");
  gdk_android_java_cache.clipboard_provider_change_listener.klass = (*env)->NewGlobalRef (env, gdk_clipboard_provider_change_listener_class);
  gdk_android_java_cache.clipboard_provider_change_listener.native_ptr = (*env)->GetFieldID (env, gdk_android_java_cache.clipboard_provider_change_listener.klass, "nativePtr", "J");
  gdk_android_java_cache.clipboard_provider_change_listener.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.clipboard_provider_change_listener.klass, "<init>", "(J)V");
  (*env)->RegisterNatives (env, gdk_clipboard_provider_change_listener_class, clipboard_provider_change_listener_natives, sizeof clipboard_provider_change_listener_natives / sizeof (JNINativeMethod));

  jclass gdk_clipboard_bitmap_drag_shadow_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ClipboardProvider$ClipboardBitmapDragShadow");
  gdk_android_java_cache.clipboard_bitmap_drag_shadow.klass = (*env)->NewGlobalRef (env, gdk_clipboard_bitmap_drag_shadow_class);
  gdk_android_java_cache.clipboard_bitmap_drag_shadow.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.clipboard_bitmap_drag_shadow.klass, "<init>", "(Landroid/view/View;Landroid/graphics/Bitmap;II)V");
  gdk_android_java_cache.clipboard_bitmap_drag_shadow.vflip = (*env)->GetStaticMethodID (env, gdk_android_java_cache.clipboard_bitmap_drag_shadow.klass, "vflip", "(Landroid/graphics/Bitmap;)Landroid/graphics/Bitmap;");

  jclass gdk_clipboard_empty_drag_shadow_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ClipboardProvider$ClipboardEmptyDragShadow");
  gdk_android_java_cache.clipboard_empty_drag_shadow.klass = (*env)->NewGlobalRef (env, gdk_clipboard_empty_drag_shadow_class);
  gdk_android_java_cache.clipboard_empty_drag_shadow.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.clipboard_empty_drag_shadow.klass, "<init>", "(Landroid/view/View;)V");

  jclass gdk_clipboard_internal_clipdata_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ClipboardProvider$InternalClipdata");
  gdk_android_java_cache.clipboard_internal_clipdata.klass = (*env)->NewGlobalRef (env, gdk_clipboard_internal_clipdata_class);
  gdk_android_java_cache.clipboard_internal_clipdata.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.clipboard_internal_clipdata.klass, "<init>", "()V");

  jclass gdk_clipboard_native_drag_identifier = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ClipboardProvider$NativeDragIdentifier");
  gdk_android_java_cache.clipboard_native_drag_identifier.klass = (*env)->NewGlobalRef (env, gdk_clipboard_native_drag_identifier);
  gdk_android_java_cache.clipboard_native_drag_identifier.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.clipboard_native_drag_identifier.klass, "<init>", "(J)V");
  gdk_android_java_cache.clipboard_native_drag_identifier.native_identifier = (*env)->GetFieldID (env, gdk_android_java_cache.clipboard_native_drag_identifier.klass, "nativeIdentifier", "J");

  jclass surface_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ToplevelActivity$ToplevelView$Surface");
  gdk_android_java_cache.surface.klass = (*env)->NewGlobalRef (env, surface_class);
  gdk_android_java_cache.surface.surface_identifier = (*env)->GetFieldID (env, gdk_android_java_cache.surface.klass, "surfaceIdentifier", "J");
  gdk_android_java_cache.surface.get_holder = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "getHolder", "()Landroid/view/SurfaceHolder;");
  gdk_android_java_cache.surface.set_ime_keyboard_state = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "setImeKeyboardState", "(Z)V");
  gdk_android_java_cache.surface.set_visibility = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "setVisibility", "(Z)V");
  gdk_android_java_cache.surface.set_input_region = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "setInputRegion", "([Landroid/graphics/RectF;)V");
  gdk_android_java_cache.surface.drop_cursor_icon = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "dropCursorIcon", "()V");
  gdk_android_java_cache.surface.set_cursor_from_id = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "setCursorFromId", "(I)V");
  gdk_android_java_cache.surface.set_cursor_from_bitmap = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "setCursorFromBitmap", "(Landroid/graphics/Bitmap;FF)V");
  gdk_android_java_cache.surface.start_dnd = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "startDND", "(Landroid/content/ClipData;Landroid/view/View$DragShadowBuilder;Lorg/gtk/android/ClipboardProvider$NativeDragIdentifier;I)V");
  gdk_android_java_cache.surface.update_dnd = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "updateDND", "(Landroid/view/View$DragShadowBuilder;)V");
  gdk_android_java_cache.surface.cancel_dnd = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "cancelDND", "()V");
  gdk_android_java_cache.surface.set_active_im_context = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "setActiveImContext", "(Lorg/gtk/android/ImContext;)V");
  gdk_android_java_cache.surface.reposition = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "reposition", "(IIII)V");
  gdk_android_java_cache.surface.drop = (*env)->GetMethodID (env, gdk_android_java_cache.surface.klass, "drop", "()V");
  (*env)->RegisterNatives (env, surface_class, surface_natives, sizeof surface_natives / sizeof (JNINativeMethod));

  jclass toplevel_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ToplevelActivity");
  gdk_android_java_cache.toplevel.klass = (*env)->NewGlobalRef (env, toplevel_class);
  gdk_android_java_cache.toplevel.native_identifier = (*env)->GetFieldID (env, gdk_android_java_cache.toplevel.klass, "nativeIdentifier", "J");
  gdk_android_java_cache.toplevel.toplevel_view = (*env)->GetFieldID (env, gdk_android_java_cache.toplevel.klass, "view", "Lorg/gtk/android/ToplevelActivity$ToplevelView;");
  POPULATE_GDK_REFCACHE_STRING (toplevel, toplevel_identifier_key, "toplevelIdentifierKey")
  POPULATE_GDK_REFCACHE_METHOD (toplevel, bind_native, "bindNative", "(J)V")
  POPULATE_GDK_REFCACHE_METHOD (toplevel, attach_toplevel_surface, "attachToplevelSurface", "()V")
  POPULATE_GDK_REFCACHE_METHOD (toplevel, post_window_configuration, "postWindowConfiguration", "(IZ)V")
  POPULATE_GDK_REFCACHE_METHOD (toplevel, post_title, "postTitle", "(Ljava/lang/String;)V")
  (*env)->RegisterNatives (env, toplevel_class, toplevel_natives, sizeof toplevel_natives / sizeof (JNINativeMethod));

  jclass toplevel_view_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ToplevelActivity$ToplevelView");
  gdk_android_java_cache.toplevel_view.klass = (*env)->NewGlobalRef (env, toplevel_view_class);
  gdk_android_java_cache.toplevel_view.set_grabbed_surface = (*env)->GetMethodID (env, gdk_android_java_cache.toplevel_view.klass, "setGrabbedSurface", "(Lorg/gtk/android/ToplevelActivity$ToplevelView$Surface;)V");
  gdk_android_java_cache.toplevel_view.push_popup = (*env)->GetMethodID (env, gdk_android_java_cache.toplevel_view.klass, "pushPopup", "(JIIII)V");

  jclass surface_exception_class = gdk_android_init_find_class_using_classloader (env, application_classloader, "org/gtk/android/ToplevelActivity$UnregisteredSurfaceException");
  gdk_android_java_cache.surface_exception.klass = (*env)->NewGlobalRef (env, surface_exception_class);
  gdk_android_java_cache.surface_exception.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.surface_exception.klass, "<init>", "(Ljava/lang/Object;)V");

  jclass android_activity_class = (*env)->FindClass (env, "android/app/Activity");
  gdk_android_java_cache.a_activity.klass = (*env)->NewGlobalRef (env, android_activity_class);
  gdk_android_java_cache.a_activity.get_task_id = (*env)->GetMethodID (env, gdk_android_java_cache.a_activity.klass, "getTaskId", "()I");
  gdk_android_java_cache.a_activity.get_window_manager = (*env)->GetMethodID (env, gdk_android_java_cache.a_activity.klass, "getWindowManager", "()Landroid/view/WindowManager;");
  gdk_android_java_cache.a_activity.finish = (*env)->GetMethodID (env, gdk_android_java_cache.a_activity.klass, "finish", "()V");
  gdk_android_java_cache.a_activity.move_task_to_back = (*env)->GetMethodID (env, gdk_android_java_cache.a_activity.klass, "moveTaskToBack", "(Z)Z");
  gdk_android_java_cache.a_activity.start_activity = (*env)->GetMethodID (env, gdk_android_java_cache.a_activity.klass, "startActivity", "(Landroid/content/Intent;)V");
  POPULATE_REFCACHE_METHOD (activity, start_activity_for_result, "startActivityForResult", "(Landroid/content/Intent;I)V")
  POPULATE_REFCACHE_METHOD (activity, finish_activity, "finishActivity", "(I)V")
  gdk_android_java_cache.a_activity.set_finish_on_touch_outside = (*env)->GetMethodID (env, gdk_android_java_cache.a_activity.klass, "setFinishOnTouchOutside", "(Z)V");
  POPULATE_REFCACHE_FIELD (activity, result_ok, "RESULT_OK")
  POPULATE_REFCACHE_FIELD (activity, result_cancelled, "RESULT_CANCELED")

  jclass android_context = (*env)->FindClass (env, "android/content/Context");
  gdk_android_java_cache.a_context.klass = (*env)->NewGlobalRef (env, android_context);
  POPULATE_REFCACHE_METHOD (context, get_content_resolver, "getContentResolver", "()Landroid/content/ContentResolver;")
  POPULATE_REFCACHE_METHOD (context, get_system_service, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;")
  POPULATE_REFCACHE_METHOD (context, get_resources, "getResources", "()Landroid/content/res/Resources;")
  POPULATE_GDK_REFCACHE_STRING (a_context, activity_service, "ACTIVITY_SERVICE")
  POPULATE_GDK_REFCACHE_STRING (a_context, clipboard_service, "CLIPBOARD_SERVICE")

  jclass android_content_resolver = (*env)->FindClass (env, "android/content/ContentResolver");
  gdk_android_java_cache.a_content_resolver.klass = (*env)->NewGlobalRef (env, android_content_resolver);
  POPULATE_REFCACHE_METHOD (content_resolver, get_type, "getType", "(Landroid/net/Uri;)Ljava/lang/String;");
  POPULATE_REFCACHE_METHOD (content_resolver, open_asset_fd, "openAssetFileDescriptor", "(Landroid/net/Uri;Ljava/lang/String;Landroid/os/CancellationSignal;)Landroid/content/res/AssetFileDescriptor;");
  POPULATE_REFCACHE_METHOD (content_resolver, open_typed_asset_fd, "openTypedAssetFileDescriptor", "(Landroid/net/Uri;Ljava/lang/String;Landroid/os/Bundle;Landroid/os/CancellationSignal;)Landroid/content/res/AssetFileDescriptor;");
  POPULATE_REFCACHE_METHOD (content_resolver, query, "query", "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;");

  jclass android_asset_fd = (*env)->FindClass (env, "android/content/res/AssetFileDescriptor");
  gdk_android_java_cache.a_asset_fd.klass = (*env)->NewGlobalRef (env, android_asset_fd);
  POPULATE_REFCACHE_METHOD (asset_fd, create_istream, "createInputStream", "()Ljava/io/FileInputStream;")
  POPULATE_REFCACHE_METHOD (asset_fd, create_ostream, "createOutputStream", "()Ljava/io/FileOutputStream;")
  gdk_android_java_cache.a_asset_fd.mode_append = (*env)->NewGlobalRef (env, (*env)->NewStringUTF (env, "wa"));
  gdk_android_java_cache.a_asset_fd.mode_read = (*env)->NewGlobalRef (env, (*env)->NewStringUTF (env, "r"));
  gdk_android_java_cache.a_asset_fd.mode_overwrite = (*env)->NewGlobalRef (env, (*env)->NewStringUTF (env, "wt"));

  jclass android_documents_contract = (*env)->FindClass (env, "android/provider/DocumentsContract");
  gdk_android_java_cache.a_documents_contract.klass = (*env)->NewGlobalRef (env, android_documents_contract);
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, get_document_id, "getDocumentId", "(Landroid/net/Uri;)Ljava/lang/String;");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, get_tree_document_id, "getTreeDocumentId", "(Landroid/net/Uri;)Ljava/lang/String;");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, build_children_from_tree, "buildChildDocumentsUriUsingTree", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, build_document_from_tree, "buildDocumentUriUsingTree", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, copy_document, "copyDocument", "(Landroid/content/ContentResolver;Landroid/net/Uri;Landroid/net/Uri;)Landroid/net/Uri;");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, create_document, "createDocument", "(Landroid/content/ContentResolver;Landroid/net/Uri;Ljava/lang/String;Ljava/lang/String;)Landroid/net/Uri;");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, delete_document, "deleteDocument", "(Landroid/content/ContentResolver;Landroid/net/Uri;)Z");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, is_child_document, "isChildDocument", "(Landroid/content/ContentResolver;Landroid/net/Uri;Landroid/net/Uri;)Z");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, is_document, "isDocumentUri", "(Landroid/content/Context;Landroid/net/Uri;)Z");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, is_tree, "isTreeUri", "(Landroid/net/Uri;)Z");
  POPULATE_STATIC_REFCACHE_METHOD (documents_contract, rename_document, "renameDocument", "(Landroid/content/ContentResolver;Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;");

  jclass android_documents_contract_document = (*env)->FindClass (env, "android/provider/DocumentsContract$Document");
  gdk_android_java_cache.a_documents_contract_document.klass = (*env)->NewGlobalRef (env, android_documents_contract_document);
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, column_document_id, "COLUMN_DOCUMENT_ID")
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, column_display_name, "COLUMN_DISPLAY_NAME")
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, column_flags, "COLUMN_FLAGS")
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, column_icon, "COLUMN_ICON")
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, column_last_modified, "COLUMN_LAST_MODIFIED")
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, column_mime_type, "COLUMN_MIME_TYPE")
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, column_size, "COLUMN_SIZE")
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, column_summary, "COLUMN_SUMMARY")
  POPULATE_REFCACHE_FIELD (documents_contract_document, flag_dir_supports_create, "FLAG_DIR_SUPPORTS_CREATE")
  POPULATE_REFCACHE_FIELD (documents_contract_document, flag_supports_copy, "FLAG_SUPPORTS_COPY")
  POPULATE_REFCACHE_FIELD (documents_contract_document, flag_supports_delete, "FLAG_SUPPORTS_DELETE")
  POPULATE_REFCACHE_FIELD (documents_contract_document, flag_supports_move, "FLAG_SUPPORTS_MOVE")
  POPULATE_REFCACHE_FIELD (documents_contract_document, flag_supports_rename, "FLAG_SUPPORTS_RENAME")
  POPULATE_REFCACHE_FIELD (documents_contract_document, flag_supports_write, "FLAG_SUPPORTS_WRITE")
  POPULATE_REFCACHE_FIELD (documents_contract_document, flag_virtual_document, "FLAG_VIRTUAL_DOCUMENT")
  POPULATE_GDK_REFCACHE_STRING (a_documents_contract_document, mime_directory, "MIME_TYPE_DIR")

  jclass android_cursor = (*env)->FindClass (env, "android/database/Cursor");
  gdk_android_java_cache.a_cursor.klass = (*env)->NewGlobalRef (env, android_cursor);
  POPULATE_REFCACHE_METHOD (cursor, get_int, "getInt", "(I)I")
  POPULATE_REFCACHE_METHOD (cursor, get_long, "getLong", "(I)J")
  POPULATE_REFCACHE_METHOD (cursor, get_string, "getString", "(I)Ljava/lang/String;")
  POPULATE_REFCACHE_METHOD (cursor, is_null, "isNull", "(I)Z")
  POPULATE_REFCACHE_METHOD (cursor, move_to_next, "moveToNext", "()Z")
  POPULATE_REFCACHE_METHOD (cursor, close, "close", "()V")

  jclass android_resources = (*env)->FindClass (env, "android/content/res/Resources");
  gdk_android_java_cache.a_resources.klass = (*env)->NewGlobalRef (env, android_resources);
  POPULATE_REFCACHE_METHOD (resources, get_configuration, "getConfiguration", "()Landroid/content/res/Configuration;")

  jclass android_configuration = (*env)->FindClass (env, "android/content/res/Configuration");
  gdk_android_java_cache.a_configuration.klass = (*env)->NewGlobalRef (env, android_configuration);
  POPULATE_REFCACHE_MEMBER (configuration, ui, "uiMode", "I")
  POPULATE_REFCACHE_FIELD (configuration, ui_night_undefined, "UI_MODE_NIGHT_UNDEFINED")
  POPULATE_REFCACHE_FIELD (configuration, ui_night_no, "UI_MODE_NIGHT_NO")
  POPULATE_REFCACHE_FIELD (configuration, ui_night_yes, "UI_MODE_NIGHT_YES")

  jclass android_clipboard_manager = (*env)->FindClass (env, "android/content/ClipboardManager");
  gdk_android_java_cache.a_clipboard_manager.klass = (*env)->NewGlobalRef (env, android_clipboard_manager);
  POPULATE_REFCACHE_METHOD (clipboard_manager, get_primary_clip, "getPrimaryClip", "()Landroid/content/ClipData;")
  POPULATE_REFCACHE_METHOD (clipboard_manager, set_primary_clip, "setPrimaryClip", "(Landroid/content/ClipData;)V")
  POPULATE_REFCACHE_METHOD (clipboard_manager, get_clip_desc, "getPrimaryClipDescription", "()Landroid/content/ClipDescription;")
  POPULATE_REFCACHE_METHOD (clipboard_manager, add_change_listener, "addPrimaryClipChangedListener", "(Landroid/content/ClipboardManager$OnPrimaryClipChangedListener;)V")
  POPULATE_REFCACHE_METHOD (clipboard_manager, remove_change_listener, "removePrimaryClipChangedListener", "(Landroid/content/ClipboardManager$OnPrimaryClipChangedListener;)V")

  jclass android_clip_desc = (*env)->FindClass (env, "android/content/ClipDescription");
  gdk_android_java_cache.a_clip_desc.klass = (*env)->NewGlobalRef (env, android_clip_desc);
  POPULATE_REFCACHE_METHOD (clip_desc, get_mime_type_count, "getMimeTypeCount", "()I")
  POPULATE_REFCACHE_METHOD (clip_desc, get_mime_type, "getMimeType", "(I)Ljava/lang/String;")
  POPULATE_GDK_REFCACHE_STRING (a_clip_desc, mime_text_html, "MIMETYPE_TEXT_HTML")
  POPULATE_GDK_REFCACHE_STRING (a_clip_desc, mime_text_plain, "MIMETYPE_TEXT_PLAIN")

  jclass android_clipdata = (*env)->FindClass (env, "android/content/ClipData");
  gdk_android_java_cache.a_clipdata.klass = (*env)->NewGlobalRef (env, android_clipdata);
  POPULATE_REFCACHE_METHOD (clipdata, add_item, "addItem", "(Landroid/content/ContentResolver;Landroid/content/ClipData$Item;)V")
  POPULATE_REFCACHE_METHOD (clipdata, get_item_count, "getItemCount", "()I")
  POPULATE_REFCACHE_METHOD (clipdata, get_item, "getItemAt", "(I)Landroid/content/ClipData$Item;")
  POPULATE_STATIC_REFCACHE_METHOD(clipdata, new_plain_text, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;")
  POPULATE_STATIC_REFCACHE_METHOD (clipdata, new_html, "newHtmlText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;Ljava/lang/String;)Landroid/content/ClipData;")
  POPULATE_STATIC_REFCACHE_METHOD (clipdata, new_uri, "newUri", "(Landroid/content/ContentResolver;Ljava/lang/CharSequence;Landroid/net/Uri;)Landroid/content/ClipData;")

  jclass android_clipdata_item = (*env)->FindClass (env, "android/content/ClipData$Item");
  gdk_android_java_cache.a_clipdata_item.klass = (*env)->NewGlobalRef (env, android_clipdata_item);
  POPULATE_REFCACHE_METHOD (clipdata_item, constructor_text, "<init>", "(Ljava/lang/CharSequence;)V")
  POPULATE_REFCACHE_METHOD (clipdata_item, constructor_html, "<init>", "(Ljava/lang/CharSequence;Ljava/lang/String;)V")
  POPULATE_REFCACHE_METHOD (clipdata_item, constructor_uri, "<init>", "(Landroid/net/Uri;)V")
  POPULATE_REFCACHE_METHOD (clipdata_item, coerce_to_text, "coerceToText", "(Landroid/content/Context;)Ljava/lang/CharSequence;")
  POPULATE_REFCACHE_METHOD (clipdata_item, get_html, "getHtmlText", "()Ljava/lang/String;")
  POPULATE_REFCACHE_METHOD (clipdata_item, get_uri, "getUri", "()Landroid/net/Uri;")

  jclass android_view_class = (*env)->FindClass (env, "android/view/View");
  gdk_android_java_cache.a_view.klass = (*env)->NewGlobalRef (env, android_view_class);
  POPULATE_REFCACHE_METHOD (view, get_context, "getContext", "()Landroid/content/Context;")
  POPULATE_REFCACHE_METHOD (view, get_display, "getDisplay", "()Landroid/view/Display;")
  POPULATE_REFCACHE_FIELD (view, drag_global, "DRAG_FLAG_GLOBAL")
  POPULATE_REFCACHE_FIELD (view, drag_global_prefix_match, "DRAG_FLAG_GLOBAL_PREFIX_URI_PERMISSION")
  POPULATE_REFCACHE_FIELD (view, drag_global_uri_read, "DRAG_FLAG_GLOBAL_URI_READ")

  jclass android_display_class = (*env)->FindClass (env, "android/view/Display");
  gdk_android_java_cache.a_display.klass = (*env)->NewGlobalRef (env, android_display_class);
  POPULATE_REFCACHE_METHOD (display, get_refresh_rate, "getRefreshRate", "()F")

  jclass android_pointericon = (*env)->FindClass (env, "android/view/PointerIcon");
  gdk_android_java_cache.a_pointericon.klass = (*env)->NewGlobalRef (env, android_pointericon);
  gdk_android_java_cache.a_pointericon.gdk_type_mapping = g_hash_table_new (g_str_hash, g_str_equal);
  POPULATE_REFCACHE_CURSOR_TYPE (type_alias, "TYPE_ALIAS", "alias")
  POPULATE_REFCACHE_CURSOR_TYPE (type_all_scroll, "TYPE_ALL_SCROLL", "all-scroll")
  POPULATE_REFCACHE_CURSOR_TYPE (type_arrow, "TYPE_ARROW", "arrow")
  POPULATE_REFCACHE_CURSOR_TYPE (type_cell, "TYPE_CELL", "cell")
  POPULATE_REFCACHE_CURSOR_TYPE (type_context_menu, "TYPE_CONTEXT_MENU", "context-menu")
  POPULATE_REFCACHE_CURSOR_TYPE (type_copy, "TYPE_COPY", "copy")
  POPULATE_REFCACHE_CURSOR_TYPE (type_crosshair, "TYPE_CROSSHAIR", "crosshair")
  POPULATE_REFCACHE_CURSOR_TYPE (type_grab, "TYPE_GRAB", "grab")
  POPULATE_REFCACHE_CURSOR_TYPE (type_grabbing, "TYPE_GRABBING", "grabbing")
  POPULATE_REFCACHE_CURSOR_TYPE (type_hand, "TYPE_HAND", "pointer")
  POPULATE_REFCACHE_CURSOR_TYPE (type_help, "TYPE_HELP", "help")
  POPULATE_REFCACHE_CURSOR_TYPE (type_horizontal_double_arrow, "TYPE_HORIZONTAL_DOUBLE_ARROW", "ew-resize")
  POPULATE_REFCACHE_CURSOR_TYPE (type_no_drop, "TYPE_NO_DROP", "no-drop")
  POPULATE_REFCACHE_CURSOR_TYPE (type_null, "TYPE_NULL", "none")
  POPULATE_REFCACHE_CURSOR_TYPE (type_text, "TYPE_TEXT", "text")
  POPULATE_REFCACHE_CURSOR_TYPE (type_top_left_diagonal_double_arrow, "TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW", "nwse-resize")
  POPULATE_REFCACHE_CURSOR_TYPE (type_top_right_diagonal_double_arrow, "TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW", "nesw-resize")
  POPULATE_REFCACHE_CURSOR_TYPE (type_vertical_double_arrow, "TYPE_VERTICAL_DOUBLE_ARROW", "ns-resize")
  POPULATE_REFCACHE_CURSOR_TYPE (type_vertical_text, "TYPE_VERTICAL_TEXT", "vertical-text")
  POPULATE_REFCACHE_CURSOR_TYPE (type_wait, "TYPE_WAIT", "wait")
  POPULATE_REFCACHE_CURSOR_TYPE (type_zoom_in, "TYPE_ZOOM_IN", "zoom-in")
  POPULATE_REFCACHE_CURSOR_TYPE (type_zoom_out, "TYPE_ZOOM_OUT", "zoom-out")
  // CSS cursors missing, due to lack of support are:
  // - (row|col)-resize and (n(w|e)?|w|s(w|e)?|e)-resize
  // - move, not-allowed and progress

  jclass android_bitmap = (*env)->FindClass (env, "android/graphics/Bitmap");
  gdk_android_java_cache.a_bitmap.klass = (*env)->NewGlobalRef (env, android_bitmap);
  POPULATE_STATIC_REFCACHE_METHOD (bitmap, create_from_array, "createBitmap", "([IIILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;")
  {
    jclass android_bitmap_config = (*env)->FindClass (env, "android/graphics/Bitmap$Config");
    jfieldID config_argb8888_field = (*env)->GetStaticFieldID (env, android_bitmap_config, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
    jobject config_argb8888 = (*env)->GetStaticObjectField (env, android_bitmap_config, config_argb8888_field);
    gdk_android_java_cache.a_bitmap.argb8888 = (*env)->NewGlobalRef (env, config_argb8888);
    (*env)->DeleteLocalRef (env, config_argb8888);
    (*env)->DeleteLocalRef (env, android_bitmap_config);
  }

  jclass android_intent_class = (*env)->FindClass (env, "android/content/Intent");
  gdk_android_java_cache.a_intent.klass = (*env)->NewGlobalRef (env, android_intent_class);
  POPULATE_REFCACHE_METHOD (intent, constructor, "<init>", "(Landroid/content/Context;Ljava/lang/Class;)V")
  POPULATE_REFCACHE_METHOD (intent, constructor_action, "<init>", "(Ljava/lang/String;)V")
  POPULATE_STATIC_REFCACHE_METHOD(intent, create_chooser, "createChooser", "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, get_data, "getData", "()Landroid/net/Uri;")
  POPULATE_REFCACHE_METHOD (intent, get_clipdata, "getClipData", "()Landroid/content/ClipData;")
  POPULATE_REFCACHE_METHOD (intent, set_data_norm, "setDataAndNormalize", "(Landroid/net/Uri;)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, add_flags, "addFlags", "(I)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, put_extra_bool, "putExtra", "(Ljava/lang/String;Z)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, put_extra_int, "putExtra", "(Ljava/lang/String;I)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, put_extra_int_array, "putExtra", "(Ljava/lang/String;[I)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, put_extra_long, "putExtra", "(Ljava/lang/String;J)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, put_extra_string, "putExtra", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, put_extra_string_array, "putExtra", "(Ljava/lang/String;[Ljava/lang/String;)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, put_extra_parcelable, "putExtra", "(Ljava/lang/String;Landroid/os/Parcelable;)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, put_extras_from_bundle, "putExtras", "(Landroid/os/Bundle;)Landroid/content/Intent;")
  POPULATE_REFCACHE_METHOD (intent, set_type, "setType", "(Ljava/lang/String;)Landroid/content/Intent;")
  POPULATE_STATIC_REFCACHE_METHOD(intent, normalize_mimetype, "normalizeMimeType", "(Ljava/lang/String;)Ljava/lang/String;")
  POPULATE_REFCACHE_FIELD (intent, flag_activity_clear_task, "FLAG_ACTIVITY_CLEAR_TASK")
  POPULATE_REFCACHE_FIELD (intent, flag_activity_multiple_task, "FLAG_ACTIVITY_MULTIPLE_TASK")
  POPULATE_REFCACHE_FIELD (intent, flag_activity_new_task, "FLAG_ACTIVITY_NEW_TASK")
  POPULATE_REFCACHE_FIELD (intent, flag_activity_no_animation, "FLAG_ACTIVITY_NO_ANIMATION")
  POPULATE_REFCACHE_FIELD (intent, flag_grant_read_perm, "FLAG_GRANT_READ_URI_PERMISSION")
  POPULATE_REFCACHE_FIELD (intent, flag_grant_write_perm, "FLAG_GRANT_WRITE_URI_PERMISSION")
  POPULATE_GDK_REFCACHE_STRING (a_intent, action_create_document, "ACTION_CREATE_DOCUMENT")
  POPULATE_GDK_REFCACHE_STRING (a_intent, action_open_document, "ACTION_OPEN_DOCUMENT")
  POPULATE_GDK_REFCACHE_STRING (a_intent, action_open_document_tree, "ACTION_OPEN_DOCUMENT_TREE")
  POPULATE_GDK_REFCACHE_STRING (a_intent, action_edit, "ACTION_EDIT")
  POPULATE_GDK_REFCACHE_STRING (a_intent, action_view, "ACTION_VIEW")
  POPULATE_GDK_REFCACHE_STRING (a_intent, category_openable, "CATEGORY_OPENABLE")
  POPULATE_GDK_REFCACHE_STRING (a_intent, extra_allow_multiple, "EXTRA_ALLOW_MULTIPLE")
  POPULATE_GDK_REFCACHE_STRING (a_intent, extra_mimetypes, "EXTRA_MIME_TYPES")
  POPULATE_GDK_REFCACHE_STRING (a_intent, extra_title, "EXTRA_TITLE")
  {
    jobject android_customtabs_session = (*env)->NewStringUTF (env, "android.support.customtabs.extra.SESSION");
    jobject android_customtabs_toolbar_color = (*env)->NewStringUTF (env, "android.support.customtabs.extra.TOOLBAR_COLOR");
    gdk_android_java_cache.a_intent.extra_customtabs_session = (*env)->NewGlobalRef (env, android_customtabs_session);
    gdk_android_java_cache.a_intent.extra_customtabs_toolbar_color = (*env)->NewGlobalRef (env, android_customtabs_toolbar_color);
    (*env)->DeleteLocalRef (env, android_customtabs_session);
    (*env)->DeleteLocalRef (env, android_customtabs_toolbar_color);
  }

  jclass android_bundle = (*env)->FindClass (env, "android/os/Bundle");
  gdk_android_java_cache.a_bundle.klass = (*env)->NewGlobalRef (env, android_bundle);
  POPULATE_REFCACHE_METHOD (bundle, constructor, "<init>", "()V")
  POPULATE_REFCACHE_METHOD (bundle, put_binder, "putBinder", "(Ljava/lang/String;Landroid/os/IBinder;)V")

  jclass android_surface_holder_class = (*env)->FindClass (env, "android/view/SurfaceHolder");
  gdk_android_java_cache.a_surfaceholder.klass = (*env)->NewGlobalRef (env, android_surface_holder_class);
  gdk_android_java_cache.a_surfaceholder.get_surface = (*env)->GetMethodID (env, gdk_android_java_cache.a_surfaceholder.klass, "getSurface", "()Landroid/view/Surface;");
  gdk_android_java_cache.a_surfaceholder.get_surface_frame = (*env)->GetMethodID (env, gdk_android_java_cache.a_surfaceholder.klass, "getSurfaceFrame", "()Landroid/graphics/Rect;");
  gdk_android_java_cache.a_surfaceholder.lock_canvas = (*env)->GetMethodID (env, gdk_android_java_cache.a_surfaceholder.klass, "lockCanvas", "()Landroid/graphics/Canvas;");
  gdk_android_java_cache.a_surfaceholder.lock_canvas_dirty = (*env)->GetMethodID (env, gdk_android_java_cache.a_surfaceholder.klass, "lockCanvas", "(Landroid/graphics/Rect;)Landroid/graphics/Canvas;");
  gdk_android_java_cache.a_surfaceholder.unlock_canvas_and_post = (*env)->GetMethodID (env, gdk_android_java_cache.a_surfaceholder.klass, "unlockCanvasAndPost", "(Landroid/graphics/Canvas;)V");

  jclass android_canvas_class = (*env)->FindClass (env, "android/graphics/Canvas");
  gdk_android_java_cache.a_canvas.klass = (*env)->NewGlobalRef (env, android_canvas_class);
  gdk_android_java_cache.a_canvas.draw_color = (*env)->GetMethodID (env, gdk_android_java_cache.a_canvas.klass, "drawColor", "(ILandroid/graphics/BlendMode;)V");

  jclass android_blendmode = (*env)->FindClass (env, "android/graphics/BlendMode");
  gdk_android_java_cache.a_blendmode.klass = (*env)->NewGlobalRef (env, android_blendmode);
  POPULATE_REFCACHE_ENUM (blendmode, clear, "CLEAR", "Landroid/graphics/BlendMode;")

  jclass android_rect_class = (*env)->FindClass (env, "android/graphics/Rect");
  gdk_android_java_cache.a_rect.klass = (*env)->NewGlobalRef (env, android_rect_class);
  gdk_android_java_cache.a_rect.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.a_rect.klass, "<init>", "(IIII)V");
  gdk_android_java_cache.a_rect.bottom = (*env)->GetFieldID (env, gdk_android_java_cache.a_rect.klass, "bottom", "I");
  gdk_android_java_cache.a_rect.left = (*env)->GetFieldID (env, gdk_android_java_cache.a_rect.klass, "left", "I");
  gdk_android_java_cache.a_rect.right = (*env)->GetFieldID (env, gdk_android_java_cache.a_rect.klass, "right", "I");
  gdk_android_java_cache.a_rect.top = (*env)->GetFieldID (env, gdk_android_java_cache.a_rect.klass, "top", "I");

  jclass android_rectf_class = (*env)->FindClass (env, "android/graphics/RectF");
  gdk_android_java_cache.a_rectf.klass = (*env)->NewGlobalRef (env, android_rectf_class);
  gdk_android_java_cache.a_rectf.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.a_rectf.klass, "<init>", "(FFFF)V");

  jclass android_input_event = (*env)->FindClass (env, "android/view/InputEvent");
  gdk_android_java_cache.a_input_event.klass = (*env)->NewGlobalRef (env, android_input_event);
  POPULATE_REFCACHE_METHOD (input_event, get_device, "getDevice", "()Landroid/view/InputDevice;")

  jclass android_input_device = (*env)->FindClass (env, "android/view/InputDevice");
  gdk_android_java_cache.a_input_device.klass = (*env)->NewGlobalRef (env, android_input_device);
  POPULATE_STATIC_REFCACHE_METHOD (input_device, get_device_from_id, "getDevice", "(I)Landroid/view/InputDevice;")
  POPULATE_REFCACHE_METHOD (input_device, get_motion_range, "getMotionRange", "(I)Landroid/view/InputDevice$MotionRange;")

  jclass android_motion_range = (*env)->FindClass (env, "android/view/InputDevice$MotionRange");
  gdk_android_java_cache.a_motion_range.klass = (*env)->NewGlobalRef (env, android_motion_range);
  POPULATE_REFCACHE_METHOD (motion_range, get_axis, "getAxis", "()I")
  POPULATE_REFCACHE_METHOD (motion_range, get_min, "getMin", "()F")
  POPULATE_REFCACHE_METHOD (motion_range, get_max, "getMax", "()F")
  POPULATE_REFCACHE_METHOD (motion_range, get_resolution, "getResolution", "()F")

  jclass android_drag_event = (*env)->FindClass (env, "android/view/DragEvent");
  gdk_android_java_cache.a_drag_event.klass = (*env)->NewGlobalRef (env, android_drag_event);
  POPULATE_REFCACHE_METHOD (drag_event, get_action, "getAction", "()I")
  POPULATE_REFCACHE_METHOD (drag_event, get_clip_data, "getClipData", "()Landroid/content/ClipData;")
  POPULATE_REFCACHE_METHOD (drag_event, get_clip_description, "getClipDescription", "()Landroid/content/ClipDescription;")
  POPULATE_REFCACHE_METHOD (drag_event, get_local_state, "getLocalState", "()Ljava/lang/Object;")
  POPULATE_REFCACHE_METHOD (drag_event, get_result, "getResult", "()Z")
  POPULATE_REFCACHE_METHOD (drag_event, get_x, "getX", "()F")
  POPULATE_REFCACHE_METHOD (drag_event, get_y, "getY", "()F")
  POPULATE_REFCACHE_FIELD (drag_event, action_started, "ACTION_DRAG_STARTED")
  POPULATE_REFCACHE_FIELD (drag_event, action_entered, "ACTION_DRAG_ENTERED")
  POPULATE_REFCACHE_FIELD (drag_event, action_location, "ACTION_DRAG_LOCATION")
  POPULATE_REFCACHE_FIELD (drag_event, action_exited, "ACTION_DRAG_EXITED")
  POPULATE_REFCACHE_FIELD (drag_event, action_ended, "ACTION_DRAG_ENDED")
  POPULATE_REFCACHE_FIELD (drag_event, action_drop, "ACTION_DROP")

  jclass android_activity_manager = (*env)->FindClass (env, "android/app/ActivityManager");
  gdk_android_java_cache.a_activity_manager.klass = (*env)->NewGlobalRef (env, android_activity_manager);
  POPULATE_REFCACHE_METHOD (activity_manager, move_task_to_font, "moveTaskToFront", "(IILandroid/os/Bundle;)V")

  jclass android_uri = (*env)->FindClass (env, "android/net/Uri");
  gdk_android_java_cache.a_uri.klass = (*env)->NewGlobalRef (env, android_uri);
  POPULATE_REFCACHE_METHOD (uri, get_path, "getPath", "()Ljava/lang/String;")
  POPULATE_REFCACHE_METHOD (uri, get_scheme, "getScheme", "()Ljava/lang/String;")
  POPULATE_REFCACHE_METHOD (uri, normalize, "normalizeScheme", "()Landroid/net/Uri;")
  POPULATE_STATIC_REFCACHE_METHOD (uri, parse, "parse", "(Ljava/lang/String;)Landroid/net/Uri;")

  jclass java_file_istream = (*env)->FindClass (env, "java/io/FileInputStream");
  gdk_android_java_cache.j_file_istream.klass = (*env)->NewGlobalRef (env, java_file_istream);
  gdk_android_java_cache.j_file_istream.get_channel = (*env)->GetMethodID (env, gdk_android_java_cache.j_file_istream.klass, "getChannel", "()Ljava/nio/channels/FileChannel;");

  jclass java_istream = (*env)->FindClass (env, "java/io/InputStream");
  gdk_android_java_cache.j_istream.klass = (*env)->NewGlobalRef (env, java_istream);
  gdk_android_java_cache.j_istream.close = (*env)->GetMethodID (env, gdk_android_java_cache.j_istream.klass, "close", "()V");
  gdk_android_java_cache.j_istream.read = (*env)->GetMethodID (env, gdk_android_java_cache.j_istream.klass, "read", "([BII)I");
  gdk_android_java_cache.j_istream.skip = (*env)->GetMethodID (env, gdk_android_java_cache.j_istream.klass, "skip", "(J)J");

  jclass java_file_ostream = (*env)->FindClass (env, "java/io/FileOutputStream");
  gdk_android_java_cache.j_file_ostream.klass = (*env)->NewGlobalRef (env, java_file_ostream);
  gdk_android_java_cache.j_file_istream.get_channel = (*env)->GetMethodID (env, gdk_android_java_cache.j_file_ostream.klass, "getChannel", "()Ljava/nio/channels/FileChannel;");

  jclass java_ostream = (*env)->FindClass (env, "java/io/OutputStream");
  gdk_android_java_cache.j_ostream.klass = (*env)->NewGlobalRef (env, java_ostream);
  gdk_android_java_cache.j_ostream.close = (*env)->GetMethodID (env, gdk_android_java_cache.j_ostream.klass, "close", "()V");
  gdk_android_java_cache.j_ostream.flush = (*env)->GetMethodID (env, gdk_android_java_cache.j_ostream.klass, "flush", "()V");
  gdk_android_java_cache.j_ostream.write = (*env)->GetMethodID (env, gdk_android_java_cache.j_ostream.klass, "write", "([BII)V");

  jclass java_file_channel = (*env)->FindClass (env, "java/nio/channels/FileChannel");
  gdk_android_java_cache.j_file_channel.klass = (*env)->NewGlobalRef (env, java_file_channel);
  gdk_android_java_cache.j_file_channel.get_position = (*env)->GetMethodID (env, gdk_android_java_cache.j_file_channel.klass, "position", "()J");
  gdk_android_java_cache.j_file_channel.set_position = (*env)->GetMethodID (env, gdk_android_java_cache.j_file_channel.klass, "position", "(J)Ljava/nio/channels/FileChannel;");
  gdk_android_java_cache.j_file_channel.get_size = (*env)->GetMethodID (env, gdk_android_java_cache.j_file_channel.klass, "size", "()J");
  gdk_android_java_cache.j_file_channel.truncate = (*env)->GetMethodID (env, gdk_android_java_cache.j_file_channel.klass, "truncate", "(J)Ljava/nio/channels/FileChannel;");

  jclass java_urlconnection = (*env)->FindClass (env, "java/net/URLConnection");
  gdk_android_java_cache.j_urlconnection.klass = (*env)->NewGlobalRef (env, java_urlconnection);
  gdk_android_java_cache.j_urlconnection.guess_content_type_for_name = (*env)->GetStaticMethodID (env, gdk_android_java_cache.j_urlconnection.klass, "guessContentTypeFromName", "(Ljava/lang/String;)Ljava/lang/String;");
  gdk_android_java_cache.j_urlconnection.mime_binary_data = (*env)->NewGlobalRef (env, (*env)->NewStringUTF (env, "application/octet-stream"));


  jclass java_arraylist = (*env)->FindClass (env, "java/util/ArrayList");
  gdk_android_java_cache.j_arraylist.klass = (*env)->NewGlobalRef (env, java_arraylist);
  gdk_android_java_cache.j_arraylist.constructor = (*env)->GetMethodID (env, gdk_android_java_cache.j_arraylist.klass, "<init>", "()V");

  jclass java_list = (*env)->FindClass (env, "java/util/List");
  gdk_android_java_cache.j_list.klass = (*env)->NewGlobalRef (env, java_list);
  gdk_android_java_cache.j_list.add = (*env)->GetMethodID (env, gdk_android_java_cache.j_list.klass, "add", "(Ljava/lang/Object;)Z");
  gdk_android_java_cache.j_list.get = (*env)->GetMethodID (env, gdk_android_java_cache.j_list.klass, "get", "(I)Ljava/lang/Object;");
  gdk_android_java_cache.j_list.size = (*env)->GetMethodID (env, gdk_android_java_cache.j_list.klass, "size", "()I");
  gdk_android_java_cache.j_list.to_array = (*env)->GetMethodID (env, gdk_android_java_cache.j_list.klass, "toArray", "([Ljava/lang/Object;)[Ljava/lang/Object;");

  jclass java_string = (*env)->FindClass (env, "java/lang/String");
  gdk_android_java_cache.j_string.klass = (*env)->NewGlobalRef (env, java_string);

  jclass java_object = (*env)->FindClass (env, "java/lang/Object");
  gdk_android_java_cache.j_object.klass = (*env)->NewGlobalRef (env, java_object);
  gdk_android_java_cache.j_object.equals = (*env)->GetMethodID (env, gdk_android_java_cache.j_object.klass, "equals", "(Ljava/lang/Object;)Z");
  gdk_android_java_cache.j_object.hash_code = (*env)->GetMethodID (env, gdk_android_java_cache.j_object.klass, "hashCode", "()I");
  gdk_android_java_cache.j_object.to_string = (*env)->GetMethodID (env, gdk_android_java_cache.j_object.klass, "toString", "()Ljava/lang/String;");

  jclass java_char_conversion_exception = (*env)->FindClass (env, "java/io/CharConversionException");
  gdk_android_java_cache.j_char_conversion_exception.klass = (*env)->NewGlobalRef (env, java_char_conversion_exception);

  gdk_android_java_cache.j_exceptions.io_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/io/IOException"));
  gdk_android_java_cache.j_exceptions.eof_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/io/EOFException"));
  gdk_android_java_cache.j_exceptions.not_found_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/io/FileNotFoundException"));
  gdk_android_java_cache.j_exceptions.access_denied_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/nio/file/AccessDeniedException"));
  gdk_android_java_cache.j_exceptions.not_empty_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/nio/file/DirectoryNotEmptyException"));
  gdk_android_java_cache.j_exceptions.exists_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/nio/file/FileAlreadyExistsException"));
  gdk_android_java_cache.j_exceptions.loop_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/nio/file/FileSystemLoopException"));
  gdk_android_java_cache.j_exceptions.no_file_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/nio/file/NoSuchFileException"));
  gdk_android_java_cache.j_exceptions.not_dir_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/nio/file/NotDirectoryException"));
  gdk_android_java_cache.j_exceptions.malformed_uri_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/net/MalformedURLException"));
  gdk_android_java_cache.j_exceptions.channel_closed_exception = (*env)->NewGlobalRef (env, (*env)->FindClass (env, "java/nio/channels/ClosedChannelException"));

  jclass java_throwable = (*env)->FindClass (env, "java/lang/Throwable");
  gdk_android_java_cache.j_throwable.klass = (*env)->NewGlobalRef (env, java_throwable);
  gdk_android_java_cache.j_throwable.get_message = (*env)->GetMethodID (env, gdk_android_java_cache.j_throwable.klass, "getMessage", "()Ljava/lang/String;");

  (*env)->PopLocalFrame (env, NULL);

  return TRUE;
}

/**
 * gdk_android_finalize: (skip)
 *
 * Frees all allocated resources and references associated with the
 * android backend.
 *
 * Since: 4.18
 */
void
gdk_android_finalize (void)
{
  JNIEnv *env = gdk_android_get_env ();
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.clipboard_provider_change_listener.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.clipboard_bitmap_drag_shadow.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.clipboard_empty_drag_shadow.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.clipboard_native_drag_identifier.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.surface.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.toplevel.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.toplevel.toplevel_identifier_key);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.toplevel_view.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.surface_exception.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_activity.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_context.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_context.activity_service);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_context.clipboard_service);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_content_resolver.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_asset_fd.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_asset_fd.mode_append);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_asset_fd.mode_read);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_asset_fd.mode_overwrite);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.column_document_id);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.column_display_name);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.column_flags);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.column_icon);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.column_last_modified);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.column_mime_type);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.column_size);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_documents_contract_document.column_summary);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_cursor.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_resources.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_configuration.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_clipboard_manager.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_clip_desc.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_clip_desc.mime_text_html);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_clip_desc.mime_text_plain);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_clipdata.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_clipdata_item.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_view.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_display.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_pointericon.klass);
  g_hash_table_unref (gdk_android_java_cache.a_pointericon.gdk_type_mapping);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_bitmap.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_bitmap.argb8888);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.action_create_document);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.action_open_document);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.action_open_document_tree);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.action_edit);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.action_view);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.category_openable);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.extra_allow_multiple);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.extra_mimetypes);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.extra_title);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.extra_customtabs_session);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_intent.extra_customtabs_toolbar_color);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_bundle.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_surfaceholder.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_canvas.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_blendmode.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_blendmode.clear);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_rect.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_rectf.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_input_event.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_input_device.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_motion_range.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_drag_event.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_activity_manager.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.a_uri.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_file_istream.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_istream.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_file_ostream.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_ostream.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_file_channel.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_urlconnection.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_urlconnection.mime_binary_data);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_arraylist.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_list.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_string.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_object.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_char_conversion_exception.klass);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.io_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.eof_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.not_found_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.access_denied_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.not_empty_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.exists_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.loop_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.no_file_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.not_dir_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.malformed_uri_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_exceptions.channel_closed_exception);
  (*env)->DeleteGlobalRef (env, gdk_android_java_cache.j_throwable.klass);

  if (gdk_android_activity)
    (*env)->DeleteGlobalRef(env, gdk_android_activity);
  gdk_android_activity = NULL;
  env = NULL;
  gdk_android_vm = NULL;
}

const GdkAndroidJavaCache *gdk_android_get_java_cache (void);

static __thread JNIEnv *gdk_android_thread_env = NULL;
JNIEnv *
gdk_android_get_env (void)
{
  if (gdk_android_thread_env)
    return gdk_android_thread_env;
  if (G_UNLIKELY (!gdk_android_vm))
    return NULL;
  gint rc = (*gdk_android_vm)->GetEnv (gdk_android_vm, (void **) &gdk_android_thread_env, JNI_VERSION_1_6);
  if (G_UNLIKELY (rc != JNI_OK))
    g_critical ("Unable to get env for the current thread. Is is attached?");
  return gdk_android_thread_env;
}

GdkAndroidThreadGuard
gdk_android_get_thread_env (void)
{
  if (gdk_android_thread_env)
    return (GdkAndroidThreadGuard) { .env = gdk_android_thread_env,
                                     .needs_detach = FALSE };
  GdkAndroidThreadGuard ret;
  gint rc = (*gdk_android_vm)->GetEnv (gdk_android_vm, (void **) &gdk_android_thread_env, JNI_VERSION_1_6);
  if (rc == JNI_OK)
    ret.needs_detach = FALSE;
  else if (rc == JNI_EDETACHED)
    {
      JavaVMAttachArgs args = {
        .version = JNI_VERSION_1_6,
        .name = "GDK Thread Environment",
        .group = NULL
      };
      rc = (*gdk_android_vm)->AttachCurrentThread (gdk_android_vm, &ret.env, &args);
      if (G_UNLIKELY (rc != JNI_OK))
        {
          g_critical ("Unable to attach current thread to the JVM");
          ret.env = NULL;
          ret.needs_detach = FALSE;
        }
      else
        ret.needs_detach = TRUE;
    }
  else
    {
      g_critical ("Failed to get env thread");
      ret.env = NULL;
      ret.needs_detach = FALSE;
    }
  gdk_android_thread_env = ret.env;
  return ret;
}

void
gdk_android_drop_thread_env (GdkAndroidThreadGuard *self)
{
  if (self->needs_detach)
    {
      gdk_android_thread_env = NULL;
      gint rc = (*gdk_android_vm)->DetachCurrentThread (gdk_android_vm);
      if (G_UNLIKELY (rc != JNI_OK))
        g_critical ("Unable to detach thread from JVM");
    }
}

jobject
gdk_android_get_activity (void)
{
  return gdk_android_activity;
}

jobject
gdk_android_init_get_user_classloader (void)
{
  return gdk_android_user_classloader;
}

const GdkAndroidJavaCache *
gdk_android_get_java_cache (void)
{
  return &gdk_android_java_cache;
}
