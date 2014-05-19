/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkdeviceprivate.h"

typedef struct GdkMirDevice      GdkMirDevice;
typedef struct GdkMirDeviceClass GdkMirDeviceClass;

#define GDK_TYPE_MIR_DEVICE              (gdk_mir_device_get_type ())
#define GDK_MIR_DEVICE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_DEVICE, GdkMirDevice))
#define GDK_MIR_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_DEVICE, GdkMirDeviceClass))
#define GDK_IS_MIR_DEVICE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_DEVICE))
#define GDK_IS_MIR_DEVICE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_DEVICE))
#define GDK_MIR_DEVICE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_DEVICE, GdkMirDeviceClass))

struct GdkMirDevice
{
  GdkDevice parent_instance;
};

struct GdkMirDeviceClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkMirDevice, gdk_mir_device, GDK_TYPE_DEVICE)

GdkDevice *
_gdk_mir_device_new (GdkDeviceManager *device_manager, const gchar *name, GdkInputSource input_source, gboolean has_cursor)
{
  return g_object_new (GDK_TYPE_MIR_DEVICE,
                       "display", gdk_device_manager_get_display (device_manager),
                       "device-manager", device_manager,
                       "name", name,
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", input_source,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", has_cursor,
                       NULL);
}

static gboolean
gdk_mir_device_get_history (GdkDevice      *device,
                            GdkWindow      *window,
                            guint32         start,
                            guint32         stop,
                            GdkTimeCoord ***events,
                            gint           *n_events)
{
  g_printerr ("gdk_mir_device_get_history\n");
  return FALSE;
}

static void
gdk_mir_device_get_state (GdkDevice       *device,
                          GdkWindow       *window,
                          gdouble         *axes,
                          GdkModifierType *mask)
{
  g_printerr ("gdk_mir_device_get_state\n");
}

static void
gdk_mir_device_set_window_cursor (GdkDevice *device,
                                  GdkWindow *window,
                                  GdkCursor *cursor)
{
  g_printerr ("gdk_mir_device_set_window_cursor\n");
}

static void
gdk_mir_device_warp (GdkDevice *device,
                     GdkScreen *screen,
                     gdouble    x,
                     gdouble    y)
{
  g_printerr ("gdk_mir_device_warp\n");
}

static void
gdk_mir_device_query_state (GdkDevice        *device,
                            GdkWindow        *window,
                            GdkWindow       **root_window,
                            GdkWindow       **child_window,
                            gdouble          *root_x,
                            gdouble          *root_y,
                            gdouble          *win_x,
                            gdouble          *win_y,
                            GdkModifierType  *mask)
{
  g_printerr ("gdk_mir_device_query_state\n");
}

static GdkGrabStatus
gdk_mir_device_grab (GdkDevice    *device,
                     GdkWindow    *window,
                     gboolean      owner_events,
                     GdkEventMask  event_mask,
                     GdkWindow    *confine_to,
                     GdkCursor    *cursor,
                     guint32       time_)
{
  g_printerr ("gdk_mir_device_grab\n");
  return GDK_GRAB_SUCCESS;
}

static void
gdk_mir_device_ungrab (GdkDevice *device,
                       guint32    time_)
{
  g_printerr ("gdk_mir_device_ungrab\n");
}

static GdkWindow *
gdk_mir_device_window_at_position (GdkDevice       *device,
                                   gdouble         *win_x,
                                   gdouble         *win_y,
                                   GdkModifierType *mask,
                                   gboolean         get_toplevel)
{
  g_printerr ("gdk_mir_device_window_at_position (%f, %f)\n", *win_x, *win_y);
  return NULL;
}

static void
gdk_mir_device_select_window_events (GdkDevice    *device,
                                     GdkWindow    *window,
                                     GdkEventMask  event_mask)
{
  g_printerr ("gdk_mir_device_select_window_events\n");
}

static void
gdk_mir_device_init (GdkMirDevice *device)
{
}

static void
gdk_mir_device_class_init (GdkMirDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_mir_device_get_history;
  device_class->get_state = gdk_mir_device_get_state;
  device_class->set_window_cursor = gdk_mir_device_set_window_cursor;
  device_class->warp = gdk_mir_device_warp;
  device_class->query_state = gdk_mir_device_query_state;
  device_class->grab = gdk_mir_device_grab;
  device_class->ungrab = gdk_mir_device_ungrab;
  device_class->window_at_position = gdk_mir_device_window_at_position;
  device_class->select_window_events = gdk_mir_device_select_window_events;
}
