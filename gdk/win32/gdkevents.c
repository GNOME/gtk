/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
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

#include "config.h"

#include <stdio.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include "gdkx.h"
#include "gdkinput.h"

#define PING() printf("%s: %d\n",__FILE__,__LINE__),fflush(stdout)

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
static gint	 gdk_event_apply_filters(MSG      *xevent,
					 GdkEvent *event,
					 GList    *filters);
static gint	 gdk_event_translate	(GdkEvent *event, 
					 MSG      *xevent,
					 gboolean *ret_val_flagp,
					 gint     *ret_valp);
static void      gdk_events_queue       (void);
static GdkEvent *gdk_event_unqueue      (void);
static gboolean  gdk_event_prepare      (gpointer  source_data, 
				 	 GTimeVal *current_time,
					 gint     *timeout);
static gboolean  gdk_event_check        (gpointer  source_data,
				 	 GTimeVal *current_time);
static gboolean  gdk_event_dispatch     (gpointer  source_data,
					 GTimeVal *current_time,
					 gpointer  user_data);

static void	 gdk_synthesize_click	(GdkEvent     *event, 
					 gint	       nclicks);

/* Private variable declarations
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
static GdkWindowPrivate *p_grab_window = NULL; /* Window that currently
						* holds the pointer grab
						*/

static GdkWindowPrivate *k_grab_window = NULL; /* Window the holds the
						* keyboard grab
						*/

static GList *client_filters;	/* Filters for client messages */

static gboolean p_grab_automatic;
static GdkEventMask p_grab_event_mask;
static gboolean p_grab_owner_events, k_grab_owner_events;
static HCURSOR p_grab_cursor;

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

static GdkWindow *curWnd = NULL;
static HWND active = NULL;
static gint curX, curY;
static gdouble curXroot, curYroot;
static UINT gdk_ping_msg;
static gboolean ignore_WM_CHAR = FALSE;
static gboolean is_AltGr_key = FALSE;

LRESULT CALLBACK 
gdk_WindowProc(HWND hwnd,
	       UINT message,
	       WPARAM wParam,
	       LPARAM lParam)
{
  GdkEvent event;
  GdkEvent *eventp;
  MSG msg;
  DWORD pos;
  gint ret_val;
  gboolean ret_val_flag;

  GDK_NOTE (EVENTS, g_print ("gdk_WindowProc: %#x\n", message));

  msg.hwnd = hwnd;
  msg.message = message;
  msg.wParam = wParam;
  msg.lParam = lParam;
  msg.time = GetTickCount ();
  pos = GetMessagePos ();
  msg.pt.x = LOWORD (pos);
  msg.pt.y = HIWORD (pos);

  if (gdk_event_translate (&event, &msg, &ret_val_flag, &ret_val))
    {
#if 1
      /* Compress configure events */
      if (event.any.type == GDK_CONFIGURE)
	{
	  GList *list = queued_events;

	  while (list != NULL
		 && (((GdkEvent *)list->data)->any.type != GDK_CONFIGURE
		     || ((GdkEvent *)list->data)->any.window != event.any.window))
	    list = list->next;
	  if (list != NULL)
	    {
	      *((GdkEvent *)list->data) = event;
	      gdk_window_unref (event.any.window);
	      /* Wake up WaitMessage */
	      PostMessage (NULL, gdk_ping_msg, 0, 0);
	      return FALSE;
	    }
	}
#endif
      eventp = gdk_event_new ();
      *eventp = event;

      gdk_event_queue_append (eventp);
#if 1
      /* Wake up WaitMessage */
      PostMessage (NULL, gdk_ping_msg, 0, 0);
#endif
      if (ret_val_flag)
	return ret_val;
      else
	return FALSE;
    }

  if (ret_val_flag)
    return ret_val;
  else
    return DefWindowProc (hwnd, message, wParam, lParam);
}

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

void
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
  if (g_pipe_readable_msg == 0)
    g_pipe_readable_msg = RegisterWindowMessage ("g-pipe-readable");

  g_source_add (GDK_PRIORITY_EVENTS, TRUE, &event_funcs, NULL, NULL, NULL);

  event_poll_fd.fd = G_WIN32_MSG_HANDLE;
  event_poll_fd.events = G_IO_IN;
  
  g_main_add_poll (&event_poll_fd, GDK_PRIORITY_EVENTS);

  button_click_time[0] = 0;
  button_click_time[1] = 0;
  button_window[0] = NULL;
  button_window[1] = NULL;
  button_number[0] = -1;
  button_number[1] = -1;

  gdk_ping_msg = RegisterWindowMessage ("gdk-ping");
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
  MSG msg;

  return (gdk_event_queue_find_first() || PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE));
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

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  MSG xevent;
  GdkEvent *event;
  GdkWindowPrivate *private = (GdkWindowPrivate *) window;

  g_return_val_if_fail (window != NULL, NULL);
  
  GDK_NOTE (EVENTS, g_print ("gdk_event_get_graphics_expose\n"));

#if 1
  /* Some nasty bugs here, just return NULL for now. */
  return NULL;
#else
  if (GetMessage (&xevent, private->xwindow, WM_PAINT, WM_PAINT))
    {
      event = gdk_event_new ();
      
      if (gdk_event_translate (event, &xevent, NULL, NULL))
	return event;
      else
	gdk_event_free (event);
    }
  
  return NULL;	
#endif
}

/************************
 * Exposure compression *
 ************************/

/* I don't bother with exposure compression on Win32. Windows compresses
 * WM_PAINT events by itself.
 */

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
  gdk_events_queue();

  return gdk_event_unqueue();
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
  GList *tmp_list;
  
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
  
  return (GdkEvent *) new_event;
}

