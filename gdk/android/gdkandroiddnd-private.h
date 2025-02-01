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

#include "gdksurfaceprivate.h"
#include "gdkdragprivate.h"
#include "gdkdropprivate.h"

#include "gdkandroiddisplay.h"

G_BEGIN_DECLS

typedef struct _GdkAndroidDrag GdkAndroidDrag;
typedef struct _GdkAndroidSurface GdkAndroidSurface;

#define GDK_TYPE_ANDROID_DRAG_SURFACE       (gdk_android_drag_surface_get_type ())
#define GDK_ANDROID_DRAG_SURFACE(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_DRAG_SURFACE, GdkAndroidDragSurface))
#define GDK_IS_ANDROID_DRAG_SURFACE(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_DRAG_SURFACE))
GType   gdk_android_drag_surface_get_type   (void);

typedef struct _GdkAndroidDragSurface {
  GdkSurface parent_instance;

  GdkAndroidDrag *drag;
  gint width,height;
  gint hot_x,hot_y;
} GdkAndroidDragSurface;

#define GDK_TYPE_ANDROID_DRAG       (gdk_android_drag_get_type ())
#define GDK_ANDROID_DRAG(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_DRAG, GdkAndroidDrag))
#define GDK_IS_ANDROID_DRAG(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_DRAG))
GType   gdk_android_drag_get_type   (void);

typedef struct _GdkAndroidDrag {
  GdkDrag parent_instance;

  GdkAndroidDragSurface *surface;
} GdkAndroidDrag;

#define GDK_TYPE_ANDROID_DROP       (gdk_android_drop_get_type ())
#define GDK_ANDROID_DROP(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_DROP, GdkAndroidDrop))
#define GDK_IS_ANDROID_DROP(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_DROP))
GType   gdk_android_drop_get_type   (void);

typedef struct _GdkAndroidDrop {
  GdkDrop parent_instance;

  GdkDragAction possible_actions;
  jobject drop;
  gboolean drop_finished;
  GdkDragAction commited_action;
} GdkAndroidDrop;


GdkDrag *gdk_android_dnd_surface_drag_begin (GdkSurface *surface, GdkDevice *device, GdkContentProvider *content, GdkDragAction actions, double dx, double dy);
void gdk_android_dnd_handle_drag_start_fail (GdkAndroidDisplay *display, jobject native_identifier);

gboolean gdk_android_dnd_surface_handle_drop_event (GdkAndroidSurface *self, jobject event);

G_END_DECLS
