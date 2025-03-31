/*
 * gtkimcontextandroid.c:
 * Copyright (C) 2024 Florian "sp1rit" <sp1rit@disroot.org>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * $Id:$
 */

#include "config.h"
#include <string.h>

#include "gtk/gtkimcontextandroid.h"
#include "gtk/gtkimmoduleprivate.h"
#include "gtk/gtkprivate.h"

#include "gdk/android/gdkandroid.h"
#include "gdk/android/gdkandroidinit-private.h"
#include "gdk/android/gdkandroidsurface-private.h"
#include "gdk/android/gdkandroidutils-private.h"

#include "gtk/gtkimcontextsimple.h"

#define GTK_IM_CONTEXT_TYPE_ANDROID           (gtk_im_context_android_get_type())
#define GTK_IM_CONTEXT_ANDROID(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_IM_CONTEXT_TYPE_ANDROID, GtkIMContextAndroid))
#define GTK_IS_IM_CONTEXT_ANDROID(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_IM_CONTEXT_TYPE_ANDROID))
#define GTK_IM_CONTEXT_ANDROID_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_IM_CONTEXT_TYPE_ANDROID, GtkIMContextAndroidClass))

typedef struct {
  jclass class;
  jmethodID constructor;
  jfieldID native_ptr;
  jmethodID reset;
  struct {
    jclass class;
    jmethodID constructor;
  } surrounding_retval;
  struct {
    jint class_text;
    jint text_flag_cap_characters;
    jint text_flag_cap_words;
    jint text_flag_cap_sentences;
    jint text_flag_auto_correct;
    jint text_flag_auto_complete;
    jint text_flag_multi_line;
    jint text_flag_ime_multi_line;
    jint text_flag_no_suggestions;
    jint text_variation_uri;
    jint text_variation_email_address;
    jint text_variation_person_name;
    jint text_variation_postal_address;
    jint text_variation_password;
    jint text_variation_visible_password;
    jint class_number;
    jint number_flag_signed;
    jint number_flag_decimal;
    jint number_variation_password;
    jint class_phone;
    jint class_datetime;
    jint datetime_variation_date;
    jint datetime_variation_time;
  } input_type;
} GtkIMContextAndroidJavaCache;

typedef struct _GtkIMContextAndroidClass
{
  GtkIMContextSimpleClass parent_class;
} GtkIMContextAndroidClass;

typedef struct _GtkIMContextAndroid
{
  GtkIMContextSimple parent;
  jobject context;
  GdkSurface *client_surface;
  GtkWidget *client_widget;
  gboolean focused;
  GtkInputPurpose input_purpose;
  GtkInputHints input_hints;
  jstring preedit;
  gint preedit_cursor;
} GtkIMContextAndroid;

static GtkIMContextAndroidJavaCache gtk_im_context_android_java_cache;

#define GTK_IM_CONTEXT_ANDROID_DECLARE_SELF(this) \
  GtkIMContextAndroid *self = (GtkIMContextAndroid *)(*env)->GetLongField (env, (this), gtk_im_context_android_java_cache.native_ptr); \
  if (self == NULL) \
    return; \
  g_return_if_fail (GTK_IS_IM_CONTEXT_ANDROID (self));
#define GTK_IM_CONTEXT_ANDROID_DECLARE_SELF_WITH_RET(this, retval) \
  GtkIMContextAndroid *self = (GtkIMContextAndroid *)(*env)->GetLongField (env, (this), gtk_im_context_android_java_cache.native_ptr); \
  if (self == NULL) \
    return (retval); \
  g_return_val_if_fail (GTK_IS_IM_CONTEXT_ANDROID (self), (retval));

