/*
 * Copyright © 2016 Red Hat, Inc
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

#ifndef __GDK_WIN32_MONITOR_PRIVATE_H__
#define __GDK_WIN32_MONITOR_PRIVATE_H__

#include <windows.h>
#include <glib.h>
#include <gio/gio.h>

#include "gdkmonitorprivate.h"

#include "gdkwin32monitor.h"

/*
 * The number is the degrees the display is rotated clockwise
 * from its natural position.
 * I.e. most displays will have 0. A normal display
 * that is rotated by 90 degrees will
 * have 90. The same display rotated counter-clockwise
 * will have 270. The same display
 * rotated by 180 degrees (i.e. upside-down)
 * will have 180.
 * Accordingly, 0 mode
 * fontsmoothing is used as-is - i.e. it is assumed that
 * subpixel structure is horizontal (ClearType does not support
 * vertical subpixels; if the display has naturally vertical
 * subpixel structure, ClearType should be disabled altogether).
 * In 90 subpixel structure has
 * its verticality flipped (rgb -> vrgb; bgr -> vbgr).
 * In 180 subpixel structure is
 * horizontally flipped (rgb -> bgr; bgr -> rgb).
 * In 270 subpixel structure is
 * flipped both horizontally and vertically
 * (rgb -> vbgr; bgr -> vrgb).
 */
typedef enum _GdkWin32MonitorRotation {
  GDK_WIN32_MONITOR_ROTATION_UNKNOWN = 0,
  GDK_WIN32_MONITOR_ROTATION_0 = 1,
  GDK_WIN32_MONITOR_ROTATION_90 = 2,
  GDK_WIN32_MONITOR_ROTATION_180 = 3,
  GDK_WIN32_MONITOR_ROTATION_270 = 4,
} GdkWin32MonitorRotation;

struct _GdkWin32Monitor
{
  GdkMonitor parent;

  /* work area */
  GdkRectangle work_rect;

  /* Device instance path (used to match GdkWin32Monitor to monitor device) */
  gchar *instance_path;

  /* Indicates display rotation and its normal proportions.
   * Used to determine pixel structure for subpixel smoothing.
   */
  GdkWin32MonitorRotation orientation;

  /* TRUE if monitor is made up by us
   * (this happens when system has logical monitors, but no physical ones).
   */
  guint madeup : 1;

  /* TRUE if we should notify GDK about this monitor being added */
  guint add    : 1;

  /* TRUE if we should notify GDK about this monitor being removed */
  guint remove : 1;
};

struct _GdkWin32MonitorClass {
  GdkMonitorClass parent_class;
};

int        _gdk_win32_monitor_compare  (GdkWin32Monitor *a, GdkWin32Monitor *b);

const gchar *_gdk_win32_monitor_get_pixel_structure (GdkMonitor *monitor);

#endif
