/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdk.h"
#include "gdkx.h"
#include "gdkprivate.h"
#include "gdkkeysyms.h"

#if HAVE_CONFIG_H
#  include <config.h>
#  if STDC_HEADERS
#    include <string.h>
#  endif
#endif

#include "gdkinput.h"

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

static GdkEvent *gdk_event_new		(void);
static gint	 gdk_event_apply_filters (XEvent   *xevent,
					  GdkEvent *event,
					  GList    *filters);
static gint	 gdk_event_translate	 (GdkEvent *event, 
					  XEvent   *xevent);
#if 0
static Bool	 gdk_event_get_type	(Display   *display, 
					 XEvent	   *xevent, 
					 XPointer   arg);
#endif
static void      gdk_events_queue       (void);
static GdkEvent* gdk_event_unqueue      (void);

static gboolean  gdk_event_prepare      (gpointer   source_data, 
				 	 GTimeVal  *current_time,
					 gint      *timeout,
					 gpointer   user_data);
static gboolean  gdk_event_check        (gpointer   source_data,
				 	 GTimeVal  *current_time,
					 gpointer   user_data);
static gboolean  gdk_event_dispatch     (gpointer   source_data,
					 GTimeVal  *current_time,
					 gpointer   user_data);

static void	 gdk_synthesize_click	(GdkEvent  *event, 
					 gint	    nclicks);

GdkFilterReturn gdk_wm_protocols_filter (GdkXEvent *xev,
					 GdkEvent  *event,
					 gpointer   data);

/* Private variable declarations
 */

static int connection_number = 0;	    /* The file descriptor number of our
					     *	connection to the X server. This
					     *	is used so that we may determine
					     *	when events are pending by using
					     *	the "select" system call.
					     */
static guint32 button_click_time[2];	    /* The last 2 button click times. Used
					     *	to determine if the latest button click
					     *	is part of a double or triple click.
					     */
static GdkWindow *button_window[2];	    /* The last 2 windows to receive button presses.
					     *	Also used to determine if the latest button
					     *	click is part of a double or triple click.
					     */
static guint button_number[2];		    /* The last 2 buttons to be pressed.
					     */
static GdkEventFunc   event_func = NULL;    /* Callback for events */
static gpointer       event_data = NULL;
static GDestroyNotify event_notify = NULL;

static GList *client_filters;	            /* Filters for client messages */

/* FIFO's for event queue, and for events put back using
 * gdk_event_put().
 */
static GList *queued_events = NULL;
static GList *queued_tail = NULL;

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  (GDestroyNotify)g_free
};

GPollFD event_poll_fd;

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

/*************************************************************
 * gdk_event_queue_find_first:
 *     Find the first event on the queue that is not still
 *     being filled in.
 *   arguments:
 *     
 *   results:
 *     Pointer to the list node for that event, or NULL
 *************************************************************/

static GList*
gdk_event_queue_find_first (void)
{
  GList *tmp_list = queued_events;

  while (tmp_list)
    {
      GdkEventPrivate *event = tmp_list->data;
      if (!(event->flags & GDK_EVENT_PENDING))
	return tmp_list;

      tmp_list = g_list_next (tmp_list);
    }

  return NULL;
}

/*************************************************************
 * gdk_event_queue_remove_link:
 *     Remove a specified list node from the event queue.
 *   arguments:
 *     node: Node to remove.
 *   results:
 *************************************************************/

static void
gdk_event_queue_remove_link (GList *node)
{
  if (node->prev)
    node->prev->next = node->next;
  else
    queued_events = node->next;
  
  if (node->next)
    node->next->prev = node->prev;
  else
    queued_tail = node->prev;
  
}

/*************************************************************
 * gdk_event_queue_append:
 *     Append an event onto the tail of the event queue.
 *   arguments:
 *     event: Event to append.
 *   results:
 *************************************************************/

static void
gdk_event_queue_append (GdkEvent *event)
{
  queued_tail = g_list_append (queued_tail, event);
  
  if (!queued_events)
    queued_events = queued_tail;
  else
    queued_tail = queued_tail->next;
}

void 
gdk_events_init (void)
{
  connection_number = ConnectionNumber (gdk_display);
  GDK_NOTE (MISC,
	    g_message ("connection number: %d", connection_number));

  g_source_add (GDK_PRIORITY_EVENTS, TRUE, &event_funcs, NULL, NULL, NULL);

  event_poll_fd.fd = connection_number;
  event_poll_fd.events = G_IO_IN;
  
  g_main_add_poll (&event_poll_fd, GDK_PRIORITY_EVENTS);

  button_click_time[0] = 0;
  button_click_time[1] = 0;
  button_window[0] = NULL;
  button_window[1] = NULL;
  button_number[0] = -1;
  button_number[1] = -1;

  gdk_add_client_message_filter (gdk_wm_protocols, 
				 gdk_wm_protocols_filter, NULL);
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
  return (gdk_event_queue_find_first() || XPending (gdk_display));
}

/*
 *--------------------------------------------------------------
 * gdk_event_get_graphics_expose
 *
 *   Waits for a GraphicsExpose or NoExpose event
 *
 * Arguments:
 *
 * Results: 
 *   For GraphicsExpose events, returns a pointer to the event
 *   converted into a GdkEvent Otherwise, returns NULL.
 *
 * Side effects:
 *
 *-------------------------------------------------------------- */

static Bool
graphics_expose_predicate (Display  *display,
			   XEvent   *xevent,
			   XPointer  arg)
{
  GdkWindowPrivate *private = (GdkWindowPrivate*) arg;
  
  g_return_val_if_fail (private != NULL, False);
  
  if (xevent->xany.window == private->xwindow &&
      (xevent->xany.type == GraphicsExpose ||
       xevent->xany.type == NoExpose))
    return True;
  else
    return False;
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  XEvent xevent;
  GdkEvent *event;
  
  g_return_val_if_fail (window != NULL, NULL);
  
  XIfEvent (gdk_display, &xevent, graphics_expose_predicate, (XPointer) window);
  
  if (xevent.xany.type == GraphicsExpose)
    {
      event = gdk_event_new ();
      
      if (gdk_event_translate (event, &xevent))
	return event;
      else
	gdk_event_free (event);
    }
  
  return NULL;	
}

/************************
 * Exposure compression *
 ************************/

/*
 * The following implements simple exposure compression. It is
 * modelled after the way Xt does exposure compression - in
 * particular compress_expose = XtExposeCompressMultiple.
 * It compress consecutive sequences of exposure events,
 * but not sequences that cross other events. (This is because
 * if it crosses a ConfigureNotify, we could screw up and
 * mistakenly compress the exposures generated for the new
 * size - could we just check for ConfigureNotify?)
 *
 * Xt compresses to a region / bounding rectangle, we compress
 * to two rectangles, and try find the two rectangles of minimal
 * area for this - this is supposed to handle the typical
 * L-shaped regions generated by OpaqueMove.
 */

