/* GDK - The GIMP Drawing Kit
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
#include "gdkinternals.h"

static void gdk_display_class_init (GdkDisplayClass *class);
static void gdk_display_init       (GdkDisplay      *display);
static void gdk_display_finalize   (GObject         *object);

static GdkDisplay *default_display = NULL;

static GObjectClass *parent_class;

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
	(GInstanceInitFunc) gdk_display_init
      };
      object_type = g_type_register_static (G_TYPE_OBJECT,
					    "GdkDisplay", &object_info, 0);
    }

  return object_type;
}

static void
gdk_display_class_init (GdkDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);

  object_class->finalize = gdk_display_finalize;
}

static void
gdk_display_init (GdkDisplay *display)
{
  _gdk_displays = g_slist_prepend (_gdk_displays, display);

  display->button_click_time[0] = display->button_click_time[1] = 0;
  display->button_window[0] = display->button_window[1] = NULL;
  display->button_number[0] = display->button_number[1] = -1;

  display->double_click_time = 250;
}

static void
gdk_display_finalize (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY_OBJECT (object);
  
  _gdk_displays = g_slist_remove (_gdk_displays, display);

  if (default_display == display)
    default_display = NULL;
  
  parent_class->finalize (object);
}

/**
 * gdk_display_get_name:
 * @display: a #GdkDisplay
 *
 * Gets the name of the display.
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
 * Gets the number of screen managed by the @display.
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
 * Returns a screen object for one of the screens of the display.
 * 
 * Returns: the #GdkScreen object
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
 * Get the default #GdkScreen for @display.
 * 
 * Returns: the default #GdkScreen object for @display
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

/**
 * gdk_set_default_display:
 * @display: a #GdkDisplay
 * 
 * Sets @display as the default display.
 **/
void
gdk_set_default_display (GdkDisplay *display)
{
  default_display = display;
}

/**
 * gdk_get_default_display:
 *
 * Gets the default #GdkDisplay. 
 * 
 * Returns: a #GdkDisplay, or %NULL if there is no default
 *   display.
 */
GdkDisplay *
gdk_get_default_display (void)
{
  return default_display;
}

/**
 * gdk_get_default_screen:
 *
 * Gets the default screen for the default display. (See
 * gdk_get_default_display ()).
 * 
 * Returns: a #GdkScreen.
 */
GdkScreen *
gdk_get_default_screen (void)
{
  return gdk_display_get_default_screen (gdk_get_default_display ());
}

/**
 * gdk_list_displays:
 *
 * List all currently open displays.
 * 
 * Return value: a newly allocated #GSList of #GdkDisplay objects.
 *  Free this list with g_slist_free() when you are done with it.
 **/
GSList *
gdk_list_displays (void)
{
  return g_slist_copy (_gdk_displays);
}

/**
 * gdk_display_get_event:
 * @display: a #GdkDisplay
 * @event: a #GdkEvent
 * 
 * Gets the next #GdkEvent to be processed for @display, fetching events from the
 * windowing system if necessary.
 * 
 * Return value: the next #GdkEvent to be processed, or %NULL if no events
 * are pending. The returned #GdkEvent should be freed with gdk_event_free().
 **/
GdkEvent*
gdk_display_get_event (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  _gdk_events_queue (display);
  return _gdk_event_unqueue (display);
}

/**
 * gdk_display_peek_event:
 * @void: 
 * 
 * Gets a copy of the first #GdkEvent in the @display's event queue, without
 * removing the event from the queue.  (Note that this function will
 * not get more events from the windowing system.  It only checks the events
 * that have already been moved to the GDK event queue.)
 * 
 * Return value: a copy of the first #GdkEvent on the event queue, or %NULL if no
 * events are in the queue. The returned #GdkEvent should be freed with
 * gdk_event_free().
 **/
GdkEvent*
gdk_display_peek_event (GdkDisplay *display)
{
  GList *tmp_list;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  tmp_list = _gdk_event_queue_find_first (display);
  
  if (tmp_list)
    return gdk_event_copy (tmp_list->data);
  else
    return NULL;
}

/**
 * gdk_display_put_event:
 * @display: a #GdkDisplay
 * @event: a #GdkEvent.
 *
 * Appends a copy of the given event onto the front of the event
 * queue for @display.
 **/
void
gdk_display_put_event (GdkDisplay *display,
		       GdkEvent   *event)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (event != NULL);

  _gdk_event_queue_append (display, gdk_event_copy (event));
}