static jint
_gtk_im_context_android_get_input_type (JNIEnv *env, jobject this)
{
  GTK_IM_CONTEXT_ANDROID_DECLARE_SELF_WITH_RET (this, 0)
  jint input_type = 0;
  switch (self->input_purpose)
    {
    case GTK_INPUT_PURPOSE_FREE_FORM:
      input_type = gtk_im_context_android_java_cache.input_type.class_text;
      break;
    case GTK_INPUT_PURPOSE_ALPHA:
      input_type = gtk_im_context_android_java_cache.input_type.class_text;
      break;
    case GTK_INPUT_PURPOSE_DIGITS:
      input_type = gtk_im_context_android_java_cache.input_type.class_number;
      break;
    case GTK_INPUT_PURPOSE_NUMBER:
      input_type = gtk_im_context_android_java_cache.input_type.class_text;
      input_type |= gtk_im_context_android_java_cache.input_type.number_flag_signed;
      input_type |= gtk_im_context_android_java_cache.input_type.number_flag_decimal;
      break;
    case GTK_INPUT_PURPOSE_PHONE:
      input_type = gtk_im_context_android_java_cache.input_type.class_phone;
      break;
    case GTK_INPUT_PURPOSE_URL:
      input_type = gtk_im_context_android_java_cache.input_type.class_text;
      input_type |= gtk_im_context_android_java_cache.input_type.text_variation_uri;
      break;
    case GTK_INPUT_PURPOSE_EMAIL:
      input_type = gtk_im_context_android_java_cache.input_type.class_text;
      input_type |= gtk_im_context_android_java_cache.input_type.text_variation_email_address;
      break;
    case GTK_INPUT_PURPOSE_NAME:
      input_type = gtk_im_context_android_java_cache.input_type.class_text;
      input_type |= gtk_im_context_android_java_cache.input_type.text_variation_person_name;
      break;
    case GTK_INPUT_PURPOSE_PASSWORD:
      input_type = gtk_im_context_android_java_cache.input_type.class_text;
      input_type |= gtk_im_context_android_java_cache.input_type.text_variation_password;
      break;
    case GTK_INPUT_PURPOSE_PIN:
      input_type = gtk_im_context_android_java_cache.input_type.class_number;
      input_type |= gtk_im_context_android_java_cache.input_type.number_variation_password;
      break;
    case GTK_INPUT_PURPOSE_TERMINAL:
      input_type = gtk_im_context_android_java_cache.input_type.class_text;
      break;
    default:
      break;
    }

  if (input_type & gtk_im_context_android_java_cache.input_type.class_text)
    {
      if (self->input_hints & GTK_INPUT_HINT_SPELLCHECK)
        input_type |= gtk_im_context_android_java_cache.input_type.text_flag_auto_correct;
      if (self->input_hints & GTK_INPUT_HINT_WORD_COMPLETION)
        input_type |= gtk_im_context_android_java_cache.input_type.text_flag_auto_complete;
      if (self->input_hints & GTK_INPUT_HINT_UPPERCASE_CHARS)
        input_type |= gtk_im_context_android_java_cache.input_type.text_flag_cap_characters;
      if (self->input_hints & GTK_INPUT_HINT_UPPERCASE_WORDS)
        input_type |= gtk_im_context_android_java_cache.input_type.text_flag_cap_words;
      if (self->input_hints & GTK_INPUT_HINT_UPPERCASE_SENTENCES)
        input_type |= gtk_im_context_android_java_cache.input_type.text_flag_cap_sentences;
    }

  return input_type;
}

static jobject
_gtk_im_context_android_get_surrounding (JNIEnv *env, jobject this)
{
  GTK_IM_CONTEXT_ANDROID_DECLARE_SELF_WITH_RET (this, NULL)
  gchar *text;
  gint cursor_idx_bytes, anchor_idx_bytes;
  if (!gtk_im_context_get_surrounding_with_selection ((GtkIMContext *)self, &text, &cursor_idx_bytes, &anchor_idx_bytes))
    return NULL;
  glong cursor_idx = g_utf8_strlen (text, cursor_idx_bytes);
  glong anchor_idx = g_utf8_strlen (text, anchor_idx_bytes);
  jstring jtext = gdk_android_utf8_to_java (text);
  g_free (text);
  return (*env)->NewObject (env, gtk_im_context_android_java_cache.surrounding_retval.class,
                            gtk_im_context_android_java_cache.surrounding_retval.constructor,
                            jtext, (jint)cursor_idx, (jint)anchor_idx);
}

