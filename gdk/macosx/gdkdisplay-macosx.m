/* GDK - The GIMP Drawing Kit
 * gdkdisplay-macosx.c
 * 
 * Copyright 2001 Sun Microsystems Inc.
 * Copyright (C) 2004 Nokia Corporation
 * Copyright (C) 2005 Hubert Figuiere
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

/*
 * derived from GdkDisplay-X11....
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <glib.h>
#include "gdkalias.h"
#include "gdkdisplay.h"
#include "gdkdisplay-macosx.h"
#include "gdkscreen.h"
#include "gdkscreen-macosx.h"
#include "gdkinternals.h"

#include <Cocoa/Cocoa.h>

static void                 gdk_display_macosx_class_init         (GdkDisplayMacOSXClass *class);
static void                 gdk_display_macosx_dispose            (GObject            *object);
static void                 gdk_display_macosx_finalize           (GObject            *object);


static gpointer parent_class = NULL;


GType
_gdk_display_macosx_get_type (void)
{
	static GType object_type = 0;

	if (!object_type)
    {
		static const GTypeInfo object_info =
			{
				sizeof (GdkDisplayMacOSXClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gdk_display_macosx_class_init,
				NULL,			/* class_finalize */
				NULL,			/* class_data */
				sizeof (GdkDisplayMacOSX),
				0,			/* n_preallocs */
				(GInstanceInitFunc) NULL,
			};
		
		object_type = g_type_register_static (GDK_TYPE_DISPLAY,
											  "GdkDisplayMacOSX",
											  &object_info, 0);
    }
	
	return object_type;
}

static void
gdk_display_macosx_class_init (GdkDisplayMacOSXClass * class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
  
	object_class->dispose = gdk_display_macosx_dispose;
	object_class->finalize = gdk_display_macosx_finalize;
	
	parent_class = g_type_class_peek_parent (class);
}


/**
 * gdk_display_open:
 * @display_name: the name of the display to open
 * @returns: a #GdkDisplay, or %NULL if the display
 *  could not be opened.
 *
 * Opens a display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_open (const gchar *display_name)
{
	GdkDisplay *display;
	GdkDisplayMacOSX *display_macosx;
	GdkWindowAttr attr;
	NSArray *screens;
	NSScreen *current_screen;
	NSScreen *default_screen;
	NSEnumerator *iter;
	int screen_count, i;
	
	display = g_object_new (GDK_TYPE_DISPLAY_MACOSX, NULL);
	display_macosx = GDK_DISPLAY_MACOSX (display);
	
	/* initialize the display's screens */ 
	
	screens = [NSScreen screens];
	screen_count = [screens count];

	display_macosx->screens = g_new (GdkScreen *, screen_count);
	default_screen = [NSScreen mainScreen];
	iter = [screens objectEnumerator];
	i = 0;
	while (current_screen = [iter nextObject]) {
		display_macosx->screens[i] = _gdk_macosx_screen_new (display, current_screen);
		if (current_screen == default_screen) {
			display_macosx->default_screen = display_macosx->screens[i];
		}
		i++;
		if(i == screen_count) {
			g_warning("screen count changes\n");
			/* FIXME don't leake */
			return NULL;
		}
	}
	display_macosx->num_screens = i;
	display_macosx->nsview_ht = NULL;

	/* just make sure we have a NSApplication created */
	[NSApplication sharedApplication];
	
	/* FIXME make all of this is OK */
	/*
	_gdk_windowing_image_init (display);
	_gdk_events_init (display);
	_gdk_input_init (display);
	_gdk_dnd_init (display);
	*/
	g_signal_emit_by_name (gdk_display_manager_get(),
						   "display_opened", display);

	return display;
}


/**
 * gdk_display_get_name:
 * @display: a #GdkDisplay
 *
 * Gets the name of the display.
 * 
 * Returns: a string representing the display name. This string is owned
 * by GDK and should not be modified or freed.
 * 
 * Since: 2.2
 */
G_CONST_RETURN gchar *
gdk_display_get_name (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  return "";
}

/**
 * gdk_display_get_n_screens:
 * @display: a #GdkDisplay
 *
 * Gets the number of screen managed by the @display.
 * 
 * Returns: number of screens.
 * 
 * Since: 2.2
 */
