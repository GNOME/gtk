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

#include <gdk/gdk.h>

#include "gdktoplevelprivate.h"

#include "gdkandroidmonitor-private.h"
#include "gdkandroidsurface-private.h"

#include "gdkandroidtoplevel.h"

G_BEGIN_DECLS

void _gdk_android_toplevel_bind_native (JNIEnv *env, jobject this, jlong native_identifier);
void _gdk_android_toplevel_on_configuration_change (JNIEnv *env, jobject this);
void _gdk_android_toplevel_on_state_change (JNIEnv *env, jobject this, jboolean has_focus, jboolean is_fullscreen);
void _gdk_android_toplevel_on_back_press (JNIEnv *env, jobject this);
void _gdk_android_toplevel_on_destroy (JNIEnv *env, jobject this);
void _gdk_android_toplevel_on_activity_result (JNIEnv *env, jobject this, jint request_code, jint response_code, jobject result);

typedef struct _GdkAndroidToplevelClass GdkAndroidToplevelClass;
struct _GdkAndroidToplevelClass
{
  GdkAndroidSurfaceClass parent_instance;
};

struct _GdkAndroidToplevel
{
  GdkAndroidSurface parent_instance;

  GdkAndroidMonitor *monitor;

  jobject intent;
  jobject activity;
  gboolean did_spawn_activity;

  GdkToplevelLayout *layout;

  gchar *title;

  guint activity_request_counter;
  // GHashTable<int, (private) GdkAndroidToplevelActivityRequest>
  GHashTable *activity_requests;
};


const GdkRGBA *gdk_android_toplevel_get_bars_color (GdkAndroidToplevel *self);

G_END_DECLS
