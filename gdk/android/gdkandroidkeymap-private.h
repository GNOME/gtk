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

#include "gdkandroidsurface-private.h"

G_BEGIN_DECLS

typedef struct _GdkAndroidKeymap      GdkAndroidKeymap;
typedef struct _GdkAndroidKeymapClass GdkAndroidKeymapClass;

#define GDK_TYPE_ANDROID_KEYMAP       (gdk_android_keymap_get_type ())
#define GDK_ANDROID_KEYMAP(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_KEYMAP, GdkAndroidKeymap))
#define GDK_IS_ANDROID_KEYMAP(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_KEYMAP))

GType gdk_android_keymap_get_type (void);

GdkAndroidKeymap *_gdk_android_keymap_new (void);

G_END_DECLS