/* Given three rectangles, find the two rectangles that cover
 * them with the smallest area.
 */
static void
gdk_add_rect_to_rects (GdkRectangle *rect1,
		       GdkRectangle *rect2, 
		       GdkRectangle *new_rect)
{
  GdkRectangle t1, t2, t3;
  gint size1, size2, size3;

  gdk_rectangle_union (rect1, rect2, &t1);
  gdk_rectangle_union (rect1, new_rect, &t2);
  gdk_rectangle_union (rect2, new_rect, &t3);

  size1 = t1.width * t1.height + new_rect->width * new_rect->height;
  size2 = t2.width * t2.height + rect2->width * rect2->height;
  size3 = t1.width * t1.height + rect1->width * rect1->height;

  if (size1 < size2)
    {
      if (size1 < size3)
	{
	  *rect1 = t1;
	  *rect2 = *new_rect;
	}
      else
	*rect2 = t3;
    }
  else
    {
      if (size2 < size3)
	*rect1 = t2;
      else
	*rect2 = t3;
    }
}

typedef struct _GdkExposeInfo GdkExposeInfo;

struct _GdkExposeInfo
{
  Window window;
  gboolean seen_nonmatching;
};

static Bool
expose_predicate (Display *display,
		  XEvent  *xevent,
		  XPointer arg)
{
  GdkExposeInfo *info = (GdkExposeInfo*) arg;

  /* Compressing across GravityNotify events is safe, because
   * we completely ignore them, so they can't change what
   * we are going to draw. Compressing across GravityNotify
   * events is necessay because during window-unshading animation
   * we'll get a whole bunch of them interspersed with
   * expose events.
   */
  if (xevent->xany.type != Expose && 
      xevent->xany.type != GravityNotify)
    {
      info->seen_nonmatching = TRUE;
    }

  if (info->seen_nonmatching ||
      xevent->xany.type != Expose ||
      xevent->xany.window != info->window)
    return FALSE;
  else
    return TRUE;
}

void
gdk_compress_exposures (XEvent    *xevent,
			GdkWindow *window)
{
  gint nrects = 1;
  gint count = 0;
  GdkRectangle rect1;
  GdkRectangle rect2;
  GdkRectangle tmp_rect;
  XEvent tmp_event;
  GdkFilterReturn result;
  GdkExposeInfo info;
  GdkEvent event;

  info.window = xevent->xany.window;
  info.seen_nonmatching = FALSE;
  
  rect1.x = xevent->xexpose.x;
  rect1.y = xevent->xexpose.y;
  rect1.width = xevent->xexpose.width;
  rect1.height = xevent->xexpose.height;

  event.any.type = GDK_EXPOSE;
  event.any.window = None;
  event.any.send_event = FALSE;
  
  while (1)
    {
      if (count == 0)
	{
	  if (!XCheckIfEvent (gdk_display, 
			      &tmp_event, 
			      expose_predicate, 
			      (XPointer)&info))
	    break;
	}
      else
	XIfEvent (gdk_display, 
		  &tmp_event, 
		  expose_predicate, 
		  (XPointer)&info);

      event.any.window = window;
      
      /* We apply filters here, and if it was filtered, completely
       * ignore the return
       */
      result = gdk_event_apply_filters (xevent, &event,
					window ? 
					  ((GdkWindowPrivate *)window)->filters
					  : gdk_default_filters);
      
      if (result != GDK_FILTER_CONTINUE)
	{
	  if (result == GDK_FILTER_TRANSLATE)
	    gdk_event_put (&event);
	  continue;
	}

      if (nrects == 1)
	{
	  rect2.x = tmp_event.xexpose.x;
	  rect2.y = tmp_event.xexpose.y;
	  rect2.width = tmp_event.xexpose.width;
	  rect2.height = tmp_event.xexpose.height;

	  nrects++;
	}
      else
	{
	  tmp_rect.x = tmp_event.xexpose.x;
	  tmp_rect.y = tmp_event.xexpose.y;
	  tmp_rect.width = tmp_event.xexpose.width;
	  tmp_rect.height = tmp_event.xexpose.height;

	  gdk_add_rect_to_rects (&rect1, &rect2, &tmp_rect);
	}

      count = tmp_event.xexpose.count;
    }

  if (nrects == 2)
    {
      gdk_rectangle_union (&rect1, &rect2, &tmp_rect);

      if ((tmp_rect.width * tmp_rect.height) <
	  2 * (rect1.height * rect1.width +
	       rect2.height * rect2.width))
	{
	  rect1 = tmp_rect;
	  nrects = 1;
	}
    }

  if (nrects == 2)
    {
      event.expose.type = GDK_EXPOSE;
      event.expose.window = window;
      event.expose.area.x = rect2.x;
      event.expose.area.y = rect2.y;
      event.expose.area.width = rect2.width;
      event.expose.area.height = rect2.height;
      event.expose.count = 0;

      gdk_event_put (&event);
    }

  xevent->xexpose.count = nrects - 1;
  xevent->xexpose.x = rect1.x;
  xevent->xexpose.y = rect1.y;
  xevent->xexpose.width = rect1.width;
  xevent->xexpose.height = rect1.height;
}

/*************************************************************
 * gdk_event_handler_set:
 *     
 *   arguments:
 *     func: Callback function to be called for each event.
 *     data: Data supplied to the function
 *     notify: function called when function is no longer needed
 * 
 *   results:
 *************************************************************/

void 
gdk_event_handler_set (GdkEventFunc   func,
		       gpointer       data,
		       GDestroyNotify notify)
{
  if (event_notify)
    (*event_notify) (event_data);

  event_func = func;
  event_data = data;
  event_notify = notify;
}