static jboolean
_gtk_im_context_android_delete_surrounding (JNIEnv *env, jobject this, jint offset, jint n_chars)
{
  GTK_IM_CONTEXT_ANDROID_DECLARE_SELF_WITH_RET (this, FALSE)
  return gtk_im_context_delete_surrounding ((GtkIMContext *)self, offset, n_chars);
}

static jobject
_gtk_im_context_android_get_preedit (JNIEnv *env, jobject this)
{
  GTK_IM_CONTEXT_ANDROID_DECLARE_SELF_WITH_RET (this, NULL)
  return self->preedit;
}

static void
_gtk_im_context_android_update_preedit (JNIEnv *env, jobject this, jstring string, jint cursor)
{
  GTK_IM_CONTEXT_ANDROID_DECLARE_SELF (this)

  gboolean preedit_started = FALSE;
  if (self->preedit)
    {
      (*env)->DeleteGlobalRef (env, self->preedit);
      preedit_started = TRUE;
    }
  self->preedit = string ? (*env)->NewGlobalRef (env, string) : NULL;
  self->preedit_cursor = cursor;
  if (self->preedit)
    {
      if (!preedit_started)
          g_signal_emit_by_name (self, "preedit-start");
      g_signal_emit_by_name (self, "preedit-changed");
    }
  else if (preedit_started)
    {
      g_signal_emit_by_name (self, "preedit-changed");
      g_signal_emit_by_name (self, "preedit-end");
    }
}

static jboolean
_gtk_im_context_android_commit (JNIEnv *env, jobject this, jstring string)
{
  GTK_IM_CONTEXT_ANDROID_DECLARE_SELF_WITH_RET (this, FALSE)
  if (self->preedit) {
    if (string == NULL)
      string = (*env)->NewLocalRef (env, self->preedit);
    (*env)->DeleteGlobalRef (env, self->preedit);
    self->preedit = NULL;
    g_signal_emit_by_name (self, "preedit-changed");
    g_signal_emit_by_name (self, "preedit-end");
  }
  if (string)
    {
      gchar *str = gdk_android_java_to_utf8 (string, NULL);
      g_signal_emit_by_name (self, "commit", str);
      g_free (str);
      return TRUE;
    }
  return FALSE;
}

G_DEFINE_TYPE_WITH_CODE (GtkIMContextAndroid, gtk_im_context_android, GTK_TYPE_IM_CONTEXT_SIMPLE,
                         gtk_im_module_ensure_extension_point ();
                         g_io_extension_point_implement (GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "android", 0))

