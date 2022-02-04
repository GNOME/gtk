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

#ifndef __GDK_MACOS_MONITOR_PRIVATE_H__
#define __GDK_MACOS_MONITOR_PRIVATE_H__

#include <AppKit/AppKit.h>

#include "gdkmacosdisplay.h"
#include "gdkmacosmonitor.h"

#include "gdkmonitorprivate.h"

G_BEGIN_DECLS

GdkMacosMonitor   *_gdk_macos_monitor_new             (GdkMacosDisplay   *display,
                                                       CGDirectDisplayID  screen_id);
CGDirectDisplayID  _gdk_macos_monitor_get_screen_id   (GdkMacosMonitor   *self);
gboolean           _gdk_macos_monitor_reconfigure     (GdkMacosMonitor   *self);
CGColorSpaceRef    _gdk_macos_monitor_copy_colorspace (GdkMacosMonitor   *self);

G_END_DECLS

#endif /* __GDK_MACOS_MONITOR_PRIVATE_H__ */
