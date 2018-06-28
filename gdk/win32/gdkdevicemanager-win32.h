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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_DEVICE_MANAGER_WIN32_H__
#define __GDK_DEVICE_MANAGER_WIN32_H__

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE_MANAGER_WIN32         (gdk_device_manager_win32_get_type ())
#define GDK_DEVICE_MANAGER_WIN32(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_MANAGER_WIN32, GdkDeviceManagerWin32))
#define GDK_DEVICE_MANAGER_WIN32_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_MANAGER_WIN32, GdkDeviceManagerWin32Class))
#define GDK_IS_DEVICE_MANAGER_WIN32(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_MANAGER_WIN32))
#define GDK_IS_DEVICE_MANAGER_WIN32_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_MANAGER_WIN32))
#define GDK_DEVICE_MANAGER_WIN32_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_MANAGER_WIN32, GdkDeviceManagerWin32Class))

typedef struct _GdkDeviceManagerWin32 GdkDeviceManagerWin32;
typedef struct _GdkDeviceManagerWin32Class GdkDeviceManagerWin32Class;

struct _GdkDeviceManagerWin32
{
  GObject parent_object;
  GdkDisplay *display;
  /* Master Devices */
  GdkDevice *core_pointer;
  GdkDevice *core_keyboard;
  /* Fake slave devices */
  GdkDevice *system_pointer;
  GdkDevice *system_keyboard;
  GList *wintab_devices;

  /* Bumped up every time a wintab device enters the proximity
   * of our context (WT_PROXIMITY). Bumped down when we either
   * receive a WT_PACKET, or a WT_CSRCHANGE.
   */
  gint dev_entered_proximity;
};

struct _GdkDeviceManagerWin32Class
{
  GObjectClass parent_class;
};

GType gdk_device_manager_win32_get_type (void) G_GNUC_CONST;

void     _gdk_input_set_tablet_active (void);
gboolean gdk_input_other_event        (GdkDisplay *display,
                                       GdkEvent   *event,
                                       MSG        *msg,
                                       GdkSurface  *window);

G_END_DECLS

#endif /* __GDK_DEVICE_MANAGER_WIN32_H__ */
