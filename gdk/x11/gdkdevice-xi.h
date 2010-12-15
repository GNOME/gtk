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

#ifndef __GDK_DEVICE_XI_H__
#define __GDK_DEVICE_XI_H__

#include "gdkdeviceprivate.h"

#include <X11/extensions/XInput.h>

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE_XI         (gdk_device_xi_get_type ())
#define GDK_DEVICE_XI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_XI, GdkDeviceXI))
#define GDK_DEVICE_XI_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_XI, GdkDeviceXIClass))
#define GDK_IS_DEVICE_XI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_XI))
#define GDK_IS_DEVICE_XI_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_XI))
#define GDK_DEVICE_XI_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_XI, GdkDeviceXIClass))

typedef struct _GdkDeviceXI GdkDeviceXI;
typedef struct _GdkDeviceXIClass GdkDeviceXIClass;

struct _GdkDeviceXI
{
  GdkDevice parent_instance;

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

struct _GdkDeviceXIClass
{
  GdkDeviceClass parent_class;
};

G_GNUC_INTERNAL
GType gdk_device_xi_get_type           (void) G_GNUC_CONST;

G_GNUC_INTERNAL
void  gdk_device_xi_update_window_info (GdkWindow *window);

G_GNUC_INTERNAL
void  gdk_device_xi_update_axes        (GdkDevice *device,
                                        gint       axes_count,
                                        gint       first_axis,
                                        gint      *axis_data);
G_GNUC_INTERNAL
void  gdk_device_xi_translate_axes     (GdkDevice *device,
                                        GdkWindow *window,
                                        gint      *axis_data,
                                        gdouble   *axes,
                                        gdouble   *x,
                                        gdouble   *y);

G_END_DECLS

#endif /* __GDK_DEVICE_XI_H__ */