/*
 *--------------------------------------------------------------
 * gdk_event_get
 *
 *   Gets the next event.
 *
 * Arguments:
 *
 * Results:
 *   If an event is waiting that we care about, returns 
 *   a pointer to that event, to be freed with gdk_event_free.
 *   Otherwise, returns NULL.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

GdkEvent*
gdk_event_get (void)
{
  gdk_events_queue ();

  return gdk_event_unqueue ();
}

/*
 *--------------------------------------------------------------
 * gdk_event_peek
 *
 *   Gets the next event.
 *
 * Arguments:
 *
 * Results:
 *   If an event is waiting that we care about, returns 
 *   a copy of that event, but does not remove it from
 *   the queue. The pointer is to be freed with gdk_event_free.
 *   Otherwise, returns NULL.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

GdkEvent*
gdk_event_peek (void)
{
  GList *tmp_list;

  tmp_list = gdk_event_queue_find_first ();
  
  if (tmp_list)
    return gdk_event_copy (tmp_list->data);
  else
    return NULL;
}

void
gdk_event_put (GdkEvent *event)
{
  GdkEvent *new_event;
  
  g_return_if_fail (event != NULL);
  
  new_event = gdk_event_copy (event);

  gdk_event_queue_append (new_event);
}

/*
 *--------------------------------------------------------------
 * gdk_event_copy
 *
 *   Copy a event structure into new storage.
 *
 * Arguments:
 *   "event" is the event struct to copy.
 *
 * Results:
 *   A new event structure.  Free it with gdk_event_free.
 *
 * Side effects:
 *   The reference count of the window in the event is increased.
 *
 *--------------------------------------------------------------
 */

static GMemChunk *event_chunk = NULL;

static GdkEvent*
gdk_event_new (void)
{
  GdkEventPrivate *new_event;
  
  if (event_chunk == NULL)
    event_chunk = g_mem_chunk_new ("events",
				   sizeof (GdkEventPrivate),
				   4096,
				   G_ALLOC_AND_FREE);
  
  new_event = g_chunk_new (GdkEventPrivate, event_chunk);
  new_event->flags = 0;
  
  return (GdkEvent*) new_event;
}

GdkEvent*
gdk_event_copy (GdkEvent *event)
{
  GdkEvent *new_event;
  
  g_return_val_if_fail (event != NULL, NULL);
  
  new_event = gdk_event_new ();
  
  *new_event = *event;
  gdk_window_ref (new_event->any.window);
  
  switch (event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      new_event->key.string = g_strdup (event->key.string);
      break;
      
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      if (event->crossing.subwindow != NULL)
	gdk_window_ref (event->crossing.subwindow);
      break;
      
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      gdk_drag_context_ref (event->dnd.context);
      break;
      
    default:
      break;
    }
  
  return new_event;
}

/*
 *--------------------------------------------------------------
 * gdk_event_free
 *
 *   Free a event structure obtained from gdk_event_copy.  Do not use
 *   with other event structures.
 *
 * Arguments:
 *   "event" is the event struct to free.
 *
 * Results:
 *
 * Side effects:
 *   The reference count of the window in the event is decreased and
 *   might be freed, too.
 *
 *-------------------------------------------------------------- */

void
gdk_event_free (GdkEvent *event)
{
  g_return_if_fail (event != NULL);

  g_assert (event_chunk != NULL); /* paranoid */
  
  if (event->any.window)
    gdk_window_unref (event->any.window);
  
  switch (event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      g_free (event->key.string);
      break;
      
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      if (event->crossing.subwindow != NULL)
	gdk_window_unref (event->crossing.subwindow);
      break;
      
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      gdk_drag_context_unref (event->dnd.context);
      break;
      
    default:
      break;
    }
  
  g_mem_chunk_free (event_chunk, event);
}

/*
 *--------------------------------------------------------------
 * gdk_event_get_time:
 *    Get the timestamp from an event.
 *   arguments:
 *     event:
 *   results:
 *    The event's time stamp, if it has one, otherwise
 *    GDK_CURRENT_TIME.
 *--------------------------------------------------------------
 */

guint32
gdk_event_get_time (GdkEvent *event)
{
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
	return event->motion.time;
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
	return event->button.time;
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
	return event->key.time;
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
	return event->crossing.time;
      case GDK_PROPERTY_NOTIFY:
	return event->property.time;
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
	return event->selection.time;
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
	return event->proximity.time;
      case GDK_DRAG_ENTER:
      case GDK_DRAG_LEAVE:
      case GDK_DRAG_MOTION:
      case GDK_DRAG_STATUS:
      case GDK_DROP_START:
      case GDK_DROP_FINISHED:
	return event->dnd.time;
      default:			/* use current time */
	break;
      }
  
  return GDK_CURRENT_TIME;
}

/*
 *--------------------------------------------------------------
 * gdk_set_show_events
 *
 *   Turns on/off the showing of events.
 *
 * Arguments:
 *   "show_events" is a boolean describing whether or
 *   not to show the events gdk receives.
 *
 * Results:
 *
 * Side effects:
 *   When "show_events" is TRUE, calls to "gdk_event_get"
 *   will output debugging informatin regarding the event
 *   received to stdout.
 *
 *--------------------------------------------------------------
 */

void
gdk_set_show_events (gboolean show_events)
{
  if (show_events)
    gdk_debug_flags |= GDK_DEBUG_EVENTS;
  else
    gdk_debug_flags &= ~GDK_DEBUG_EVENTS;
}

gboolean
gdk_get_show_events (void)
{
  return (gdk_debug_flags & GDK_DEBUG_EVENTS) != 0;
}

static void
gdk_io_destroy (gpointer data)
{
  GdkIOClosure *closure = data;

  if (closure->notify)
    closure->notify (closure->data);

  g_free (closure);
}

/* What do we do with G_IO_NVAL?
 */
#define READ_CONDITION (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define WRITE_CONDITION (G_IO_OUT | G_IO_ERR)
#define EXCEPTION_CONDITION (G_IO_PRI)

static gboolean  
gdk_io_invoke (GIOChannel   *source,
	       GIOCondition  condition,
	       gpointer      data)
{
  GdkIOClosure *closure = data;
  GdkInputCondition gdk_cond = 0;

  if (condition & READ_CONDITION)
    gdk_cond |= GDK_INPUT_READ;
  if (condition & WRITE_CONDITION)
    gdk_cond |= GDK_INPUT_WRITE;
  if (condition & EXCEPTION_CONDITION)
    gdk_cond |= GDK_INPUT_EXCEPTION;

  if (closure->condition & gdk_cond)
    closure->function (closure->data, g_io_channel_unix_get_fd (source), gdk_cond);

  return TRUE;
}

gint
gdk_input_add_full (gint	      source,
		    GdkInputCondition condition,
		    GdkInputFunction  function,
		    gpointer	      data,
		    GdkDestroyNotify  destroy)
{
  guint result;
  GdkIOClosure *closure = g_new (GdkIOClosure, 1);
  GIOChannel *channel;
  GIOCondition cond = 0;

  closure->function = function;
  closure->condition = condition;
  closure->notify = destroy;
  closure->data = data;

  if (condition & GDK_INPUT_READ)
    cond |= READ_CONDITION;
  if (condition & GDK_INPUT_WRITE)
    cond |= WRITE_CONDITION;
  if (condition & GDK_INPUT_EXCEPTION)
    cond |= EXCEPTION_CONDITION;

  channel = g_io_channel_unix_new (source);
  result = g_io_add_watch_full (channel, G_PRIORITY_DEFAULT, cond, 
				gdk_io_invoke,
				closure, gdk_io_destroy);
  g_io_channel_unref (channel);

  return result;
}

