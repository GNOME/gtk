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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include "gdkscreen.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay.h"
#include "gdkdisplay-x11.h"
#include "gdkx.h"
#include "gdkalias.h"

#include <X11/Xatom.h>

#ifdef HAVE_SOLARIS_XINERAMA
#include <X11/extensions/xinerama.h>
#endif
#ifdef HAVE_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

static void         gdk_screen_x11_dispose     (GObject		  *object);
static void         gdk_screen_x11_finalize    (GObject		  *object);
static void	    init_xinerama_support      (GdkScreen	  *screen);
static void	    init_randr_support	       (GdkScreen	  *screen);

enum
{
  WINDOW_MANAGER_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkScreenX11, _gdk_screen_x11, GDK_TYPE_SCREEN)

static void
_gdk_screen_x11_class_init (GdkScreenX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->dispose = gdk_screen_x11_dispose;
  object_class->finalize = gdk_screen_x11_finalize;

  signals[WINDOW_MANAGER_CHANGED] =
    g_signal_new (g_intern_static_string ("window_manager_changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkScreenX11Class, window_manager_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
_gdk_screen_x11_init (GdkScreenX11 *screen)
{
}

/**
 * gdk_screen_get_display:
 * @screen: a #GdkScreen
 *
 * Gets the display to which the @screen belongs.
 * 
 * Returns: the display to which @screen belongs
 *
 * Since: 2.2
 **/
GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_X11 (screen)->display;
}
/**
 * gdk_screen_get_width:
 * @screen: a #GdkScreen
 *
 * Gets the width of @screen in pixels
 * 
 * Returns: the width of @screen in pixels.
 *
 * Since: 2.2
 **/
gint
gdk_screen_get_width (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return WidthOfScreen (GDK_SCREEN_X11 (screen)->xscreen);
}

/**
 * gdk_screen_get_height:
 * @screen: a #GdkScreen
 *
 * Gets the height of @screen in pixels
 * 
 * Returns: the height of @screen in pixels.
 *
 * Since: 2.2
 **/
gint
gdk_screen_get_height (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return HeightOfScreen (GDK_SCREEN_X11 (screen)->xscreen);
}

/**
 * gdk_screen_get_width_mm:
 * @screen: a #GdkScreen
 *
 * Gets the width of @screen in millimeters. 
 * Note that on some X servers this value will not be correct.
 * 
 * Returns: the width of @screen in millimeters.
 *
 * Since: 2.2
 **/
gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);  

  return WidthMMOfScreen (GDK_SCREEN_X11 (screen)->xscreen);
}

/**
 * gdk_screen_get_height_mm:
 * @screen: a #GdkScreen
 *
 * Returns the height of @screen in millimeters. 
 * Note that on some X servers this value will not be correct.
 * 
 * Returns: the heigth of @screen in millimeters.
 *
 * Since: 2.2
 **/
gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return HeightMMOfScreen (GDK_SCREEN_X11 (screen)->xscreen);
}

/**
 * gdk_screen_get_number:
 * @screen: a #GdkScreen
 *
 * Gets the index of @screen among the screens in the display
 * to which it belongs. (See gdk_screen_get_display())
 * 
 * Returns: the index
 *
 * Since: 2.2
 **/
gint
gdk_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);  
  
  return GDK_SCREEN_X11 (screen)->screen_num;
}

/**
 * gdk_screen_get_root_window:
 * @screen: a #GdkScreen
 *
 * Gets the root window of @screen. 
 * 
 * Returns: the root window
 *
 * Since: 2.2
 **/
GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_X11 (screen)->root_window;
}

/**
 * gdk_screen_get_default_colormap:
 * @screen: a #GdkScreen
 *
 * Gets the default colormap for @screen.
 * 
 * Returns: the default #GdkColormap.
 *
 * Since: 2.2
 **/
GdkColormap *
gdk_screen_get_default_colormap (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_X11 (screen)->default_colormap;
}

/**
 * gdk_screen_set_default_colormap:
 * @screen: a #GdkScreen
 * @colormap: a #GdkColormap
 *
 * Sets the default @colormap for @screen.
 *
 * Since: 2.2
 **/
void
gdk_screen_set_default_colormap (GdkScreen   *screen,
				 GdkColormap *colormap)
{
  GdkColormap *old_colormap;
  
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  old_colormap = GDK_SCREEN_X11 (screen)->default_colormap;

  GDK_SCREEN_X11 (screen)->default_colormap = g_object_ref (colormap);
  
  if (old_colormap)
    g_object_unref (old_colormap);
}

