/*
 * gdkdisplay.c
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
#include "gdkdisplay.h"
#include "gdkdisplaymgr.h"
#include "gdkinternals.h"


static void gdk_display_class_init (GdkDisplayClass * klass);
static gpointer parent_class = NULL;

GType
gdk_display_get_type (void)
{

  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
	sizeof (GdkDisplayClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gdk_display_class_init,
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (GdkDisplay),
	0,			/* n_preallocs */
	(GInstanceInitFunc) NULL,
      };
      object_type = g_type_register_static (G_TYPE_OBJECT,
					    "GdkDisplay", &object_info, 0);
    }

  return object_type;
}


static void
gdk_display_class_init (GdkDisplayClass * klass)
{
  parent_class = g_type_class_peek_parent (klass);
}

/**
 * gdk_display_new:
 * @argc : Address of the argc parameter of your main function. 
 *         Changed if any arguments were handled. 
 * @argv : Address of the argv parameter of main. Any parameters 
 *	   understood by gdk_display_new are stripped before return. 
 * @display_name:  the X server connection name 
 * ie. "<hostname>:<XserverNumber>.<ScreenNumber>"
 *
 * Attempts to create and initialize a new connection to 
 * the X server @display_name
 * 
 * Returns: a #GdkDisplay if successful, NULL otherwise.
 */
GdkDisplay *
gdk_display_new (int argc, char **argv, char *display_name)
{
  GdkDisplay *display = NULL;
  GdkScreen *screen = NULL;
  
  display = _gdk_windowing_init_check_for_display (argc,argv,display_name);
  if (!display)
    return NULL;
  
  screen = gdk_display_get_default_screen (display);
  
  _gdk_visual_init (screen);
  _gdk_windowing_window_init (screen);
  _gdk_windowing_image_init (display);
  _gdk_events_init (display);
  _gdk_input_init (display);
  _gdk_dnd_init (display);
  
  return display;
}

/**
 * gdk_display_get_name:
 * @display: a #GdkDisplay
 *
 * Returns a display name.
 * 
 * Returns: a string representing the display name.
 */
G_CONST_RETURN gchar *
gdk_display_get_name (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return GDK_DISPLAY_GET_CLASS (display)->get_display_name (display);
}

/**
 * gdk_display_get_n_screens:
 * @display: a #GdkDisplay
 *
 * Returns the number of screen managed by the @display.
 * 
 * Returns: number of screens.
 */

gint
gdk_display_get_n_screens (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  return GDK_DISPLAY_GET_CLASS (display)->get_n_screens (display);
}

/**
 * gdk_display_get_screen:
 * @display: a #GdkDisplay
 * @screen_num: the screen number
 *
 * Initializes a #GdkScreen for the @display
 * 
 * Returns: a #GdkScreen.
 */

GdkScreen *
gdk_display_get_screen (GdkDisplay * display, gint screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return GDK_DISPLAY_GET_CLASS (display)->get_screen (display, screen_num);
}

/**
 * gdk_display_get_default_screen:
 * @display: a #GdkDisplay
 *
 * Returns the default #GdkScreen for @display
 * 
 * Returns: a #GdkScreen.
 */

GdkScreen *
gdk_display_get_default_screen (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return GDK_DISPLAY_GET_CLASS (display)->get_default_screen (display);
}

/**
 * gdk_display_close:
 * @display: a #GdkDisplay
 *
 * Closes and cleanup the resources used by the @display
 */

void
gdk_display_close (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_object_unref (G_OBJECT (display));
}
