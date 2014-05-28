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

#include "gdkmir.h"
#include "gdkdisplay.h"
#include "gdkscreen.h"
#include "gdkdevicemanager.h"
#include "gdkkeys.h"
#include "gdkwindowimpl.h"

typedef struct _GdkMirWindowImpl GdkMirWindowImpl;
typedef struct _GdkMirWindowReference GdkMirWindowReference;
typedef struct _GdkMirEventSource GdkMirEventSource;

#define GDK_TYPE_MIR_WINDOW_IMPL              (gdk_mir_window_impl_get_type ())
#define GDK_MIR_WINDOW_IMPL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_MIR, GdkMirWindowImpl))
#define GDK_IS_WINDOW_IMPL_MIR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_MIR))

struct _GdkMirWindowImpl
{
  GdkWindowImpl parent_instance;

  /* Desired surface attributes */
  MirSurfaceType surface_type; // FIXME
  MirSurfaceState surface_state;

  /* Pattern for background */
  cairo_pattern_t *background;

  /* Current button state for checking which buttons are being pressed / released */
  gdouble x;
  gdouble y;
  MirMotionButton button_state;

  /* Surface being rendered to (only exists when window visible) */
  MirSurface *surface;

  /* Cairo context for current frame */
  cairo_surface_t *cairo_surface;

  /* TRUE if the window can be seen */
  gboolean visible;

  /* TRUE if cursor is inside this window */
  gboolean cursor_inside;
};


GdkDisplay *_gdk_mir_display_open (const gchar *display_name);

GdkScreen *_gdk_mir_screen_new (GdkDisplay *display);

GdkDeviceManager *_gdk_mir_device_manager_new (GdkDisplay *display);

GdkDevice *_gdk_mir_device_manager_get_keyboard (GdkDeviceManager *device_manager);

GdkKeymap *_gdk_mir_keymap_new (void);

GdkDevice *_gdk_mir_keyboard_new (GdkDeviceManager *device_manager, const gchar *name);

GdkDevice *_gdk_mir_pointer_new (GdkDeviceManager *device_manager, const gchar *name);

void _gdk_mir_pointer_set_location (GdkDevice *pointer, gdouble x, gdouble y, GdkWindow *window, GdkModifierType mask);

GdkCursor *_gdk_mir_cursor_new (GdkDisplay *display, GdkCursorType type);

GdkWindowImpl *_gdk_mir_window_impl_new (void);

GdkMirEventSource *_gdk_mir_display_get_event_source (GdkDisplay *display);

GdkMirEventSource *_gdk_mir_event_source_new (GdkDisplay *display);

GdkMirWindowReference *_gdk_mir_event_source_get_window_reference (GdkWindow *window);

void _gdk_mir_window_reference_unref (GdkMirWindowReference *ref);

void _gdk_mir_event_source_queue (GdkMirWindowReference *window_ref, const MirEvent *event);

#endif /* __GDK_PRIVATE_MIR_H__ */