gint
gdk_input_add (gint		 source,
	       GdkInputCondition condition,
	       GdkInputFunction	 function,
	       gpointer		 data)
{
  return gdk_input_add_full (source, condition, function, data, NULL);
}

void
gdk_input_remove (gint tag)
{
  g_source_remove (tag);
}

static gint
gdk_event_apply_filters (XEvent *xevent,
			 GdkEvent *event,
			 GList *filters)
{
  GList *tmp_list;
  GdkFilterReturn result;
  
  tmp_list = filters;
  
  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter*) tmp_list->data;
      
      tmp_list = tmp_list->next;
      result = filter->function (xevent, event, filter->data);
      if (result !=  GDK_FILTER_CONTINUE)
	return result;
    }
  
  return GDK_FILTER_CONTINUE;
}

void 
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
  GdkClientFilter *filter = g_new (GdkClientFilter, 1);

  filter->type = message_type;
  filter->function = func;
  filter->data = data;
  
  client_filters = g_list_prepend (client_filters, filter);
}

/* Hack because GDK_RELEASE_MASK (a mistake in and of itself) was
 * accidentally given a value that overlaps with real bits in the
 * state field.
 */
static inline guint
translate_state (guint xstate)
{
  return xstate & ~GDK_RELEASE_MASK;
}

