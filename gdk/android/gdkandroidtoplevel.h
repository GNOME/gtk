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

#pragma once

#if !defined (__GDKANDROID_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/android/gdkandroid.h> can be included directly."
#endif

#include <gdk/gdk.h>

#include <jni.h>

G_BEGIN_DECLS

typedef struct _GdkAndroidToplevel GdkAndroidToplevel;

#define GDK_TYPE_ANDROID_TOPLEVEL       (gdk_android_toplevel_get_type ())
#define GDK_ANDROID_TOPLEVEL(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_TOPLEVEL, GdkAndroidToplevel))
#define GDK_IS_ANDROID_TOPLEVEL(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_TOPLEVEL))

GDK_AVAILABLE_IN_4_18
GType gdk_android_toplevel_get_type (void);

GDK_AVAILABLE_IN_4_18
jobject
gdk_android_toplevel_get_activity (GdkAndroidToplevel *self);

GDK_AVAILABLE_IN_4_18
gboolean
gdk_android_toplevel_launch_activity (GdkAndroidToplevel *self, jobject intent, GError **error);

GDK_AVAILABLE_IN_4_18
void gdk_android_toplevel_launch_activity_for_result_async (GdkAndroidToplevel *self, jobject intent, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
GDK_AVAILABLE_IN_4_18
gboolean gdk_android_toplevel_launch_activity_for_result_finish (GdkAndroidToplevel *self, GAsyncResult *result, jint *response, jobject *data, GError **error);

G_END_DECLS
