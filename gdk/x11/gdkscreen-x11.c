/*
 * gdkscreen-x11.c
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

#include <glib.h>
#include "gdkscreen.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay.h"
#include "gdkdisplay-x11.h"
#include "gdkx.h"

static void         gdk_screen_x11_class_init       (GdkScreenX11Class *klass);
static GdkDisplay * gdk_screen_x11_get_display           (GdkScreen             *screen);
static gint         gdk_screen_x11_get_width             (GdkScreen             *screen);
static gint         gdk_screen_x11_get_height            (GdkScreen             *screen);
static gint         gdk_screen_x11_get_width_mm          (GdkScreen             *screen);
static gint         gdk_screen_x11_get_height_mm         (GdkScreen             *screen);
static gint         gdk_screen_x11_get_default_depth     (GdkScreen             *screen);
static GdkWindow *  gdk_screen_x11_get_root_window       (GdkScreen             *screen);
static gint         gdk_screen_x11_get_screen_num        (GdkScreen             *screen);
static GdkColormap *gdk_screen_x11_get_default_colormap  (GdkScreen             *screen);
static void         gdk_screen_x11_set_default_colormap  (GdkScreen             *screen,
							  GdkColormap           *colormap);
static GdkWindow *  gdk_screen_x11_get_window_at_pointer (GdkScreen             *screen,
							  gint                  *win_x,
							  gint                  *win_y);
static void         gdk_screen_x11_finalize		 (GObject		*object);

static gint          gdk_screen_x11_get_n_monitors        (GdkScreen       *screen);
static void          gdk_screen_x11_get_monitor_geometry  (GdkScreen       *screen,
							   gint             num_monitor,
							   GdkRectangle    *dest);

GType gdk_screen_x11_get_type ();
static gpointer parent_class = NULL;

GType
gdk_screen_x11_get_type ()
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (GdkScreenX11Class),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) gdk_screen_x11_finalize,
	  (GClassInitFunc) gdk_screen_x11_class_init,
	  NULL,			/* class_finalize */
	  NULL,			/* class_data */
	  sizeof (GdkScreenX11),
	  0,			/* n_preallocs */
	  (GInstanceInitFunc) NULL,
	};
      object_type = g_type_register_static (GDK_TYPE_SCREEN,
					    "GdkScreenX11",
					    &object_info, 0);
    }
  return object_type;
}

void
gdk_screen_x11_class_init (GdkScreenX11Class * klass)
{
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);
  
  screen_class->get_display = gdk_screen_x11_get_display;
  screen_class->get_width = gdk_screen_x11_get_width;
  screen_class->get_height = gdk_screen_x11_get_height;
  screen_class->get_width_mm = gdk_screen_x11_get_width_mm;
  screen_class->get_height_mm = gdk_screen_x11_get_height_mm;
  screen_class->get_root_depth = gdk_screen_x11_get_default_depth;
  screen_class->get_screen_num = gdk_screen_x11_get_screen_num;
  screen_class->get_root_window = gdk_screen_x11_get_root_window;
  screen_class->get_default_colormap = gdk_screen_x11_get_default_colormap;
  screen_class->set_default_colormap = gdk_screen_x11_set_default_colormap;
  screen_class->get_window_at_pointer = gdk_screen_x11_get_window_at_pointer;
  screen_class->get_n_monitors = gdk_screen_x11_get_n_monitors;
  screen_class->get_monitor_geometry = gdk_screen_x11_get_monitor_geometry;
  
  G_OBJECT_CLASS (klass)->finalize = gdk_screen_x11_finalize;
  parent_class = g_type_class_peek_parent (klass);
}

static GdkDisplay *
gdk_screen_x11_get_display (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return screen_x11->display;
}

static gint
gdk_screen_x11_get_width (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return WidthOfScreen (screen_x11->xscreen);
}

static gint
gdk_screen_x11_get_height (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return HeightOfScreen (screen_x11->xscreen);
}

static gint
gdk_screen_x11_get_width_mm (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return WidthMMOfScreen (screen_x11->xscreen);
}

static gint
gdk_screen_x11_get_height_mm (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return HeightMMOfScreen (screen_x11->xscreen);
}

static gint
gdk_screen_x11_get_default_depth (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return DefaultDepthOfScreen (screen_x11->xscreen);
}

static gint
gdk_screen_x11_get_screen_num (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  
  return screen_x11->screen_num;
}

static GdkWindow *
gdk_screen_x11_get_root_window (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return screen_x11->root_window;
}

static GdkColormap *
gdk_screen_x11_get_default_colormap (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

  return screen_x11->default_colormap;
}

static void
gdk_screen_x11_set_default_colormap (GdkScreen *screen,
				     GdkColormap * colormap)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  
  screen_x11->default_colormap = colormap;
}

static GdkWindow *
gdk_screen_x11_get_window_at_pointer (GdkScreen *screen,
				      gint      *win_x,
				      gint      *win_y)
{
  GdkWindow *window;
  Window root;
  Window xwindow;
  Window xwindow_last = 0;
  Display *xdisplay;
  int rootx = -1, rooty = -1;
  int winx, winy;
  unsigned int xmask;

  xwindow = GDK_SCREEN_XROOTWIN (screen);
  xdisplay = GDK_SCREEN_XDISPLAY (screen);

  XGrabServer (xdisplay);
  while (xwindow)
    {
      xwindow_last = xwindow;
      XQueryPointer (xdisplay, xwindow,
		     &root, &xwindow, &rootx, &rooty, &winx, &winy, &xmask);
    }
  XUngrabServer (xdisplay);

  window = gdk_window_lookup_for_display (GDK_SCREEN_DISPLAY(screen),
					  xwindow_last);
  if (win_x)
    *win_x = window ? winx : -1;
  if (win_y)
    *win_y = window ? winy : -1;

  return window;
}

static void
gdk_screen_x11_finalize (GObject *object)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (object);
  /* int i; */
  g_object_unref (G_OBJECT (screen_x11->root_window));
  
  /* Visual Part (Need to implement finalize for Visuals for a clean
   * finalize) */
  /* for (i=0;i<screen_x11->nvisuals;i++)
    g_object_unref (G_OBJECT (screen_x11->visuals[i]));*/
  g_free (screen_x11->visuals);
  g_hash_table_destroy (screen_x11->visual_hash);
  /* X settings */
  g_free (screen_x11->xsettings_client);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint 
gdk_screen_x11_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 1);
  return GDK_SCREEN_X11 (screen)->num_monitors;
}

static void
gdk_screen_x11_get_monitor_geometry (GdkScreen    *screen, 
				     gint          num_monitor,
				     GdkRectangle *dest)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  g_return_if_fail (num_monitor < GDK_SCREEN_X11 (screen)->num_monitors);

  *dest = screen_x11->monitors[num_monitor];
}

/**
 * gdk_x11_screen_get_xscreen:
 * @screen: a #GdkScreen.
 * @returns: an Xlib <type>Screen*</type>
 *
 * Returns the screen of a #GdkScreen.
 */
Screen *
gdk_x11_screen_get_xscreen (GdkScreen *screen)
{
  return GDK_SCREEN_X11 (screen)->xscreen;
}


/**
 * gdk_x11_screen_get_screen_number:
 * @screen: a #GdkScreen.
 * @returns: the position of @screen among the screens of
 *   its display.
 *
 * Returns the index of a #GdkScreen.
 */
int
gdk_x11_screen_get_screen_number (GdkScreen *screen)
{
  return GDK_SCREEN_X11 (screen)->screen_num;
}