static gint
gdk_event_translate (GdkEvent *event,
		     XEvent   *xevent)
{
  
  GdkWindow *window;
  GdkWindowPrivate *window_private;
  static XComposeStatus compose;
  KeySym keysym;
  int charcount;
#ifdef USE_XIM
  static gchar* buf = NULL;
  static gint buf_len= 0;
#else
  char buf[16];
#endif
  gint return_val;
  
  return_val = FALSE;
  
  /* Find the GdkWindow that this event occurred in.
   * 
   * We handle events with window=None
   *  specially - they are generated by XFree86's XInput under
   *  some circumstances.
   */
  
  if ((xevent->xany.window == None) &&
      gdk_input_vtable.window_none_event)
    {
      return_val = gdk_input_vtable.window_none_event (event,xevent);
      
      if (return_val >= 0)	/* was handled */
	return return_val;
      else
	return_val = FALSE;
    }
  
  window = gdk_window_lookup (xevent->xany.window);
  window_private = (GdkWindowPrivate *) window;
  
  if (window != NULL)
    gdk_window_ref (window);
  
  event->any.window = window;
  event->any.send_event = xevent->xany.send_event ? TRUE : FALSE;
  
  if (window_private && window_private->destroyed)
    {
      if (xevent->type != DestroyNotify)
	return FALSE;
    }
  else
    {
      /* Check for filters for this window
       */
      GdkFilterReturn result;
      result = gdk_event_apply_filters (xevent, event,
					window_private
					?window_private->filters
					:gdk_default_filters);
      
      if (result != GDK_FILTER_CONTINUE)
	{
	  return (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	}
    }

#ifdef USE_XIM
  if (window == NULL && gdk_xim_window && xevent->type == KeyPress &&
      !((GdkWindowPrivate *) gdk_xim_window)->destroyed)
    {
      /*
       * If user presses a key in Preedit or Status window, keypress event
       * is sometimes sent to these windows. These windows are not managed
       * by GDK, so we redirect KeyPress event to xim_window.
       *
       * If someone want to use the window whitch is not managed by GDK
       * and want to get KeyPress event, he/she must register the filter
       * function to gdk_default_filters to intercept the event.
       */

      GdkFilterReturn result;

      window = gdk_xim_window;
      window_private = (GdkWindowPrivate *) window;
      gdk_window_ref (window);
      event->any.window = window;

      GDK_NOTE (XIM,
	g_message ("KeyPress event is redirected to xim_window: %#lx",
	  	   xevent->xany.window));

      result = gdk_event_apply_filters (xevent, event,
	  				window_private->filters);
      if (result != GDK_FILTER_CONTINUE)
	return (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
    }
#endif

  /* We do a "manual" conversion of the XEvent to a
   *  GdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */

  return_val = TRUE;

  switch (xevent->type)
    {
    case KeyPress:
      /* Lookup the string corresponding to the given keysym.
       */
      
#ifdef USE_XIM
      if (buf_len == 0) 
	{
	  buf_len = 128;
	  buf = g_new (gchar, buf_len);
	}
      keysym = GDK_VoidSymbol;
      
      if (gdk_xim_ic && gdk_xim_ic->xic)
	{
	  Status status;
	  
	  /* Clear keyval. Depending on status, may not be set */
	  charcount = XmbLookupString(gdk_xim_ic->xic,
				      &xevent->xkey, buf, buf_len-1,
				      &keysym, &status);
	  if (status == XBufferOverflow)
	    {			  /* retry */
	      /* alloc adequate size of buffer */
	      GDK_NOTE (XIM,
			g_message("XIM: overflow (required %i)", charcount));
	      
	      while (buf_len <= charcount)
		buf_len *= 2;
	      buf = (gchar *) g_realloc (buf, buf_len);
	      
	      charcount = XmbLookupString (gdk_xim_ic->xic,
					   &xevent->xkey, buf, buf_len-1,
					   &keysym, &status);
	    }
	  if (status == XLookupNone)
	    {
	      return_val = FALSE;
	      break;
	    }
	}
      else
	charcount = XLookupString (&xevent->xkey, buf, buf_len,
				   &keysym, &compose);
#else
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 &keysym, &compose);
#endif
      event->key.keyval = keysym;
      
      if (charcount > 0 && buf[charcount-1] == '\0')
	charcount --;
      else
	buf[charcount] = '\0';
      
      /* Print debugging info. */
      
#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_EVENTS)
	{
	  g_message ("key press:\twindow: %ld  key: %12s  %d",
		     xevent->xkey.window,
		     event->key.keyval ? XKeysymToString (event->key.keyval) : "(none)",
		     event->key.keyval);
	  if (charcount > 0)
	    g_message ("\t\tlength: %4d string: \"%s\"",
		       charcount, buf);
	}
#endif /* G_ENABLE_DEBUG */
      
      event->key.type = GDK_KEY_PRESS;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = translate_state (xevent->xkey.state);
      event->key.string = g_strdup (buf);
      event->key.length = charcount;
      
      break;
      
    case KeyRelease:
      /* Lookup the string corresponding to the given keysym.
       */
#ifdef USE_XIM
      if (buf_len == 0) 
	{
	  buf_len = 128;
	  buf = g_new (gchar, buf_len);
	}
#endif
      keysym = GDK_VoidSymbol;
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 &keysym, &compose);
      event->key.keyval = keysym;      
      
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
		g_message ("key release:\t\twindow: %ld	 key: %12s  %d",
			   xevent->xkey.window,
			   XKeysymToString (event->key.keyval),
			   event->key.keyval));
      
      event->key.type = GDK_KEY_RELEASE;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = translate_state (xevent->xkey.state);
      event->key.length = 0;
      event->key.string = NULL;
      
      break;
      
    case ButtonPress:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
		g_message ("button press:\t\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      event->button.type = GDK_BUTTON_PRESS;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x;
      event->button.y = xevent->xbutton.y;
      event->button.x_root = (gfloat)xevent->xbutton.x_root;
      event->button.y_root = (gfloat)xevent->xbutton.y_root;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = translate_state (xevent->xbutton.state);
      event->button.button = xevent->xbutton.button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;
      
      if ((event->button.time < (button_click_time[1] + TRIPLE_CLICK_TIME)) &&
	  (event->button.window == button_window[1]) &&
	  (event->button.button == button_number[1]))
	{
	  gdk_synthesize_click (event, 3);
	  
	  button_click_time[1] = 0;
	  button_click_time[0] = 0;
	  button_window[1] = NULL;
	  button_window[0] = 0;
	  button_number[1] = -1;
	  button_number[0] = -1;
	}
      else if ((event->button.time < (button_click_time[0] + DOUBLE_CLICK_TIME)) &&
	       (event->button.window == button_window[0]) &&
	       (event->button.button == button_number[0]))
	{
	  gdk_synthesize_click (event, 2);
	  
	  button_click_time[1] = button_click_time[0];
	  button_click_time[0] = event->button.time;
	  button_window[1] = button_window[0];
	  button_window[0] = event->button.window;
	  button_number[1] = button_number[0];
	  button_number[0] = event->button.button;
	}
      else
	{
	  button_click_time[1] = 0;
	  button_click_time[0] = event->button.time;
	  button_window[1] = NULL;
	  button_window[0] = event->button.window;
	  button_number[1] = -1;
	  button_number[0] = event->button.button;
	}

      break;
      
    case ButtonRelease:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
		g_message ("button release:\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      event->button.type = GDK_BUTTON_RELEASE;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x;
      event->button.y = xevent->xbutton.y;
      event->button.x_root = (gfloat)xevent->xbutton.x_root;
      event->button.y_root = (gfloat)xevent->xbutton.y_root;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = translate_state (xevent->xbutton.state);
      event->button.button = xevent->xbutton.button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;
      
      break;
      
    case MotionNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s", 
			   xevent->xmotion.window,
			   xevent->xmotion.x, xevent->xmotion.y,
			   (xevent->xmotion.is_hint) ? "true" : "false"));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = xevent->xmotion.x;
      event->motion.y = xevent->xmotion.y;
      event->motion.x_root = (gfloat)xevent->xmotion.x_root;
      event->motion.y_root = (gfloat)xevent->xmotion.y_root;
      event->motion.pressure = 0.5;
      event->motion.xtilt = 0;
      event->motion.ytilt = 0;
      event->motion.state = translate_state (xevent->xmotion.state);
      event->motion.is_hint = xevent->xmotion.is_hint;
      event->motion.source = GDK_SOURCE_MOUSE;
      event->motion.deviceid = GDK_CORE_POINTER;
      
      break;
      
    case EnterNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld",
			   xevent->xcrossing.window,
			   xevent->xcrossing.detail,
			   xevent->xcrossing.subwindow));
      
      /* Tell XInput stuff about it if appropriate */
      if (window_private &&
	  !window_private->destroyed &&
	  (window_private->extension_events != 0) &&
	  gdk_input_vtable.enter_event)
	gdk_input_vtable.enter_event (&xevent->xcrossing, window);
      
      event->crossing.type = GDK_ENTER_NOTIFY;
      event->crossing.window = window;
      
      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
	event->crossing.subwindow = gdk_window_lookup (xevent->xcrossing.subwindow);
      else
	event->crossing.subwindow = NULL;
      
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x;
      event->crossing.y = xevent->xcrossing.y;
      event->crossing.x_root = xevent->xcrossing.x_root;
      event->crossing.y_root = xevent->xcrossing.y_root;
      
      /* Translate the crossing mode into Gdk terms.
       */
      switch (xevent->xcrossing.mode)
	{
	case NotifyNormal:
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  break;
	case NotifyGrab:
	  event->crossing.mode = GDK_CROSSING_GRAB;
	  break;
	case NotifyUngrab:
	  event->crossing.mode = GDK_CROSSING_UNGRAB;
	  break;
	};
      
      /* Translate the crossing detail into Gdk terms.
       */
      switch (xevent->xcrossing.detail)
	{
	case NotifyInferior:
	  event->crossing.detail = GDK_NOTIFY_INFERIOR;
	  break;
	case NotifyAncestor:
	  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	  break;
	case NotifyVirtual:
	  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	  break;
	case NotifyNonlinear:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
	  break;
	case NotifyNonlinearVirtual:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  break;
	default:
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  break;
	}
      
      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = translate_state (xevent->xcrossing.state);
  
      break;
      
    case LeaveNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
		g_message ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld",
			   xevent->xcrossing.window,
			   xevent->xcrossing.detail, xevent->xcrossing.subwindow));
      
      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = window;
      
      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
	event->crossing.subwindow = gdk_window_lookup (xevent->xcrossing.subwindow);
      else
	event->crossing.subwindow = NULL;
      
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x;
      event->crossing.y = xevent->xcrossing.y;
      event->crossing.x_root = xevent->xcrossing.x_root;
      event->crossing.y_root = xevent->xcrossing.y_root;
      
      /* Translate the crossing mode into Gdk terms.
       */
      switch (xevent->xcrossing.mode)
	{
	case NotifyNormal:
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  break;
	case NotifyGrab:
	  event->crossing.mode = GDK_CROSSING_GRAB;
	  break;
	case NotifyUngrab:
	  event->crossing.mode = GDK_CROSSING_UNGRAB;
	  break;
	};
      
      /* Translate the crossing detail into Gdk terms.
       */
      switch (xevent->xcrossing.detail)
	{
	case NotifyInferior:
	  event->crossing.detail = GDK_NOTIFY_INFERIOR;
	  break;
	case NotifyAncestor:
	  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	  break;
	case NotifyVirtual:
	  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	  break;
	case NotifyNonlinear:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
	  break;
	case NotifyNonlinearVirtual:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  break;
	default:
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  break;
	}
      
      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = translate_state (xevent->xcrossing.state);
      
      break;
      
    case FocusIn:
    case FocusOut:
      /* We only care about focus events that indicate that _this_
       * window (not a ancestor or child) got or lost the focus
       */
      switch (xevent->xfocus.detail)
	{
	case NotifyAncestor:
	case NotifyInferior:
	case NotifyNonlinear:
	  /* Print debugging info.
	   */
	  GDK_NOTE (EVENTS,
		    g_message ("focus %s:\t\twindow: %ld",
			       (xevent->xany.type == FocusIn) ? "in" : "out",
			       xevent->xfocus.window));
	  
 	  /* gdk_keyboard_grab() causes following events. These events confuse
 	   * the XIM focus, so ignore them.
 	   */
 	  if (xevent->xfocus.mode == NotifyGrab ||
 	      xevent->xfocus.mode == NotifyUngrab)
 	    break;
	  
	  event->focus_change.type = GDK_FOCUS_CHANGE;
	  event->focus_change.window = window;
	  event->focus_change.in = (xevent->xany.type == FocusIn);

	  break;
	default:
	  return_val = FALSE;
	}
      break;
      
    case KeymapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("keymap notify"));

      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case Expose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("expose:\t\twindow: %ld  %d	x,y: %d %d  w,h: %d %d%s",
			   xevent->xexpose.window, xevent->xexpose.count,
			   xevent->xexpose.x, xevent->xexpose.y,
			   xevent->xexpose.width, xevent->xexpose.height,
			   event->any.send_event ? " (send)" : ""));
      gdk_compress_exposures (xevent, window);
      
      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->xexpose.x;
      event->expose.area.y = xevent->xexpose.y;
      event->expose.area.width = xevent->xexpose.width;
      event->expose.area.height = xevent->xexpose.height;
      event->expose.count = xevent->xexpose.count;
      
      break;
      
    case GraphicsExpose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("graphics expose:\tdrawable: %ld",
			   xevent->xgraphicsexpose.drawable));
      
      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->xgraphicsexpose.x;
      event->expose.area.y = xevent->xgraphicsexpose.y;
      event->expose.area.width = xevent->xgraphicsexpose.width;
      event->expose.area.height = xevent->xgraphicsexpose.height;
      event->expose.count = xevent->xexpose.count;
      
      break;
      
    case NoExpose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("no expose:\t\tdrawable: %ld",
			   xevent->xnoexpose.drawable));
      
      event->no_expose.type = GDK_NO_EXPOSE;
      event->no_expose.window = window;
      
      break;
      
    case VisibilityNotify:
      /* Print debugging info.
       */
