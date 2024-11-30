/* gdkdrmmonitor.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include <gdk/gdk.h>

#include "gdkdrmdisplay-private.h"
#include "gdkdrmmonitor-private.h"
#include "gdkdrmsurface-private.h"

struct _GdkDrmMonitor
{
  GdkMonitor parent_instance;
};

struct _GdkDrmMonitorClass
{
  GdkMonitorClass parent_class;
};

G_DEFINE_FINAL_TYPE (GdkDrmMonitor, gdk_drm_monitor, GDK_TYPE_MONITOR)

static void
gdk_drm_monitor_dispose (GObject *object)
{
  G_OBJECT_CLASS (gdk_drm_monitor_parent_class)->dispose (object);
}

static void
gdk_drm_monitor_class_init (GdkDrmMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gdk_drm_monitor_dispose;
}

static void
gdk_drm_monitor_init (GdkDrmMonitor *self)
{
}

GdkDrmMonitor *
_gdk_drm_monitor_new (GdkDrmDisplay *display)
{
  GdkDrmMonitor *self;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  self = g_object_new (GDK_TYPE__MONITOR,
                       "display", display,
                       NULL);

  return g_steal_pointer (&self);
}

void
_gdk_drm_monitor_get_workarea (GdkMonitor   *monitor,
                               GdkRectangle *workarea)
{
  *workarea = (GdkRectangle) { 0, 0, 0, 0 };
}
