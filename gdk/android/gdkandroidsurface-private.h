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

#include "gdksurfaceprivate.h"

#include <android/native_window.h>

#include <jni.h>

#include "gdkandroidsurface.h"

G_BEGIN_DECLS

void _gdk_android_surface_bind_native (JNIEnv *env, jobject this, jlong identifier);
void _gdk_android_surface_on_attach (JNIEnv *env, jobject this);
void _gdk_android_surface_on_layout_surface (JNIEnv *env, jobject this, jint width, jint height, jfloat scale);
void _gdk_android_surface_on_layout_position (JNIEnv *env, jobject this, jint x, jint y);
void _gdk_android_surface_on_detach (JNIEnv *env, jobject this);

void _gdk_android_surface_on_dnd_start_failed (JNIEnv *env, jobject this, jobject native_identifier);

void _gdk_android_surface_on_motion_event (JNIEnv *env, jobject this, jint event_identifier, jobject event);
void _gdk_android_surface_on_key_event (JNIEnv *env, jobject this, jobject event);
jboolean _gdk_android_surface_on_drag_event (JNIEnv *env, jobject this, jobject event);

void _gdk_android_surface_on_visibility_ui_thread (JNIEnv *env, jobject this, jboolean visible);

typedef struct
{
  gint x;
  gint y;
  gint width;
  gint height;
  gfloat scale;
} GdkAndroidSurfaceConfiguration;

struct _GdkAndroidSurface
{
  GdkSurface parent_instance;

  jobject surface;
  GMutex native_lock;
  ANativeWindow *native;

  gboolean visible;

  // during _surface_set_visibility we do not know the size of the surface yet.
  // this allows us to delay the surface mapping to the _surface_on_layout call
  gboolean delayed_map;

  GdkAndroidSurfaceConfiguration cfg;

  GdkDrop *active_drop;
};

struct _GdkAndroidSurfaceClass
{
  GdkSurfaceClass parent_class;

  void (*on_layout) (GdkAndroidSurface *self);
  void (*reposition) (GdkAndroidSurface *self);
};

typedef struct _GdkAndroidToplevel GdkAndroidToplevel;
GdkAndroidToplevel *gdk_android_surface_get_toplevel (GdkAndroidSurface *self);

G_END_DECLS
