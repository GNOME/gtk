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

#include "gdkclipboardprivate.h"

G_BEGIN_DECLS

void _gdk_android_clipboard_on_clipboard_changed (JNIEnv *env, jobject this);

typedef struct _GdkAndroidClipboard
{
  GdkClipboard parent_instance;

  GCancellable *cancellable;

  jobject manager;
  jobject listener;
} GdkAndroidClipboard;

#define GDK_TYPE_ANDROID_CLIPBOARD       (gdk_android_clipboard_get_type ())
#define GDK_ANDROID_CLIPBOARD(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_CLIPBOARD, GdkAndroidClipboard))
#define GDK_IS_ANDROID_CLIPBOARD(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_CLIPBOARD))
GType gdk_android_clipboard_get_type (void);

GdkClipboard *gdk_android_clipboard_new (GdkDisplay *display);

void
gdk_android_clipboard_clipdata_from_provider_async (GdkContentProvider *provider,
                                                    GdkContentFormats  *formats,
                                                    jobject             context,
                                                    GCancellable       *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer            user_data);

jobject
gdk_android_clipboard_clipdata_from_provider_finish (GdkContentProvider *provider,
                                                     GAsyncResult       *result,
                                                     GError            **error);

GdkContentFormats * gdk_android_clipboard_description_to_formats (jobject clipdesc);

void gdk_android_clipdata_read_async (GTask *task, jobject clipdata, GdkContentFormats *formats);
GInputStream *gdk_android_clipdata_read_finish (GAsyncResult *result, const char **out_mime_type, GError **error);

/*
 * As the android clipboard listener is less than useless in actually notifing us about clipboard
 * changes (it only seems to notify us of our own changes), we need to access available formats
 * in another way.
 * Additionally, we may only access the clipboard while in focus, so our best option is to call this
 * method each time a toplevel window gains focus.
 */
void gdk_android_clipboard_update_remote_formats (GdkAndroidClipboard *self);

G_END_DECLS