static void
gdk_screen_x11_dispose (GObject *object)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (object);

  _gdk_x11_events_uninit_screen (GDK_SCREEN (object));

  if (screen_x11->default_colormap)
    {
      g_object_unref (screen_x11->default_colormap);
      screen_x11->default_colormap = NULL;
    }

  if (screen_x11->system_colormap)
    {
      g_object_unref (screen_x11->system_colormap);
      screen_x11->system_colormap = NULL;
    }

  if (screen_x11->rgba_colormap)
    {
      g_object_unref (screen_x11->rgba_colormap);
      screen_x11->rgba_colormap = NULL;
    }

  if (screen_x11->root_window)
    _gdk_window_destroy (screen_x11->root_window, TRUE);

  G_OBJECT_CLASS (_gdk_screen_x11_parent_class)->dispose (object);

  screen_x11->xdisplay = NULL;
  screen_x11->xscreen = NULL;
  screen_x11->screen_num = -1;
  screen_x11->xroot_window = None;
  screen_x11->wmspec_check_window = None;
}

static void
gdk_screen_x11_finalize (GObject *object)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (object);
  gint          i;

  if (screen_x11->root_window)
    g_object_unref (screen_x11->root_window);

  if (screen_x11->renderer)
    g_object_unref (screen_x11->renderer);

  /* Visual Part */
  for (i = 0; i < screen_x11->nvisuals; i++)
    g_object_unref (screen_x11->visuals[i]);
  g_free (screen_x11->visuals);
  g_hash_table_destroy (screen_x11->visual_hash);

  g_free (screen_x11->window_manager_name);

  g_hash_table_destroy (screen_x11->colormap_hash);

  g_free (screen_x11->monitors);

  G_OBJECT_CLASS (_gdk_screen_x11_parent_class)->finalize (object);
}

/**
 * gdk_screen_get_n_monitors:
 * @screen: a #GdkScreen.
 *
 * Returns the number of monitors which @screen consists of.
 *
 * Returns: number of monitors which @screen consists of.
 *
 * Since: 2.2
 **/
gint 
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  
  return GDK_SCREEN_X11 (screen)->num_monitors;
}

/**
 * gdk_screen_get_monitor_geometry:
 * @screen : a #GdkScreen.
 * @monitor_num: the monitor number. 
 * @dest : a #GdkRectangle to be filled with the monitor geometry
 *
 * Retrieves the #GdkRectangle representing the size and position of 
 * the individual monitor within the entire screen area.
 * 
 * Note that the size of the entire screen area can be retrieved via 
 * gdk_screen_get_width() and gdk_screen_get_height().
 *
 * Since: 2.2
 **/
void 
gdk_screen_get_monitor_geometry (GdkScreen    *screen,
				 gint          monitor_num,
				 GdkRectangle *dest)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (monitor_num < GDK_SCREEN_X11 (screen)->num_monitors);
  g_return_if_fail (monitor_num >= 0);

  *dest = GDK_SCREEN_X11 (screen)->monitors[monitor_num];
}

/**
 * gdk_screen_get_rgba_colormap:
 * @screen: a #GdkScreen.
 * 
 * Gets a colormap to use for creating windows or pixmaps with an
 * alpha channel. The windowing system on which GTK+ is running
 * may not support this capability, in which case %NULL will
 * be returned. Even if a non-%NULL value is returned, its
 * possible that the window's alpha channel won't be honored
 * when displaying the window on the screen: in particular, for
 * X an appropriate windowing manager and compositing manager
 * must be running to provide appropriate display.
 * 
 * Return value: a colormap to use for windows with an alpha channel
 *   or %NULL if the capability is not available.
 *
 * Since: 2.8
 **/
GdkColormap *
gdk_screen_get_rgba_colormap (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  screen_x11 = GDK_SCREEN_X11 (screen);

  if (!screen_x11->rgba_visual)
    return NULL;

  if (!screen_x11->rgba_colormap)
    screen_x11->rgba_colormap = gdk_colormap_new (screen_x11->rgba_visual,
						  FALSE);
  
  return screen_x11->rgba_colormap;
}

/**
 * gdk_screen_get_rgba_visual:
 * @screen: a #GdkScreen
 * 
 * Gets a visual to use for creating windows or pixmaps with an
 * alpha channel. See the docs for gdk_screen_get_rgba_colormap()
 * for caveats.
 * 
 * Return value: a visual to use for windows with an alpha channel
 *   or %NULL if the capability is not available.
 *
 * Since: 2.8
 **/
