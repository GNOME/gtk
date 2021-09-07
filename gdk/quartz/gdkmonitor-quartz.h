/*
 * Copyright © 2017 Tom Schoonjans
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_QUARTZ_MONITOR_PRIVATE_H__
#define __GDK_QUARTZ_MONITOR_PRIVATE_H__

#include <glib.h>
#include <gio/gio.h>
#include <AppKit/AppKit.h>

#include "gdkmonitorprivate.h"

#include "gdkquartzmonitor.h"
#include "gdkprivate-quartz.h"

struct _GdkQuartzMonitor
{
  GdkMonitor parent;
  CGDirectDisplayID id;
};

struct _GdkQuartzMonitorClass {
  GdkMonitorClass parent_class;
};

#endif
