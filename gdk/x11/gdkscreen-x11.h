/*
 * gdkscreen-x11.h
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_X11_SCREEN__
#define __GDK_X11_SCREEN__

#include "gdkscreenprivate.h"
#include "gdkx11screen.h"
#include "gdkvisual.h"
#include <X11/X.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS
  
typedef struct _GdkX11Monitor GdkX11Monitor;

struct _GdkX11Screen
{
  GdkScreen parent_instance;

  GdkDisplay *display;
  Display *xdisplay;
  Screen *xscreen;
  Window xroot_window;
  GdkWindow *root_window;
  gint screen_num;
  /* Xinerama/RandR 1.2 */
  gint  n_monitors;
  GdkX11Monitor *monitors;
  gint primary_monitor;

  gint width;
  gint height;

  gint window_scale;
  gboolean fixed_window_scale;

  /* Xft resources for the display, used for default values for
   * the Xft/ XSETTINGS
   */
  gint xft_hintstyle;
  gint xft_rgba;
  gint xft_dpi;

  /* Window manager */
  long last_wmspec_check_time;
  Window wmspec_check_window;
  char *window_manager_name;

  /* X Settings */
  GdkWindow *xsettings_manager_window;
  Atom xsettings_selection_atom;
  GHashTable *xsettings; /* string of GDK settings name => GValue */

  /* TRUE if wmspec_check_window has changed since last
   * fetch of _NET_SUPPORTED
   */
  guint need_refetch_net_supported : 1;
  /* TRUE if wmspec_check_window has changed since last
   * fetch of window manager name
   */
  guint need_refetch_wm_name : 1;
  guint is_composited : 1;
  guint xft_init : 1; /* Whether we've intialized these values yet */
  guint xft_antialias : 1;
  guint xft_hinting : 1;

  /* Visual Part */
  gint nvisuals;
  GdkVisual **visuals;
  GdkVisual *system_visual;
  gint available_depths[7];
  GdkVisualType available_types[6];
  gint16 navailable_depths;
  gint16 navailable_types;
  GHashTable *visual_hash;
  GdkVisual *rgba_visual;

  /* cache for window->translate vfunc */
  GC subwindow_gcs[32];

  /* cache for Xinerama monitor indices */
  GHashTable *xinerama_matches;
};

struct _GdkX11ScreenClass
{
  GdkScreenClass parent_class;

  void (* window_manager_changed) (GdkX11Screen *x11_screen);
};

GType       _gdk_x11_screen_get_type (void);
GdkScreen * _gdk_x11_screen_new      (GdkDisplay *display,
				      gint	  screen_number);

void _gdk_x11_screen_setup                  (GdkScreen *screen);
void _gdk_x11_screen_update_visuals_for_gl  (GdkScreen *screen);
void _gdk_x11_screen_window_manager_changed (GdkScreen *screen);
void _gdk_x11_screen_size_changed           (GdkScreen *screen,
					     XEvent    *event);
void _gdk_x11_screen_process_owner_change   (GdkScreen *screen,
					     XEvent    *event);
gint _gdk_x11_screen_get_xinerama_index     (GdkScreen *screen,
					     gint       monitor_num);
void _gdk_x11_screen_get_edge_monitors      (GdkScreen *screen,
					     gint      *top,
					     gint      *bottom,
					     gint      *left,
					     gint      *right);
void _gdk_x11_screen_set_window_scale       (GdkX11Screen *x11_screen,
					     int        scale);

G_END_DECLS

#endif /* __GDK_X11_SCREEN__ */