static const JNINativeMethod im_context_natives[] = {
  { .name = "getInputType", .signature = "()I", .fnPtr = _gtk_im_context_android_get_input_type },
  { .name = "getSurrounding", .signature = "()Lorg/gtk/android/ImContext$SurroundingRetVal;", .fnPtr = _gtk_im_context_android_get_surrounding },
  { .name = "deleteSurrounding", .signature = "(II)Z", .fnPtr = _gtk_im_context_android_delete_surrounding },
  { .name = "getPreedit", .signature = "()Ljava/lang/String;", .fnPtr = _gtk_im_context_android_get_preedit },
  { .name = "updatePreedit", .signature = "(Ljava/lang/String;I)V", .fnPtr = _gtk_im_context_android_update_preedit },
  { .name = "commit", .signature = "(Ljava/lang/String;)Z", .fnPtr = _gtk_im_context_android_commit }
};
static GtkIMContextAndroidJavaCache *
gdk_im_context_init_jni (void)
{
  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 1);
  jclass im_context = gdk_android_init_find_class_using_classloader (env,
                                                                     gdk_android_init_get_user_classloader (),
                                                                     "org/gtk/android/ImContext");
  gtk_im_context_android_java_cache.class = (*env)->NewGlobalRef (env, im_context);
  (*env)->RegisterNatives (env, gtk_im_context_android_java_cache.class, im_context_natives, sizeof im_context_natives / sizeof (JNINativeMethod));
  gtk_im_context_android_java_cache.constructor = (*env)->GetMethodID (env,
                                                                       gtk_im_context_android_java_cache.class,
                                                                       "<init>",
                                                                       "(J)V");
  gtk_im_context_android_java_cache.native_ptr = (*env)->GetFieldID (env,
                                                                     gtk_im_context_android_java_cache.class,
                                                                     "native_ptr",
                                                                     "J");
  gtk_im_context_android_java_cache.reset = (*env)->GetStaticMethodID (env,
                                                                       gtk_im_context_android_java_cache.class,
                                                                       "reset",
                                                                       "(Landroid/view/View;)V");
  jclass im_context_surrounding_retval = gdk_android_init_find_class_using_classloader (env,
                                                                                        gdk_android_init_get_user_classloader (),
                                                                                        "org/gtk/android/ImContext$SurroundingRetVal");
  gtk_im_context_android_java_cache.surrounding_retval.class = (*env)->NewGlobalRef (env, im_context_surrounding_retval);
  gtk_im_context_android_java_cache.surrounding_retval.constructor = (*env)->GetMethodID (env,
                                                                                          im_context_surrounding_retval,
                                                                                          "<init>",
                                                                                          "(Ljava/lang/String;II)V");


#define FILL_INPUT_TYPE(cname, jname) \
  gtk_im_context_android_java_cache.input_type.cname = (*env)->GetStaticIntField (env, input_type, (*env)->GetStaticFieldID (env, input_type, ("TYPE_" jname), "I"));
  jclass input_type = (*env)->FindClass (env, "android/text/InputType");
  FILL_INPUT_TYPE (class_text, "CLASS_TEXT")
  FILL_INPUT_TYPE (text_flag_cap_characters, "TEXT_FLAG_CAP_CHARACTERS")
  FILL_INPUT_TYPE (text_flag_cap_words, "TEXT_FLAG_CAP_WORDS")
  FILL_INPUT_TYPE (text_flag_cap_sentences, "TEXT_FLAG_CAP_WORDS")
  FILL_INPUT_TYPE (text_flag_auto_correct, "TEXT_FLAG_AUTO_CORRECT")
  FILL_INPUT_TYPE (text_flag_auto_complete, "TEXT_FLAG_AUTO_COMPLETE")
  FILL_INPUT_TYPE (text_flag_multi_line, "TEXT_FLAG_MULTI_LINE")
  FILL_INPUT_TYPE (text_flag_ime_multi_line, "TEXT_FLAG_IME_MULTI_LINE")
  FILL_INPUT_TYPE (text_flag_no_suggestions, "TEXT_FLAG_NO_SUGGESTIONS")
  FILL_INPUT_TYPE (text_variation_uri, "TEXT_VARIATION_URI")
  FILL_INPUT_TYPE (text_variation_email_address, "TEXT_VARIATION_EMAIL_ADDRESS")
  FILL_INPUT_TYPE (text_variation_person_name, "TEXT_VARIATION_PERSON_NAME")
  FILL_INPUT_TYPE (text_variation_postal_address, "TEXT_VARIATION_POSTAL_ADDRESS")
  FILL_INPUT_TYPE (text_variation_password, "TEXT_VARIATION_PASSWORD")
  FILL_INPUT_TYPE (text_variation_visible_password, "TEXT_VARIATION_VISIBLE_PASSWORD")
  FILL_INPUT_TYPE (class_number, "CLASS_NUMBER")
  FILL_INPUT_TYPE (number_flag_signed, "NUMBER_FLAG_SIGNED")
  FILL_INPUT_TYPE (number_flag_decimal, "NUMBER_FLAG_DECIMAL")
  FILL_INPUT_TYPE (number_variation_password, "NUMBER_VARIATION_PASSWORD")
  FILL_INPUT_TYPE (class_phone, "CLASS_PHONE")
  FILL_INPUT_TYPE (class_datetime, "CLASS_DATETIME")
  FILL_INPUT_TYPE (datetime_variation_date, "DATETIME_VARIATION_DATE")
  FILL_INPUT_TYPE (datetime_variation_time, "DATETIME_VARIATION_TIME")

  (*env)->PopLocalFrame (env, NULL);
  return &gtk_im_context_android_java_cache;
}

