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

#pragma once

#include <gdk/gdk.h>
#include <jni.h>

G_BEGIN_DECLS

void _gdk_android_glib_context_run_on_main (JNIEnv *env, jclass klass, jobject runnable);

GdkRectangle gdk_android_utils_rect_to_gdk (jobject rect);

jint gdk_android_utils_color_to_android (const GdkRGBA *rgba);

jstring gdk_android_utf8n_to_java (const gchar *utf8, gssize utf8_len);
jstring gdk_android_utf8_to_java (const gchar *utf8);
gchar *gdk_android_java_to_utf8 (jstring string, gsize *len);

void gdk_android_utils_unref_jobject (jobject object);

#define GDK_ANDROID_ERROR (gdk_android_error_quark ())
GQuark gdk_android_error_quark (void);
typedef enum {
  GDK_ANDROID_JAVA_EXCEPTION
} GdkAndroidErrorEnum;
gboolean gdk_android_check_exception (GError **error);

G_END_DECLS
