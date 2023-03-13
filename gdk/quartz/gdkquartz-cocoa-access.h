/* gdkquartz-gtk-only.h
 *
 * Copyright (C) 2005-2007 Imendio AB
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

#ifndef __GDK_QUARTZ_COCOA_ACCESS_H__
#define __GDK_QUARTZ_COCOA_ACCESS_H__

#ifndef __OBJC__
#error "This header declares Cocoa types and can be included only from source files compiled with Objective-C."
#endif

#include <AppKit/AppKit.h>
#include <gdk/gdk.h>

GDK_AVAILABLE_IN_ALL
NSEvent  *gdk_quartz_event_get_nsevent              (GdkEvent  *event);
GDK_AVAILABLE_IN_ALL
NSWindow *gdk_quartz_window_get_nswindow            (GdkWindow *window);
GDK_AVAILABLE_IN_ALL
NSView   *gdk_quartz_window_get_nsview              (GdkWindow *window);

#endif