GdkVisual *
gdk_screen_get_rgba_visual (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  screen_x11 = GDK_SCREEN_X11 (screen);

  return screen_x11->rgba_visual;
}

/**
 * gdk_x11_screen_get_xscreen:
 * @screen: a #GdkScreen.
 * @returns: an Xlib <type>Screen*</type>
 *
 * Returns the screen of a #GdkScreen.
 *
 * Since: 2.2
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
 *
 * Since: 2.2
 */
int
gdk_x11_screen_get_screen_number (GdkScreen *screen)
{
  return GDK_SCREEN_X11 (screen)->screen_num;
}

static gboolean
check_is_composited (GdkDisplay *display,
		     GdkScreenX11 *screen_x11)
{
  Atom xselection = gdk_x11_atom_to_xatom_for_display (display, screen_x11->cm_selection_atom);
  Window xwindow;
  
  xwindow = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display), xselection);

  return xwindow != None;
}

static GdkAtom
make_cm_atom (int screen_number)
{
  gchar *name = g_strdup_printf ("_NET_WM_CM_S%d", screen_number);
  GdkAtom atom = gdk_atom_intern (name, FALSE);
  g_free (name);
  return atom;
}

GdkScreen *
_gdk_x11_screen_new (GdkDisplay *display,
		     gint	 screen_number) 
{
  GdkScreen *screen;
  GdkScreenX11 *screen_x11;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  screen = g_object_new (GDK_TYPE_SCREEN_X11, NULL);

  screen_x11 = GDK_SCREEN_X11 (screen);
  screen_x11->display = display;
  screen_x11->xdisplay = display_x11->xdisplay;
  screen_x11->xscreen = ScreenOfDisplay (display_x11->xdisplay, screen_number);
  screen_x11->screen_num = screen_number;
  screen_x11->xroot_window = RootWindow (display_x11->xdisplay,screen_number);
  screen_x11->wmspec_check_window = None;
  /* we want this to be always non-null */
  screen_x11->window_manager_name = g_strdup ("unknown");
  screen_x11->cm_selection_atom = make_cm_atom (screen_number);
  screen_x11->is_composited = check_is_composited (display, screen_x11);
  
  init_xinerama_support (screen);
  init_randr_support (screen);
  
  _gdk_visual_init (screen);
  _gdk_windowing_window_init (screen);
  
  return screen;
}

/**
 * gdk_screen_is_composited:
 * @screen: a #GdkScreen
 * 
 * Returns whether windows with an RGBA visual can reasonably
 * be expected to have their alpha channel drawn correctly on
 * the screen.
 *
 * On X11 this function returns whether a compositing manager is
 * compositing @screen.
 * 
 * Return value: Whether windows with RGBA visuals can reasonably be
 * expected to have their alpha channels drawn correctly on the screen.
 * 
 * Since: 2.10
 **/
gboolean
gdk_screen_is_composited (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  screen_x11 = GDK_SCREEN_X11 (screen);

  return screen_x11->is_composited;
}

#ifdef HAVE_XINERAMA
static gboolean
check_solaris_xinerama (GdkScreen *screen)
{
#ifdef HAVE_SOLARIS_XINERAMA
  
  if (XineramaGetState (GDK_SCREEN_XDISPLAY (screen),
			gdk_screen_get_number (screen)))
    {
      XRectangle monitors[MAXFRAMEBUFFERS];
      unsigned char hints[16];
      gint result;
      GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

      result = XineramaGetInfo (GDK_SCREEN_XDISPLAY (screen),
				gdk_screen_get_number (screen),
				monitors, hints,
				&screen_x11->num_monitors);
      /* Yes I know it should be Success but the current implementation 
          returns the num of monitor*/
      if (result == 0)
	{
	  /* FIXME: We need to trap errors, since XINERAMA isn't always XINERAMA.
	   */ 
	  g_error ("error while retrieving Xinerama information");
	}
      else
	{
	  int i;
	  screen_x11->monitors = g_new0 (GdkRectangle, screen_x11->num_monitors);
	  
	  for (i = 0; i < screen_x11->num_monitors; i++)
	    {
	      screen_x11->monitors[i].x = monitors[i].x;
	      screen_x11->monitors[i].y = monitors[i].y;
	      screen_x11->monitors[i].width = monitors[i].width;
	      screen_x11->monitors[i].height = monitors[i].height;
	    }

	  return TRUE;
	}
    }
#endif /* HAVE_SOLARIS_XINERAMA */
  
  return FALSE;
}

