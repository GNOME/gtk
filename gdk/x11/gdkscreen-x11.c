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

static void         gdk_X11_screen_impl_class_init       (GdkScreenImplX11Class *klass);
static GdkDisplay * gdk_X11_screen_get_display           (GdkScreen             *screen);
static gint         gdk_X11_screen_get_width             (GdkScreen             *screen);
static gint         gdk_X11_screen_get_height            (GdkScreen             *screen);
static gint         gdk_X11_screen_get_width_mm          (GdkScreen             *screen);
static gint         gdk_X11_screen_get_height_mm         (GdkScreen             *screen);
static gint         gdk_X11_screen_get_default_depth     (GdkScreen             *screen);
static GdkWindow *  gdk_X11_screen_get_root_window       (GdkScreen             *screen);
static gint         gdk_X11_screen_get_screen_num        (GdkScreen             *screen);
static GdkColormap *gdk_X11_screen_get_default_colormap  (GdkScreen             *screen);
static void         gdk_X11_screen_set_default_colormap  (GdkScreen             *screen,
							  GdkColormap           *colormap);
static GdkWindow *  gdk_X11_screen_get_window_at_pointer (GdkScreen             *screen,
							  gint                  *win_x,
							  gint                  *win_y);
static void         gdk_X11_screen_finalize		 (GObject		*object);

GType gdk_X11_screen_impl_get_type ();
static gpointer parent_class = NULL;

GType
gdk_X11_screen_impl_get_type ()
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
	sizeof (GdkScreenImplX11Class),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) gdk_X11_screen_finalize,
	(GClassInitFunc) gdk_X11_screen_impl_class_init,
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (GdkScreenImplX11),
	0,			/* n_preallocs */
	(GInstanceInitFunc) NULL,
      };
      object_type = g_type_register_static (GDK_TYPE_SCREEN,
					    "GdkScreenImplX11",
					    &object_info, 0);
    }
  return object_type;
}

void
gdk_X11_screen_impl_class_init (GdkScreenImplX11Class * klass)
{
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);
  screen_class->get_display = gdk_X11_screen_get_display;
  screen_class->get_width = gdk_X11_screen_get_width;
  screen_class->get_height = gdk_X11_screen_get_height;
  screen_class->get_width_mm = gdk_X11_screen_get_width_mm;
  screen_class->get_height_mm = gdk_X11_screen_get_height_mm;
  screen_class->get_root_depth = gdk_X11_screen_get_default_depth;
  screen_class->get_screen_num = gdk_X11_screen_get_screen_num;
  screen_class->get_root_window = gdk_X11_screen_get_root_window;
  screen_class->get_default_colormap = gdk_X11_screen_get_default_colormap;
  screen_class->set_default_colormap = gdk_X11_screen_set_default_colormap;
  screen_class->get_window_at_pointer = gdk_X11_screen_get_window_at_pointer;
  G_OBJECT_CLASS (klass)->finalize = gdk_X11_screen_finalize;
  parent_class = g_type_class_peek_parent (klass);
}

static GdkDisplay *
gdk_X11_screen_get_display (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return screen_impl->display;
}

static gint
gdk_X11_screen_get_width (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return (gint) WidthOfScreen (screen_impl->xscreen);
}

static gint
gdk_X11_screen_get_height (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return (gint) HeightOfScreen (screen_impl->xscreen);
}

static gint
gdk_X11_screen_get_width_mm (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return (gint) WidthMMOfScreen (screen_impl->xscreen);
}

static gint
gdk_X11_screen_get_height_mm (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return (gint) HeightMMOfScreen (screen_impl->xscreen);
}

static gint
gdk_X11_screen_get_default_depth (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return DefaultDepthOfScreen (screen_impl->xscreen);
}

static gint
gdk_X11_screen_get_screen_num (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);
  return screen_impl->screen_num;
}

static GdkWindow *
gdk_X11_screen_get_root_window (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  gdk_drawable_ref(GDK_DRAWABLE(screen_impl->root_window));
  return screen_impl->root_window;
}

static GdkColormap *
gdk_X11_screen_get_default_colormap (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);

  return screen_impl->default_colormap;
}

static void
gdk_X11_screen_set_default_colormap (GdkScreen * screen,
				     GdkColormap * colormap)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);
  
  screen_impl->default_colormap = colormap;
}

static GdkWindow *
gdk_X11_screen_get_window_at_pointer (GdkScreen *screen,
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

Window   
gdk_x11_screen_get_root_xwindow  (GdkScreen   *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return GDK_SCREEN_IMPL_X11 (screen)->xroot_window;
}

static void
gdk_X11_screen_finalize (GObject *object)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (object);
  int i;
  g_object_unref (G_OBJECT (screen_impl->root_window));
  
  /* Visual Part (Need to implement finalize for Visuals for a clean
   * finalize) */
  /* for (i=0;i<screen_impl->nvisuals;i++)
    g_object_unref (G_OBJECT (screen_impl->visuals[i]));*/
  g_free (screen_impl->visuals);
  g_hash_table_destroy (screen_impl->visual_hash);
  /* X settings */
  g_free (screen_impl->xsettings_client);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

