/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <config.h>
#include "gdk.h"
#include "gdkprivate-fb.h"
#include "gdkinternals.h"
#include "gdkfb.h"

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

static gboolean fb_events_prepare  (GSource    *source,
				    gint       *timeout);
static gboolean fb_events_check    (GSource    *source);
static gboolean fb_events_dispatch (GSource    *source,
				    GSourceFunc callback,
				    gpointer    user_data);

static GSourceFuncs fb_events_funcs = {
  fb_events_prepare,
  fb_events_check,
  fb_events_dispatch,
  NULL
};

guint32
gdk_fb_get_time(void)
{
  GTimeVal tv;

  g_get_current_time (&tv);
  return (guint32) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void 
_gdk_events_init (void)
{
  GSource *source;

  source = g_source_new (&fb_events_funcs, sizeof (GSource));
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);
}

/*
 *--------------------------------------------------------------
 * gdk_events_pending
 *
 *   Returns if events are pending on the queue.
 *
 * Arguments:
 *
 * Results:
 *   Returns TRUE if events are pending
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gboolean
gdk_events_pending (void)
{
  return fb_events_check (NULL);
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  GdkDisplay *display = gdk_display_get_default ();
  GList *ltmp;
  g_return_val_if_fail (window != NULL, NULL);
  
  for (ltmp = display->queued_events; ltmp; ltmp = ltmp->next)
    {
      GdkEvent *event = ltmp->data;
      if (event->type == GDK_EXPOSE &&
	  event->expose.window == window)
	break;
    }

  if (ltmp)
    {
      GdkEvent *retval = ltmp->data;

      _gdk_event_queue_remove_link (display, ltmp);
      g_list_free_1 (ltmp);

      return retval;
    }

  return NULL;
}

void
_gdk_events_queue (GdkDisplay *display)
{  
}

static gboolean
fb_events_prepare (GSource    *source,
		   gint       *timeout)
{
  *timeout = -1;

  return fb_events_check (source);
}

static gboolean
fb_events_check (GSource    *source)
{
  gboolean retval;

  GDK_THREADS_ENTER ();

  retval = (_gdk_event_queue_find_first (gdk_display_get_default ()) != NULL);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
fb_events_dispatch (GSource  *source,
		    GSourceFunc callback,
		    gpointer  user_data)
{
  GdkEvent *event;

  GDK_THREADS_ENTER ();

  while ((event = _gdk_event_unqueue (gdk_display_get_default ())))
    {
      if (event->type == GDK_EXPOSE &&
	  event->expose.window == _gdk_parent_root)
	gdk_window_clear_area (event->expose.window,
			       event->expose.area.x,
			       event->expose.area.y,
			       event->expose.area.width,
			       event->expose.area.height);

      else if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);

      gdk_event_free (event);
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}

/*
 *--------------------------------------------------------------
 * gdk_flush
 *
 *   Flushes the Xlib output buffer and then waits
 *   until all requests have been received and processed
 *   by the X server. The only real use for this function
 *   is in dealing with XShm.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_flush (void)
{
}

gboolean
gdk_event_send_client_message_for_display (GdkDisplay      *display,
					   GdkEvent        *event,
					   GdkNativeWindow  winid)
{
  return FALSE;
}

void
gdk_screen_broadcast_client_message (GdkScreen *screen,
				     GdkEvent  *sev)
{
}

gboolean
gdk_screen_get_setting (GdkScreen   *screen,
			const gchar *name,
			GValue      *value)
{
  return FALSE;
}

void
gdk_display_sync (GdkDisplay *display)
{
}

void
gdk_display_flush (GdkDisplay * display)
{
}
