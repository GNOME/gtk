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

#include <gdk/gdkwindow.h>
#include "gdkdevice-xi.h"
#include "gdkprivate-x11.h"
#include "gdkintl.h"
#include "gdkx.h"

static void gdk_device_xi_constructed  (GObject *object);
static void gdk_device_xi_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec);
static void gdk_device_xi_get_property (GObject      *object,
                                        guint         prop_id,
                                        GValue       *value,
                                        GParamSpec   *pspec);


G_DEFINE_TYPE (GdkDeviceXI, gdk_device_xi, GDK_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_DEVICE_ID
};

static void
gdk_device_xi_class_init (GdkDeviceXIClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gdk_device_xi_constructed;
  object_class->set_property = gdk_device_xi_set_property;
  object_class->get_property = gdk_device_xi_get_property;

  g_object_class_install_property (object_class,
				   PROP_DEVICE_ID,
				   g_param_spec_int ("device-id",
                                                     P_("Device ID"),
                                                     P_("Device ID"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gdk_device_xi_init (GdkDeviceXI *device)
{
}

static void
gdk_device_xi_constructed (GObject *object)
{
  GdkDeviceXI *device;
  GdkDisplay *display;

  device = GDK_DEVICE_XI (object);
  display = gdk_device_get_display (GDK_DEVICE (object));

  gdk_error_trap_push ();
  device->xdevice = XOpenDevice (GDK_DISPLAY_XDISPLAY (display),
                                 device->device_id);

  if (gdk_error_trap_pop ())
    g_warning ("Device %s can't be opened", GDK_DEVICE (device)->name);

  if (G_OBJECT_CLASS (gdk_device_xi_parent_class)->constructed)
    G_OBJECT_CLASS (gdk_device_xi_parent_class)->constructed (object);
}

static void
gdk_device_xi_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GdkDeviceXI *device = GDK_DEVICE_XI (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      device->device_id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_xi_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GdkDeviceXI *device = GDK_DEVICE_XI (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_int (value, device->device_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}
