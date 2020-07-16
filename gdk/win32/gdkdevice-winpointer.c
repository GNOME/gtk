/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 the GTK team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdkwindow.h>

#include <windows.h>

#include "gdkwin32.h"
#include "gdkdevice-winpointer.h"

G_DEFINE_TYPE (GdkDeviceWinpointer, gdk_device_winpointer, GDK_TYPE_DEVICE)

static GdkGrabStatus
gdk_device_winpointer_grab (GdkDevice    *device,
                            GdkWindow    *window,
                            gboolean      owner_events,
                            GdkEventMask  event_mask,
                            GdkWindow    *confine_to,
                            GdkCursor    *cursor,
                            guint32       time_)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_winpointer_ungrab (GdkDevice *device,
                              guint32    time_)
{
}

static void
gdk_device_winpointer_init (GdkDeviceWinpointer *device_winpointer)
{
  device_winpointer->device_handle = NULL;
  device_winpointer->hid_props = NULL;
  device_winpointer->start_cursor_id = 0;
  device_winpointer->end_cursor_id = 0;
  device_winpointer->frame_id = 0;
  device_winpointer->scale_x = 0.0;
  device_winpointer->scale_y = 0.0;
  device_winpointer->num_axes = 0;
  device_winpointer->index_axis_pressure = -1;
  device_winpointer->index_axis_xtilt = -1;
  device_winpointer->index_axis_ytilt = -1;
  device_winpointer->index_axis_rotation = -1;
}

static void
gdk_device_winpointer_finalize (GdkDeviceWinpointer *device_winpointer)
{
  g_array_unref (device_winpointer->hid_props);
}

static void
gdk_device_winpointer_class_init (GdkDeviceWinpointerClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  /*TODO: is it needed for slave devices? */
  device_class->grab = gdk_device_winpointer_grab;
  device_class->ungrab = gdk_device_winpointer_ungrab;

  /*klass->finalize = gdk_device_winpointer_finalize;*/
}