#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_EVENTS)
	switch (xevent->xvisibility.state)
	  {
	  case VisibilityFullyObscured:
	    g_message ("visibility notify:\twindow: %ld	 none",
		       xevent->xvisibility.window);
	    break;
	  case VisibilityPartiallyObscured:
	    g_message ("visibility notify:\twindow: %ld	 partial",
		       xevent->xvisibility.window);
	    break;
	  case VisibilityUnobscured:
	    g_message ("visibility notify:\twindow: %ld	 full",
		       xevent->xvisibility.window);
	    break;
	  }
#endif /* G_ENABLE_DEBUG */
      
      event->visibility.type = GDK_VISIBILITY_NOTIFY;
      event->visibility.window = window;
      
      switch (xevent->xvisibility.state)
	{
	case VisibilityFullyObscured:
	  event->visibility.state = GDK_VISIBILITY_FULLY_OBSCURED;
	  break;
	  
	case VisibilityPartiallyObscured:
	  event->visibility.state = GDK_VISIBILITY_PARTIAL;
	  break;
	  
	case VisibilityUnobscured:
	  event->visibility.state = GDK_VISIBILITY_UNOBSCURED;
	  break;
	}
      
      break;
      
    case CreateNotify:
      GDK_NOTE (EVENTS,
		g_message ("create notify:\twindow: %ld  x,y: %d %d	w,h: %d %d  b-w: %d  parent: %ld	 ovr: %d",
			   xevent->xcreatewindow.window,
			   xevent->xcreatewindow.x,
			   xevent->xcreatewindow.y,
			   xevent->xcreatewindow.width,
			   xevent->xcreatewindow.height,
			   xevent->xcreatewindow.border_width,
			   xevent->xcreatewindow.parent,
			   xevent->xcreatewindow.override_redirect));
      /* not really handled */
      break;
      
    case DestroyNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("destroy notify:\twindow: %ld",
			   xevent->xdestroywindow.window));
      
      event->any.type = GDK_DESTROY;
      event->any.window = window;
      
      return_val = window_private && !window_private->destroyed;
      
      if (window && window_private->xwindow != GDK_ROOT_WINDOW())
	gdk_window_destroy_notify (window);
      break;
      
    case UnmapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("unmap notify:\t\twindow: %ld",
			   xevent->xmap.window));
      
      event->any.type = GDK_UNMAP;
      event->any.window = window;
      
      if (gdk_xgrab_window == window_private)
	gdk_xgrab_window = NULL;
      
      break;
      
    case MapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("map notify:\t\twindow: %ld",
			   xevent->xmap.window));
      
      event->any.type = GDK_MAP;
      event->any.window = window;
      
      break;
      
    case ReparentNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("reparent notify:\twindow: %ld  x,y: %d %d  parent: %ld	ovr: %d",
			   xevent->xreparent.window,
			   xevent->xreparent.x,
			   xevent->xreparent.y,
			   xevent->xreparent.parent,
			   xevent->xreparent.override_redirect));

      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case ConfigureNotify:
      /* Print debugging info.
       */
      while (0 && /* don't reorder ConfigureNotify events at all */
	     XPending (gdk_display) > 0 &&
	     XCheckTypedWindowEvent (gdk_display, xevent->xany.window,
				     ConfigureNotify, xevent))
	{
	  GdkFilterReturn result;
	  
	  GDK_NOTE (EVENTS, 
		    g_message ("configure notify discarded:\twindow: %ld",
			       xevent->xconfigure.window));
	  
	  result = gdk_event_apply_filters (xevent, event,
					    window_private
					    ?window_private->filters
					    :gdk_default_filters);
	  
	  /* If the result is GDK_FILTER_REMOVE, there will be
	   * trouble, but anybody who filtering the Configure events
	   * better know what they are doing
	   */
	  if (result != GDK_FILTER_CONTINUE)
	    {
	      return (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	    }
	  
	  /*XSync (gdk_display, 0);*/
	}
      
      
      GDK_NOTE (EVENTS,
		g_message ("configure notify:\twindow: %ld  x,y: %d %d	w,h: %d %d  b-w: %d  above: %ld	 ovr: %d%s",
			   xevent->xconfigure.window,
			   xevent->xconfigure.x,
			   xevent->xconfigure.y,
			   xevent->xconfigure.width,
			   xevent->xconfigure.height,
			   xevent->xconfigure.border_width,
			   xevent->xconfigure.above,
			   xevent->xconfigure.override_redirect,
			   !window
			   ? " (discarding)"
			   : window_private->window_type == GDK_WINDOW_CHILD
			   ? " (discarding child)"
			   : ""));
      if (window &&
	  !window_private->destroyed &&
	  (window_private->extension_events != 0) &&
	  gdk_input_vtable.configure_event)
	gdk_input_vtable.configure_event (&xevent->xconfigure, window);

      if (!window || window_private->window_type == GDK_WINDOW_CHILD)
	return_val = FALSE;
      else
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.width = xevent->xconfigure.width;
	  event->configure.height = xevent->xconfigure.height;
	  
	  if (!xevent->xconfigure.x &&
	      !xevent->xconfigure.y &&
	      !window_private->destroyed)
	    {
	      gint tx = 0;
	      gint ty = 0;
	      Window child_window = 0;

	      gdk_error_trap_push ();
	      if (XTranslateCoordinates (window_private->xdisplay,
					 window_private->xwindow,
					 gdk_root_window,
					 0, 0,
					 &tx, &ty,
					 &child_window))
		{
		  if (!gdk_error_trap_pop ())
		    {
		      event->configure.x = tx;
		      event->configure.y = ty;
		    }
		}
	      else
		gdk_error_trap_pop ();
	    }
	  else
	    {
	      event->configure.x = xevent->xconfigure.x;
	      event->configure.y = xevent->xconfigure.y;
	    }
	  window_private->x = event->configure.x;
	  window_private->y = event->configure.y;
	  window_private->width = xevent->xconfigure.width;
	  window_private->height = xevent->xconfigure.height;
	  if (window_private->resize_count > 1)
	    window_private->resize_count -= 1;
	}
      break;
      
    case PropertyNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		gchar *atom = gdk_atom_name (xevent->xproperty.atom);
		g_message ("property notify:\twindow: %ld, atom(%ld): %s%s%s",
			   xevent->xproperty.window,
			   xevent->xproperty.atom,
			   atom ? "\"" : "",
			   atom ? atom : "unknown",
			   atom ? "\"" : "");
		);
      
      event->property.type = GDK_PROPERTY_NOTIFY;
      event->property.window = window;
      event->property.atom = xevent->xproperty.atom;
      event->property.time = xevent->xproperty.time;
      event->property.state = xevent->xproperty.state;
      
      break;
      
    case SelectionClear:
      GDK_NOTE (EVENTS,
		g_message ("selection clear:\twindow: %ld",
			   xevent->xproperty.window));
      
      event->selection.type = GDK_SELECTION_CLEAR;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionclear.selection;
      event->selection.time = xevent->xselectionclear.time;
      
      break;
      
    case SelectionRequest:
      GDK_NOTE (EVENTS,
		g_message ("selection request:\twindow: %ld",
			   xevent->xproperty.window));
      
      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionrequest.selection;
      event->selection.target = xevent->xselectionrequest.target;
      event->selection.property = xevent->xselectionrequest.property;
      event->selection.requestor = xevent->xselectionrequest.requestor;
      event->selection.time = xevent->xselectionrequest.time;
      
      break;
      
    case SelectionNotify:
      GDK_NOTE (EVENTS,
		g_message ("selection notify:\twindow: %ld",
			   xevent->xproperty.window));
      
      
      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = xevent->xselection.selection;
      event->selection.target = xevent->xselection.target;
      event->selection.property = xevent->xselection.property;
      event->selection.time = xevent->xselection.time;
      
      break;
      
    case ColormapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("colormap notify:\twindow: %ld",
			   xevent->xcolormap.window));
      
      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case ClientMessage:
      {
	GList *tmp_list;
	GdkFilterReturn result = GDK_FILTER_CONTINUE;

	/* Print debugging info.
	 */
	GDK_NOTE (EVENTS,
		  g_message ("client message:\twindow: %ld",
			     xevent->xclient.window));
	
	tmp_list = client_filters;
	while (tmp_list)
	  {
	    GdkClientFilter *filter = tmp_list->data;
	    if (filter->type == xevent->xclient.message_type)
	      {
		result = (*filter->function) (xevent, event, filter->data);
		break;
	      }
	    
	    tmp_list = tmp_list->next;
	  }

	switch (result)
	  {
	  case GDK_FILTER_REMOVE:
	    return_val = FALSE;
	    break;
	  case GDK_FILTER_TRANSLATE:
	    return_val = TRUE;
	    break;
	  case GDK_FILTER_CONTINUE:
	    /* Send unknown ClientMessage's on to Gtk for it to use */
	    event->client.type = GDK_CLIENT_EVENT;
	    event->client.window = window;
	    event->client.message_type = xevent->xclient.message_type;
	    event->client.data_format = xevent->xclient.format;
	    memcpy(&event->client.data, &xevent->xclient.data,
		   sizeof(event->client.data));
	  }
      }
      
      break;
      
    case MappingNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("mapping notify"));
      
      /* Let XLib know that there is a new keyboard mapping.
       */
      XRefreshKeyboardMapping (&xevent->xmapping);
      return_val = FALSE;
      break;
      
    default:
      /* something else - (e.g., a Xinput event) */
      
      if (window_private &&
	  !window_private->destroyed &&
	  (window_private->extension_events != 0) &&
	  gdk_input_vtable.other_event)
	return_val = gdk_input_vtable.other_event(event, xevent, window);
      else
	return_val = FALSE;
      
      break;
    }
  
  if (return_val)
    {
      if (event->any.window)
	gdk_window_ref (event->any.window);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
	   (event->any.type == GDK_LEAVE_NOTIFY)) &&
	  (event->crossing.subwindow != NULL))
	gdk_window_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = GDK_NOTHING;
    }
  
  if (window)
    gdk_window_unref (window);
  
  return return_val;
}

