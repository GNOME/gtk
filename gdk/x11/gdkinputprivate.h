/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_INPUTPRIVATE_H__
#define __GDK_INPUTPRIVATE_H__

#include "config.h"
#include "gdkinput.h"
#include "gdkevents.h"
#include "gdkx.h"

#include <X11/Xlib.h>

#ifndef XINPUT_NONE
#include <X11/extensions/XInput.h>
#endif


typedef struct _GdkAxisInfo    GdkAxisInfo;
typedef struct _GdkDevicePrivate GdkDevicePrivate;
typedef struct _GdkInputWindow GdkInputWindow;

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

struct _GdkDevicePrivate
{
  GdkDevice info;

  guint32 deviceid;
  

#ifndef XINPUT_NONE
  /* information about the axes */
  GdkAxisInfo *axes;

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

struct _GdkDeviceClass
{
  GObjectClass parent_class;
};

struct _GdkInputWindow
{
  /* gdk window */
  GdkWindow *window;

  /* Extension mode (GDK_EXTENSION_EVENTS_ALL/CURSOR) */
  GdkExtensionMode mode;

  /* position relative to root window */
  gint root_x;
  gint root_y;

  /* rectangles relative to window of windows obscuring this one */
  GdkRectangle *obscuring;
  gint num_obscuring;

  /* Is there a pointer grab for this window ? */
  gint grabbed;
};



/* Global data */

#define GDK_IS_CORE(d) (((GdkDevice *)(d)) == gdk_core_pointer)

extern GList *gdk_input_devices;
extern GList *gdk_input_windows;

/* information about network port and host for gxid daemon */
extern gchar           *gdk_input_gxid_host;
extern gint             gdk_input_gxid_port;
extern gint             gdk_input_ignore_core;

/* Function declarations */

GdkInputWindow *gdk_input_window_find        (GdkWindow *window);
void            gdk_input_window_destroy     (GdkWindow *window);
GdkTimeCoord ** _gdk_device_allocate_history (GdkDevice *device,
					      gint       n_events);
void            _gdk_init_input_core         (void);

/* The following functions are provided by each implementation
 * (xfree, gxi, and none)
 */
gint             _gdk_input_enable_window     (GdkWindow        *window,
					      GdkDevicePrivate *gdkdev);
gint             _gdk_input_disable_window    (GdkWindow        *window,
					      GdkDevicePrivate *gdkdev);
gint             _gdk_input_window_none_event (GdkEvent         *event,
					      XEvent           *xevent);
void             _gdk_input_configure_event  (XConfigureEvent  *xevent,
					      GdkWindow        *window);
void             _gdk_input_enter_event      (XCrossingEvent   *xevent,
					      GdkWindow        *window);
gint             _gdk_input_other_event      (GdkEvent         *event,
					      XEvent           *xevent,
					      GdkWindow        *window);
gint             _gdk_input_grab_pointer     (GdkWindow        *window,
					      gint              owner_events,
					      GdkEventMask      event_mask,
					      GdkWindow        *confine_to,
					      guint32           time);
void             _gdk_input_ungrab_pointer   (guint32           time);
gboolean         _gdk_device_get_history     (GdkDevice         *device,
					      GdkWindow         *window,
					      guint32            start,
					      guint32            stop,
					      GdkTimeCoord    ***events,
					      gint              *n_events);

#ifndef XINPUT_NONE

#define GDK_MAX_DEVICE_CLASSES 13

gint               gdk_input_common_init                (gint              include_core);
GdkDevicePrivate * gdk_input_find_device                (guint32           id);
void               gdk_input_get_root_relative_geometry (Display          *dpy,
							 Window            w,
							 int              *x_ret,
							 int              *y_ret,
							 int              *width_ret,
							 int              *height_ret);
void               gdk_input_common_find_events         (GdkWindow        *window,
							 GdkDevicePrivate *gdkdev,
							 gint              mask,
							 XEventClass      *classes,
							 int              *num_classes);
void               gdk_input_common_select_events       (GdkWindow        *window,
							 GdkDevicePrivate *gdkdev);
gint               gdk_input_common_other_event         (GdkEvent         *event,
							 XEvent           *xevent,
							 GdkInputWindow   *input_window,
							 GdkDevicePrivate *gdkdev);

#endif /* !XINPUT_NONE */

#endif /* __GDK_INPUTPRIVATE_H__ */
