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
  /*TODO: implicit grab on WM_POINTERDOWN till WM_POINTERUP */
  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_winpointer_ungrab (GdkDevice *device,
                              guint32    time_)
{
}

static void
gdk_device_winpointer_class_init (GdkDeviceWinpointerClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->grab = gdk_device_winopinter_grab;
  device_class->ungrab = gdk_device_winopinter_ungrab;
}

static void
gdk_device_winpointer_init (GdkDeviceWinpointer *device_winpointer)
{
}
