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

#include "gdkandroidcontentfile.h"

G_BEGIN_DECLS

gboolean gdk_android_content_file_has_exception (JNIEnv *env, GError **err);


typedef struct _GdkAndroidJavaFileInputStream      GdkAndroidJavaFileInputStream;
typedef struct _GdkAndroidJavaFileInputStreamClass GdkAndroidJavaFileInputStreamClass;

#define GDK_TYPE_ANDROID_JAVA_FILE_INPUT_STREAM       (gdk_android_java_file_input_stream_get_type ())
#define GDK_ANDROID_JAVA_FILE_INPUT_STREAM(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_JAVA_FILE_INPUT_STREAM, GdkAndroidJavaFileInputStream))
#define GDK_IS_ANDROID_JAVA_FILE_INPUT_STREAM(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_JAVA_FILE_INPUT_STREAM))
GType gdk_android_java_file_input_stream_get_type (void);

GFileInputStream *gdk_android_java_file_input_stream_wrap (JNIEnv *env, jobject file_input_stream);


typedef struct _GdkAndroidJavaFileOutputStream      GdkAndroidJavaFileOutputStream;
typedef struct _GdkAndroidJavaFileOutputStreamClass GdkAndroidJavaFileOutputStreamClass;

#define GDK_TYPE_ANDROID_JAVA_FILE_OUTPUT_STREAM       (gdk_android_java_file_output_stream_get_type ())
#define GDK_ANDROID_JAVA_FILE_OUTPUT_STREAM(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_JAVA_FILE_OUTPUT_STREAM, GdkAndroidJavaFileOutputStream))
#define GDK_IS_ANDROID_JAVA_FILE_OUTPUT_STREAM(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_JAVA_FILE_OUTPUT_STREAM))
GType gdk_android_java_file_output_stream_get_type (void);

GFileOutputStream *gdk_android_java_file_output_stream_wrap (JNIEnv *env, jobject file_output_stream);

G_END_DECLS