static void
gdk_im_context_android_finalize (GObject *object)
{
  GtkIMContextAndroid *self = (GtkIMContextAndroid *)object;
  JNIEnv *env = gdk_android_get_env ();
  (*env)->SetLongField (env, self->context, gtk_im_context_android_java_cache.native_ptr, 0);
  (*env)->DeleteGlobalRef (env, self->context);
  if (self->preedit)
    (*env)->DeleteGlobalRef (env, self->preedit);
  G_OBJECT_CLASS(gtk_im_context_android_parent_class)->finalize (object);
}

static void
gtk_im_context_android_update_ime_keyboard (GtkIMContextAndroid *self)
{
  if (!GDK_IS_ANDROID_SURFACE (self->client_surface))
    return;
  GdkAndroidSurface *surface = (GdkAndroidSurface *)self->client_surface;
  if (surface->surface == NULL)
    return;

  JNIEnv *env = gdk_android_display_get_env (gdk_surface_get_display (self->client_surface));
  (*env)->CallVoidMethod (env, surface->surface,
                          gdk_android_get_java_cache ()->surface.set_active_im_context,
                          self->context);
  (*env)->CallVoidMethod (env, surface->surface,
                          gdk_android_get_java_cache ()->surface.set_ime_keyboard_state,
                          self->focused);
}

static void
gtk_im_context_android_set_client_widget (GtkIMContext *context,
                                          GtkWidget    *widget)
{
  GtkIMContextAndroid *self = GTK_IM_CONTEXT_ANDROID (context);

  GTK_DEBUG (MODULES, "gtk_im_context_android_set_client_surface: %p", widget);

  self->client_widget = widget;
  self->client_surface = NULL;

  if (widget != NULL)
    {
      GtkNative *native = gtk_widget_get_native (widget);
      if (native != NULL)
        {
          self->client_surface = gtk_native_get_surface (native);
          gtk_im_context_android_update_ime_keyboard (self);
        }
    }

  if (GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->set_client_widget)
    GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->set_client_widget (context, widget);
}

static void
gtk_im_context_android_get_preedit_string (GtkIMContext   *context,
                                           gchar         **string,
                                           PangoAttrList **attrs,
                                           gint           *cursor_pos)
{
  GtkIMContextAndroid *self = GTK_IM_CONTEXT_ANDROID (context);
  if (!self->preedit)
    return GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->get_preedit_string (context, string, attrs, cursor_pos);

  gchar* utf8 = gdk_android_java_to_utf8 (self->preedit, NULL);
  if (cursor_pos)
    *cursor_pos = self->preedit_cursor >= 0 ? self->preedit_cursor : g_utf8_strlen (utf8, -1);
  if (attrs)
    {
      *attrs = pango_attr_list_new ();
      if (utf8 && *utf8)
        {
          gsize len = strlen (utf8);
          PangoAttribute *attr;
          attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
          attr->start_index = 0;
          attr->end_index = len;
          pango_attr_list_insert (*attrs, attr);
          attr = pango_attr_fallback_new (TRUE);
          attr->start_index = 0;
          attr->end_index = len;
          pango_attr_list_insert (*attrs, attr);
        }
    }
  if (string)
    *string = utf8;
  else
    g_free (string);
}

