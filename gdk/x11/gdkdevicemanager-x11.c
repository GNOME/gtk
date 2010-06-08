/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gdkx.h"
#include "gdkdevicemanager-core.h"

#ifdef XINPUT_XFREE
#include "gdkdevicemanager-xi.h"
#ifdef XINPUT_2
#include "gdkdevicemanager-xi2.h"
#endif
#endif

GdkDeviceManager *
_gdk_device_manager_new (GdkDisplay *display)
{
  if (!g_getenv ("GDK_CORE_DEVICE_EVENTS"))
    {
#if defined (XINPUT_2) || defined (XINPUT_XFREE)
      int opcode, firstevent, firsterror;
      Display *xdisplay;

      xdisplay = GDK_DISPLAY_XDISPLAY (display);

      if (XQueryExtension (xdisplay, "XInputExtension",
                           &opcode, &firstevent, &firsterror))
        {
#ifdef XINPUT_2
          int major, minor;

          major = 2;
          minor = 0;

          if (_gdk_enable_multidevice &&
              XIQueryVersion (xdisplay, &major, &minor) != BadRequest)
            {
              GdkDeviceManagerXI2 *device_manager_xi2;

              device_manager_xi2 = g_object_new (GDK_TYPE_DEVICE_MANAGER_XI2,
                                                 "display", display,
                                                 NULL);
              device_manager_xi2->opcode = opcode;

              return GDK_DEVICE_MANAGER (device_manager_xi2);
            }
          else
#endif /* XINPUT_2 */
            return g_object_new (GDK_TYPE_DEVICE_MANAGER_XI,
                                 "display", display,
                                 "event-base", firstevent,
                                 NULL);
        }
#endif /* XINPUT_2 || XINPUT_XFREE */
    }

  return g_object_new (GDK_TYPE_DEVICE_MANAGER_CORE,
                       "display", display,
                       NULL);
}
