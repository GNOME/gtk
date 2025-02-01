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

#include "gdkdisplayprivate.h"

#include "gdkandroidseat-private.h"

#include "gdkandroiddisplay.h"

G_BEGIN_DECLS

struct _GdkAndroidDisplay
{
  GdkDisplay parent_instance;

  GMutex surface_lock;
  // GHashTable<long, GdkAndroidSurface>
  GHashTable *surfaces;

  GListStore *monitors;
  GdkAndroidSeat *seat;
  GdkKeymap *keymap;

  // GHashTable<long, GdkAndroidDrag>
  GHashTable *drags;

  GdkAndroidDisplayNightMode night_mode;
};

struct _GdkAndroidDisplayClass
{
  GdkDisplayClass parent_class;
};

GdkDisplay *_gdk_android_display_open (const char *display_name);

GdkAndroidDisplay *gdk_android_display_get_display_instance (void);

// ret transfers full
GdkAndroidSurface *gdk_android_display_get_surface_from_identifier (GdkAndroidDisplay *self, glong identifier);
// surface: transfers none
void gdk_android_display_add_surface (GdkAndroidDisplay *self, GdkAndroidSurface *surface);

void gdk_android_display_update_night_mode (GdkAndroidDisplay *self, jobject context);

G_END_DECLS
