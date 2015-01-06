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
#include "gdkdevicemanager.h"
#include "gdkevents.h"

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
  GdkDeviceKey *keys;
  GdkDeviceManager *manager;
  GdkDisplay *display;
  /* Paired master for master,
   * associated master for slaves
   */
  GdkDevice *associated;
  GList *slaves;
  GdkDeviceType type;
  GArray *axes;

  GPtrArray *tools;
  GdkDeviceTool *last_tool;
};

struct _GdkDeviceClass
{
  GObjectClass parent_class;

  gboolean (* get_history)   (GdkDevice      *device,
                              GdkWindow      *window,
                              guint32         start,
                              guint32         stop,
                              GdkTimeCoord ***events,
                              gint           *n_events);

  void (* get_state)         (GdkDevice       *device,
                              GdkWindow       *window,
                              gdouble         *axes,
                              GdkModifierType *mask);

  void (* set_window_cursor) (GdkDevice *device,
                              GdkWindow *window,
                              GdkCursor *cursor);

  void (* warp)              (GdkDevice  *device,
                              GdkScreen  *screen,
                              gdouble     x,
                              gdouble     y);
  void (* query_state)       (GdkDevice       *device,
                              GdkWindow       *window,
                              GdkWindow      **root_window,
                              GdkWindow      **child_window,
                              gdouble          *root_x,
                              gdouble          *root_y,
                              gdouble          *win_x,
                              gdouble          *win_y,
                              GdkModifierType  *mask);
  GdkGrabStatus (* grab)     (GdkDevice        *device,
                              GdkWindow        *window,
                              gboolean          owner_events,
                              GdkEventMask      event_mask,
                              GdkWindow        *confine_to,
                              GdkCursor        *cursor,
                              guint32           time_);
  void          (*ungrab)    (GdkDevice        *device,
                              guint32           time_);

  GdkWindow * (* window_at_position) (GdkDevice       *device,
                                      double          *win_x,
                                      double          *win_y,
                                      GdkModifierType *mask,
                                      gboolean         get_toplevel);
  void (* select_window_events)      (GdkDevice       *device,
                                      GdkWindow       *window,
                                      GdkEventMask     event_mask);
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

gboolean   _gdk_device_translate_window_coord (GdkDevice *device,
                                               GdkWindow *window,
                                               guint      index,
                                               gdouble    value,
                                               gdouble   *axis_value);

gboolean   _gdk_device_translate_screen_coord (GdkDevice *device,
                                               GdkWindow *window,
                                               gdouble    window_root_x,
                                               gdouble    window_root_y,
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
                                               GdkWindow        *window,
                                               GdkWindow       **root_window,
                                               GdkWindow       **child_window,
                                               gdouble          *root_x,
                                               gdouble          *root_y,
                                               gdouble          *win_x,
                                               gdouble          *win_y,
                                               GdkModifierType  *mask);
GdkWindow * _gdk_device_window_at_position    (GdkDevice        *device,
                                               gdouble          *win_x,
                                               gdouble          *win_y,
                                               GdkModifierType  *mask,
                                               gboolean          get_toplevel);

/* Device tools */
GdkDeviceTool *gdk_device_tool_new    (guint          serial);
GdkDeviceTool *gdk_device_lookup_tool (GdkDevice     *device,
                                       guint          serial);
void           gdk_device_add_tool    (GdkDevice     *device,
                                       GdkDeviceTool *tool);
void           gdk_device_update_tool (GdkDevice     *device,
                                       GdkDeviceTool *tool);

G_END_DECLS

#endif /* __GDK_DEVICE_PRIVATE_H__ */
