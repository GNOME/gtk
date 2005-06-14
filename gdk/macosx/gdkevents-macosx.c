/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2005 Hubert Figuiere
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */
/*
 * MacOS X port by Hubert Figuiere
 */


#include <config.h>

#include "gdk.h"
#include "gdkinternals.h"

#include <Carbon/Carbon.h>



/*
 * Currently does nothing
 */
void
gdk_flush (void)
{

}



/**
 * gdk_event_send_client_message_for_display:
 * @display: the #GdkDisplay for the window where the message is to be sent.
 * @event: the #GdkEvent to send, which should be a #GdkEventClient.
 * @winid: the window to send the client message to.
 *
 * On X11, sends an X ClientMessage event to a given window. On
 * Windows, sends a message registered with the name
 * GDK_WIN32_CLIENT_MESSAGE.
 *
 * This could be used for communicating between different
 * applications, though the amount of data is limited to 20 bytes on
 * X11, and to just four bytes on Windows.
 *
 * Returns: non-zero on success.
 *
 * Since: 2.2
 */
gboolean
gdk_event_send_client_message_for_display (GdkDisplay     *display,
										   GdkEvent       *event,
										   GdkNativeWindow winid)
{
	/* FIXME */
	g_assert(false);
	return false;
}


void
_gdk_events_queue (GdkDisplay *display)
{
	while (!_gdk_event_queue_find_first(display))
	{
		/* FIXME */
	}
}