gint
gdk_display_get_n_screens (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  
  return [[NSScreen screens] count];
}

/**
 * gdk_display_get_screen:
 * @display: a #GdkDisplay
 * @screen_num: the screen number
 *
 * Returns a screen object for one of the screens of the display.
 *
 * Returns: the #GdkScreen object
 *
 * Since: 2.2
 */
GdkScreen *
gdk_display_get_screen (GdkDisplay * display, gint screen_num)
{
	g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
	g_return_val_if_fail ((GDK_DISPLAY_MACOSX (display)->num_screens) > screen_num, NULL);
	
	return GDK_DISPLAY_MACOSX (display)->screens[screen_num];
}

/**
 * gdk_display_get_default_screen:
 * @display: a #GdkDisplay
 *
 * Get the default #GdkScreen for @display.
 * 
 * Returns: the default #GdkScreen object for @display
 *
 * Since: 2.2
 */
GdkScreen *
gdk_display_get_default_screen (GdkDisplay * display)
{
	g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
	
	return GDK_DISPLAY_MACOSX (display)->default_screen;
}


/**
 * gdk_display_pointer_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g. GDK_CURRENT_TIME).
 *
 * Release any pointer grab.
 *
 * Since: 2.2
 */
void
gdk_display_pointer_ungrab (GdkDisplay *display,
			    guint32     time)
{
	// FIXME nothing to do
}

/**
 * gdk_display_pointer_is_grabbed:
 * @display: a #GdkDisplay
 *
 * Test if the pointer is grabbed.
 *
 * Returns: %TRUE if an active X pointer grab is in effect
 *
 * Since: 2.2
 */
gboolean
gdk_display_pointer_is_grabbed (GdkDisplay * display)
{
	// FIXME I don't think we have grabs. Maybe should we 
	// store that state in the GdkDisplay....
	return false;
}

/**
 * gdk_display_keyboard_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g #GDK_CURRENT_TIME).
 *
 * Release any keyboard grab
 *
 * Since: 2.2
 */
void
gdk_display_keyboard_ungrab (GdkDisplay *display,
			     guint32     time)
{

}

/**
 * gdk_display_beep:
 * @display: a #GdkDisplay
 *
 * Emits a short beep on @display
 *
 * Since: 2.2
 */
void
gdk_display_beep (GdkDisplay * display)
{
	g_return_if_fail (GDK_IS_DISPLAY (display));

	NSBeep();
}

/**
 * gdk_display_sync:
 * @display: a #GdkDisplay
 *
 * Flushes any requests queued for the windowing system and waits until all
 * requests have been handled. This is often used for making sure that the
 * display is synchronized with the current state of the program. Calling
 * gdk_display_sync() before gdk_error_trap_pop() makes sure that any errors
 * generated from earlier requests are handled before the error trap is 
 * removed.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 *
 * Since: 2.2
 */
void
gdk_display_sync (GdkDisplay * display)
{
	g_return_if_fail (GDK_IS_DISPLAY (display));
	/* FIXME see if we can do something with CoreGraphics */
}

/**
 * gdk_display_flush:
 * @display: a #GdkDisplay
 *
 * Flushes any requests queued for the windowing system; this happens automatically
 * when the main loop blocks waiting for new events, but if your application
 * is drawing without returning control to the main loop, you may need
 * to call this function explicitely. A common case where this function
 * needs to be called is when an application is executing drawing commands
 * from a thread other than the thread where the main loop is running.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 *
 * Since: 2.4
 */
void 
gdk_display_flush (GdkDisplay *display)
{
	g_return_if_fail (GDK_IS_DISPLAY (display));

	/* FIXME see if we can do something with CoreGraphics */
}

/**
 * gdk_display_get_default_group:
 * @display: a #GdkDisplay
 * 
 * Returns the default group leader window for all toplevel windows
 * on @display. This window is implicitly created by GDK. 
 * See gdk_window_set_group().
 * 
 * Return value: The default group leader window for @display
 *
 * Since: 2.4
 **/
GdkWindow *gdk_display_get_default_group (GdkDisplay *display)
{
	g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

	g_warning ("gdk_display_get_default_group not yet implemented");
	
	return NULL;
}

