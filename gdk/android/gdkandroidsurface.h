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

#if !defined (__GDKANDROID_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/android/gdkandroid.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GdkAndroidSurface      GdkAndroidSurface;
typedef struct _GdkAndroidSurfaceClass GdkAndroidSurfaceClass;

#define GDK_TYPE_ANDROID_SURFACE            (gdk_android_surface_get_type ())
#define GDK_ANDROID_SURFACE(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_SURFACE, GdkAndroidSurface))
#define GDK_ANDROID_SURFACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_ANDROID_SURFACE, GdkAndroidSurfaceClass))
#define GDK_IS_ANDROID_SURFACE(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_SURFACE))
#define GDK_IS_ANDROID_SURFACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_ANDROID_SURFACE))
#define GDK_ANDROID_SURFACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_ANDROID_SURFACE, GdkAndroidSurfaceClass))

GDK_AVAILABLE_IN_4_18
GType gdk_android_surface_get_type (void);

G_END_DECLS
