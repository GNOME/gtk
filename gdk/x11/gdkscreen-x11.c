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

#ifdef HAVE_SOLARIS_XINERAMA
#include <X11/extensions/xinerama.h>
#endif
#ifdef HAVE_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

static void         gdk_screen_x11_class_init  (GdkScreenX11Class *klass);
static void         gdk_screen_x11_dispose     (GObject		  *object);
static void         gdk_screen_x11_finalize    (GObject		  *object);
static void	    init_xinerama_support      (GdkScreen	  *screen);
static void	    init_randr_support	       (GdkScreen	  *screen);

enum
{
  WINDOW_MANAGER_CHANGED,
  LAST_SIGNAL
};

static gpointer parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GType
_gdk_screen_x11_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (GdkScreenX11Class),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
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

static void
gdk_screen_x11_class_init (GdkScreenX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->dispose = gdk_screen_x11_dispose;
  object_class->finalize = gdk_screen_x11_finalize;

  parent_class = g_type_class_peek_parent (klass);

  signals[WINDOW_MANAGER_CHANGED] =
    g_signal_new ("window_manager_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkScreenX11Class, window_manager_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
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

  g_object_unref (screen_x11->default_colormap);
  screen_x11->default_colormap = NULL;
  
  screen_x11->root_window = NULL;

  screen_x11->xdisplay = NULL;
  screen_x11->xscreen = NULL;
  screen_x11->screen_num = -1;
  screen_x11->xroot_window = None;
  screen_x11->wmspec_check_window = None;
  
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gdk_screen_x11_finalize (GObject *object)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (object);
  /* int i; */
  g_object_unref (screen_x11->root_window);

  if (screen_x11->renderer)
    g_object_unref (screen_x11->renderer);
  
  /* Visual Part (Need to implement finalize for Visuals for a clean
   * finalize) */
  /* for (i=0;i<screen_x11->nvisuals;i++)
    g_object_unref (screen_x11->visuals[i]);*/
  g_free (screen_x11->visuals);
  g_hash_table_destroy (screen_x11->visual_hash);

  g_free (screen_x11->window_manager_name);  

  g_hash_table_destroy (screen_x11->colormap_hash);
  /* X settings */
  g_free (screen_x11->xsettings_client);
  g_free (screen_x11->monitors);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
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
  
  init_xinerama_support (screen);
  init_randr_support (screen);
  
  _gdk_visual_init (screen);
  _gdk_windowing_window_init (screen);

  return screen;
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
      if (screen_x11->num_monitors <= 0)
	{
	  /* FIXME: We need to trap errors, since XINERAMA isn't always XINERAMA.
	   *        I don't think the num_monitors <= 0 check has any validity.
	   */ 
	  g_error ("error while retrieving Xinerama information");
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

#define __GDK_SCREEN_X11_C__
#include "gdkaliasdef.c"