GdkEvent*
gdk_event_copy (GdkEvent *event)
{
  GdkEvent *new_event;
  gchar *s;
  
  g_return_val_if_fail (event != NULL, NULL);
  
  new_event = gdk_event_new ();
  
  *new_event = *event;
  gdk_window_ref (new_event->any.window);
  
  switch (event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      if (event->key.length > 0)
	{
	  s = event->key.string;
	  new_event->key.string = g_malloc (event->key.length + 1);
	  memcpy (new_event->key.string, s, event->key.length + 1);
	}
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
gdk_set_show_events (gint show_events)
{
  if (show_events)
    gdk_debug_flags |= GDK_DEBUG_EVENTS;
  else
    gdk_debug_flags &= ~GDK_DEBUG_EVENTS;
}

gint
gdk_get_show_events (void)
{
  return gdk_debug_flags & GDK_DEBUG_EVENTS;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_grab
 *
 *   Grabs the pointer to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "event_mask" masks only interesting events
 *   "confine_to" limits the cursor movement to the specified window
 *   "cursor" changes the cursor for the duration of the grab
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_pointer_ungrab
 *
 *--------------------------------------------------------------
 */

gint
gdk_pointer_grab (GdkWindow *	  window,
		  gint		  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
  GdkWindowPrivate *window_private;
  HWND xwindow;
  HWND xconfine_to;
  HCURSOR xcursor;
  GdkWindowPrivate *confine_to_private;
  GdkCursorPrivate *cursor_private;
  gint return_val;

  g_return_val_if_fail (window != NULL, 0);
  
  window_private = (GdkWindowPrivate*) window;
  confine_to_private = (GdkWindowPrivate*) confine_to;
  cursor_private = (GdkCursorPrivate*) cursor;
  
  xwindow = window_private->xwindow;
  
  if (!confine_to || confine_to_private->destroyed)
    xconfine_to = NULL;
  else
    xconfine_to = confine_to_private->xwindow;
  
  if (!cursor)
    xcursor = NULL;
  else
    xcursor = cursor_private->xcursor;
  
  if (gdk_input_vtable.grab_pointer)
    return_val = gdk_input_vtable.grab_pointer (window,
						owner_events,
						event_mask,
						confine_to,
						time);
  else
    return_val = Success;
  
  if (return_val == Success)
    {
      if (!window_private->destroyed)
      {
	GDK_NOTE (EVENTS, g_print ("gdk_pointer_grab: %#x %s %#x\n",
				   xwindow,
				   (owner_events ? "TRUE" : "FALSE"),
				   xcursor));
	p_grab_event_mask = event_mask;
	p_grab_owner_events = owner_events != 0;
	p_grab_automatic = FALSE;

#if 0 /* Menus don't work if we use mouse capture. Pity, because many other
       * things work better with mouse capture.
       */
	SetCapture (xwindow);
#endif
	return_val = GrabSuccess;
      }
      else
	return_val = AlreadyGrabbed;
    }
  
  if (return_val == GrabSuccess)
    {
      p_grab_window = window_private;
      p_grab_cursor = xcursor;
    }
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_ungrab
 *
 *   Releases any pointer grab
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
gdk_pointer_ungrab (guint32 time)
{
  if (gdk_input_vtable.ungrab_pointer)
    gdk_input_vtable.ungrab_pointer (time);
#if 0
  if (GetCapture () != NULL)
    ReleaseCapture ();
#endif
  GDK_NOTE (EVENTS, g_print ("gdk_pointer_ungrab\n"));

  p_grab_window = NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_is_grabbed
 *
 *   Tell wether there is an active x pointer grab in effect
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_pointer_is_grabbed (void)
{
  return p_grab_window != NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_grab
 *
 *   Grabs the keyboard to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_keyboard_ungrab
 *
 *--------------------------------------------------------------
 */

gint
gdk_keyboard_grab (GdkWindow *	   window,
		   gint		   owner_events,
		   guint32	   time)
{
  GdkWindowPrivate *window_private;
  gint return_val;
  
  g_return_val_if_fail (window != NULL, 0);
  
  window_private = (GdkWindowPrivate*) window;
  
  GDK_NOTE (EVENTS, g_print ("gdk_keyboard_grab %#x\n",
			     window_private->xwindow));

  if (!window_private->destroyed)
    {
      k_grab_owner_events = owner_events != 0;
      return_val = GrabSuccess;
    }
  else
    return_val = AlreadyGrabbed;

  if (return_val == GrabSuccess)
    k_grab_window = window_private;
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_ungrab
 *
 *   Releases any keyboard grab
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
gdk_keyboard_ungrab (guint32 time)
{
  GDK_NOTE (EVENTS, g_print ("gdk_keyboard_ungrab\n"));

  k_grab_window = NULL;
}

static void
gdk_io_destroy (gpointer data)
{
  GdkIOClosure *closure = data;

  if (closure->notify)
    closure->notify (closure->data);

  g_free (closure);
}

static gboolean  
gdk_io_invoke (GIOChannel   *source,
	       GIOCondition  condition,
	       gpointer      data)
{
  GdkIOClosure *closure = data;
  GdkInputCondition gdk_cond = 0;

  if (condition & (G_IO_IN | G_IO_PRI))
    gdk_cond |= GDK_INPUT_READ;
  if (condition & G_IO_OUT)
    gdk_cond |= GDK_INPUT_WRITE;
  if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
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
    cond |= (G_IO_IN | G_IO_PRI);
  if (condition & GDK_INPUT_WRITE)
    cond |= G_IO_OUT;
  if (condition & GDK_INPUT_EXCEPTION)
    cond |= G_IO_ERR|G_IO_HUP|G_IO_NVAL;

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
gdk_event_apply_filters (MSG      *xevent,
			 GdkEvent *event,
			 GList    *filters)
{
  GdkEventFilter *filter;
  GList *tmp_list;
  GdkFilterReturn result;
  
  tmp_list = filters;
  
  while (tmp_list)
    {
      filter = (GdkEventFilter *) tmp_list->data;
      
      result = (*filter->function) (xevent, event, filter->data);
      if (result !=  GDK_FILTER_CONTINUE)
	return result;
      
      tmp_list = tmp_list->next;
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

static void
synthesize_crossing_events (GdkWindow *window,
			    MSG       *xevent)
{
  GdkEvent *event;
  GdkWindowPrivate *window_private = (GdkWindowPrivate *) window;
  GdkWindowPrivate *curWnd_private = (GdkWindowPrivate *) curWnd;
  
  if (curWnd && (curWnd_private->event_mask & GDK_LEAVE_NOTIFY_MASK))
    {
      GDK_NOTE (EVENTS, g_print ("synthesizing LEAVE_NOTIFY event\n"));

      event = gdk_event_new ();
      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = curWnd;
      gdk_window_ref (event->crossing.window);
      event->crossing.subwindow = NULL;
      event->crossing.time = xevent->time;
      event->crossing.x = curX;
      event->crossing.y = curY;
      event->crossing.x_root = curXroot;
      event->crossing.y_root = curYroot;
      event->crossing.mode = GDK_CROSSING_NORMAL;
      event->crossing.detail = GDK_NOTIFY_UNKNOWN;

      event->crossing.focus = TRUE; /* ??? */
      event->crossing.state = 0; /* ??? */

      gdk_event_queue_append (event);
    }

  if (window_private && (window_private->event_mask & GDK_ENTER_NOTIFY_MASK))
    {
      GDK_NOTE (EVENTS, g_print ("synthesizing ENTER_NOTIFY event\n"));
      
      event = gdk_event_new ();
      event->crossing.type = GDK_ENTER_NOTIFY;
      event->crossing.window = window;
      gdk_window_ref (event->crossing.window);
      event->crossing.subwindow = NULL;
      event->crossing.time = xevent->time;
      event->crossing.x = LOWORD (xevent->lParam);
      event->crossing.y = HIWORD (xevent->lParam);
      event->crossing.x_root = (gfloat) xevent->pt.x;
      event->crossing.y_root = (gfloat) xevent->pt.y;
      event->crossing.mode = GDK_CROSSING_NORMAL;
      event->crossing.detail = GDK_NOTIFY_UNKNOWN;
      
      event->crossing.focus = TRUE; /* ??? */
      event->crossing.state = 0; /* ??? */
      
      gdk_event_queue_append (event);

      if (window_private->extension_events != 0
	  && gdk_input_vtable.enter_event)
	gdk_input_vtable.enter_event (&event->crossing, window);
    }
  
  if (curWnd)
    gdk_window_unref (curWnd);
  curWnd = window;
  gdk_window_ref (curWnd);
}

static gint
gdk_event_translate (GdkEvent *event,
		     MSG      *xevent,
		     gboolean *ret_val_flagp,
		     gint     *ret_valp)
{
  GdkWindow *window;
  GdkWindowPrivate *window_private;

  GdkColormapPrivate *colormap_private;
  HWND owner;
  DWORD dwStyle;
  PAINTSTRUCT paintstruct;
  HDC hdc;
  HBRUSH hbr;
  RECT rect;
  POINT pt;
  GdkWindowPrivate *curWnd_private;
  GdkEventMask mask;
  int button;
  int i, j;
  gchar buf[256];
  gint charcount;
  gint return_val;
  gboolean flag;
  
  return_val = FALSE;
  
  if (ret_val_flagp)
    *ret_val_flagp = FALSE;

  if (xevent->message == gdk_ping_msg)
    {
      /* Messages we post ourselves just to wakeup WaitMessage.  */
      return FALSE;
    }

  window = gdk_window_lookup (xevent->hwnd);
  window_private = (GdkWindowPrivate *) window;
  
  if (xevent->message == g_pipe_readable_msg)
    {
      GDK_NOTE (EVENTS, g_print ("g_pipe_readable_msg: %d %d\n",
				 xevent->wParam, xevent->lParam));

      g_io_channel_win32_pipe_readable (xevent->wParam, xevent->lParam);
      return FALSE;
    }

  if (window != NULL)
    gdk_window_ref (window);
  else
    {
      /* Handle WM_QUIT here ? */
      if (xevent->message == WM_QUIT)
	{
	  GDK_NOTE (EVENTS, g_print ("WM_QUIT: %d\n", xevent->wParam));
	  exit (xevent->wParam);
	}
      else if (xevent->message == WM_MOVE
	       || xevent->message == WM_SIZE)
	{
	  /* It's quite normal to get these messages before we have
	   * had time to register the window in our lookup table, or
	   * when the window is being destroyed and we already have
	   * removed it. Repost the same message to our queue so that
	   * we will get it later when we are prepared.
	   */
	  PostMessage (xevent->hwnd, xevent->message,
		       xevent->wParam, xevent->lParam);
	}
      else if (xevent->message == WM_NCCREATE
	       || xevent->message == WM_CREATE
	       || xevent->message == WM_GETMINMAXINFO
	       || xevent->message == WM_NCCALCSIZE
	       || xevent->message == WM_NCDESTROY
	       || xevent->message == WM_DESTROY)
	{
	  /* Nothing */
	}
      return FALSE;
    }
  
  event->any.window = window;

  if (window_private && window_private->destroyed)
    {
    }
  else
    {
      /* Check for filters for this window */
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

  if (xevent->message == gdk_selection_notify_msg)
    {
      GDK_NOTE (SELECTION, g_print ("gdk_selection_notify_msg: %#x\n",
				    xevent->hwnd));

      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = xevent->wParam;
      event->selection.target = xevent->lParam;
      event->selection.property = gdk_selection_property;
      event->selection.time = xevent->time;

      return_val = window_private && !window_private->destroyed;

      /* Will pass through switch below without match */
    }
  else if (xevent->message == gdk_selection_request_msg)
    {
      GDK_NOTE (SELECTION, g_print ("gdk_selection_request_msg: %#x\n",
				    xevent->hwnd));

      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = gdk_clipboard_atom;
      event->selection.target = GDK_TARGET_STRING;
      event->selection.property = gdk_selection_property;
      event->selection.requestor = (guint32) xevent->hwnd;
      event->selection.time = xevent->time;

      return_val = window_private && !window_private->destroyed;

      /* Again, will pass through switch below without match */
    }
  else if (xevent->message == gdk_selection_clear_msg)
    {
      GDK_NOTE (SELECTION, g_print ("gdk_selection_clear_msg: %#x\n",
				    xevent->hwnd));

      event->selection.type = GDK_SELECTION_CLEAR;
      event->selection.window = window;
      event->selection.selection = xevent->wParam;
      event->selection.time = xevent->time;

      return_val = window_private && !window_private->destroyed;

      /* Once again, we will pass through switch below without match */
    }
  else
    {
      GList *tmp_list;
      GdkFilterReturn result = GDK_FILTER_CONTINUE;

      tmp_list = client_filters;
      while (tmp_list)
	{
	  GdkClientFilter *filter = tmp_list->data;
	  if (filter->type == xevent->message)
	    {
	      GDK_NOTE (EVENTS, g_print ("client filter matched\n"));
	      result = (*filter->function) (xevent, event, filter->data);
	      switch (result)
		{
		case GDK_FILTER_REMOVE:
		  return_val = FALSE;
		  break;

		case GDK_FILTER_TRANSLATE:
		  return_val = TRUE;
		  break;

		case GDK_FILTER_CONTINUE:
		  return_val = TRUE;
		  event->client.type = GDK_CLIENT_EVENT;
		  event->client.window = window;
		  event->client.message_type = xevent->message;
		  event->client.data_format = 0;
		  event->client.data.l[0] = xevent->wParam;
		  event->client.data.l[1] = xevent->lParam;
		  break;
		}
	      goto bypass_switch; /* Ouch */
	    }
	  tmp_list = tmp_list->next;
	}
    }

  switch (xevent->message)
    {
    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
      GDK_NOTE (EVENTS,
		g_print ("WM_SYSKEY%s: %#x  key: %s  %#x %#.08x\n",
			 (xevent->message == WM_SYSKEYUP ? "UP" : "DOWN"),
			 xevent->hwnd,
			 (GetKeyNameText (xevent->lParam, buf,
					  sizeof (buf)) > 0 ?
			  buf : ""),
			 xevent->wParam,
			 xevent->lParam));

      /* Let the system handle Alt-Tab and Alt-Enter */
      if (xevent->wParam == VK_TAB
	  || xevent->wParam == VK_RETURN
	  || xevent->wParam == VK_F4)
	break;
      /* If posted without us having keyboard focus, ignore */
      if (!(xevent->lParam & 0x20000000))
	break;
#if 0
      /* don't generate events for just the Alt key */
      if (xevent->wParam == VK_MENU)
	break;
#endif
      /* Jump to code in common with WM_KEYUP and WM_KEYDOWN */
      goto keyup_or_down;

    case WM_KEYUP:
    case WM_KEYDOWN:
      GDK_NOTE (EVENTS, 
		g_print ("WM_KEY%s: %#x  key: %s  %#x %#.08x\n",
			 (xevent->message == WM_KEYUP ? "UP" : "DOWN"),
			 xevent->hwnd,
			 (GetKeyNameText (xevent->lParam, buf,
					  sizeof (buf)) > 0 ?
			  buf : ""),
			 xevent->wParam,
			 xevent->lParam));

      ignore_WM_CHAR = TRUE;
    keyup_or_down:
      if (k_grab_window != NULL
	  && !k_grab_owner_events)
	{
	  /* Keyboard is grabbed with owner_events FALSE */
	  GDK_NOTE (EVENTS,
		    g_print ("...grabbed, owner_events FALSE, "
			     "sending to %#x\n", k_grab_window->xwindow));
	  event->key.window = (GdkWindow *) k_grab_window;
	}
      else if (window_private
	       && (((xevent->message == WM_KEYUP
		     || xevent->message == WM_SYSKEYUP)
		    && !(window_private->event_mask & GDK_KEY_RELEASE_MASK))
		   || ((xevent->message == WM_KEYDOWN
			|| xevent->message == WM_SYSKEYDOWN)
		       && !(window_private->event_mask & GDK_KEY_PRESS_MASK))))
	{
	  /* Owner window doesn't want it */
	  if (k_grab_window != NULL
	      && k_grab_owner_events)
	    {
	      /* Keyboard is grabbed with owner_events TRUE */
	      GDK_NOTE (EVENTS,
			g_print ("...grabbed, owner_events TRUE, doesn't want it, "
				 "sending to %#x\n", k_grab_window->xwindow));
	      event->key.window = (GdkWindow *) k_grab_window;
	    }
	  else
	    {
	      /* Owner doesn't want it, neither is it grabbed, so
	       * propagate to parent.
	       */
	      if (window_private->parent == (GdkWindow *) &gdk_root_parent)
		break;
	      gdk_window_unref (window);
	      window = window_private->parent;
	      gdk_window_ref (window);
	      window_private = (GdkWindowPrivate *) window;
	      GDK_NOTE (EVENTS, g_print ("...propagating to %#x\n",
					 window_private->xwindow));
	      goto keyup_or_down;
	    }
	}
	      
      switch (xevent->wParam)
	{
	case VK_LBUTTON:
	  event->key.keyval = GDK_Pointer_Button1; break;
	case VK_RBUTTON:
	  event->key.keyval = GDK_Pointer_Button3; break;
	case VK_MBUTTON:
	  event->key.keyval = GDK_Pointer_Button2; break;
	case VK_CANCEL:
	  event->key.keyval = GDK_Cancel; break;
	case VK_BACK:
	  event->key.keyval = GDK_BackSpace; break;
	case VK_TAB:
	  event->key.keyval = GDK_Tab; break;
	case VK_CLEAR:
	  event->key.keyval = GDK_Clear; break;
	case VK_RETURN:
	  event->key.keyval = GDK_Return; break;
	case VK_SHIFT:
	  event->key.keyval = GDK_Shift_L; break;
	case VK_CONTROL:
	  if (xevent->lParam & 0x01000000)
	    event->key.keyval = GDK_Control_R;
	  else
	    event->key.keyval = GDK_Control_L;
	  break;
	case VK_MENU:
	  if (xevent->lParam & 0x01000000)
	    {
	      /* AltGr key comes in as Control+Right Alt */
	      if (GetKeyState (VK_CONTROL) < 0)
		{
		  ignore_WM_CHAR = FALSE;
		  is_AltGr_key = TRUE;
		}
	      event->key.keyval = GDK_Alt_R;
	    }
	  else
	    event->key.keyval = GDK_Alt_L;
	  break;
	case VK_PAUSE:
	  event->key.keyval = GDK_Pause; break;
	case VK_CAPITAL:
	  event->key.keyval = GDK_Caps_Lock; break;
	case VK_ESCAPE:
	  event->key.keyval = GDK_Escape; break;
	case VK_PRIOR:
	  event->key.keyval = GDK_Prior; break;
	case VK_NEXT:
	  event->key.keyval = GDK_Next; break;
	case VK_END:
	  event->key.keyval = GDK_End; break;
	case VK_HOME:
	  event->key.keyval = GDK_Home; break;
	case VK_LEFT:
	  event->key.keyval = GDK_Left; break;
	case VK_UP:
	  event->key.keyval = GDK_Up; break;
	case VK_RIGHT:
	  event->key.keyval = GDK_Right; break;
	case VK_DOWN:
	  event->key.keyval = GDK_Down; break;
	case VK_SELECT:
	  event->key.keyval = GDK_Select; break;
	case VK_PRINT:
	  event->key.keyval = GDK_Print; break;
	case VK_EXECUTE:
	  event->key.keyval = GDK_Execute; break;
	case VK_INSERT:
	  event->key.keyval = GDK_Insert; break;
	case VK_DELETE:
	  event->key.keyval = GDK_Delete; break;
	case VK_HELP:
	  event->key.keyval = GDK_Help; break;
	case VK_NUMPAD0:
	case VK_NUMPAD1:
	case VK_NUMPAD2:
	case VK_NUMPAD3:
	case VK_NUMPAD4:
	case VK_NUMPAD5:
	case VK_NUMPAD6:
	case VK_NUMPAD7:
	case VK_NUMPAD8:
	case VK_NUMPAD9:
	  /* Apparently applications work better if we just pass numpad digits
	   * on as real digits? So wait for the WM_CHAR instead.
	   */
	  ignore_WM_CHAR = FALSE;
	  break;
	case VK_MULTIPLY:
	  event->key.keyval = GDK_KP_Multiply; break;
	case VK_ADD:
	  event->key.keyval = GDK_KP_Add; break;
	case VK_SEPARATOR:
	  event->key.keyval = GDK_KP_Separator; break;
	case VK_SUBTRACT:
	  event->key.keyval = GDK_KP_Subtract; break;
	case VK_DECIMAL:
#if 0
	  event->key.keyval = GDK_KP_Decimal; break;
#else
	  /* The keypad decimal key should also be passed on as the decimal
	   * sign ('.' or ',' depending on the Windows locale settings,
	   * apparently). So wait for the WM_CHAR here, also.
	   */
	  ignore_WM_CHAR = FALSE;
	  break;
#endif
	case VK_DIVIDE:
	  event->key.keyval = GDK_KP_Divide; break;
	case VK_F1:
	  event->key.keyval = GDK_F1; break;
	case VK_F2:
	  event->key.keyval = GDK_F2; break;
	case VK_F3:
	  event->key.keyval = GDK_F3; break;
	case VK_F4:
	  event->key.keyval = GDK_F4; break;
	case VK_F5:
	  event->key.keyval = GDK_F5; break;
	case VK_F6:
	  event->key.keyval = GDK_F6; break;
	case VK_F7:
	  event->key.keyval = GDK_F7; break;
	case VK_F8:
	  event->key.keyval = GDK_F8; break;
	case VK_F9:
	  event->key.keyval = GDK_F9; break;
	case VK_F10:
	  event->key.keyval = GDK_F10; break;
	case VK_F11:
	  event->key.keyval = GDK_F11; break;
	case VK_F12:
	  event->key.keyval = GDK_F12; break;
	case VK_F13:
	  event->key.keyval = GDK_F13; break;
	case VK_F14:
	  event->key.keyval = GDK_F14; break;
	case VK_F15:
	  event->key.keyval = GDK_F15; break;
	case VK_F16:
	  event->key.keyval = GDK_F16; break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (GetKeyState (VK_CONTROL) < 0)
	    /* Control-digits won't come in as a WM_CHAR */
	    event->key.keyval = GDK_0 + (xevent->wParam - '0');
	  else
	    {
	      ignore_WM_CHAR = FALSE;
	      event->key.keyval = GDK_VoidSymbol;
	    }
	  break;
	default:
	  if (xevent->message == WM_SYSKEYDOWN || xevent->message == WM_SYSKEYUP)
	    {
	      event->key.keyval = xevent->wParam;
	    }
	  else
	    {
	      ignore_WM_CHAR = FALSE;
	      event->key.keyval = GDK_VoidSymbol;
	    }
	  break;
	}

      if (!ignore_WM_CHAR)
	break;

      is_AltGr_key = FALSE;
      event->key.type = ((xevent->message == WM_KEYDOWN
			  || xevent->message == WM_SYSKEYDOWN) ?
			 GDK_KEY_PRESS : GDK_KEY_RELEASE);
      event->key.window = window;
      event->key.time = xevent->time;
      event->key.state = 0;
      if (GetKeyState (VK_SHIFT) < 0)
	event->key.state |= GDK_SHIFT_MASK;
      if (GetKeyState (VK_CAPITAL) & 0x1)
	event->key.state |= GDK_LOCK_MASK;
      if (GetKeyState (VK_CONTROL) < 0)
	event->key.state |= GDK_CONTROL_MASK;
      if (xevent->wParam != VK_MENU && GetKeyState (VK_MENU) < 0)
	event->key.state |= GDK_MOD1_MASK;
      return_val = window_private && !window_private->destroyed;
      event->key.string = NULL;
      event->key.length = 0;
      break;

    case WM_CHAR:
      GDK_NOTE (EVENTS, 
		g_print ("WM_CHAR: %#x  char: %#x %#.08x  %s\n",
			 xevent->hwnd,
			 xevent->wParam,
			 xevent->lParam,
			 (ignore_WM_CHAR ? "ignored" : "")));

      if (ignore_WM_CHAR)
	{
	  ignore_WM_CHAR = FALSE;
	  break;
	}

    wm_char:
      /* This doesn't handle the rather theorethical case that a window
       * wants key presses but still wants releases to be propagated,
       * for instance.
       */
      if (k_grab_window != NULL
	  && !k_grab_owner_events)
	{
	  /* Keyboard is grabbed with owner_events FALSE */
	  GDK_NOTE (EVENTS,
		    g_print ("...grabbed, owner_events FALSE, "
			     "sending to %#x\n", k_grab_window->xwindow));
	  event->key.window = (GdkWindow *) k_grab_window;
	}
      else if (window_private
	       && !(window_private->event_mask & (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK)))
	{
	  /* Owner window doesn't want it */
	  if (k_grab_window != NULL
	      && k_grab_owner_events)
	    {
	      /* Keyboard is grabbed with owner_events TRUE */
	      GDK_NOTE (EVENTS,
			g_print ("...grabbed, owner_events TRUE, doesn't want it, "
				 "sending to %#x\n", k_grab_window->xwindow));
	      event->key.window = (GdkWindow *) k_grab_window;
	    }
	  else
	    {
	      /* Owner doesn't want it, neither is it grabbed, so
	       * propagate to parent.
	       */
	      if (window_private->parent == (GdkWindow *) &gdk_root_parent)
		g_assert_not_reached (); /* Should've been handled above */

	      gdk_window_unref (window);
	      window = window_private->parent;
	      gdk_window_ref (window);
	      window_private = (GdkWindowPrivate *) window;
	      GDK_NOTE (EVENTS, g_print ("...propagating to %#x\n",
					 window_private->xwindow));
	      goto wm_char;
	    }
	}
      
      return_val = window_private && !window_private->destroyed;
      if (return_val && (window_private->event_mask & GDK_KEY_RELEASE_MASK))
	{
	  /* Return the release event, and maybe append the press
	   * event to the queued_events list (from which it will vbe
	   * fetched before the release event).
	   */
	  event->key.type = GDK_KEY_RELEASE;
	  event->key.keyval = xevent->wParam;
	  event->key.window = window;
	  event->key.time = xevent->time;
	  event->key.state = 0;
	  if (GetKeyState (VK_SHIFT) < 0)
	    event->key.state |= GDK_SHIFT_MASK;
	  if (GetKeyState (VK_CAPITAL) & 0x1)
	    event->key.state |= GDK_LOCK_MASK;
	  if (is_AltGr_key)
	    ;
	  else if (GetKeyState (VK_CONTROL) < 0)
	    {
	      event->key.state |= GDK_CONTROL_MASK;
	      if (event->key.keyval < ' ')
		event->key.keyval += '@';
	    }
	  else if (event->key.keyval < ' ')
	    {
	      event->key.state |= GDK_CONTROL_MASK;
	      event->key.keyval += '@';
	    }
	  if (!is_AltGr_key && GetKeyState (VK_MENU) < 0)
	    event->key.state |= GDK_MOD1_MASK;
	  event->key.string = g_malloc (2);
	  event->key.length = 1;
	  event->key.string[0] = xevent->wParam; /* ??? */
	  event->key.string[1] = 0;

	  if (window_private->event_mask & GDK_KEY_PRESS_MASK)
	    {
	      /* Append also a GDK_KEY_PRESS event to the pushback list.  */
	      GdkEvent *event2 = gdk_event_copy (event);
	      event2->key.type = GDK_KEY_PRESS;
	      charcount = xevent->lParam & 0xFFFF;
	      if (charcount > sizeof (buf)- 1)
		charcount = sizeof (buf) - 1;
	      g_free (event2->key.string);
	      event2->key.string = g_malloc (charcount + 1);
	      for (i = 0; i < charcount; i++)
		event2->key.string[i] = event->key.keyval;
	      event2->key.string[charcount] = 0;
	      event2->key.length = charcount;

	      gdk_event_queue_append (event2);
	    }
	}
      else if (return_val && (window_private->event_mask & GDK_KEY_PRESS_MASK))
	{
	  /* Return just the GDK_KEY_PRESS event. */
	  event->key.type = GDK_KEY_PRESS;
	  charcount = xevent->lParam & 0xFFFF;
	  if (charcount > sizeof (buf)- 1)
	    charcount = sizeof (buf) - 1;
	  event->key.keyval = xevent->wParam;
	  event->key.window = window;
	  event->key.time = xevent->time;
	  event->key.state = 0;
	  if (GetKeyState (VK_SHIFT) < 0)
	    event->key.state |= GDK_SHIFT_MASK;
	  if (GetKeyState (VK_CAPITAL) & 0x1)
	    event->key.state |= GDK_LOCK_MASK;
	  if (is_AltGr_key)
	    ;
	  else if (GetKeyState (VK_CONTROL) < 0)
	    {
	      event->key.state |= GDK_CONTROL_MASK;
	      if (event->key.keyval < ' ')
		event->key.keyval += '@';
	    }
	  else if (event->key.keyval < ' ')
	    {
	      event->key.state |= GDK_CONTROL_MASK;
	      event->key.keyval += '@';
	    }
	  if (!is_AltGr_key && GetKeyState (VK_MENU) < 0)
	    event->key.state |= GDK_MOD1_MASK;
	  event->key.string = g_malloc (charcount + 1);
	  for (i = 0; i < charcount; i++)
	    event->key.string[i] = event->key.keyval;
	  event->key.string[charcount] = 0;
	  event->key.length = charcount;
	}
      else
	return_val = FALSE;
      is_AltGr_key = FALSE;
      break;

    case WM_LBUTTONDOWN:
      button = 1;
      goto buttondown0;
    case WM_MBUTTONDOWN:
      button = 2;
      goto buttondown0;
    case WM_RBUTTONDOWN:
      button = 3;

    buttondown0:
      GDK_NOTE (EVENTS, 
		g_print ("WM_%cBUTTONDOWN: %#x  x,y: %d %d  button: %d\n",
			 " LMR"[button],
			 xevent->hwnd,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam),
			 button));

      if (window_private
	  && (window_private->extension_events != 0)
	  && gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      if (window != curWnd)
	synthesize_crossing_events (window, xevent);

      event->button.type = GDK_BUTTON_PRESS;
    buttondown:
      event->button.window = window;
      if (window_private)
	mask = window_private->event_mask;
      else
	mask = 0;		/* ??? */

      if (p_grab_window != NULL
	   && !p_grab_owner_events)
	{
	  /* Pointer is grabbed with owner_events FALSE */
	  GDK_NOTE (EVENTS, g_print ("...grabbed, owner_events FALSE\n"));
	  mask = p_grab_event_mask;
	  if (!(mask & GDK_BUTTON_PRESS_MASK))
	    /* Grabber doesn't want it */
	    break;
	  else
	    event->button.window = (GdkWindow *) p_grab_window;
	  GDK_NOTE (EVENTS, g_print ("...sending to %#x\n",
				     p_grab_window->xwindow));
	}
      else if (window_private
	       && !(mask & GDK_BUTTON_PRESS_MASK))
	{
	  /* Owner window doesn't want it */
	  if (p_grab_window != NULL
	      && p_grab_owner_events)
	    {
	      /* Pointer is grabbed wíth owner_events TRUE */ 
	      GDK_NOTE (EVENTS, g_print ("...grabbed, owner_events TRUE, doesn't want it\n"));
	      mask = p_grab_event_mask;
	      if (!(mask & GDK_BUTTON_PRESS_MASK))
		/* Grabber doesn't want it either */
		break;
	      else
		event->button.window = (GdkWindow *) p_grab_window;
	      GDK_NOTE (EVENTS, g_print ("...sending to %#x\n",
					 p_grab_window->xwindow));
	    }
	  else
	    {
	      /* Owner doesn't want it, neither is it grabbed, so
	       * propagate to parent.
	       */
	      /* Yes, this code is duplicated twice below. So shoot me. */
	      if (window_private->parent == (GdkWindow *) &gdk_root_parent)
		break;
	      pt.x = LOWORD (xevent->lParam);
	      pt.y = HIWORD (xevent->lParam);
	      ClientToScreen (window_private->xwindow, &pt);
	      gdk_window_unref (window);
	      window = window_private->parent;
	      gdk_window_ref (window);
	      window_private = (GdkWindowPrivate *) window;
	      ScreenToClient (window_private->xwindow, &pt);
	      xevent->lParam = MAKELPARAM (pt.x, pt.y);
	      GDK_NOTE (EVENTS, g_print ("...propagating to %#x\n",
					 window_private->xwindow));
	      goto buttondown; /* What did Dijkstra say? */
	    }
	}

      /* Emulate X11's automatic active grab */
      if (!p_grab_window)
	{
	  /* No explicit active grab, let's start one automatically */
	  GDK_NOTE (EVENTS, g_print ("...automatic grab started\n"));
	  gdk_pointer_grab (window, TRUE, window_private->event_mask,
			    NULL, NULL, 0);
	  p_grab_automatic = TRUE;
	}

      event->button.time = xevent->time;
      event->button.x = LOWORD (xevent->lParam);
      event->button.y = HIWORD (xevent->lParam);
      event->button.x_root = (gfloat)xevent->pt.x;
      event->button.y_root = (gfloat)xevent->pt.y;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = 0;
      if (xevent->wParam & MK_CONTROL)
	event->button.state |= GDK_CONTROL_MASK;
      if (xevent->wParam & MK_LBUTTON)
	event->button.state |= GDK_BUTTON1_MASK;
      if (xevent->wParam & MK_MBUTTON)
	event->button.state |= GDK_BUTTON2_MASK;
      if (xevent->wParam & MK_RBUTTON)
	event->button.state |= GDK_BUTTON3_MASK;
      if (xevent->wParam & MK_SHIFT)
	event->button.state |= GDK_SHIFT_MASK;
      if (GetKeyState (VK_MENU) < 0)
	event->button.state |= GDK_MOD1_MASK;
      if (GetKeyState (VK_CAPITAL) & 0x1)
	event->button.state |= GDK_LOCK_MASK;
      event->button.button = button;
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
      return_val = window_private && !window_private->destroyed;
      if (return_val
	  && p_grab_window != NULL
	  && event->any.window == (GdkWindow *) p_grab_window
	  && p_grab_window != window_private)
	{
	  /* Translate coordinates to grabber */
	  pt.x = event->button.x;
	  pt.y = event->button.y;
	  ClientToScreen (window_private->xwindow, &pt);
	  ScreenToClient (p_grab_window->xwindow, &pt);
	  event->button.x = pt.x;
	  event->button.y = pt.y;
	  GDK_NOTE (EVENTS, g_print ("...new coords are +%d+%d\n", pt.x, pt.y));
	}
      break;

    case WM_LBUTTONUP:
      button = 1;
      goto buttonup0;
    case WM_MBUTTONUP:
      button = 2;
      goto buttonup0;
    case WM_RBUTTONUP:
      button = 3;

    buttonup0:
      GDK_NOTE (EVENTS, 
		g_print ("WM_%cBUTTONUP: %#x  x,y: %d %d  button: %d\n",
			 " LMR"[button],
			 xevent->hwnd,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam),
			 button));

      if (window_private
	  && (window_private->extension_events != 0)
	  && gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      if (window != curWnd)
	synthesize_crossing_events (window, xevent);

      event->button.type = GDK_BUTTON_RELEASE;
    buttonup:
      event->button.window = window;
      if (window_private)
	mask = window_private->event_mask;
      else
	mask = 0;

      if (p_grab_window != NULL
	   && !p_grab_owner_events)
	{
	  /* Pointer is grabbed with owner_events FALSE */
	  GDK_NOTE (EVENTS, g_print ("...grabbed, owner_events FALSE\n"));
	  mask = p_grab_event_mask;
	  if (!(mask & GDK_BUTTON_RELEASE_MASK))
	    /* Grabber doesn't want it */
	    break;
	  else
	    event->button.window = (GdkWindow *) p_grab_window;
	  GDK_NOTE (EVENTS, g_print ("...sending to %#x\n",
				     p_grab_window->xwindow));
	}
      else if (window_private
	       && !(mask & GDK_BUTTON_RELEASE_MASK))
	{
	  /* Owner window doesn't want it */
	  if (p_grab_window != NULL
	      && p_grab_owner_events)
	    {
	      /* Pointer is grabbed wíth owner_events TRUE */
	      GDK_NOTE (EVENTS, g_print ("...grabbed, owner_events TRUE, doesn't want it\n"));
	      mask = p_grab_event_mask;
	      if (!(mask & GDK_BUTTON_RELEASE_MASK))
		/* Grabber doesn't want it */
		break;
	      else
		event->button.window = (GdkWindow *) p_grab_window;
	      GDK_NOTE (EVENTS, g_print ("...sending to %#x\n",
					 p_grab_window->xwindow));
	    }
	  else
	    {
	      /* Owner doesn't want it, neither is it grabbed, so
	       * propagate to parent.
	       */
	      if (window_private->parent == (GdkWindow *) &gdk_root_parent)
		break;
	      pt.x = LOWORD (xevent->lParam);
	      pt.y = HIWORD (xevent->lParam);
	      ClientToScreen (window_private->xwindow, &pt);
	      gdk_window_unref (window);
	      window = window_private->parent;
	      gdk_window_ref (window);
	      window_private = (GdkWindowPrivate *) window;
	      ScreenToClient (window_private->xwindow, &pt);
	      xevent->lParam = MAKELPARAM (pt.x, pt.y);
	      GDK_NOTE (EVENTS, g_print ("...propagating to %#x\n",
					 window_private->xwindow));
	      goto buttonup;
	    }
	}

      event->button.time = xevent->time;
      event->button.x = LOWORD (xevent->lParam);
      event->button.y = HIWORD (xevent->lParam);
      event->button.x_root = (gfloat)xevent->pt.x;
      event->button.y_root = (gfloat)xevent->pt.y;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = 0;
      if (xevent->wParam & MK_CONTROL)
	event->button.state |= GDK_CONTROL_MASK;
      if (xevent->wParam & MK_LBUTTON)
	event->button.state |= GDK_BUTTON1_MASK;
      if (xevent->wParam & MK_MBUTTON)
	event->button.state |= GDK_BUTTON2_MASK;
      if (xevent->wParam & MK_RBUTTON)
	event->button.state |= GDK_BUTTON3_MASK;
      if (xevent->wParam & MK_SHIFT)
	event->button.state |= GDK_SHIFT_MASK;
      event->button.button = button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;
      return_val = window_private && !window_private->destroyed;
      if (return_val
	  && p_grab_window != NULL
	  && event->any.window == (GdkWindow *) p_grab_window
	  && p_grab_window != window_private)
	{
	  /* Translate coordinates to grabber */
	  pt.x = event->button.x;
	  pt.y = event->button.y;
	  ClientToScreen (window_private->xwindow, &pt);
	  ScreenToClient (p_grab_window->xwindow, &pt);
	  event->button.x = pt.x;
	  event->button.y = pt.y;
	  GDK_NOTE (EVENTS, g_print ("...new coords are +%d+%d\n", pt.x, pt.y));
	}
      if (p_grab_window != NULL
	  && p_grab_automatic
	  && (event->button.state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) == 0)
	gdk_pointer_ungrab (0);
      break;

    case WM_MOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print ("WM_MOUSEMOVE: %#x  %#x +%d+%d\n",
			 xevent->hwnd, xevent->wParam,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

#if 0
      /* Try hard not to generate events for windows that shouldn't
	 get any.  This is hard because we don't want pushbuttons to
	 highlight when the cursor moves over them if the window is
	 inactive. We dont want tooltips windows to be active. OTOH,
	 also menus are popup windows, but they definitely should
	 get events. Aw shit. Skip this.
       */
      dwStyle = GetWindowLong (xevent->hwnd, GWL_STYLE);
      if (active == NULL ||
	  !(active == xevent->hwnd
	    || (dwStyle & WS_POPUP)
	    || IsChild (active, xevent->hwnd)))
	break;
#else
      { /* HB: only process mouse move messages
         * if we own the active window.
         */
	  DWORD ProcessID_ActWin;
	  DWORD ProcessID_this;

	  GetWindowThreadProcessId(GetActiveWindow(), &ProcessID_ActWin);
	  GetWindowThreadProcessId(xevent->hwnd, &ProcessID_this);
	  if (ProcessID_ActWin != ProcessID_this)
          break;
     }
#endif
      if (window != curWnd)
	synthesize_crossing_events (window, xevent);

      if (window_private
	  && (window_private->extension_events != 0)
	  && gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

    mousemotion:
      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      if (window_private)
	mask = window_private->event_mask;
      else
	mask = 0;

      if (p_grab_window
	  && !p_grab_owner_events)
	{
	  /* Pointer is grabbed with owner_events FALSE */
	  GDK_NOTE (EVENTS,
		    g_print ("...grabbed, owner_events FALSE\n"));
	  mask = p_grab_event_mask;
	  if (!((mask & GDK_POINTER_MOTION_MASK)
		|| ((xevent->wParam & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON))
		    && (mask & GDK_BUTTON_MOTION_MASK))
		|| ((xevent->wParam & MK_LBUTTON)
		    && (mask & GDK_BUTTON1_MOTION_MASK))
		|| ((xevent->wParam & MK_MBUTTON)
		    && (mask & GDK_BUTTON2_MOTION_MASK))
		|| ((xevent->wParam & MK_RBUTTON)
		    && (mask & GDK_BUTTON3_MOTION_MASK))))
	    break;
	  else
	    event->motion.window = (GdkWindow *) p_grab_window;
	  GDK_NOTE (EVENTS, g_print ("...sending to %#x\n",
				     p_grab_window->xwindow));
	}
      else if (window_private
	       && !((mask & GDK_POINTER_MOTION_MASK)
		    || ((xevent->wParam & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON))
			&& (mask & GDK_BUTTON_MOTION_MASK))
		    || ((xevent->wParam & MK_LBUTTON)
			&& (mask & GDK_BUTTON1_MOTION_MASK))
		    || ((xevent->wParam & MK_MBUTTON)
			&& (mask & GDK_BUTTON2_MOTION_MASK))
		    || ((xevent->wParam & MK_RBUTTON)
			&& (mask & GDK_BUTTON3_MOTION_MASK))))
	{
	  /* Owner window doesn't want it */
	  if (p_grab_window != NULL
	      && p_grab_owner_events)
	    {
	      /* Pointer is grabbed wíth owner_events TRUE */
	      GDK_NOTE (EVENTS, g_print ("...grabbed, owner_events TRUE, doesn't want it\n"));
	      mask = p_grab_event_mask;
	      if (!((p_grab_event_mask & GDK_POINTER_MOTION_MASK)
		    || ((xevent->wParam & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON))
			&& (mask & GDK_BUTTON_MOTION_MASK))
		    || ((xevent->wParam & MK_LBUTTON)
			&& (mask & GDK_BUTTON1_MOTION_MASK))
		    || ((xevent->wParam & MK_MBUTTON)
			&& (mask & GDK_BUTTON2_MOTION_MASK))
		    || ((xevent->wParam & MK_RBUTTON)
			&& (mask & GDK_BUTTON3_MOTION_MASK))))
		/* Grabber doesn't want it either */
		break;
	      else
		event->motion.window = (GdkWindow *) p_grab_window;
	      GDK_NOTE (EVENTS, g_print ("...sending to %#x\n",
					 p_grab_window->xwindow));
	    }
	  else
	    {
	      /* Owner doesn't want it, neither is it grabbed, so
	       * propagate to parent.
	       */
	      if (window_private->parent == (GdkWindow *) &gdk_root_parent)
		break;
	      pt.x = LOWORD (xevent->lParam);
	      pt.y = HIWORD (xevent->lParam);
	      ClientToScreen (window_private->xwindow, &pt);
	      gdk_window_unref (window);
	      window = window_private->parent;
	      gdk_window_ref (window);
	      window_private = (GdkWindowPrivate *) window;
	      ScreenToClient (window_private->xwindow, &pt);
	      xevent->lParam = MAKELPARAM (pt.x, pt.y);
	      GDK_NOTE (EVENTS, g_print ("...propagating to %#x\n",
					 window_private->xwindow));
	      goto mousemotion;
	    }
	}

      event->motion.time = xevent->time;
      event->motion.x = curX = LOWORD (xevent->lParam);
      event->motion.y = curY = HIWORD (xevent->lParam);
      event->motion.x_root = xevent->pt.x;
      event->motion.y_root = xevent->pt.y;
      curXroot = event->motion.x_root;
      curYroot = event->motion.y_root;
      event->motion.pressure = 0.5;
      event->motion.xtilt = 0;
      event->motion.ytilt = 0;
      event->button.state = 0;
      if (xevent->wParam & MK_CONTROL)
	event->button.state |= GDK_CONTROL_MASK;
      if (xevent->wParam & MK_LBUTTON)
	event->button.state |= GDK_BUTTON1_MASK;
      if (xevent->wParam & MK_MBUTTON)
	event->button.state |= GDK_BUTTON2_MASK;
      if (xevent->wParam & MK_RBUTTON)
	event->button.state |= GDK_BUTTON3_MASK;
      if (xevent->wParam & MK_SHIFT)
	event->button.state |= GDK_SHIFT_MASK;
      if (mask & GDK_POINTER_MOTION_HINT_MASK)
	event->motion.is_hint = NotifyHint;
      else
	event->motion.is_hint = NotifyNormal;
      event->motion.source = GDK_SOURCE_MOUSE;
      event->motion.deviceid = GDK_CORE_POINTER;

      return_val = window_private && !window_private->destroyed;
      if (return_val
	  && p_grab_window != NULL
	  && event->any.window == (GdkWindow *) p_grab_window
	  && p_grab_window != window_private)
	{
	  /* Translate coordinates to grabber */
	  pt.x = event->motion.x;
	  pt.y = event->motion.y;
	  ClientToScreen (window_private->xwindow, &pt);
	  ScreenToClient (p_grab_window->xwindow, &pt);
	  event->motion.x = pt.x;
	  event->motion.y = pt.y;
	  GDK_NOTE (EVENTS, g_print ("...new coords are +%d+%d\n", pt.x, pt.y));
	}
      break;

    case WM_NCMOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print ("WM_NCMOUSEMOVE: %#x  x,y: %d %d\n",
			 xevent->hwnd,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));
#if 0
      if (active == NULL || active != xevent->hwnd)
	break;
#endif
      curWnd_private = (GdkWindowPrivate *) curWnd;
      if (curWnd != NULL
	  && (curWnd_private->event_mask & GDK_LEAVE_NOTIFY_MASK))
	{
	  GDK_NOTE (EVENTS, g_print ("...synthesizing LEAVE_NOTIFY event\n"));

	  event->crossing.type = GDK_LEAVE_NOTIFY;
	  event->crossing.window = curWnd;
	  event->crossing.subwindow = NULL;
	  event->crossing.time = xevent->time;
	  event->crossing.x = curX;
	  event->crossing.y = curY;
	  event->crossing.x_root = curXroot;
	  event->crossing.y_root = curYroot;
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;

	  event->crossing.focus = TRUE; /* ??? */
	  event->crossing.state = 0; /* ??? */
	  gdk_window_unref (curWnd);
	  curWnd = NULL;

	  return_val = TRUE;
	}
      break;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
      if (window_private
	  && !(window_private->event_mask & GDK_FOCUS_CHANGE_MASK))
	break;

      GDK_NOTE (EVENTS, g_print ("WM_%sFOCUS: %#x\n",
				 (xevent->message == WM_SETFOCUS ? "SET" : "KILL"),
				 xevent->hwnd));
      
      event->focus_change.type = GDK_FOCUS_CHANGE;
      event->focus_change.window = window;
      event->focus_change.in = (xevent->message == WM_SETFOCUS);
      return_val = window_private && !window_private->destroyed;
      break;
#if 0
    case WM_ACTIVATE:
      GDK_NOTE (EVENTS, g_print ("WM_ACTIVATE: %#x  %d\n",
				 xevent->hwnd, LOWORD (xevent->wParam)));
      if (LOWORD (xevent->wParam) == WA_INACTIVE)
	active = (HWND) xevent->lParam;
      else
	active = xevent->hwnd;
      break;
#endif
    case WM_ERASEBKGND:
      GDK_NOTE (EVENTS, g_print ("WM_ERASEBKGND: %#x  dc %#x\n",
				 xevent->hwnd, xevent->wParam));
      
      if (!window_private || window_private->destroyed)
	break;
      colormap_private = (GdkColormapPrivate *) window_private->colormap;
      hdc = (HDC) xevent->wParam;
      if (colormap_private
	  && colormap_private->xcolormap->rc_palette)
	{
	  int k;

	  if (SelectPalette (hdc,  colormap_private->xcolormap->palette,
			     FALSE) == NULL)
	    g_warning ("WM_ERASEBKGND: SelectPalette failed");
	  if ((k = RealizePalette (hdc)) == GDI_ERROR)
	    g_warning ("WM_ERASEBKGND: RealizePalette failed");
#if 0
	  g_print ("WM_ERASEBKGND: selected %#x, realized %d colors\n",
		   colormap_private->xcolormap->palette, k);
#endif
	}
      *ret_val_flagp = TRUE;
      *ret_valp = 1;

      if (window_private->bg_type == GDK_WIN32_BG_TRANSPARENT)
	break;

      if (window_private->bg_type == GDK_WIN32_BG_PARENT_RELATIVE)
	{
	  /* If this window should have the same background as the
	   * parent, fetch the parent. (And if the same goes for
	   * the parent, fetch the grandparent, etc.)
	   */
	  while (window_private
		 && window_private->bg_type == GDK_WIN32_BG_PARENT_RELATIVE)
	    window_private = (GdkWindowPrivate *) window_private->parent;
	}

      if (window_private->bg_type == GDK_WIN32_BG_PIXEL)
	{
	  COLORREF bg;
	  GetClipBox (hdc, &rect);
	  GDK_NOTE (EVENTS, g_print ("...%dx%d@+%d+%d BG_PIXEL %s\n",
				     rect.right - rect.left,
				     rect.bottom - rect.top,
				     rect.left, rect.top,
				     gdk_color_to_string (&window_private->bg_pixel)));
#ifdef MULTIPLE_WINDOW_CLASSES
	  bg = PALETTEINDEX (window_private->bg_pixel.pixel);
#else
	  bg = GetNearestColor (hdc, RGB (window_private->bg_pixel.red >> 8,
					  window_private->bg_pixel.green >> 8,
					  window_private->bg_pixel.blue >> 8));
#endif
	  hbr = CreateSolidBrush (bg);
#if 0
	  g_print ("...CreateSolidBrush (%.08x) = %.08x\n", bg, hbr);
#endif
	  if (!FillRect (hdc, &rect, hbr))
	    g_warning ("WM_ERASEBKGND: FillRect failed");
	  DeleteObject (hbr);
	}
      else if (window_private->bg_type == GDK_WIN32_BG_PIXMAP)
	{
	  GdkPixmapPrivate *pixmap_private;
	  HDC bgdc;
	  HGDIOBJ oldbitmap;

	  pixmap_private = (GdkPixmapPrivate *) window_private->bg_pixmap;
	  GetClipBox (hdc, &rect);

	  if (pixmap_private->width <= 8
	      && pixmap_private->height <= 8)
	    {
	      GDK_NOTE (EVENTS, g_print ("...small pixmap, using brush\n"));
	      hbr = CreatePatternBrush (pixmap_private->xwindow);
	      if (!FillRect (hdc, &rect, hbr))
		g_warning ("WM_ERASEBKGND: FillRect failed");
	      DeleteObject (hbr);
	    }
	  else
	    {
	      GDK_NOTE (EVENTS,
			g_print ("...blitting pixmap %#x (%dx%d) "
				 "all over the place,\n"
				 "...clip box = %dx%d@+%d+%d\n",
				 pixmap_private->xwindow,
				 pixmap_private->width, pixmap_private->height,
				 rect.right - rect.left, rect.bottom - rect.top,
				 rect.left, rect.top));

	      if (!(bgdc = CreateCompatibleDC (hdc)))
		{
		  g_warning ("WM_ERASEBKGND: CreateCompatibleDC failed");
		  break;
		}
	      if (!(oldbitmap = SelectObject (bgdc, pixmap_private->xwindow)))
		{
		  g_warning ("WM_ERASEBKGND: SelectObject failed");
		  DeleteDC (bgdc);
		  break;
		}
	      i = 0;
	      while (i < rect.right)
		{
		  j = 0;
		  while (j < rect.bottom)
		    {
		      if (i + pixmap_private->width >= rect.left
			  && j + pixmap_private->height >= rect.top)
			{
			  if (!BitBlt (hdc, i, j,
				       pixmap_private->width, pixmap_private->height,
				       bgdc, 0, 0, SRCCOPY))
			    {
			      g_warning ("WM_ERASEBKGND: BitBlt failed");
			      goto loopexit;
			    }
			}
		      j += pixmap_private->height;
		    }
		  i += pixmap_private->width;
		}
	    loopexit:
	      SelectObject (bgdc, oldbitmap);
	      DeleteDC (bgdc);
	    }
	}
      else
	{
	  GDK_NOTE (EVENTS, g_print ("...BLACK_BRUSH (?)\n"));
#ifdef MULTIPLE_WINDOW_CLASSES
	  hbr = (HBRUSH) GetClassLong (window_private->xwindow,
				       GCL_HBRBACKGROUND);
#else
	  hbr = GetStockObject (BLACK_BRUSH);
#endif
	  GetClipBox (hdc, &rect);
	  if (!FillRect (hdc, &rect, hbr))
	    g_warning ("WM_ERASEBKGND: FillRect failed");
	}
      break;

    case WM_PAINT:
      hdc = BeginPaint (xevent->hwnd, &paintstruct);

      GDK_NOTE (EVENTS,
		g_print ("WM_PAINT: %#x  %dx%d@+%d+%d %s dc %#x\n",
			 xevent->hwnd,
			 paintstruct.rcPaint.right - paintstruct.rcPaint.left,
			 paintstruct.rcPaint.bottom - paintstruct.rcPaint.top,
			 paintstruct.rcPaint.left, paintstruct.rcPaint.top,
			 (paintstruct.fErase ? "erase" : ""),
			 hdc));

      EndPaint (xevent->hwnd, &paintstruct);

      if (window_private
	  && !(window_private->event_mask & GDK_EXPOSURE_MASK))
	break;

      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = paintstruct.rcPaint.left;
      event->expose.area.y = paintstruct.rcPaint.top;
      event->expose.area.width = paintstruct.rcPaint.right - paintstruct.rcPaint.left;
      event->expose.area.height = paintstruct.rcPaint.bottom - paintstruct.rcPaint.top;
      event->expose.count = 1;

      return_val = window_private && !window_private->destroyed;
      break;

#ifndef MULTIPLE_WINDOW_CLASSES
    case WM_SETCURSOR:
      GDK_NOTE (EVENTS, g_print ("WM_SETCURSOR: %#x %#x %#x\n",
				 xevent->hwnd,
				 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      return_val = FALSE;
      if (LOWORD (xevent->lParam) != HTCLIENT)
	break;
      if (p_grab_window != NULL && p_grab_cursor != NULL)
	{
	  GDK_NOTE (EVENTS, g_print ("...SetCursor(%#x)\n", p_grab_cursor));
	  SetCursor (p_grab_cursor);
	}
      else if (window_private
	       && !window_private->destroyed
	       && window_private->xcursor)
	{
	  GDK_NOTE (EVENTS, g_print ("...SetCursor(%#x)\n",
				     window_private->xcursor));
	  SetCursor (window_private->xcursor);
	}
      *ret_val_flagp = TRUE;
      *ret_valp = FALSE;
      break;
#endif

#if 0
    case WM_QUERYOPEN:
      GDK_NOTE (EVENTS, g_print ("WM_QUERYOPEN: %#x\n",
				 xevent->hwnd));
      *ret_val_flagp = TRUE;
      *ret_valp = TRUE;

      if (window_private
	  && !(window_private->event_mask & GDK_STRUCTURE_MASK))
	break;

      event->any.type = GDK_MAP;
      event->any.window = window;

      return_val = window_private && !window_private->destroyed;
      break;
#endif

#if 1
    case WM_SHOWWINDOW:
      GDK_NOTE (EVENTS, g_print ("WM_SHOWWINDOW: %#x  %d\n",
				 xevent->hwnd,
				 xevent->wParam));

      if (window_private
	  && !(window_private->event_mask & GDK_STRUCTURE_MASK))
	break;

      event->any.type = (xevent->wParam ? GDK_MAP : GDK_UNMAP);
      event->any.window = window;

      if (event->any.type == GDK_UNMAP
	  && p_grab_window == window_private)
	gdk_pointer_ungrab (xevent->time);

      if (event->any.type == GDK_UNMAP
	  && k_grab_window == window_private)
	gdk_keyboard_ungrab (xevent->time);

      return_val = window_private && !window_private->destroyed;
      break;
#endif
    case WM_SIZE:
      GDK_NOTE (EVENTS,
		g_print ("WM_SIZE: %#x  %s %dx%d\n",
			 xevent->hwnd,
			 (xevent->wParam == SIZE_MAXHIDE ? "MAXHIDE" :
			  (xevent->wParam == SIZE_MAXIMIZED ? "MAXIMIZED" :
			   (xevent->wParam == SIZE_MAXSHOW ? "MAXSHOW" :
			    (xevent->wParam == SIZE_MINIMIZED ? "MINIMIZED" :
			     (xevent->wParam == SIZE_RESTORED ? "RESTORED" : "?"))))),
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      if (window_private
	  && !(window_private->event_mask & GDK_STRUCTURE_MASK))
	break;
      if (window_private != NULL
	  && xevent->wParam == SIZE_MINIMIZED)
	{
#if 1
	  event->any.type = GDK_UNMAP;
	  event->any.window = window;

	  if (p_grab_window == window_private)
	    gdk_pointer_ungrab (xevent->time);

	  if (k_grab_window == window_private)
	    gdk_keyboard_ungrab (xevent->time);

	  return_val = !window_private->destroyed;
#endif
	}
      else if (window_private != NULL
	       && (xevent->wParam == SIZE_RESTORED
		   || xevent->wParam == SIZE_MAXIMIZED)
#if 1
	       && window_private->window_type != GDK_WINDOW_CHILD
#endif
								 )
	{
	  if (LOWORD (xevent->lParam) == 0)
	    break;

	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  pt.x = 0;
	  pt.y = 0;
	  ClientToScreen (xevent->hwnd, &pt);
	  event->configure.x = pt.x;
	  event->configure.y = pt.y;
	  event->configure.width = LOWORD (xevent->lParam);
	  event->configure.height = HIWORD (xevent->lParam);
	  window_private->x = event->configure.x;
	  window_private->y = event->configure.y;
	  window_private->width = event->configure.width;
	  window_private->height = event->configure.height;
	  if (window_private->resize_count > 1)
	    window_private->resize_count -= 1;
	  
	  return_val = !window_private->destroyed;
	  if (return_val
	      && window_private->extension_events != 0
	      && gdk_input_vtable.configure_event)
	    gdk_input_vtable.configure_event (&event->configure, window);
	}
      break;

    case WM_SIZING:
      GDK_NOTE (EVENTS, g_print ("WM_SIZING: %#x\n", xevent->hwnd));
      if (ret_val_flagp == NULL)
	  g_warning ("ret_val_flagp is NULL but we got a WM_SIZING?");
      else if (window_private != NULL
	       && window_private->hint_flags &
	       (GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE))
	{
	  LPRECT lprc = (LPRECT) xevent->lParam;

	  if (window_private->hint_flags & GDK_HINT_MIN_SIZE)
	    {
	      gint w = lprc->right - lprc->left;
	      gint h = lprc->bottom - lprc->top;

	      if (w < window_private->hint_min_width)
		{
		  if (xevent->wParam == WMSZ_BOTTOMLEFT
		      || xevent->wParam == WMSZ_LEFT
		      || xevent->wParam == WMSZ_TOPLEFT)
		    lprc->left = lprc->right - window_private->hint_min_width;
		  else
		    lprc->right = lprc->left + window_private->hint_min_width;
		  *ret_val_flagp = TRUE;
		  *ret_valp = TRUE;
		}
	      if (h < window_private->hint_min_height)
		{
		  if (xevent->wParam == WMSZ_BOTTOMLEFT
		      || xevent->wParam == WMSZ_BOTTOM
		      || xevent->wParam == WMSZ_BOTTOMRIGHT)
		    lprc->bottom = lprc->top + window_private->hint_min_height;
		  else
		    lprc->top = lprc->bottom - window_private->hint_min_height;
		  *ret_val_flagp = TRUE;
		  *ret_valp = TRUE;
		}
	    }
	  if (window_private->hint_flags & GDK_HINT_MAX_SIZE)
	    {
	      gint w = lprc->right - lprc->left;
	      gint h = lprc->bottom - lprc->top;

	      if (w > window_private->hint_max_width)
		{
		  if (xevent->wParam == WMSZ_BOTTOMLEFT
		      || xevent->wParam == WMSZ_LEFT
		      || xevent->wParam == WMSZ_TOPLEFT)
		    lprc->left = lprc->right - window_private->hint_max_width;
		  else
		    lprc->right = lprc->left + window_private->hint_max_width;
		  *ret_val_flagp = TRUE;
		  *ret_valp = TRUE;
		}
	      if (h > window_private->hint_max_height)
		{
		  if (xevent->wParam == WMSZ_BOTTOMLEFT
		      || xevent->wParam == WMSZ_BOTTOM
		      || xevent->wParam == WMSZ_BOTTOMRIGHT)
		    lprc->bottom = lprc->top + window_private->hint_max_height;
		  else
		    lprc->top = lprc->bottom - window_private->hint_max_height;
		  *ret_val_flagp = TRUE;
		  *ret_valp = TRUE;
		}
	    }
	}
      break;

    case WM_MOVE:
      GDK_NOTE (EVENTS, g_print ("WM_MOVE: %#x  +%d+%d\n",
				 xevent->hwnd,
				 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      if (window_private
	  && !(window_private->event_mask & GDK_STRUCTURE_MASK))
	break;
      if (window_private != NULL
	  && window_private->window_type != GDK_WINDOW_CHILD)
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.x = LOWORD (xevent->lParam);
	  event->configure.y = HIWORD (xevent->lParam);
	  GetClientRect (xevent->hwnd, &rect);
	  event->configure.width = rect.right;
	  event->configure.height = rect.bottom;
	  window_private->x = event->configure.x;
	  window_private->y = event->configure.y;
	  window_private->width = event->configure.width;
	  window_private->height = event->configure.height;
	  
	  return_val = !window_private->destroyed;
	}
      break;

    case WM_CLOSE:
      GDK_NOTE (EVENTS, g_print ("WM_CLOSE: %#x\n", xevent->hwnd));
      event->any.type = GDK_DELETE;
      event->any.window = window;
      
      return_val = window_private && !window_private->destroyed;
      break;

#if 0
    /* No, don't use delayed rendering after all. It works only if the
     * delayed SetClipboardData is called from the WindowProc, it
     * seems. (The #else part below is test code for that. It succeeds
     * in setting the clipboard data. But if I call SetClipboardData
     * in gdk_property_change (as a consequence of the
     * GDK_SELECTION_REQUEST event), it fails.  I deduce that this is
     * because delayed rendering requires that SetClipboardData is
     * called in the window procedure.)
     */
    case WM_RENDERFORMAT:
    case WM_RENDERALLFORMATS:
      flag = FALSE;
      GDK_NOTE (EVENTS, flag = TRUE);
      GDK_NOTE (SELECTION, flag = TRUE);
      if (flag)
	g_print ("WM_%s: %#x %#x (%s)\n",
		 (xevent->message == WM_RENDERFORMAT ? "RENDERFORMAT" :
		  "RENDERALLFORMATS"),
		 xevent->hwnd,
		 xevent->wParam,
		 (xevent->wParam == CF_TEXT ? "CF_TEXT" :
		  (xevent->wParam == CF_DIB ? "CF_DIB" :
		   (xevent->wParam == CF_UNICODETEXT ? "CF_UNICODETEXT" :
		    (GetClipboardFormatName (xevent->wParam, buf, sizeof (buf)), buf)))));

#if 0
      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = gdk_clipboard_atom;
      if (xevent->wParam == CF_TEXT)
	event->selection.target = GDK_TARGET_STRING;
      else
	{
	  GetClipboardFormatName (xevent->wParam, buf, sizeof (buf));
	  event->selection.target = gdk_atom_intern (buf, FALSE);
	}
      event->selection.property = gdk_selection_property;
      event->selection.requestor = (guint32) xevent->hwnd;
      event->selection.time = xevent->time;
      return_val = window_private && !window_private->destroyed;
#else
      /* Test code, to see if SetClipboardData works when called from
       * the window procedure.
       */
      {
	HGLOBAL hdata = GlobalAlloc (GMEM_MOVEABLE|GMEM_DDESHARE, 10);
	char *ptr = GlobalLock (hdata);
	strcpy (ptr, "Huhhaa");
	GlobalUnlock (hdata);
	if (!SetClipboardData (CF_TEXT, hdata))
	  g_print ("SetClipboardData failed: %d\n", GetLastError ());
      }
      *ret_valp = 0;
      *ret_val_flagp = TRUE;
      return_val = FALSE;
#endif
      break;
#endif /* No delayed rendering */

    case WM_DESTROY:
      GDK_NOTE (EVENTS, g_print ("WM_DESTROY: %#x\n", xevent->hwnd));
      event->any.type = GDK_DESTROY;
      event->any.window = window;
      if (window != NULL && window == curWnd)
	{
	  gdk_window_unref (curWnd);
	  curWnd = NULL;
	}

      if (p_grab_window == window_private)
	gdk_pointer_ungrab (xevent->time);

      if (k_grab_window == window_private)
	gdk_keyboard_ungrab (xevent->time);

      return_val = window_private && !window_private->destroyed;
      break;

      /* Handle WINTAB events here, as we know that gdkinput.c will
       * use the fixed WT_DEFBASE as lcMsgBase, and we thus can use the
       * constants as case labels.
       */
    case WT_PACKET:
      GDK_NOTE (EVENTS, g_print ("WT_PACKET: %d %#x\n",
				 xevent->wParam, xevent->lParam));
      goto wintab;
      
    case WT_CSRCHANGE:
      GDK_NOTE (EVENTS, g_print ("WT_CSRCHANGE: %d %#x\n",
				 xevent->wParam, xevent->lParam));
      goto wintab;
      
    case WT_PROXIMITY:
      GDK_NOTE (EVENTS,
		g_print ("WT_PROXIMITY: %#x %d %d\n",
			 xevent->wParam,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));
    wintab:
      return_val = gdk_input_vtable.other_event(event, xevent);
      break;
    }

bypass_switch:

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

static void
gdk_events_queue (void)
{
  GList *node;
  GdkEvent *event;
  MSG msg;

  GDK_NOTE (EVENTS, g_print ("gdk_events_queue: %s\n",
			     (queued_events ? "yes" : "none")));

  while (!gdk_event_queue_find_first()
	 && PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
      GDK_NOTE (EVENTS, g_print ("gdk_events_queue: got event\n"));
      TranslateMessage (&msg);

      event = gdk_event_new ();
      
      event->any.type = GDK_NOTHING;
      event->any.window = NULL;
      event->any.send_event = FALSE;

      ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

      gdk_event_queue_append (event);
      node = queued_tail;

      if (gdk_event_translate (event, &msg, NULL, NULL))
	  ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
      else
	{
	  DefWindowProc (msg.hwnd, msg.message, msg.wParam, msg.lParam);
	  gdk_event_queue_remove_link (node);
	  g_list_free_1 (node);
	  gdk_event_free (event);
	}
    }
}

static gboolean  
gdk_event_prepare (gpointer  source_data, 
		   GTimeVal *current_time,
		   gint     *timeout)
{
  MSG msg;
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  *timeout = -1;

  GDK_NOTE (EVENTS, g_print ("gdk_event_prepare\n"));

  retval = (gdk_event_queue_find_first () != NULL)
	      || PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_check (gpointer  source_data,
		 GTimeVal *current_time)
{
  MSG msg;
  gboolean retval;
  
  GDK_NOTE (EVENTS, g_print ("gdk_event_check\n"));

  GDK_THREADS_ENTER ();

  if (event_poll_fd.revents & G_IO_IN)
    retval = (gdk_event_queue_find_first () != NULL)
	      || PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE);
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
 
  GDK_NOTE (EVENTS, g_print ("gdk_event_dispatch\n"));

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
  /* XXX */
  return FALSE;
}

void
gdk_event_send_clientmessage_toall (GdkEvent *event)
{
  /* XXX */
}

