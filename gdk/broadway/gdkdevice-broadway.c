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

#include "gdkdevice-broadway.h"

#include "gdkwindow.h"
#include "gdkprivate-broadway.h"

static gboolean gdk_broadway_device_get_history (GdkDevice      *device,
						 GdkWindow      *window,
						 guint32         start,
						 guint32         stop,
						 GdkTimeCoord ***events,
						 gint           *n_events);
static void gdk_broadway_device_get_state (GdkDevice       *device,
					   GdkWindow       *window,
					   gdouble         *axes,
					   GdkModifierType *mask);
static void gdk_broadway_device_set_window_cursor (GdkDevice *device,
						   GdkWindow *window,
						   GdkCursor *cursor);
static void gdk_broadway_device_warp (GdkDevice *device,
				      GdkScreen *screen,
				      gint       x,
				      gint       y);
static gboolean gdk_broadway_device_query_state (GdkDevice        *device,
						 GdkWindow        *window,
						 GdkWindow       **root_window,
						 GdkWindow       **child_window,
						 gint             *root_x,
						 gint             *root_y,
						 gint             *win_x,
						 gint             *win_y,
						 GdkModifierType  *mask);
static GdkGrabStatus gdk_broadway_device_grab   (GdkDevice     *device,
						 GdkWindow     *window,
						 gboolean       owner_events,
						 GdkEventMask   event_mask,
						 GdkWindow     *confine_to,
						 GdkCursor     *cursor,
						 guint32        time_);
static void          gdk_broadway_device_ungrab (GdkDevice     *device,
						 guint32        time_);
static GdkWindow * gdk_broadway_device_window_at_position (GdkDevice       *device,
							   gint            *win_x,
							   gint            *win_y,
							   GdkModifierType *mask,
							   gboolean         get_toplevel);
static void      gdk_broadway_device_select_window_events (GdkDevice       *device,
							   GdkWindow       *window,
							   GdkEventMask     event_mask);


G_DEFINE_TYPE (GdkBroadwayDevice, gdk_broadway_device, GDK_TYPE_DEVICE)

static void
gdk_broadway_device_class_init (GdkBroadwayDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_broadway_device_get_history;
  device_class->get_state = gdk_broadway_device_get_state;
  device_class->set_window_cursor = gdk_broadway_device_set_window_cursor;
  device_class->warp = gdk_broadway_device_warp;
  device_class->query_state = gdk_broadway_device_query_state;
  device_class->grab = gdk_broadway_device_grab;
  device_class->ungrab = gdk_broadway_device_ungrab;
  device_class->window_at_position = gdk_broadway_device_window_at_position;
  device_class->select_window_events = gdk_broadway_device_select_window_events;
}

static void
gdk_broadway_device_init (GdkBroadwayDevice *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_Y, 0, 0, 1);
}

static gboolean
gdk_broadway_device_get_history (GdkDevice      *device,
				 GdkWindow      *window,
				 guint32         start,
				 guint32         stop,
				 GdkTimeCoord ***events,
				 gint           *n_events)
{
  return FALSE;
}

static void
gdk_broadway_device_get_state (GdkDevice       *device,
			       GdkWindow       *window,
			       gdouble         *axes,
			       GdkModifierType *mask)
{
  gint x_int, y_int;

  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (axes)
    {
      axes[0] = x_int;
      axes[1] = y_int;
    }
}

static void
gdk_broadway_device_set_window_cursor (GdkDevice *device,
				       GdkWindow *window,
				       GdkCursor *cursor)
{
}

static void
gdk_broadway_device_warp (GdkDevice *device,
			  GdkScreen *screen,
			  gint       x,
			  gint       y)
{
}

static gboolean
gdk_broadway_device_query_state (GdkDevice        *device,
				 GdkWindow        *window,
				 GdkWindow       **root_window,
				 GdkWindow       **child_window,
				 gint             *root_x,
				 gint             *root_y,
				 gint             *win_x,
				 gint             *win_y,
				 GdkModifierType  *mask)
{
  return FALSE;
}

static GdkGrabStatus
gdk_broadway_device_grab (GdkDevice    *device,
			  GdkWindow    *window,
			  gboolean      owner_events,
			  GdkEventMask  event_mask,
			  GdkWindow    *confine_to,
			  GdkCursor    *cursor,
			  guint32       time_)
{
  return GDK_GRAB_NOT_VIEWABLE;
}

static void
gdk_broadway_device_ungrab (GdkDevice *device,
			    guint32    time_)
{
}

static GdkWindow *
gdk_broadway_device_window_at_position (GdkDevice       *device,
					gint            *win_x,
					gint            *win_y,
					GdkModifierType *mask,
					gboolean         get_toplevel)
{
  return NULL;
}

static void
gdk_broadway_device_select_window_events (GdkDevice    *device,
					  GdkWindow    *window,
					  GdkEventMask  event_mask)
{
}
