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

#include "config.h"

#include <gdk/gdk.h>

#include "gdkmacosmonitor-private.h"

G_DEFINE_TYPE (GdkMacosMonitor, gdk_macos_monitor, GDK_TYPE_MONITOR)

static void
gdk_macos_monitor_finalize (GObject *object)
{
  GdkMacosMonitor *self = (GdkMacosMonitor *)object;

  G_OBJECT_CLASS (gdk_macos_monitor_parent_class)->finalize (object);
}

static void
gdk_macos_monitor_class_init (GdkMacosMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_macos_monitor_finalize;
}

static void
gdk_macos_monitor_init (GdkMacosMonitor *self)
{
}

GdkMonitor *
_gdk_macos_monitor_new (GdkMacosDisplay   *display,
                        CGDirectDisplayID  screen_id)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  return g_object_new (GDK_TYPE_MACOS_MONITOR,
                       "display", display,
                       NULL);
}
