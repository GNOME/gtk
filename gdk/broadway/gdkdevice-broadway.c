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

#include "config.h"
#include <stdlib.h>

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
static void gdk_broadway_device_query_state (GdkDevice        *device,
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

static void
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
  GdkWindow *toplevel;
  GdkWindowImplBroadway *impl;
  GdkDisplay *display;
  GdkBroadwayDisplay *broadway_display;
  GdkScreen *screen;
  gint device_root_x, device_root_y;

  if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
    return;

  display = gdk_device_get_display (device);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);
  toplevel = impl->wrapper;

  if (root_window)
    {
      screen = gdk_window_get_screen (window);
      *root_window = gdk_screen_get_root_window (screen);
    }

  if (broadway_display->output)
    {
      _gdk_broadway_display_consume_all_input (display);
      if (root_x)
	*root_x = broadway_display->future_root_x;
      if (root_y)
	*root_y = broadway_display->future_root_y;
      /* TODO: Should really use future_x/y when we get configure events */
      if (win_x)
	*win_x = broadway_display->future_root_x - toplevel->x;
      if (win_y)
	*win_y = broadway_display->future_root_y - toplevel->y;
      if (mask)
	*mask = broadway_display->future_state;
      if (child_window)
	{
	  if (gdk_window_get_window_type (toplevel) == GDK_WINDOW_ROOT)
	    *child_window =
	      g_hash_table_lookup (broadway_display->id_ht,
				   GINT_TO_POINTER (broadway_display->future_mouse_in_toplevel));
	  else
	    *child_window = toplevel; /* No native children */
	}
      return;
    }

  /* Fallback when unconnected */

  device_root_x = broadway_display->last_x;
  device_root_y = broadway_display->last_y;

  if (root_x)
    *root_x = device_root_x;
  if (root_y)
    *root_y = device_root_y;
  if (win_x)
    *win_x = device_root_y - toplevel->x;
  if (win_y)
    *win_y = device_root_y - toplevel->y;
  if (mask)
    *mask = broadway_display->last_state;
  if (child_window)
    {
      if (gdk_window_get_window_type (toplevel) == GDK_WINDOW_ROOT)
	{
	  *child_window = broadway_display->mouse_in_toplevel;
	  if (*child_window == NULL)
	    *child_window = toplevel;
	}
      else
	{
	  /* No native children */
	  *child_window = toplevel;
	}
    }

  return;
}

void
_gdk_broadway_window_grab_check_destroy (GdkWindow *window)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkBroadwayDisplay *broadway_display;
  GdkDeviceManager *device_manager;
  GdkDeviceGrabInfo *grab;
  GList *devices, *d;

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  device_manager = gdk_display_get_device_manager (display);

  /* Get all devices */
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (d = devices; d; d = d->next)
    {
      /* Make sure there is no lasting grab in this native window */
      grab = _gdk_display_get_last_device_grab (display, d->data);

      if (grab && grab->native_window == window)
	{
	  grab->serial_end = grab->serial_start;
	  grab->implicit_ungrab = TRUE;

	  broadway_display->pointer_grab_window = NULL;
	}

    }

  g_list_free (devices);
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
  GdkDisplay *display;
  GdkBroadwayDisplay *broadway_display;

  display = gdk_device_get_display (device);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      return GDK_GRAB_SUCCESS;
    }
  else
    {
      /* Device is a pointer */

      if (broadway_display->pointer_grab_window != NULL &&
	  time_ != 0 && broadway_display->pointer_grab_time > time_)
	return GDK_GRAB_ALREADY_GRABBED;

      if (time_ == 0)
	time_ = broadway_display->last_seen_time;

      broadway_display->pointer_grab_window = window;
      broadway_display->pointer_grab_owner_events = owner_events;
      broadway_display->pointer_grab_time = time_;

      if (broadway_display->output)
	{
	  broadway_output_grab_pointer (broadway_display->output,
					GDK_WINDOW_IMPL_BROADWAY (window->impl)->id,
					owner_events);
	  gdk_display_flush (display);
	}

      /* TODO: What about toplevel grab events if we're not connected? */

      return GDK_GRAB_SUCCESS;
    }
}

#define TIME_IS_LATER(time1, time2)                        \
  ( (( time1 > time2 ) && ( time1 - time2 < ((guint32)-1)/2 )) ||  \
    (( time1 < time2 ) && ( time2 - time1 > ((guint32)-1)/2 ))     \
  )

static void
gdk_broadway_device_ungrab (GdkDevice *device,
			    guint32    time_)
{
  GdkDisplay *display;
  GdkBroadwayDisplay *broadway_display;
  GdkDeviceGrabInfo *grab;
  guint32 serial;

  display = gdk_device_get_display (device);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
    }
  else
    {
      /* Device is a pointer */

      if (broadway_display->pointer_grab_window != NULL &&
	  time_ != 0 && broadway_display->pointer_grab_time > time_)
	return;

      /* TODO: What about toplevel grab events if we're not connected? */

      if (broadway_display->output)
	{
	  serial = broadway_output_ungrab_pointer (broadway_display->output);
	  gdk_display_flush (display);
	}
      else
	{
	  serial = broadway_display->saved_serial;
	}

      grab = _gdk_display_get_last_device_grab (display, device);
      if (grab &&
	  (time_ == GDK_CURRENT_TIME ||
	   grab->time == GDK_CURRENT_TIME ||
	   !TIME_IS_LATER (grab->time, time_)))
	grab->serial_end = serial;

      broadway_display->pointer_grab_window = NULL;
    }
}

static GdkWindow *
gdk_broadway_device_window_at_position (GdkDevice       *device,
					gint            *win_x,
					gint            *win_y,
					GdkModifierType *mask,
					gboolean         get_toplevel)
{
  gboolean res;
  GdkScreen *screen;
  GdkWindow *root_window;
  GdkWindow *window;

  screen = gdk_screen_get_default ();
  root_window = gdk_screen_get_root_window (screen);

  gdk_broadway_device_query_state (device, root_window, NULL, &window, NULL, NULL, win_x, win_y, mask);

  return window;
}

static void
gdk_broadway_device_select_window_events (GdkDevice    *device,
					  GdkWindow    *window,
					  GdkEventMask  event_mask)
{
}
