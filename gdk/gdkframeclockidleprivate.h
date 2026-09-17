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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2010.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* Uninstalled header, internal to GDK */

#pragma once

#include "gdkframeclockprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_FRAME_CLOCK_IDLE            (gdk_frame_clock_idle_get_type())
GDK_DECLARE_INTERNAL_TYPE (GdkFrameClockIdle, gdk_frame_clock_idle, GDK, FRAME_CLOCK_IDLE, GdkFrameClock)

struct _GdkFrameClockIdle
{
  GdkFrameClock parent_instance;
};

struct _GdkFrameClockIdleClass
{
  GdkFrameClockClass parent_class;
};

GdkFrameClock *_gdk_frame_clock_idle_new                     (void);
void           _gdk_frame_clock_idle_set_pace_when_throttled (GdkFrameClockIdle *self,
                                                              gboolean           enabled);

G_END_DECLS