/**
 * gdk_macosx_display_grab:
 * @display: a #GdkDisplay 
 * 
 * Call XGrabServer() on @display. 
 * To ungrab the display again, use gdk_macosx_display_ungrab(). 
 *
 * gdk_macosx_display_grab()/gdk_macosx_display_ungrab() calls can be nested.
 *
 * Since: 2.2
 **/
void
gdk_macosx_display_grab (GdkDisplay * display)
{

}

/**
 * gdk_macosx_display_ungrab:
 * @display: a #GdkDisplay
 * 
 * Ungrab @display after it has been grabbed with 
 * gdk_macosx_display_grab(). 
 *
 * Since: 2.2
 **/
void
gdk_macosx_display_ungrab (GdkDisplay * display)
{

}

static void
gdk_display_macosx_dispose (GObject *object)
{
	GdkDisplayMacOSX *display_macosx;
	gint i;
	
	display_macosx = GDK_DISPLAY_MACOSX (object);
	
	for (i = 0; i < display_macosx->num_screens; i++) {
		_gdk_screen_close (display_macosx->screens[i]);
	}
	
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gdk_display_macosx_finalize (GObject *object)
{
	GdkDisplayMacOSX *display_macosx = GDK_DISPLAY_MACOSX (object);
	
	g_free (display_macosx->screens);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
	/* FIXME */
	g_warning ("_gdk_windowing_set_default_display() not implemented on MacOS X\n");
}



/**
 * gdk_notify_startup_complete:
 * 
 * Indicates to the GUI environment that the application has finished
 * loading. If the applications opens windows, this function is
 * normally called after opening the application's initial set of
 * windows.
 * 
 * GTK+ will call this function automatically after opening the first
 * #GtkWindow unless gtk_window_set_auto_startup_notification() is called 
 * to disable that feature.
 *
 * Since: 2.2
 **/
void
gdk_notify_startup_complete (void)
{
	/* FIXME */
	g_warning ("gdk_notify_startup_complete () not implemented on MacOS X\n");
}


/**
 * gdk_display_supports_selection_notification:
 * @display: a #GdkDisplay
 * 
 * Returns whether #GdkEventOwnerChange events will be 
 * sent when the owner of a selection changes.
 * 
 * Return value: whether #GdkEventOwnerChange events will 
 *               be sent.
 *
 * Since: 2.6
 **/
gboolean 
gdk_display_supports_selection_notification (GdkDisplay *display)
{
	/* FIXME I'm not sure we support that */

	return FALSE;
}

/**
 * gdk_display_request_selection_notification:
 * @display: a #GdkDisplay
 * @selection: the #GdkAtom naming the selection for which
 *             ownership change notification is requested
 * 
 * Request #GdkEventOwnerChange events for ownership changes
 * of the selection named by the given atom.
 * 
 * Return value: whether #GdkEventOwnerChange events will 
 *               be sent.
 *
 * Since: 2.6
 **/
gboolean gdk_display_request_selection_notification  (GdkDisplay *display,
						      GdkAtom     selection)

{
	/* FIXME I'm not sure we support that */
	
    return FALSE;
}

/**
 * gdk_display_supports_clipboard_persistence
 * @display: a #GdkDisplay
 *
 * Returns whether the speicifed display supports clipboard
 * persistance; i.e. if it's possible to store the clipboard data after an
 * application has quit. On MacOSX this checks if a clipboard daemon is
 * running.
 *
 * Returns: %TRUE if the display supports clipboard persistance.
 *
 * Since: 2.6
 */
gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
	/* It might make sense to cache this */
	return TRUE;
}

/**
 * gdk_display_store_clipboard
 * @display:          a #GdkDisplay
 * @clipboard_window: a #GdkWindow belonging to the clipboard owner
 * @time_:            a timestamp
 * @targets:	      an array of targets that should be saved, or %NULL 
 *                    if all available targets should be saved.
 * @n_targets:        length of the @targets array
 *
 * Issues a request to the clipboard manager to store the
 * clipboard data.
 *
 * Since: 2.6
 */
void
gdk_display_store_clipboard (GdkDisplay *display,
			     GdkWindow  *clipboard_window,
			     guint32     time_,
			     GdkAtom    *targets,
			     gint        n_targets)
{
	/* FIXME */
	g_warning ("gdk_display_store_clipboard() not implemented on MacOS X\n");
}

