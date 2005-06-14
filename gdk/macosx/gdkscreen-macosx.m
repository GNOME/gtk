/*
 * gdkscreen-macosx.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 * Copyright 2005 Hubert Figuiere
 *
 * Hubert Figuiere <hub@figuiere.net>
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

#include <Cocoa/Cocoa.h>

#include <glib.h>
#include "gdkmacosx.h"
#include "gdkalias.h"
#include "gdkscreen.h"
#include "gdkscreen-macosx.h"
#include "gdkdisplay.h"
#include "gdkdisplay-macosx.h"


static void         gdk_screen_macosx_class_init  (GdkScreenMacOSXClass *klass);
static void         gdk_screen_macosx_dispose     (GObject		  *object);
static void         gdk_screen_macosx_finalize    (GObject		  *object);
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
_gdk_screen_macosx_get_type (void)
{
	static GType object_type = 0;
	
	if (!object_type)
    {
		static const GTypeInfo object_info =
			{
				sizeof (GdkScreenMacOSXClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gdk_screen_macosx_class_init,
				NULL,			/* class_finalize */
				NULL,			/* class_data */
				sizeof (GdkScreenMacOSX),
				0,			/* n_preallocs */
				(GInstanceInitFunc) NULL,
			};
		object_type = g_type_register_static (GDK_TYPE_SCREEN,
											  "GdkScreenMacOSX",
											  &object_info, 0);
    }
	return object_type;
}

static void
gdk_screen_macosx_class_init (GdkScreenMacOSXClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->dispose = gdk_screen_macosx_dispose;
	object_class->finalize = gdk_screen_macosx_finalize;
	
	parent_class = g_type_class_peek_parent (klass);
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
	
	return GDK_SCREEN_MACOSX (screen)->display;
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
	NSArray *all_screens;
	NSRect frame;
	NSScreen *ns_screen;
	NSEnumerator *iter;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

	frame = NSMakeRect(0,0,0,0);

	all_screens = [NSScreen screens];
	iter = [all_screens objectEnumerator];

	while(ns_screen = [iter nextObject])
	{
		frame = NSUnionRect(frame, [ns_screen frame]);
	}

	return frame.size.width;
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
	NSArray *all_screens;
	NSRect frame;
	NSScreen *ns_screen;
	NSEnumerator *iter;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

	frame = NSMakeRect(0,0,0,0);

	all_screens = [NSScreen screens];
	iter = [all_screens objectEnumerator];

	while(ns_screen = [iter nextObject])
	{
		frame = NSUnionRect(frame, [ns_screen frame]);
	}
	
	return frame.size.height;
}


static NSSize
_gdk_screen_get_dpi(GdkScreen *screen)
{
	NSSize s;
	NSScreen *ns_screen = [NSScreen mainScreen];

	NSDictionary *props = [ns_screen deviceDescription];
	
	s = [[props objectForKey:NSDeviceResolution] size];
	
	return s;
}
/*
@"NSScreenNumber"
*/

/**
 * gdk_screen_get_width_mm:
 * @screen: a #GdkScreen
 *
 * Gets the width of @screen in millimeters. 
 * Note that on some X servers this value will not be correct.
 * 
 * Returns: the width of @screen in pixels.
 *
 * Since: 2.2
 **/
gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
	NSSize s;
	g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);  

	s = _gdk_screen_get_dpi(screen);
	return gdk_screen_get_width(screen) / (s.width / 25.4);
}

/**
 * gdk_screen_get_height_mm:
 * @screen: a #GdkScreen
 *
 * Returns the height of @screen in millimeters. 
 * Note that on some X servers this value will not be correct.
 * 
 * Returns: the heigth of @screen in pixels.
 *
 * Since: 2.2
 **/
gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
	NSSize s;
	g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

	s = _gdk_screen_get_dpi(screen);
	return gdk_screen_get_height(screen) / (s.height / 25.4);
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

	return 0;
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

	// FIXME
	
	return NULL;
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

  return GDK_SCREEN_MACOSX (screen)->default_colormap;
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
	
	old_colormap = GDK_SCREEN_MACOSX (screen)->default_colormap;
	
	GDK_SCREEN_MACOSX (screen)->default_colormap = g_object_ref (colormap);
	
	if (old_colormap)
		g_object_unref (old_colormap);
}

static void
gdk_screen_macosx_dispose (GObject *object)
{
	GdkScreenMacOSX *screen_macosx = GDK_SCREEN_MACOSX (object);
	
	//_gdk_macosx_events_uninit_screen (GDK_SCREEN (object));
	
	g_object_unref (screen_macosx->default_colormap);
	screen_macosx->default_colormap = NULL;
	
	screen_macosx->root_window = NULL;
	
	screen_macosx->display = NULL;
	
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gdk_screen_macosx_finalize (GObject *object)
{
	GdkScreenMacOSX *screen_macosx = GDK_SCREEN_MACOSX (object);
	
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
	return [[NSScreen screens] count];
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
	NSRect rect;

	g_return_if_fail (GDK_IS_SCREEN (screen));
/*	g_return_if_fail (monitor_num < GDK_SCREEN_MACOSX (screen)->num_monitors);
  g_return_if_fail (monitor_num >= 0);*/

	rect = [[[NSScreen screens] objectAtIndex:monitor_num] frame];

	*dest = ns_to_gdkrect(rect);
}



GdkScreen *
_gdk_macosx_screen_new (GdkDisplay *display,
						NSScreen	  *screen) 
{
	GdkScreen *gscreen;
	GdkScreenMacOSX *screen_macosx;
	GdkDisplayMacOSX *display_macosx = GDK_DISPLAY_MACOSX (display);
	
	gscreen = g_object_new (GDK_TYPE_SCREEN_MACOSX, NULL);
	
	screen_macosx = GDK_SCREEN_MACOSX (gscreen);
	screen_macosx->display = display;
	screen_macosx->default_colormap = NULL;
//	screen_macosx->xroot_window = RootWindow (display_macosx->xdisplay,screen_number);
	
	_gdk_visual_init (gscreen);
	_gdk_windowing_window_init (gscreen);
	
	return gscreen;
}



/**
 * _gdk_windowing_substitute_screen_number:
 * @display_name : The name of a display, in the form used by 
 *                 gdk_display_open (). If %NULL a default value
 *                 will be used. On MacOSX, this is derived from the DISPLAY
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
	return g_strdup (display_name);
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
	return g_strdup ("");
}
