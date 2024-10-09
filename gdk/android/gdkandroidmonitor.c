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

#include "config.h"

#include <gdk/gdk.h>
#include <math.h>

#include "gdkandroidinit-private.h"
#include "gdkandroiddisplay-private.h"
#include "gdkandroidutils-private.h"

#include "gdkandroidmonitor-private.h"

struct _GdkAndroidMonitor
{
  GdkMonitor parent_instance;
  guint toplevel_counter;
};

struct _GdkAndroidMonitorClass
{
  GdkMonitorClass parent_class;
};


/**
 * GdkAndroidMonitor:
 *
 * The Android implementation of [class@Gdk.Monitor].
 *
 * Since: 4.18
 */
G_DEFINE_TYPE (GdkAndroidMonitor, gdk_android_monitor, GDK_TYPE_MONITOR)

static void
gdk_android_monitor_class_init (GdkAndroidMonitorClass *klass)
{
}

static void
gdk_android_monitor_init (GdkAndroidMonitor *self)
{
}

GdkMonitor *
gdk_android_monitor_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_ANDROID_MONITOR, "display", display, NULL);
}

void
gdk_android_monitor_update (GdkAndroidMonitor  *self,
                            const GdkRectangle *bounds,
                            gfloat              density)
{
  gdk_monitor_set_geometry (GDK_MONITOR (self), bounds);
  gdk_monitor_set_scale (GDK_MONITOR (self), density);
}

void
gdk_android_monitor_add_toplevel (GdkAndroidMonitor *self)
{
  if (self->toplevel_counter++ == 0)
    g_list_store_append (((GdkAndroidDisplay *) gdk_monitor_get_display ((GdkMonitor *) self))->monitors, self);
}

void
gdk_android_monitor_drop_toplevel (GdkAndroidMonitor *self)
{
  if (--self->toplevel_counter == 0)
    {
      GListStore *monitors = ((GdkAndroidDisplay *) gdk_monitor_get_display ((GdkMonitor *) self))->monitors;
      guint index;
      if (g_list_store_find (monitors, self, &index))
        g_list_store_remove (monitors, index);
    }
}
