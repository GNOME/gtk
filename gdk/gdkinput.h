/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_INPUT_H__
#define __GDK_INPUT_H__

#ifndef XINPUT_NONE
#include <X11/extensions/XInput.h>
#endif

typedef struct _GdkAxisInfo    GdkAxisInfo;
typedef struct _GdkInputVTable GdkInputVTable;
typedef struct _GdkDevicePrivate GdkDevicePrivate;
typedef struct _GdkInputWindow GdkInputWindow;

struct _GdkInputVTable {
  gint (*set_mode) (guint32 deviceid, GdkInputMode mode);
  void (*set_axes) (guint32 deviceid, GdkAxisUse *axes);
  void (*set_key)  (guint32 deviceid,
		    guint   index,
		    guint   keyval,
		    GdkModifierType modifiers);
	
  GdkTimeCoord* (*motion_events) (GdkWindow *window,
				  guint32 deviceid,
				  guint32 start,
				  guint32 stop,
				  gint *nevents_return);
  void (*get_pointer)   (GdkWindow       *window,
			 guint32	  deviceid,
			 gdouble         *x,
			 gdouble         *y,
			 gdouble         *pressure,
			 gdouble         *xtilt,
			 gdouble         *ytilt,
			 GdkModifierType *mask);
  gint (*grab_pointer) (GdkWindow *     window,
			gint            owner_events,
			GdkEventMask    event_mask,
			GdkWindow *     confine_to,
			guint32         time);
  void (*ungrab_pointer) (guint32 time);

  void (*configure_event) (XConfigureEvent *xevent, GdkWindow *window);
  void (*enter_event) (XCrossingEvent *xevent, GdkWindow *window);
  gint (*other_event) (GdkEvent *event, XEvent *xevent, GdkWindow *window);
  /* Handle an unidentified event. Returns TRUE if handled, FALSE
     otherwise */
  gint (*window_none_event) (GdkEvent *event, XEvent *xevent);
  gint (*enable_window) (GdkWindow *window, GdkDevicePrivate *gdkdev);
  gint (*disable_window) (GdkWindow *window, GdkDevicePrivate *gdkdev);
};

/* information about a device axis */
struct _GdkAxisInfo
{
  /* reported x resolution */
  gint xresolution;

  /* reported x minimum/maximum values */
  gint xmin_value, xmax_value;

  /* calibrated resolution (for aspect ration) - only relative values
     between axes used */
  gint resolution;
  
  /* calibrated minimum/maximum values */
  gint min_value, max_value;
};

#define GDK_INPUT_NUM_EVENTC 6

struct _GdkDevicePrivate {
  GdkDeviceInfo  info;

#ifndef XINPUT_NONE
  /* information about the axes */
  GdkAxisInfo *axes;

  /* reverse lookup on axis use type */
  gint axis_for_use[GDK_AXIS_LAST];
  
  /* Information about XInput device */
  XDevice       *xdevice;

  /* minimum key code for device */
  gint min_keycode;	       

  int buttonpress_type, buttonrelease_type, keypress_type,
      keyrelease_type, motionnotify_type, proximityin_type, 
      proximityout_type, changenotify_type;

  /* true if we need to select a different set of events, but
     can't because this is the core pointer */
  gint needs_update;

  /* Mask of buttons (used for button grabs) */
  gint button_state;

  /* true if we've claimed the device as active. (used only for XINPUT_GXI) */
  gint claimed;
#endif /* !XINPUT_NONE */
};

struct _GdkInputWindow
{
  /* gdk window */
  GdkWindow *window;

  /* Extension mode (GDK_EXTENSION_EVENTS_ALL/CURSOR) */
  GdkExtensionMode mode;

  /* position relative to root window */
  gint16 root_x;
  gint16 root_y;

  /* rectangles relative to window of windows obscuring this one */
  GdkRectangle *obscuring;
  gint num_obscuring;

  /* Is there a pointer grab for this window ? */
  gint grabbed;
};

/* Global data */

extern GdkInputVTable gdk_input_vtable;
/* information about network port and host for gxid daemon */
extern gchar           *gdk_input_gxid_host;
extern gint             gdk_input_gxid_port;
extern gint             gdk_input_ignore_core;

/* Function declarations */

void gdk_input_window_destroy (GdkWindow *window);

#endif /* __GDK_INPUT_H__ */
