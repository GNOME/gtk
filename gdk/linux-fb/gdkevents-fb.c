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

#include "gdk.h"
#include "gdkprivate-fb.h"
#include "gdkinternals.h"
#include "gdkfb.h"

#include "gdkkeysyms.h"

#if HAVE_CONFIG_H
#  include <config.h>
#  if STDC_HEADERS
#    include <string.h>
#  endif
#endif

typedef struct _GdkIOClosure GdkIOClosure;
typedef struct _GdkEventPrivate GdkEventPrivate;

#define DOUBLE_CLICK_TIME      250
#define TRIPLE_CLICK_TIME      500
#define DOUBLE_CLICK_DIST      5
#define TRIPLE_CLICK_DIST      5

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0
} GdkEventFlags;

struct _GdkIOClosure
{
  GdkInputFunction function;
  GdkInputCondition condition;
  GdkDestroyNotify notify;
  gpointer data;
};

struct _GdkEventPrivate
{
  GdkEvent event;
  guint    flags;
};

/* 
 * Private function declarations
 */

/* Private variable declarations
 */

#if 0
static GList *client_filters;	            /* Filters for client messages */

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  (GDestroyNotify)g_free
};
#endif

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

static gboolean fb_events_prepare  (gpointer  source_data,
				    GTimeVal *current_time,
				    gint     *timeout,
				    gpointer  user_data);
static gboolean fb_events_check    (gpointer  source_data,
				    GTimeVal *current_time,
				    gpointer  user_data);
static gboolean fb_events_dispatch (gpointer  source_data,
				    GTimeVal *dispatch_time,
				    gpointer  user_data);

void 
gdk_events_init (void)
{
  static GSourceFuncs fb_events_funcs = {
    fb_events_prepare,
    fb_events_check,
    fb_events_dispatch,
    NULL
  };

  g_source_add (GDK_PRIORITY_EVENTS, TRUE, &fb_events_funcs, NULL, NULL, NULL);
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
  return gdk_event_queue_find_first () ? TRUE : FALSE;
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  GList *ltmp;
  g_return_val_if_fail (window != NULL, NULL);
  
  for (ltmp = gdk_queued_events; ltmp; ltmp = ltmp->next)
    {
      GdkEvent *event = ltmp->data;
      if (event->type == GDK_EXPOSE &&
	  event->expose.window == window)
	break;
    }

  if (ltmp)
    {
      GdkEvent *retval = ltmp->data;

      gdk_event_queue_remove_link (ltmp);
      g_list_free_1 (ltmp);

      return retval;
    }

  return NULL;
}

void
gdk_events_queue (void)
{  
}

static gboolean
fb_events_prepare (gpointer  source_data, 
		   GTimeVal *current_time,
		   gint     *timeout,
		   gpointer  user_data)
{
  *timeout = -1;

  return fb_events_check (source_data, current_time, user_data);
}

static gboolean
fb_events_check (gpointer  source_data,
		 GTimeVal *current_time,
		 gpointer  user_data)
{
  gboolean retval;

  GDK_THREADS_ENTER ();

  retval = (gdk_event_queue_find_first () != NULL);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
fb_events_dispatch (gpointer source_data,
		    GTimeVal *dispatch_time,
		    gpointer user_data)
{
  GdkEvent *event;

  GDK_THREADS_ENTER ();

  while ((event = gdk_event_unqueue ()))
    {
      if (event->type == GDK_EXPOSE &&
	  event->expose.window == gdk_parent_root)
	gdk_window_clear_area (event->expose.window,
			       event->expose.area.x,
			       event->expose.area.y,
			       event->expose.area.width,
			       event->expose.area.height);

      else if (gdk_event_func)
	(*gdk_event_func) (event, gdk_event_data);

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
gdk_event_send_client_message (GdkEvent *event, guint32 xid)
{
  return FALSE;
}

void
gdk_event_send_clientmessage_toall (GdkEvent *sev)
{
}
