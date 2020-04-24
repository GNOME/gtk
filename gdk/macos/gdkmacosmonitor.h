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

#ifndef __GDK_MACOS_MONITOR_H__
#define __GDK_MACOS_MONITOR_H__

#if !defined (__GDKMACOS_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/macos/gdkmacos.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GdkMacosMonitor      GdkMacosMonitor;
typedef struct _GdkMacosMonitorClass GdkMacosMonitorClass;

#define GDK_TYPE_MACOS_MONITOR       (gdk_macos_monitor_get_type())
#define GDK_MACOS_MONITOR(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MACOS_MONITOR, GdkMacosMonitor))
#define GDK_IS_MACOS_MONITOR(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MACOS_MONITOR))

GDK_AVAILABLE_IN_ALL
GType gdk_macos_monitor_get_type (void);

G_END_DECLS

#endif /* __GDK_MACOS_MONITOR_H__ */