static gboolean
check_xfree_xinerama (GdkScreen *screen)
{
#ifdef HAVE_XFREE_XINERAMA
  if (XineramaIsActive (GDK_SCREEN_XDISPLAY (screen)))
    {
      GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
      XineramaScreenInfo *monitors = XineramaQueryScreens (GDK_SCREEN_XDISPLAY (screen),
							   &screen_x11->num_monitors);
      if (screen_x11->num_monitors <= 0 || monitors == NULL)
	{
	  /* If Xinerama doesn't think we have any monitors, try acting as
	   * though we had no Xinerama. If the "no monitors" condition
	   * is because XRandR 1.2 is currently switching between CRTCs,
	   * we'll be notified again when we have our monitor back,
	   * and can go back into Xinerama-ish mode at that point. */
	  if (monitors)
	    XFree (monitors);
	  return FALSE;
	}
      else
	{
	  int i;
	  screen_x11->monitors = g_new0 (GdkRectangle, screen_x11->num_monitors);
	  
	  for (i = 0; i < screen_x11->num_monitors; i++)
	    {
	      screen_x11->monitors[i].x = monitors[i].x_org;
	      screen_x11->monitors[i].y = monitors[i].y_org;
	      screen_x11->monitors[i].width = monitors[i].width;
	      screen_x11->monitors[i].height = monitors[i].height;
	    }

	  XFree (monitors);

	  return TRUE;
	}
    }
#endif /* HAVE_XFREE_XINERAMA */
  
  return FALSE;
}
#endif /* HAVE_XINERAMA */

static void
init_xinerama_support (GdkScreen * screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
#ifdef HAVE_XINERAMA
  int opcode, firstevent, firsterror;
#endif

  if (screen_x11->monitors)
    g_free (screen_x11->monitors);
  
#ifdef HAVE_XINERAMA  
  if (XQueryExtension (GDK_SCREEN_XDISPLAY (screen), "XINERAMA",
		       &opcode, &firstevent, &firsterror))
    {
      if (check_solaris_xinerama (screen) ||
	  check_xfree_xinerama (screen))
	return;
    }
#endif /* HAVE_XINERAMA */

  /* No Xinerama
   */
#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_XINERAMA)
    {
      /* Fake Xinerama mode by splitting the screen into 4 monitors.
       * Also draw a little cross to make the monitor boundaries visible.
       */
      XSetWindowAttributes atts;
      Window win;
      gint w, h;

      w = WidthOfScreen (screen_x11->xscreen);
      h = HeightOfScreen (screen_x11->xscreen);
      screen_x11->num_monitors = 4;
      screen_x11->monitors = g_new0 (GdkRectangle, 4);
      screen_x11->monitors[0].x = 0;
      screen_x11->monitors[0].y = 0;
      screen_x11->monitors[0].width = w / 2;
      screen_x11->monitors[0].height = h / 2;
      screen_x11->monitors[1].x = w / 2;
      screen_x11->monitors[1].y = 0;
      screen_x11->monitors[1].width = w / 2;
      screen_x11->monitors[1].height = h / 2;
      screen_x11->monitors[2].x = 0;
      screen_x11->monitors[2].y = h / 2;
      screen_x11->monitors[2].width = w / 2;
      screen_x11->monitors[2].height = h / 2;
      screen_x11->monitors[3].x = w / 2;
      screen_x11->monitors[3].y = h / 2;
      screen_x11->monitors[3].width = w / 2;
      screen_x11->monitors[3].height = h / 2;
      atts.override_redirect = 1;
      atts.background_pixel = WhitePixel(GDK_SCREEN_XDISPLAY (screen), 
					 screen_x11->screen_num);
      win = XCreateWindow(GDK_SCREEN_XDISPLAY (screen), 
			  screen_x11->xroot_window, 0, h / 2, w, 1, 0, 
			  DefaultDepth(GDK_SCREEN_XDISPLAY (screen), 
				       screen_x11->screen_num),
			  InputOutput, 
			  DefaultVisual(GDK_SCREEN_XDISPLAY (screen), 
					screen_x11->screen_num),
			  CWOverrideRedirect|CWBackPixel, 
			  &atts);
      XMapRaised(GDK_SCREEN_XDISPLAY (screen), win); 
      win = XCreateWindow(GDK_SCREEN_XDISPLAY (screen), 
			  screen_x11->xroot_window, w/2 , 0, 1, h, 0, 
			  DefaultDepth(GDK_SCREEN_XDISPLAY (screen), 
				       screen_x11->screen_num),
			  InputOutput, 
			  DefaultVisual(GDK_SCREEN_XDISPLAY (screen), 
					screen_x11->screen_num),
			  CWOverrideRedirect|CWBackPixel, 
			  &atts);
      XMapRaised(GDK_SCREEN_XDISPLAY (screen), win); 
    }
  else
#endif
    {
       screen_x11->num_monitors = 1;
       screen_x11->monitors = g_new0 (GdkRectangle, 1);
       screen_x11->monitors[0].x = 0;
       screen_x11->monitors[0].y = 0;
       screen_x11->monitors[0].width = WidthOfScreen (screen_x11->xscreen);
       screen_x11->monitors[0].height = HeightOfScreen (screen_x11->xscreen);
    }
}