GdkFilterReturn
gdk_wm_protocols_filter (GdkXEvent *xev,
			 GdkEvent  *event,
			 gpointer data)
{
  XEvent *xevent = (XEvent *)xev;

  if ((Atom) xevent->xclient.data.l[0] == gdk_wm_delete_window)
    {
  /* The delete window request specifies a window
   *  to delete. We don't actually destroy the
   *  window because "it is only a request". (The
   *  window might contain vital data that the
   *  program does not want destroyed). Instead
   *  the event is passed along to the program,
   *  which should then destroy the window.
   */
      GDK_NOTE (EVENTS,
		g_message ("delete window:\t\twindow: %ld",
			   xevent->xclient.window));
      
      event->any.type = GDK_DELETE;

      return GDK_FILTER_TRANSLATE;
    }
  else if ((Atom) xevent->xclient.data.l[0] == gdk_wm_take_focus)
    {
    }

  return GDK_FILTER_REMOVE;
}

#if 0
static Bool
gdk_event_get_type (Display  *display,
		    XEvent   *xevent,
		    XPointer  arg)
{
  GdkEvent event;
  GdkPredicate *pred;
  
  if (gdk_event_translate (&event, xevent))
    {
      pred = (GdkPredicate*) arg;
      return (* pred->func) (&event, pred->data);
    }
  
  return FALSE;
}
#endif

