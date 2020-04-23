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
#include <math.h>

#include "gdkmacosdisplay-private.h"
#include "gdkmacosmonitor-private.h"
#include "gdkmacosutils-private.h"

struct _GdkMacosMonitor
{
  GdkMonitor        parent_instance;
  CGDirectDisplayID screen_id;
  guint             has_opengl : 1;
};

struct _GdkMacosMonitorClass
{
  GdkMonitorClass parent_class;
};

G_DEFINE_TYPE (GdkMacosMonitor, gdk_macos_monitor, GDK_TYPE_MONITOR)

static void
gdk_macos_monitor_get_workarea (GdkMonitor   *monitor,
                                GdkRectangle *geometry)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosMonitor *self = (GdkMacosMonitor *)monitor;

  g_assert (GDK_IS_MACOS_MONITOR (self));
  g_assert (geometry != NULL);

  *geometry = monitor->geometry;

  for (id obj in [NSScreen screens])
    {
      CGDirectDisplayID screen_id;

      screen_id = [[[obj deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];

      if (screen_id == self->screen_id)
        {
          NSScreen *screen = (NSScreen *)obj;
          NSRect visibleFrame = [screen visibleFrame];
          int x = visibleFrame.origin.x;
          int y = visibleFrame.origin.y + visibleFrame.size.height;

          _gdk_macos_display_from_display_coords (GDK_MACOS_DISPLAY (monitor->display),
                                                  x, y, &x, &y);

          geometry->x = x;
          geometry->y = y;
          geometry->width = visibleFrame.size.width;
          geometry->height = visibleFrame.size.height;

          break;
        }
    }

  GDK_END_MACOS_ALLOC_POOL;
}

static void
gdk_macos_monitor_class_init (GdkMacosMonitorClass *klass)
{
  GdkMonitorClass *monitor_class = GDK_MONITOR_CLASS (klass);

  monitor_class->get_workarea = gdk_macos_monitor_get_workarea;
}

static void
gdk_macos_monitor_init (GdkMacosMonitor *self)
{
}

static GdkSubpixelLayout
GetSubpixelLayout (CGDirectDisplayID screen_id)
{
#if 0
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkSubpixelLayout subpixel_layout = GDK_SUBPIXEL_LAYOUT_UNKNOWN;
  io_service_t iosvc;
  NSDictionary *dict;
  guint layout;
  gboolean rotation;

  rotation = CGDisplayRotation (screen_id);
  iosvc = CGDisplayIOServicePort (screen_id);
  dict = (NSDictionary *)IODisplayCreateInfoDictionary (iosvc, kIODisplayOnlyPreferredName);
  layout = [[dict objectForKey:@kDisplaySubPixelLayout] unsignedIntValue];

  switch (layout)
    {
    case kDisplaySubPixelLayoutRGB:
      if (rotation == 0.0)
        subpixel_layout = GDK_SUBPIXEL_LAYOUT_HORIZONTAL_RGB;
      else if (rotation == 90.0)
        subpixel_layout = GDK_SUBPIXEL_LAYOUT_VERTICAL_RGB;
      else if (rotation == 180.0 || rotation == -180.0)
        subpixel_layout = GDK_SUBPIXEL_LAYOUT_HORIZONTAL_BGR;
      else if (rotation == -90.0)
        subpixel_layout = GDK_SUBPIXEL_LAYOUT_VERTICAL_BGR;
      break;

    case kDisplaySubPixelLayoutBGR:
      if (rotation == 0.0)
        subpixel_layout = GDK_SUBPIXEL_LAYOUT_HORIZONTAL_BGR;
      else if (rotation == 90.0)
        subpixel_layout = GDK_SUBPIXEL_LAYOUT_VERTICAL_BGR;
      else if (rotation == 180.0 || rotation == -180.0)
        subpixel_layout = GDK_SUBPIXEL_LAYOUT_HORIZONTAL_RGB;
      else if (rotation == -90.0 || rotation == 270.0)
        subpixel_layout = GDK_SUBPIXEL_LAYOUT_VERTICAL_RGB;
      break;

    default:
      break;
    }

  GDK_END_MACOS_ALLOC_POOL;

  return subpixel_layout;
#endif

  return GDK_SUBPIXEL_LAYOUT_UNKNOWN;
}

static char *
GetLocalizedName (CGDirectDisplayID screen_id)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  char *name = NULL;

  for (id obj in [NSScreen screens])
    {
      CGDirectDisplayID this_id = [[[obj deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];

      if (screen_id == this_id)
        {
          NSString *str = [(NSScreen *)obj localizedName];
          name = g_strdup ([str UTF8String]);
          break;
        }
    }

  GDK_END_MACOS_ALLOC_POOL;

  return g_steal_pointer (&name);
}

static gchar *
GetConnectorName (CGDirectDisplayID screen_id)
{
  guint unit = CGDisplayUnitNumber (screen_id);
  return g_strdup_printf ("unit-%u", unit);
}

gboolean
_gdk_macos_monitor_reconfigure (GdkMacosMonitor *self)
{
  GdkSubpixelLayout subpixel_layout;
  CGDisplayModeRef mode;
  GdkMacosDisplay *display;
  GdkRectangle geom;
  gboolean has_opengl;
  CGSize size;
  CGRect bounds;
  CGRect main_bounds;
  size_t width;
  size_t pixel_width;
  gchar *connector;
  gchar *name;
  int refresh_rate;
  int scale_factor = 1;
  int width_mm;
  int height_mm;

  g_return_val_if_fail (GDK_IS_MACOS_MONITOR (self), FALSE);

  if (!(mode = CGDisplayCopyDisplayMode (self->screen_id)))
    return FALSE;

  size = CGDisplayScreenSize (self->screen_id);
  bounds = CGDisplayBounds (self->screen_id);
  main_bounds = CGDisplayBounds (CGMainDisplayID ());
  width = CGDisplayModeGetWidth (mode);
  pixel_width = CGDisplayModeGetPixelWidth (mode);
  refresh_rate = CGDisplayModeGetRefreshRate (mode);
  has_opengl = CGDisplayUsesOpenGLAcceleration (self->screen_id);
  subpixel_layout = GetSubpixelLayout (self->screen_id);
  name = GetLocalizedName (self->screen_id);
  connector = GetConnectorName (self->screen_id);

  if (width != 0 && pixel_width != 0)
    scale_factor = MAX (1, pixel_width / width);

  width_mm = size.width;
  height_mm = size.height;

  CGDisplayModeRelease (mode);

  /* This requires that the display bounds have been
   * updated before the monitor is reconfigured.
   */
  display = GDK_MACOS_DISPLAY (GDK_MONITOR (self)->display);
  _gdk_macos_display_from_display_coords (display,
                                          bounds.origin.x,
                                          bounds.origin.y,
                                          &geom.x, &geom.y);
  geom.width = bounds.size.width;
  geom.height = bounds.size.height;

  gdk_monitor_set_connector (GDK_MONITOR (self), connector);
  gdk_monitor_set_model (GDK_MONITOR (self), name);
  gdk_monitor_set_position (GDK_MONITOR (self), geom.x, geom.y);
  gdk_monitor_set_size (GDK_MONITOR (self), geom.width, geom.height);
  gdk_monitor_set_physical_size (GDK_MONITOR (self), width_mm, height_mm);
  gdk_monitor_set_scale_factor (GDK_MONITOR (self), scale_factor);
  gdk_monitor_set_refresh_rate (GDK_MONITOR (self), refresh_rate);
  gdk_monitor_set_subpixel_layout (GDK_MONITOR (self), GDK_SUBPIXEL_LAYOUT_UNKNOWN);

  /* We might be able to use this at some point to change which GSK renderer
   * we use for surfaces on this monitor.  For example, it might be better
   * to use cairo if we cannot use OpenGL (or it would be software) anyway.
   * Presumably that is more common in cases where macOS is running under
   * an emulator such as QEMU.
   */
  self->has_opengl = !!has_opengl;

  g_free (name);
  g_free (connector);

  return TRUE;
}

GdkMacosMonitor *
_gdk_macos_monitor_new (GdkMacosDisplay   *display,
                        CGDirectDisplayID  screen_id)
{
  GdkMacosMonitor *self;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  self = g_object_new (GDK_TYPE_MACOS_MONITOR,
                       "display", display,
                       NULL);

  self->screen_id = screen_id;

  _gdk_macos_monitor_reconfigure (self);

  return g_steal_pointer (&self);
}

CGDirectDisplayID
_gdk_macos_monitor_get_screen_id (GdkMacosMonitor *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_MONITOR (self), 0);

  return self->screen_id;
}