static void
init_randr_support (GdkScreen * screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  
  XSelectInput (GDK_SCREEN_XDISPLAY (screen),
		screen_x11->xroot_window,
		StructureNotifyMask);
}

void
_gdk_x11_screen_size_changed (GdkScreen *screen,
			      XEvent    *event)
{
#ifdef HAVE_RANDR
  if (!XRRUpdateConfiguration (event))
    return;
#else
  if (event->type == ConfigureNotify)
    {
      XConfigureEvent *rcevent = (XConfigureEvent *) event;
      Screen	    *xscreen = gdk_x11_screen_get_xscreen (screen);
      
      xscreen->width   = rcevent->width;
      xscreen->height  = rcevent->height;
    }
  else
    return;
#endif
  
  init_xinerama_support (screen);
  g_signal_emit_by_name (screen, "size_changed");
}

void
_gdk_x11_screen_window_manager_changed (GdkScreen *screen)
{
  g_signal_emit (screen, signals[WINDOW_MANAGER_CHANGED], 0);
}

void
_gdk_x11_screen_process_owner_change (GdkScreen *screen,
				      XEvent *event)
{
#ifdef HAVE_XFIXES
  XFixesSelectionNotifyEvent *selection_event = (XFixesSelectionNotifyEvent *)event;
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  Atom xcm_selection_atom = gdk_x11_atom_to_xatom_for_display (screen_x11->display,
							       screen_x11->cm_selection_atom);

  if (selection_event->selection == xcm_selection_atom)
    {
      gboolean composited = selection_event->owner != None;

      if (composited != screen_x11->is_composited)
	{
	  screen_x11->is_composited = composited;

	  g_signal_emit_by_name (screen, "composited_changed");
	}
    }
#endif
}

/**
 * _gdk_windowing_substitute_screen_number:
 * @display_name : The name of a display, in the form used by 
 *                 gdk_display_open (). If %NULL a default value
 *                 will be used. On X11, this is derived from the DISPLAY
 *                 environment variable.
 * @screen_number : The number of a screen within the display
 *                  referred to by @display_name.
 *
 * Modifies a @display_name to make @screen_number the default
 * screen when the display is opened.
 *
 * Return value: a newly allocated string holding the resulting
 *   display name. Free with g_free().
 */
gchar * 
_gdk_windowing_substitute_screen_number (const gchar *display_name,
					 gint         screen_number)
{
  GString *str;
  gchar   *p;

  if (!display_name)
    display_name = getenv ("DISPLAY");

  if (!display_name)
    return NULL;

  str = g_string_new (display_name);

  p = strrchr (str->str, '.');
  if (p && p >	strchr (str->str, ':'))
    g_string_truncate (str, p - str->str);

  g_string_append_printf (str, ".%d", screen_number);

  return g_string_free (str, FALSE);
}

/**
 * gdk_screen_make_display_name:
 * @screen: a #GdkScreen
 * 
 * Determines the name to pass to gdk_display_open() to get
 * a #GdkDisplay with this screen as the default screen.
 * 
 * Return value: a newly allocated string, free with g_free()
 *
 * Since: 2.2
 **/
gchar *
gdk_screen_make_display_name (GdkScreen *screen)
{
  const gchar *old_display;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  old_display = gdk_display_get_name (gdk_screen_get_display (screen));

  return _gdk_windowing_substitute_screen_number (old_display, 
						  gdk_screen_get_number (screen));
}