static void
gtk_im_context_android_reset (GtkIMContext *context)
{
  GtkIMContextAndroid *self = GTK_IM_CONTEXT_ANDROID (context);
  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 1);

  if (self->preedit)
    {
      (*env)->DeleteGlobalRef (env, self->preedit);
      self->preedit = NULL;
      g_signal_emit_by_name (self, "preedit-changed");
      g_signal_emit_by_name (self, "preedit-end");
    }

  if (GDK_IS_ANDROID_SURFACE (self->client_surface))
    {
      GdkAndroidSurface *surface = (GdkAndroidSurface *)self->client_surface;
      if (surface->surface)
        (*env)->CallStaticVoidMethod (env, gtk_im_context_android_java_cache.class,
                                  gtk_im_context_android_java_cache.reset,
                                  surface->surface);
    }
  (*env)->PopLocalFrame (env, NULL);
  GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->reset (context);
}

static void
gtk_im_context_android_focus_in (GtkIMContext *context)
{
  GTK_DEBUG (MODULES, "gtk_im_context_android_focus_in");

  GtkIMContextAndroid *self = GTK_IM_CONTEXT_ANDROID (context);
  self->focused = TRUE;
  gtk_im_context_android_update_ime_keyboard (self);

  if (GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->focus_in)
    GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->focus_in (context);
}

static void
gtk_im_context_android_focus_out (GtkIMContext *context)
{
  GTK_DEBUG (MODULES, "gtk_im_context_android_focus_out");

  GtkIMContextAndroid *self = GTK_IM_CONTEXT_ANDROID (context);
  self->focused = FALSE;
  gtk_im_context_android_update_ime_keyboard (self);

  if (GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->focus_out)
    GTK_IM_CONTEXT_CLASS (gtk_im_context_android_parent_class)->focus_out (context);
}

static gboolean
gtk_im_context_activate_osk_keyboard (GtkIMContext *context,
                                      GdkEvent     *event)
{
  GtkIMContextAndroid *self = (GtkIMContextAndroid *)context;
  gtk_im_context_android_update_ime_keyboard (self);
  return self->focused;
}

static void
gtk_im_context_android_class_init (GtkIMContextAndroidClass *klass)
{
  GTK_DEBUG (MODULES, "gtk_im_context_android_class_init");
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkIMContextClass *context_class = GTK_IM_CONTEXT_CLASS (klass);

  static GOnce jni_init = G_ONCE_INIT;
  g_once (&jni_init, (GThreadFunc)gdk_im_context_init_jni, NULL);

  object_class->finalize = gdk_im_context_android_finalize;
  context_class->set_client_widget = gtk_im_context_android_set_client_widget;
  context_class->get_preedit_string = gtk_im_context_android_get_preedit_string;
  context_class->reset = gtk_im_context_android_reset;
  context_class->focus_in = gtk_im_context_android_focus_in;
  context_class->focus_out = gtk_im_context_android_focus_out;
  context_class->activate_osk_with_event = gtk_im_context_activate_osk_keyboard;
}

static void
gtk_im_context_android_init (GtkIMContextAndroid *self)
{
  GTK_DEBUG (MODULES, "gtk_im_context_android_init");

  self->focused = FALSE;
  self->input_purpose = GTK_INPUT_PURPOSE_FREE_FORM;
  self->input_hints = GTK_INPUT_HINT_NONE;

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 1);
  self->context = (*env)->NewGlobalRef(env,
                                       (*env)->NewObject (env, gtk_im_context_android_java_cache.class,
                                                          gtk_im_context_android_java_cache.constructor,
                                                          self));
  (*env)->PopLocalFrame (env, NULL);

  self->preedit_cursor = -1;
}
