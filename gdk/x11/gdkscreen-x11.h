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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_X11_SCREEN__
#define __GDK_X11_SCREEN__

#include "gdkscreenprivate.h"
#include "gdkx11screen.h"
#include "gdkvisual.h"
#include "xsettings-client.h"
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
  gint screen_num;
  Window xroot_window;
  GdkWindow *root_window;

  /* Window manager */
  long last_wmspec_check_time;
  Window wmspec_check_window;
  char *window_manager_name;
  /* TRUE if wmspec_check_window has changed since last
   * fetch of _NET_SUPPORTED
   */
  guint need_refetch_net_supported : 1;
  /* TRUE if wmspec_check_window has changed since last
   * fetch of window manager name
   */
  guint need_refetch_wm_name : 1;
  
  /* Visual Part */
  GdkVisual *system_visual;
  GdkVisual **visuals;
  gint nvisuals;
  gint available_depths[7];
  gint navailable_depths;
  GdkVisualType available_types[6];
  gint navailable_types;
  GHashTable *visual_hash;
  GdkVisual *rgba_visual;
  
  /* X settings */
  XSettingsClient *xsettings_client;
  guint xsettings_in_init : 1;
  
  /* Xinerama/RandR 1.2 */
  gint		 n_monitors;
  GdkX11Monitor	*monitors;
  gint           primary_monitor;

  /* cache for window->translate vfunc */
  GC subwindow_gcs[32];

  /* Xft resources for the display, used for default values for
   * the Xft/ XSETTINGS
   */
  gboolean xft_init;		/* Whether we've intialized these values yet */
  gboolean xft_antialias;
  gboolean xft_hinting;
  gint xft_hintstyle;
  gint xft_rgba;
  gint xft_dpi;

  GdkAtom cm_selection_atom;
  gboolean is_composited;
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
void _gdk_x11_screen_window_manager_changed (GdkScreen *screen);
void _gdk_x11_screen_size_changed           (GdkScreen *screen,
					     XEvent    *event);
void _gdk_x11_screen_process_owner_change   (GdkScreen *screen,
					     XEvent    *event);

G_END_DECLS

#endif /* __GDK_X11_SCREEN__ */