/**
 * gdk_screen_get_active_window
 * @screen: a #GdkScreen
 *
 * Returns the screen's currently active window.
 *
 * On X11, this is done by inspecting the _NET_ACTIVE_WINDOW property
 * on the root window, as described in the <ulink
 * url="http://www.freedesktop.org/Standards/wm-spec">Extended Window
 * Manager Hints</ulink>. If there is no currently currently active
 * window, or the window manager does not support the
 * _NET_ACTIVE_WINDOW hint, this function returns %NULL.
 *
 * On other platforms, this function may return %NULL, depending on whether
 * it is implementable on that platform.
 *
 * The returned window should be unrefed using g_object_unref() when
 * no longer needed.
 *
 * Return value: the currently active window, or %NULL.
 *
 * Since: 2.10
 **/
GdkWindow *
gdk_screen_get_active_window (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11;
  GdkWindow *ret = NULL;
  Atom type_return;
  gint format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (!gdk_x11_screen_supports_net_wm_hint (screen,
                                            gdk_atom_intern_static_string ("_NET_ACTIVE_WINDOW")))
    return NULL;

  screen_x11 = GDK_SCREEN_X11 (screen);

  if (XGetWindowProperty (screen_x11->xdisplay, screen_x11->xroot_window,
	                  gdk_x11_get_xatom_by_name_for_display (screen_x11->display,
			                                         "_NET_ACTIVE_WINDOW"),
		          0, 1, False, XA_WINDOW, &type_return,
		          &format_return, &nitems_return,
                          &bytes_after_return, &data)
      == Success)
    {
      if ((type_return == XA_WINDOW) && (format_return == 32) && (data))
        {
          GdkNativeWindow window = *(GdkNativeWindow *) data;

          if (window != None)
            {
              ret = gdk_window_foreign_new_for_display (screen_x11->display,
                                                        *(GdkNativeWindow *) data);
            }
        }
    }

  if (data)
    XFree (data);

  return ret;
}

/**
 * gdk_screen_get_window_stack
 * @screen: a #GdkScreen
 *
 * Returns a #GList of #GdkWindow<!-- -->s representing the current
 * window stack.
 *
 * On X11, this is done by inspecting the _NET_CLIENT_LIST_STACKING
 * property on the root window, as described in the <ulink
 * url="http://www.freedesktop.org/Standards/wm-spec">Extended Window
 * Manager Hints</ulink>. If the window manager does not support the
 * _NET_CLIENT_LIST_STACKING hint, this function returns %NULL.
 *
 * On other platforms, this function may return %NULL, depending on whether
 * it is implementable on that platform.
 *
 * The returned list is newly allocated and owns references to the
 * windows it contains, so it should be freed using g_list_free() and
 * its windows unrefed using g_object_unref() when no longer needed.
 *
 * Return value: a list of #GdkWindow<!-- -->s for the current window stack,
 *               or %NULL.
 *
 * Since: 2.10
 **/
GList *
gdk_screen_get_window_stack (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11;
  GList *ret = NULL;
  Atom type_return;
  gint format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (!gdk_x11_screen_supports_net_wm_hint (screen,
                                            gdk_atom_intern_static_string ("_NET_CLIENT_LIST_STACKING")))
    return NULL;

  screen_x11 = GDK_SCREEN_X11 (screen);

  if (XGetWindowProperty (screen_x11->xdisplay, screen_x11->xroot_window,
	                  gdk_x11_get_xatom_by_name_for_display (screen_x11->display,
			                                         "_NET_CLIENT_LIST_STACKING"),
		          0, G_MAXLONG, False, XA_WINDOW, &type_return,
		          &format_return, &nitems_return,
                          &bytes_after_return, &data)
      == Success)
    {
      if ((type_return == XA_WINDOW) && (format_return == 32) &&
          (data) && (nitems_return > 0))
        {
          GdkNativeWindow *stack = (GdkNativeWindow *) data;
          GdkWindow *win;
          int i;

          for (i = 0; i < nitems_return; i++)
            {
              win = gdk_window_foreign_new_for_display (screen_x11->display,
                                                        stack[i]);

              if (win != NULL)
                ret = g_list_append (ret, win);
            }
        }
    }

  if (data)
    XFree (data);

  return ret;
}

#define __GDK_SCREEN_X11_C__
#include "gdkaliasdef.c"
