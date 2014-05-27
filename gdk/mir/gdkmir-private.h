/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_PRIVATE_MIR_H__
#define __GDK_PRIVATE_MIR_H__

#include "config.h"

#include "gdkdisplay.h"
#include "gdkscreen.h"
#include "gdkdevicemanager.h"
#include "gdkkeys.h"
#include "gdkwindowimpl.h"

GdkDisplay *_gdk_mir_display_open (const gchar *display_name);

GdkScreen *_gdk_mir_screen_new (GdkDisplay *display);

GdkDeviceManager *_gdk_mir_device_manager_new (GdkDisplay *display);

GdkDevice *_gdk_mir_device_manager_get_keyboard (GdkDeviceManager *device_manager);

GdkKeymap *_gdk_mir_keymap_new (void);

GdkDevice *_gdk_mir_keyboard_new (GdkDeviceManager *device_manager, const gchar *name);

GdkDevice *_gdk_mir_pointer_new (GdkDeviceManager *device_manager, const gchar *name);

void _gdk_mir_pointer_set_location (GdkDevice *pointer, gdouble x, gdouble y, GdkWindow *window, GdkModifierType mask);

GdkCursor *_gdk_mir_cursor_new (GdkDisplay *display, GdkCursorType type);

GdkWindowImpl *_gdk_mir_window_impl_new (int width, int height, GdkEventMask event_mask);

#endif /* __GDK_PRIVATE_MIR_H__ */
