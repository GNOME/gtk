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

#ifndef __GDK_DEVICE_PRIVATE_H__
#define __GDK_DEVICE_PRIVATE_H__

#include "gdkdevice.h"
#include "gdkdevicetool.h"
#include "gdkevents.h"
#include "gdkseat.h"

G_BEGIN_DECLS

#define GDK_DEVICE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE, GdkDeviceClass))
#define GDK_IS_DEVICE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE))
#define GDK_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE, GdkDeviceClass))

typedef struct _GdkDeviceClass GdkDeviceClass;
typedef struct _GdkDeviceKey GdkDeviceKey;

struct _GdkDeviceKey
{
  guint keyval;
  GdkModifierType modifiers;
};

struct _GdkDevice
{
  GObject parent_instance;

  gchar *name;
  GdkInputSource source;
  GdkInputMode mode;
  gboolean has_cursor;
  gint num_keys;
  GdkAxisFlags axis_flags;
  GdkDeviceKey *keys;
  GdkDisplay *display;
  /* Paired master for master,
   * associated master for slaves
   */
  GdkDevice *associated;
  GList *slaves;
  GdkDeviceType type;
  GArray *axes;
  guint num_touches;

  gchar *vendor_id;
  gchar *product_id;

  GdkSeat *seat;
  GdkDeviceTool *last_tool;
};

struct _GdkDeviceClass
{
  GObjectClass parent_class;

  gboolean (* get_history)   (GdkDevice      *device,
                              GdkSurface      *surface,
                              guint32         start,
                              guint32         stop,
                              GdkTimeCoord ***events,
                              gint           *n_events);

  void (* get_state)         (GdkDevice       *device,
                              GdkSurface       *surface,
                              gdouble         *axes,
                              GdkModifierType *mask);

  void (* set_surface_cursor)(GdkDevice *device,
                              GdkSurface *surface,
                              GdkCursor *cursor);

  void (* query_state)       (GdkDevice       *device,
                              GdkSurface       *surface,
                              GdkSurface      **child_surface,
                              gdouble          *root_x,
                              gdouble          *root_y,
                              gdouble          *win_x,
                              gdouble          *win_y,
                              GdkModifierType  *mask);
  GdkGrabStatus (* grab)     (GdkDevice        *device,
                              GdkSurface        *surface,
                              gboolean          owner_events,
                              GdkEventMask      event_mask,
                              GdkSurface        *confine_to,
                              GdkCursor        *cursor,
                              guint32           time_);
  void          (*ungrab)    (GdkDevice        *device,
                              guint32           time_);

  GdkSurface * (* surface_at_position) (GdkDevice       *device,
                                        double          *win_x,
                                        double          *win_y,
                                        GdkModifierType *mask,
                                        gboolean         get_toplevel);
};

void  _gdk_device_set_associated_device (GdkDevice *device,
                                         GdkDevice *relative);

void  _gdk_device_reset_axes (GdkDevice   *device);
guint _gdk_device_add_axis   (GdkDevice   *device,
                              GdkAtom      label_atom,
                              GdkAxisUse   use,
                              gdouble      min_value,
                              gdouble      max_value,
                              gdouble      resolution);
void _gdk_device_get_axis_info (GdkDevice  *device,
				guint       index,
				GdkAtom    *label_atom,
				GdkAxisUse *use,
				gdouble    *min_value,
				gdouble    *max_value,
				gdouble    *resolution);

void _gdk_device_set_keys    (GdkDevice   *device,
                              guint        num_keys);

gboolean   _gdk_device_translate_surface_coord (GdkDevice *device,
                                                GdkSurface *surface,
                                                guint      index,
                                                gdouble    value,
                                                gdouble   *axis_value);

gboolean   _gdk_device_translate_screen_coord (GdkDevice *device,
                                               GdkSurface *surface,
                                               gdouble    surface_root_x,
                                               gdouble    surface_root_y,
                                               gdouble    screen_width,
                                               gdouble    screen_height,
                                               guint      index,
                                               gdouble    value,
                                               gdouble   *axis_value);

gboolean   _gdk_device_translate_axis         (GdkDevice *device,
                                               guint      index,
                                               gdouble    value,
                                               gdouble   *axis_value);

GdkTimeCoord ** _gdk_device_allocate_history  (GdkDevice *device,
                                               gint       n_events);

void _gdk_device_add_slave (GdkDevice *device,
                            GdkDevice *slave);
void _gdk_device_remove_slave (GdkDevice *device,
                               GdkDevice *slave);
void _gdk_device_query_state                  (GdkDevice        *device,
                                               GdkSurface        *surface,
                                               GdkSurface       **child_surface,
                                               gdouble          *root_x,
                                               gdouble          *root_y,
                                               gdouble          *win_x,
                                               gdouble          *win_y,
                                               GdkModifierType  *mask);
GdkSurface * _gdk_device_surface_at_position  (GdkDevice        *device,
                                               gdouble          *win_x,
                                               gdouble          *win_y,
                                               GdkModifierType  *mask,
                                               gboolean          get_toplevel);

void  gdk_device_set_seat  (GdkDevice *device,
                            GdkSeat   *seat);

void           gdk_device_update_tool (GdkDevice     *device,
                                       GdkDeviceTool *tool);

GdkInputMode gdk_device_get_input_mode (GdkDevice *device);

G_END_DECLS

#endif /* __GDK_DEVICE_PRIVATE_H__ */
