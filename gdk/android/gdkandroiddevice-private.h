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

#include "gdkdeviceprivate.h"

#include "gdkandroidinit-private.h"

#include "gdkandroiddevice.h"

G_BEGIN_DECLS

typedef struct _GdkAndroidSurface GdkAndroidSurface;

struct _GdkAndroidDevice
{
  GdkDevice parent_instance;

  gint32 button_state;

  GdkModifierType last_mods;
  GdkAndroidSurface *last;
  gfloat last_x, last_y;
};

void gdk_android_device_maybe_update_surface (GdkAndroidDevice *self, GdkAndroidSurface *new_surface, GdkModifierType new_mods, guint32 timestamp, gfloat x, gfloat y);
void gdk_android_device_keyboard_maybe_update_surface_focus (GdkAndroidDevice *self, GdkAndroidSurface *new_surface);

G_END_DECLS
