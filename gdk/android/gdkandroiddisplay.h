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

/**
 * GdkAndroidDisplayNightMode:
 * @GDK_ANDROID_DISPLAY_NIGHT_UNDEFINED: No night mode set in the UI configuration
 * @GDK_ANDROID_DISPLAY_NIGHT_NO: Night mode disabled bit set in the UI configuration
 * @GDK_ANDROID_DISPLAY_NIGHT_YES: Night mode enabled bit set in the UI configuration
 *
 * Used in [class@Gdk.AndroidDisplay] to handle the current night mode
 * setting in the
 * [UI configuration](https://developer.android.com/reference/android/content/res/Configuration#uiMode).
 *
 * Since: 4.18
 */
#define GDK_TYPE_ANDROID_DISPLAY_NIGHT_MODE (gdk_android_display_night_mode_get_type ())
GDK_AVAILABLE_IN_4_18
GType gdk_android_display_night_mode_get_type (void);
typedef enum
{
  GDK_ANDROID_DISPLAY_NIGHT_UNDEFINED,
  GDK_ANDROID_DISPLAY_NIGHT_NO,
  GDK_ANDROID_DISPLAY_NIGHT_YES,
} GdkAndroidDisplayNightMode;

typedef struct _GdkAndroidDisplay GdkAndroidDisplay;
typedef struct _GdkAndroidDisplayClass GdkAndroidDisplayClass;

#define GDK_TYPE_ANDROID_DISPLAY       (gdk_android_display_get_type ())
#define GDK_ANDROID_DISPLAY(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ANDROID_DISPLAY, GdkAndroidDisplay))
#define GDK_IS_ANDROID_DISPLAY(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ANDROID_DISPLAY))

GDK_AVAILABLE_IN_4_18
GType gdk_android_display_get_type (void);

GDK_AVAILABLE_IN_4_18
JNIEnv *gdk_android_display_get_env (GdkDisplay *self);

GDK_AVAILABLE_IN_4_18
GdkAndroidDisplayNightMode gdk_android_display_get_night_mode (GdkAndroidDisplay *self);


GDK_AVAILABLE_IN_4_18
gpointer gdk_android_display_get_egl_display (GdkAndroidDisplay *self);

G_END_DECLS
