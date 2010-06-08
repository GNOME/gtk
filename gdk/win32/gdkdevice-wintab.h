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

#ifndef __GDK_DEVICE_WINTAB_H__
#define __GDK_DEVICE_WINTAB_H__

#include <gdk/gdkdeviceprivate.h>

#include <windows.h>
#include <wintab.h>

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE_WINTAB         (gdk_device_wintab_get_type ())
#define GDK_DEVICE_WINTAB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_WINTAB, GdkDeviceWintab))
#define GDK_DEVICE_WINTAB_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_WINTAB, GdkDeviceWintabClass))
#define GDK_IS_DEVICE_WINTAB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_WINTAB))
#define GDK_IS_DEVICE_WINTAB_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_WINTAB))
#define GDK_DEVICE_WINTAB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_WINTAB, GdkDeviceWintabClass))

typedef struct _GdkDeviceWintab GdkDeviceWintab;
typedef struct _GdkDeviceWintabClass GdkDeviceWintabClass;

struct _GdkDeviceWintab
{
  GdkDevice parent_instance;

  gint *last_axis_data;
  gint button_state;

  /* WINTAB stuff: */
  HCTX hctx;
  /* Cursor number */
  UINT cursor;
  /* The cursor's CSR_PKTDATA */
  WTPKT pktdata;
  /* Azimuth and altitude axis */
  AXIS orientation_axes[2];
};

struct _GdkDeviceWintabClass
{
  GdkDeviceClass parent_class;
};

GType gdk_device_wintab_get_type (void) G_GNUC_CONST;

GdkEventMask _gdk_device_wintab_get_events (GdkDeviceWintab *device,
                                            GdkWindow       *window);
gboolean     _gdk_device_wintab_get_window_coords (GdkWindow *window,
                                                   gdouble   *root_x,
                                                   gdouble   *root_y);
void         _gdk_device_wintab_update_window_coords (GdkWindow *window);

void         _gdk_device_wintab_translate_axes (GdkDeviceWintab *device,
                                                GdkWindow       *window,
                                                gdouble         *axes,
                                                gdouble         *x,
                                                gdouble         *y);

G_END_DECLS

#endif /* __GDK_DEVICE_WINTAB_H__ */
