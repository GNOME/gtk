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

#include <jni.h>

#include "gdkandroiddisplay.h"
#include "gdkandroidmonitor.h"

#include "gdkmonitorprivate.h"

G_BEGIN_DECLS

GdkMonitor *
gdk_android_monitor_new (GdkDisplay *display);

void
gdk_android_monitor_update (GdkAndroidMonitor *self, const GdkRectangle *bounds, gfloat density);

void
gdk_android_monitor_add_toplevel (GdkAndroidMonitor *self);

void
gdk_android_monitor_drop_toplevel (GdkAndroidMonitor *self);

G_END_DECLS
