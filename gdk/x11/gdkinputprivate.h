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

/* Addition used for extension_events mask */
#define GDK_ALL_DEVICES_MASK (1<<30)

struct _GdkInputWindow
{
  GList *windows; /* GdkWindow:s with extension_events set */

  /* gdk window */
  GdkWindow *impl_window; /* an impl window */
  GdkWindow *button_down_window;

  /* position relative to root window */
  gint root_x;
  gint root_y;

  /* Is there a pointer grab for this window ? */
  gint grabbed;
};

/* Global data */

#define GDK_IS_CORE(d) (((GdkDevice *)(d)) == gdk_device_get_display (d)->core_pointer)

/* Function declarations */

GdkInputWindow *_gdk_input_window_find       (GdkWindow *window);
void            _gdk_input_window_destroy    (GdkWindow *window);
void            _gdk_init_input_core         (GdkDisplay *display);

/* The following functions are provided by each implementation
 * (xfree, gxi, and none)
 */
void             _gdk_input_configure_event  (XConfigureEvent  *xevent,
					      GdkWindow        *window);
void             _gdk_input_crossing_event   (GdkWindow        *window,
					      gboolean          enter);
gboolean         _gdk_input_other_event      (GdkEvent         *event,
					      XEvent           *xevent,
					      GdkWindow        *window);
gint             _gdk_input_grab_pointer     (GdkWindow        *window,
					      GdkWindow        *native_window,
					      gint              owner_events,
					      GdkEventMask      event_mask,
					      GdkWindow        *confine_to,
					      guint32           time);
void             _gdk_input_ungrab_pointer   (GdkDisplay       *display,
					      guint32           time);
gboolean         _gdk_device_get_history     (GdkDevice         *device,
					      GdkWindow         *window,
					      guint32            start,
					      guint32            stop,
					      GdkTimeCoord    ***events,
					      gint              *n_events);

#ifndef XINPUT_NONE

#define GDK_MAX_DEVICE_CLASSES 13

gint               _gdk_input_common_init               (GdkDisplay	  *display,
							 gint              include_core);
GdkDevice *        _gdk_input_find_device               (GdkDisplay	  *display,
							 guint32           id);
void               _gdk_input_get_root_relative_geometry(GdkWindow        *window,
							 int              *x_ret,
							 int              *y_ret);
void               _gdk_input_common_find_events        (GdkDevice        *device,
							 gint              mask,
							 XEventClass      *classes,
							 int              *num_classes);
void               _gdk_input_select_events             (GdkWindow        *impl_window,
                                                         GdkDevice        *device);
#if 0
gint               _gdk_input_common_other_event        (GdkEvent         *event,
							 XEvent           *xevent,
							 GdkWindow        *window,
                                                         GdkDevice        *device);
#endif

#endif /* !XINPUT_NONE */

#endif /* __GDK_INPUTPRIVATE_H__ */
