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

#ifndef __GDK_DEVICE_PRIVATE_XI_H__
#define __GDK_DEVICE_PRIVATE_XI_H__

#include "gdkx11device-xi.h"
#include "gdkdeviceprivate.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>

G_BEGIN_DECLS

struct _GdkX11DeviceXI
{
  GdkDevice parent_instance;

  /*< private >*/
  guint32 device_id;
  XDevice *xdevice;

  gint button_press_type;
  gint button_release_type;
  gint key_press_type;
  gint key_release_type;
  gint motion_notify_type;
  gint proximity_in_type;
  gint proximity_out_type;
  gint state_notify_type;

  /* minimum key code for device */
  gint min_keycode;
  gint *axis_data;
  guint in_proximity : 1;
};

struct _GdkX11DeviceXIClass
{
  GdkDeviceClass parent_class;
};

G_END_DECLS

#endif /* __GDK_DEVICE_PRIVATE_XI_H__ */
