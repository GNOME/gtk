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

#include <android/input.h>

#include "gdkseatprivate.h"

#include "gdkandroidsurface-private.h"

#include "gdkandroidseat.h"

G_BEGIN_DECLS

struct _GdkAndroidSeatClass
{
  GdkSeatClass parent_class;
};

struct _GdkAndroidSeat
{
  GdkSeat parent_instance;

  GdkDevice *logical_pointer;
  GdkDevice *logical_touchscreen;
  GdkDevice *logical_keyboard;

  GdkDeviceTool *stylus;
  GdkDeviceTool *eraser;
  GdkDeviceTool *mouse;

  // ToplevelActivity.ToplevelView
  jobject active_grab_view;
};

GdkAndroidSeat *gdk_android_seat_new (GdkDisplay *display);

GdkDeviceTool *gdk_android_seat_get_device_tool (GdkAndroidSeat *self, gint32 tool_type);

gboolean gdk_android_seat_normalize_range (JNIEnv *env, jobject device, const AInputEvent *event, size_t pointer_index, guint32 mask, gfloat from, gfloat to, gdouble *out);
gdouble *gdk_android_seat_create_axes_from_motion_event (const AInputEvent *event, size_t pointer_index);
void gdk_android_seat_consume_event (GdkDisplay *display, GdkEvent *event);

G_END_DECLS