static void
gdk_events_queue (void)
{
  GList *node;
  GdkEvent *event;
  XEvent xevent;

  while (!gdk_event_queue_find_first() && XPending (gdk_display))
    {
#ifdef USE_XIM
      Window w = None;
      
      XNextEvent (gdk_display, &xevent);
      if (gdk_xim_window)
 	switch (xevent.type)
 	  {
 	  case KeyPress:
 	  case KeyRelease:
 	  case ButtonPress:
 	  case ButtonRelease:
 	    w = GDK_WINDOW_XWINDOW (gdk_xim_window);
 	    break;
 	  }
      
      if (XFilterEvent (&xevent, w))
	continue;
#else
      XNextEvent (gdk_display, &xevent);
#endif
      
      event = gdk_event_new ();
      
      event->any.type = GDK_NOTHING;
      event->any.window = NULL;
      event->any.send_event = xevent.xany.send_event ? TRUE : FALSE;

      ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

      gdk_event_queue_append (event);
      node = queued_tail;

      if (gdk_event_translate (event, &xevent))
	{
	  ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
	}
      else
	{
	  gdk_event_queue_remove_link (node);
	  g_list_free_1 (node);
	  gdk_event_free (event);
	}
    }
}

static gboolean  
gdk_event_prepare (gpointer  source_data, 
		   GTimeVal *current_time,
		   gint     *timeout,
		   gpointer  user_data)
{
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  *timeout = -1;

  retval = (gdk_event_queue_find_first () != NULL) || XPending (gdk_display);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_check (gpointer  source_data,
		 GTimeVal *current_time,
		 gpointer  user_data)
{
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  if (event_poll_fd.revents & G_IO_IN)
    retval = (gdk_event_queue_find_first () != NULL) || XPending (gdk_display);
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static GdkEvent*
gdk_event_unqueue (void)
{
  GdkEvent *event = NULL;
  GList *tmp_list;

  tmp_list = gdk_event_queue_find_first ();

  if (tmp_list)
    {
      event = tmp_list->data;
      gdk_event_queue_remove_link (tmp_list);
      g_list_free_1 (tmp_list);
    }

  return event;
}

static gboolean  
gdk_event_dispatch (gpointer  source_data,
		    GTimeVal *current_time,
		    gpointer  user_data)
{
  GdkEvent *event;
 
  GDK_THREADS_ENTER ();

  gdk_events_queue();
  event = gdk_event_unqueue();

  if (event)
    {
      if (event_func)
	(*event_func) (event, event_data);
      
      gdk_event_free (event);
    }
  
  GDK_THREADS_LEAVE ();

  return TRUE;
}

static void
gdk_synthesize_click (GdkEvent *event,
		      gint	nclicks)
{
  GdkEvent temp_event;
  
  g_return_if_fail (event != NULL);
  
  temp_event = *event;
  temp_event.type = (nclicks == 2) ? GDK_2BUTTON_PRESS : GDK_3BUTTON_PRESS;
  
  gdk_event_put (&temp_event);
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message (GdkEvent *event, guint32 xid)
{
  XEvent sev;
  
  g_return_val_if_fail(event != NULL, FALSE);
  
  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = gdk_display;
  sev.xclient.format = event->client.data_format;
  sev.xclient.window = xid;
  memcpy(&sev.xclient.data, &event->client.data, sizeof(sev.xclient.data));
  sev.xclient.message_type = event->client.message_type;
  
  return gdk_send_xevent (xid, False, NoEventMask, &sev);
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message_to_all_recurse (XEvent  *xev, 
					      guint32  xid,
					      guint    level)
{
  static GdkAtom wm_state_atom = GDK_NONE;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  Window *ret_children, ret_root, ret_parent;
  unsigned int ret_nchildren;
  gint old_warnings = gdk_error_warnings;
  gboolean send = FALSE;
  gboolean found = FALSE;
  int i;

  if (!wm_state_atom)
    wm_state_atom = gdk_atom_intern ("WM_STATE", FALSE);

  gdk_error_warnings = FALSE;
  gdk_error_code = 0;
  XGetWindowProperty (gdk_display, xid, wm_state_atom, 0, 0, False, AnyPropertyType,
		      &type, &format, &nitems, &after, &data);

  if (gdk_error_code)
    {
      gdk_error_warnings = old_warnings;

      return FALSE;
    }

  if (type)
    {
      send = TRUE;
      XFree (data);
    }
  else
    {
      /* OK, we're all set, now let's find some windows to send this to */
      if (XQueryTree (gdk_display, xid, &ret_root, &ret_parent,
		      &ret_children, &ret_nchildren) != True ||
	  gdk_error_code)
	{
	  gdk_error_warnings = old_warnings;

	  return FALSE;
	}

      for(i = 0; i < ret_nchildren; i++)
	if (gdk_event_send_client_message_to_all_recurse (xev, ret_children[i], level + 1))
	  found = TRUE;

      XFree (ret_children);
    }

  if (send || (!found && (level == 1)))
    {
      xev->xclient.window = xid;
      gdk_send_xevent (xid, False, NoEventMask, xev);
    }

  gdk_error_warnings = old_warnings;

  return (send || found);
}

void
gdk_event_send_clientmessage_toall (GdkEvent *event)
{
  XEvent sev;
  gint old_warnings = gdk_error_warnings;

  g_return_if_fail(event != NULL);
  
  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = gdk_display;
  sev.xclient.format = event->client.data_format;
  memcpy(&sev.xclient.data, &event->client.data, sizeof(sev.xclient.data));
  sev.xclient.message_type = event->client.message_type;

  gdk_event_send_client_message_to_all_recurse(&sev, gdk_root_window, 0);

  gdk_error_warnings = old_warnings;
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
  XSync (gdk_display, False);
}


