/*
 * Copyright © 2020 Red Hat, Inc.
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

#ifndef __GDK_MACOS_DISPLAY_PRIVATE_H__
#define __GDK_MACOS_DISPLAY_PRIVATE_H__

#include <AppKit/AppKit.h>

#include "gdkmacosdisplay.h"

G_BEGIN_DECLS

GdkDisplay      *_gdk_macos_display_open                         (const gchar     *display_name);
int              _gdk_macos_display_get_fd                       (GdkMacosDisplay *self);
void             _gdk_macos_display_queue_events                 (GdkMacosDisplay *self);
void             _gdk_macos_display_to_display_coords            (GdkMacosDisplay *self,
                                                                  int              x,
                                                                  int              y,
                                                                  int             *out_x,
                                                                  int             *out_y);
void             _gdk_macos_display_from_display_coords          (GdkMacosDisplay *self,
                                                                  int              x,
                                                                  int              y,
                                                                  int             *out_x,
                                                                  int             *out_y);
NSScreen        *_gdk_macos_display_get_screen_at_display_coords (GdkMacosDisplay *self,
                                                                  int              x,
                                                                  int              y);
GdkEvent        *_gdk_macos_display_translate                    (GdkMacosDisplay *self,
                                                                  NSEvent         *event);
void             _gdk_macos_display_break_all_grabs              (GdkMacosDisplay *self,
                                                                  guint32          time);
GdkModifierType  _gdk_macos_display_get_current_mouse_modifiers  (GdkMacosDisplay *self);

G_END_DECLS

#endif /* __GDK_MACOS_DISPLAY_PRIVATE_H__ */
