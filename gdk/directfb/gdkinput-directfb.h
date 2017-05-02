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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002       convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#ifndef __GDK_INPUT_DIRECTFB_H__
#define __GDK_INPUT_DIRECTFB_H__

extern GdkModifierType _gdk_directfb_modifiers;
extern int _gdk_directfb_mouse_x, _gdk_directfb_mouse_y;

typedef struct _GdkAxisInfo      GdkAxisInfo;

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

#define GDK_IS_CORE(d) (((GdkDevice *)(d)) == _gdk_core_pointer)

extern GList *_gdk_input_devices;
extern GList *_gdk_input_windows;

extern gint   _gdk_input_ignore_core;

/* Function declarations */

/* The following functions are provided by each implementation
 */
gint             _gdk_input_window_none_event(GdkEvent          *event,
                                              gchar             *msg);
void             _gdk_input_configure_event  (GdkEventConfigure *event,
                                              GdkWindow         *window);
void             _gdk_input_enter_event      (GdkEventCrossing  *event,
                                              GdkWindow         *window);
gint             _gdk_input_other_event      (GdkEvent          *event,
                                              gchar             *msg,
                                              GdkWindow         *window);

/* These should be in gdkinternals.h */

GdkInputWindow  * gdk_input_window_find      (GdkWindow         *window);

void              gdk_input_window_destroy   (GdkWindow         *window);

gint             _gdk_input_enable_window    (GdkWindow         *window,
                                              GdkDevice         *gdkdev);
gint             _gdk_input_disable_window   (GdkWindow         *window,
                                              GdkDevice         *gdkdev);
gint             _gdk_input_grab_pointer     (GdkWindow         *window,
                                              gint               owner_events,
                                              GdkEventMask       event_mask,
                                              GdkWindow         *confine_to,
                                              guint32            time);
void             _gdk_input_ungrab_pointer   (guint32            time);
gboolean         _gdk_device_get_history     (GdkDevice         *device,
                                              GdkWindow         *window,
                                              guint32            start,
                                              guint32            stop,
                                              GdkTimeCoord    ***events,
                                              gint              *n_events);

gint             gdk_input_common_init        (gint              include_core);
gint             gdk_input_common_other_event (GdkEvent         *event,
                                               gchar            *msg,
                                               GdkInputWindow   *input_window,
                                               GdkWindow        *window);

void         _gdk_directfb_keyboard_init      (void);
void         _gdk_directfb_keyboard_exit      (void);

void         gdk_directfb_translate_key_event (DFBWindowEvent   *dfb_event,
                                               GdkEventKey      *event);

#endif /* __GDK_INPUT_DIRECTFB_H__ */
