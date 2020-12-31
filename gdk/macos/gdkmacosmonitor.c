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
  NSRect            workarea;
  guint             has_opengl : 1;
};

struct _GdkMacosMonitorClass
{
  GdkMonitorClass parent_class;
};

G_DEFINE_TYPE (GdkMacosMonitor, gdk_macos_monitor, GDK_TYPE_MONITOR)

/**
 * gdk_macos_monitor_get_workarea:
 * @monitor: a #GdkMonitor
 * @workarea: (out): a #GdkRectangle to be filled with
 *     the monitor workarea
 *
 * Retrieves the size and position of the “work area” on a monitor
 * within the display coordinate space. The returned geometry is in
 * ”application pixels”, not in ”device pixels” (see
 * gdk_monitor_get_scale_factor()).
 */
void
gdk_macos_monitor_get_workarea (GdkMonitor   *monitor,
                                GdkRectangle *geometry)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosMonitor *self = (GdkMacosMonitor *)monitor;
  int x,  y;

  g_assert (GDK_IS_MACOS_MONITOR (self));
  g_assert (geometry != NULL);

  x = self->workarea.origin.x;
  y = self->workarea.origin.y + self->workarea.size.height;

  _gdk_macos_display_from_display_coords (GDK_MACOS_DISPLAY (monitor->display),
                                          x, y,
                                          &x, &y);

  geometry->x = x;
  geometry->y = y;
  geometry->width = self->workarea.size.width;
  geometry->height = self->workarea.size.height;

  GDK_END_MACOS_ALLOC_POOL;
}

static void
gdk_macos_monitor_class_init (GdkMacosMonitorClass *klass)
{
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
GetLocalizedName (NSScreen *screen)
{
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_15_AND_LATER
  GDK_BEGIN_MACOS_ALLOC_POOL;

  NSString *str;
  char *name;

  g_assert (screen);

  str = [screen localizedName];
  name = g_strdup ([str UTF8String]);

  GDK_END_MACOS_ALLOC_POOL;

  return g_steal_pointer (&name);
#else
  return NULL;
#endif
}

static char *
GetConnectorName (CGDirectDisplayID screen_id)
{
  guint unit = CGDisplayUnitNumber (screen_id);
  return g_strdup_printf ("unit-%u", unit);
}

static NSScreen *
find_screen (CGDirectDisplayID screen_id)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  NSScreen *screen = NULL;

  for (id obj in [NSScreen screens])
    {
      if (screen_id == [[[obj deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue])
        {
          screen = (NSScreen *)obj;
          break;
        }
    }

  GDK_END_MACOS_ALLOC_POOL;

  return screen;
}

gboolean
_gdk_macos_monitor_reconfigure (GdkMacosMonitor *self)
{
  GdkSubpixelLayout subpixel_layout;
  CGDisplayModeRef mode;
  GdkMacosDisplay *display;
  NSScreen *screen;
  GdkRectangle geom;
  gboolean has_opengl;
  CGSize size;
  CGRect bounds;
  size_t width;
  size_t pixel_width;
  char *connector;
  char *name;
  int refresh_rate;
  int scale_factor = 1;
  int width_mm;
  int height_mm;

  g_return_val_if_fail (GDK_IS_MACOS_MONITOR (self), FALSE);

  display = GDK_MACOS_DISPLAY (GDK_MONITOR (self)->display);

  if (!(screen = find_screen (self->screen_id)) ||
      !(mode = CGDisplayCopyDisplayMode (self->screen_id)))
    return FALSE;

  size = CGDisplayScreenSize (self->screen_id);
  bounds = [screen frame];
  width = CGDisplayModeGetWidth (mode);
  pixel_width = CGDisplayModeGetPixelWidth (mode);
  has_opengl = CGDisplayUsesOpenGLAcceleration (self->screen_id);
  subpixel_layout = GetSubpixelLayout (self->screen_id);
  name = GetLocalizedName (screen);
  connector = GetConnectorName (self->screen_id);

  if (width != 0 && pixel_width != 0)
    scale_factor = MAX (1, pixel_width / width);

  width_mm = size.width;
  height_mm = size.height;

  geom.x = bounds.origin.x - display->min_x;
  geom.y = display->height - bounds.origin.y - bounds.size.height + display->min_y;
  geom.width = bounds.size.width;
  geom.height = bounds.size.height;

  /* We will often get 0 back from CGDisplayModeGetRefreshRate().  We
   * can fallback by getting it from CoreVideo based on a CVDisplayLink
   * setting (which is also used by the frame clock).
   */
  if (!(refresh_rate = CGDisplayModeGetRefreshRate (mode)))
    refresh_rate = _gdk_macos_display_get_nominal_refresh_rate (display);

  gdk_monitor_set_connector (GDK_MONITOR (self), connector);
  gdk_monitor_set_model (GDK_MONITOR (self), name);
  gdk_monitor_set_geometry (GDK_MONITOR (self), &geom);
  gdk_monitor_set_physical_size (GDK_MONITOR (self), width_mm, height_mm);
  gdk_monitor_set_scale_factor (GDK_MONITOR (self), scale_factor);
  gdk_monitor_set_refresh_rate (GDK_MONITOR (self), refresh_rate);
  gdk_monitor_set_subpixel_layout (GDK_MONITOR (self), GDK_SUBPIXEL_LAYOUT_UNKNOWN);

  self->workarea = [screen visibleFrame];

  /* We might be able to use this at some point to change which GSK renderer
   * we use for surfaces on this monitor.  For example, it might be better
   * to use cairo if we cannot use OpenGL (or it would be software) anyway.
   * Presumably that is more common in cases where macOS is running under
   * an emulator such as QEMU.
   */
  self->has_opengl = !!has_opengl;

  CGDisplayModeRelease (mode);
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
