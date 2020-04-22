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
 * LANDSCAPE means that the display is naturally landscape
 * (i.e. what almost all displays in the world use).
 * PORTRAIT means that the display is naturally portrait
 * (almost unheard of).
 * The number is the degrees the display is rotated clockwise
 * from its natural position.
 * I.e. most displays will have LANDSCAPE_0. A normal display
 * that is rotated by 90 degrees into portrait mode will
 * have LANDSCAPE_90. The same display rotated counter-clockwise
 * into portrait mode will have LANDSCAPE_270. The same display
 * rotated by 180 degrees (landscape again, but upside-down)
 * will have LANDSCAPE_180.
 * PORTRAIT is the same, but for displays that are normally
 * in portrait mode.
 * Accordingly, in LANDSCAPE_0 and PORTRAIT_0 modes
 * fontsmoothing is used as-is - i.e. it is assumed that
 * subpixes structure is horizontal (ClearType does not support
 * vertical subpixels; if the display has naturally vertical
 * subpixel structure, ClearType should be disabled altogether).
 * In LANDSCAPE_90 and PORTRAIT_90 subpixel structure has
 * its verticality flipped (rgb -> vrgb; bgr -> vbgr).
 * In LANDSCAPE_180 and PORTRAIT_180 subpixel structure is
 * horizontally flipped (rgb -> bgr; bgr -> rgb).
 * In LANDSCAPE_270 and PORTRAIT_270 subpixel structure is
 * flipped both horizontally and vertically
 * (rgb -> vbgr; bgr -> vrgb).
 */
typedef enum _GdkWin32MonitorOrientation {
  GDK_WIN32_DISPLAY_UNKNOWN = 0,
  GDK_WIN32_DISPLAY_LANDSCAPE_0 = 1,
  GDK_WIN32_DISPLAY_LANDSCAPE_90 = 2,
  GDK_WIN32_DISPLAY_LANDSCAPE_180 = 3,
  GDK_WIN32_DISPLAY_LANDSCAPE_270 = 4,
  GDK_WIN32_DISPLAY_PORTRAIT_0 = 5,
  GDK_WIN32_DISPLAY_PORTRAIT_90 = 6,
  GDK_WIN32_DISPLAY_PORTRAIT_180 = 7,
  GDK_WIN32_DISPLAY_PORTRAIT_270 = 8,
} GdkWin32MonitorOrientation;

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
  GdkWin32MonitorOrientation orientation;

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
