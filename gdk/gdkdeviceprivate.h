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

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_DEVICE_PRIVATE_H__
#define __GDK_DEVICE_PRIVATE_H__

#include <gdk/gdkdevice.h>

G_BEGIN_DECLS

#define GDK_DEVICE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE, GdkDeviceClass))
#define GDK_IS_DEVICE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE))
#define GDK_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE, GdkDeviceClass))

typedef struct _GdkDeviceClass GdkDeviceClass;

struct _GdkDeviceClass
{
  GObjectClass parent_class;

  gboolean (* get_history) (GdkDevice      *device,
                            GdkWindow      *window,
                            guint32         start,
                            guint32         stop,
                            GdkTimeCoord ***events,
                            guint          *n_events);

  void (* get_state) (GdkDevice       *device,
                      GdkWindow       *window,
                      gdouble         *axes,
                      GdkModifierType *mask);

  void (* set_window_cursor) (GdkDevice *device,
                              GdkWindow *window,
                              GdkCursor *cursor);

  void (* warp)              (GdkDevice  *device,
                              GdkScreen  *screen,
                              gint        x,
                              gint        y);
};


void  _gdk_device_reset_axes (GdkDevice   *device);
guint _gdk_device_add_axis   (GdkDevice   *device,
                              GdkAtom      label_atom,
                              GdkAxisUse   use,
                              gdouble      min_value,
                              gdouble      max_value,
                              gdouble      resolution);

gboolean _gdk_device_translate_axis (GdkDevice *device,
                                     gdouble    window_width,
                                     gdouble    window_height,
                                     gdouble    window_x,
                                     gdouble    window_y,
                                     guint      index,
                                     gdouble    value,
                                     gdouble   *axis_value);

G_END_DECLS

#endif /* __GDK_DEVICE_PRIVATE_H__ */
