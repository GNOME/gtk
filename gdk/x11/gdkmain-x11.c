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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "../config.h"

#include <ctype.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H_ */

#define XLIB_ILLEGAL_ACCESS
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/cursorfont.h>
#include "gdk.h"
#include "gdkprivate.h"
#include "gdkinput.h"


#ifndef X_GETTIMEOFDAY
#define X_GETTIMEOFDAY(tv)  gettimeofday (tv, NULL)
#endif /* X_GETTIMEOFDAY */


#define DOUBLE_CLICK_TIME      250
#define TRIPLE_CLICK_TIME      500
#define DOUBLE_CLICK_DIST      5
#define TRIPLE_CLICK_DIST      5


#ifndef NO_FD_SET
#  define SELECT_MASK fd_set
#else
#  ifndef _AIX
     typedef long fd_mask;
#  endif
#  if defined(_IBMR2)
#    define SELECT_MASK void
#  else
#    define SELECT_MASK int
#  endif
#endif


typedef struct _GdkInput      GdkInput;
typedef struct _GdkPredicate  GdkPredicate;

struct _GdkInput
{
  gint tag;
  gint source;
  GdkInputCondition condition;
  GdkInputFunction function;
  gpointer data;
};

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

/* 
 * Private function declarations
 */
static gint      gdk_event_wait         (void);
static gint      gdk_event_translate    (GdkEvent     *event, 
				         XEvent       *xevent);
static Bool      gdk_event_get_type     (Display      *display, 
					 XEvent       *xevent, 
					 XPointer      arg);
static void      gdk_synthesize_click   (GdkEvent     *event, 
					 gint          nclicks);

static void      gdk_dnd_drag_begin     (GdkWindow    *initial_window);
static void      gdk_dnd_drag_enter     (Window        dest);
static void      gdk_dnd_drag_leave     (Window        dest);
static void      gdk_dnd_drag_end       (Window        dest, 
					 GdkPoint      coords);
static GdkAtom   gdk_dnd_check_types    (GdkWindow    *window,
					 XEvent       *xevent);
static void      gdk_print_atom         (GdkAtom       anatom);

/* 
 * old junk from offix, we might use it though so leave it 
 */
static Window       gdk_drop_get_client_window   (Display     *dpy, 
						  Window       win);
static GdkWindow *  gdk_drop_get_real_window     (GdkWindow   *w, 
						  guint16     *x,
						  guint16     *y);
static void         gdk_exit_func                (void);
static int          gdk_x_error                  (Display     *display, 
						  XErrorEvent *error);
static int          gdk_x_io_error               (Display     *display);
static RETSIGTYPE   gdk_signal                   (int          signum);


/* Private variable declarations
 */
static int initialized = 0;                         /* 1 if the library is initialized,
						     * 0 otherwise.
						     */
static int connection_number = 0;                   /* The file descriptor number of our
						     *  connection to the X server. This
						     *  is used so that we may determine
						     *  when events are pending by using
						     *  the "select" system call.
						     */

static gint received_destroy_notify = FALSE;        /* Did we just receive a destroy notify
						     *  event? If so, we need to actually
						     *  destroy the window which received
						     *  it now.
						     */
static GdkWindow *window_to_destroy = NULL;         /* If we previously received a destroy
						     *  notify event then this is the window
						     *  which received that event.
						     */

static struct timeval start;                        /* The time at which the library was
						     *  last initialized.
						     */
static struct timeval timer;                        /* Timeout interval to use in the call
						     *  to "select". This is used in
						     *  conjunction with "timerp" to create
						     *  a maximum time to wait for an event
						     *  to arrive.
						     */
static struct timeval *timerp;                      /* The actual timer passed to "select"
						     *  This may be NULL, in which case
						     *  "select" will block until an event
						     *  arrives.
						     */
static guint32 timer_val;                           /* The timeout length as specified by
						     *  the user in milliseconds.
						     */
static GList *inputs;                               /* A list of the input file descriptors
						     *  that we care about. Each list node
						     *  contains a GdkInput struct that describes
						     *  when we are interested in the specified
						     *  file descriptor. That is, when it is
						     *  available for read, write or has an
						     *  exception pending.
						     */
static guint32 button_click_time[2];                /* The last 2 button click times. Used
						     *  to determine if the latest button click
						     *  is part of a double or triple click.
						     */
static GdkWindow *button_window[2];                 /* The last 2 windows to receive button presses.
						     *  Also used to determine if the latest button
						     *  click is part of a double or triple click.
						     */
static guint button_number[2];                      /* The last 2 buttons to be pressed.
						     */

#define OTHER_XEVENT_BUFSIZE 4
static XEvent other_xevent[OTHER_XEVENT_BUFSIZE];   /* XEvents passed along to user  */
static int other_xevent_i = 0;
static GList *putback_events = NULL;

static gulong base_id;
static gint autorepeat;


/*
 *--------------------------------------------------------------
 * gdk_init
 *
 *   Initialize the library for use.
 *
 * Arguments:
 *   "argc" is the number of arguments.
 *   "argv" is an array of strings.
 *
 * Results:
 *   "argc" and "argv" are modified to reflect any arguments
 *   which were not handled. (Such arguments should either
 *   be handled by the application or dismissed).
 *
 * Side effects:
 *   The library is initialized.
 *
 *--------------------------------------------------------------
 */

void
gdk_init (int    *argc,
	  char ***argv)
{
  XKeyboardState keyboard_state;
  int synchronize;
  int i, j, k;
  XClassHint *class_hint;
  int argc_orig = *argc;
  char **argv_orig;

  argv_orig = malloc ((argc_orig + 1) * sizeof (char*));
  for (i = 0; i < argc_orig; i++)
    argv_orig[i] = g_strdup ((*argv)[i]);
  argv_orig[argc_orig] = NULL;

  X_GETTIMEOFDAY (&start);

  signal (SIGHUP, gdk_signal);
  signal (SIGINT, gdk_signal);
  signal (SIGQUIT, gdk_signal);
  signal (SIGBUS, gdk_signal);
  signal (SIGSEGV, gdk_signal);
  signal (SIGPIPE, gdk_signal);
  signal (SIGTERM, gdk_signal);

  gdk_display_name = NULL;

  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);

  synchronize = FALSE;

  if (argc && argv)
    {
      if (*argc > 0)
	gdk_progname = (*argv)[0];

      for (i = 1; i < *argc;)
	{
	  if (strcmp ("--display", (*argv)[i]) == 0)
	    {
	      (*argv)[i] = NULL;

	      if ((i + 1) < *argc)
		{
		  gdk_display_name = g_strdup ((*argv)[i + 1]);
		  (*argv)[i + 1] = NULL;
		  i += 1;
		}
	    }
	  else if (strcmp ("--sync", (*argv)[i]) == 0)
	    {
	      (*argv)[i] = NULL;
	      synchronize = TRUE;
	    }
	  else if (strcmp ("--show-events", (*argv)[i]) == 0)
	    {
	      (*argv)[i] = NULL;
	      gdk_show_events = TRUE;
	    }
	  else if (strcmp ("--no-show-events", (*argv)[i]) == 0)
	    {
	      (*argv)[i] = NULL;
	      gdk_show_events = FALSE;
	    }
	  else if (strcmp ("--no-xshm", (*argv)[i]) == 0)
	    {
	      (*argv)[i] = NULL;
	      gdk_use_xshm = FALSE;
	    }
	  else if (strcmp ("--debug-level", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc)
		{
		  (*argv)[i++] = NULL;
		  gdk_debug_level = atoi ((*argv)[i]);
		  (*argv)[i] = NULL;
		}
	    }
	  else if (strcmp ("-name", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc)
		{
		  (*argv)[i++] = NULL;
		  gdk_progname = (*argv)[i];
		  (*argv)[i] = NULL;
		}
	    }
	  else if (strcmp ("-class", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc)
		{
		  (*argv)[i++] = NULL;
		  gdk_progclass = (*argv)[i];
		  (*argv)[i] = NULL;
		}
	    }
#ifdef XINPUT_GXI
	  else if (strcmp ("--gxid_host", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc)
		{
		  (*argv)[i++] = NULL;
		  gdk_input_gxid_host = ((*argv)[i]);
		  (*argv)[i] = NULL;
		}
	    }
	  else if (strcmp ("--gxid_port", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc)
		{
		  (*argv)[i++] = NULL;
		  gdk_input_gxid_port = atoi ((*argv)[i]);
		  (*argv)[i] = NULL;
		}
	    }
#endif
	  i += 1;
	}

      for (i = 1; i < *argc; i++)
	{
	  for (k = i; k < *argc; k++)
	    if ((*argv)[k] != NULL)
	      break;

	  if (k > i)
	    {
	      k -= i;
	      for (j = i + k; j < *argc; j++)
		(*argv)[j-k] = (*argv)[j];
	      *argc -= k;
	    }
	}
    }
  else
    {
      gdk_progname = "<unknown>";
    }

  gdk_display = XOpenDisplay (gdk_display_name);
  if (!gdk_display)
    g_error ("cannot open display: %s", XDisplayName (gdk_display_name));

  /* This is really crappy. We have to look into the display structure
   *  to find the base resource id. This is only needed for recording
   *  and playback of events.
   */
  /* base_id = RESOURCE_BASE; */
  base_id = 0;
  if (gdk_show_events)
    g_print ("base id: %lu\n", base_id);

  connection_number = ConnectionNumber (gdk_display);
  if (gdk_debug_level >= 1)
    g_print ("connection number: %d\n", connection_number);

  if (synchronize)
    XSynchronize (gdk_display, True);

  gdk_screen = DefaultScreen (gdk_display);
  gdk_root_window = RootWindow (gdk_display, gdk_screen);

  gdk_leader_window = XCreateSimpleWindow(gdk_display, gdk_root_window,
					  10, 10, 10, 10, 0, 0 , 0);
  class_hint = XAllocClassHint();
  class_hint->res_name = gdk_progname;
  class_hint->res_class = gdk_progclass;
  XSetClassHint(gdk_display, gdk_leader_window, class_hint);
  XSetCommand(gdk_display, gdk_leader_window, argv_orig, argc_orig);
  XFree (class_hint);

  gdk_wm_delete_window = XInternAtom (gdk_display, "WM_DELETE_WINDOW", True);
  gdk_wm_take_focus = XInternAtom (gdk_display, "WM_TAKE_FOCUS", True);
  gdk_wm_protocols = XInternAtom (gdk_display, "WM_PROTOCOLS", True);
  gdk_wm_window_protocols[0] = gdk_wm_delete_window;
  gdk_wm_window_protocols[1] = gdk_wm_take_focus;
  gdk_selection_property = XInternAtom (gdk_display, "GDK_SELECTION", False);

  gdk_dnd.gdk_XdeEnter = gdk_atom_intern("_XDE_ENTER", FALSE);
  gdk_dnd.gdk_XdeLeave = gdk_atom_intern("_XDE_LEAVE", FALSE);
  gdk_dnd.gdk_XdeRequest = gdk_atom_intern("_XDE_REQUEST", FALSE);
  gdk_dnd.gdk_XdeDataAvailable = gdk_atom_intern("_XDE_DATA_AVAILABLE", FALSE);
  gdk_dnd.gdk_XdeTypelist = gdk_atom_intern("_XDE_TYPELIST", FALSE);
  gdk_dnd.gdk_cursor_dragdefault = XCreateFontCursor(gdk_display, XC_bogosity);
  gdk_dnd.gdk_cursor_dragok = XCreateFontCursor(gdk_display, XC_heart);

  XGetKeyboardControl (gdk_display, &keyboard_state);
  autorepeat = keyboard_state.global_auto_repeat;

  timer.tv_sec = 0;
  timer.tv_usec = 0;
  timerp = NULL;

  button_click_time[0] = 0;
  button_click_time[1] = 0;
  button_window[0] = NULL;
  button_window[1] = NULL;
  button_number[0] = -1;
  button_number[1] = -1;

  if (ATEXIT (gdk_exit_func))
    g_warning ("unable to register exit function");

  gdk_visual_init ();
  gdk_window_init ();
  gdk_image_init ();
  gdk_input_init ();

  initialized = 1;
}

/*
 *--------------------------------------------------------------
 * gdk_exit
 *
 *   Restores the library to an un-itialized state and exits
 *   the program using the "exit" system call.
 *
 * Arguments:
 *   "errorcode" is the error value to pass to "exit".
 *
 * Results:
 *   Allocated structures are freed and the program exits
 *   cleanly.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_exit (int errorcode)
{
 /* de-initialisation is done by the gdk_exit_funct(),
    no need to do this here (Alex J.) */
 exit (errorcode);
}

/*
 *--------------------------------------------------------------
 * gdk_set_locale
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gchar*
gdk_set_locale ()
{
  if (!setlocale (LC_ALL,""))
    g_print ("locale not supported by C library\n");

  if (!XSupportsLocale ())
    {
      g_print ("locale not supported by Xlib, locale set to C\n");
      setlocale (LC_ALL, "C");
    }

  if (!XSetLocaleModifiers (""))
    {
      g_print ("can not set locale modifiers\n");
    }

  return setlocale (LC_ALL,NULL);
}

/*
 *--------------------------------------------------------------
 * gdk_events_pending
 *
 *   Returns the number of events pending on the queue.
 *   These events have already been read from the server
 *   connection.
 *
 * Arguments:
 *
 * Results:
 *   Returns the number of events on XLib's event queue.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_events_pending ()
{
  return XPending (gdk_display);
}

/*
 *--------------------------------------------------------------
 * gdk_event_get
 *
 *   Gets the next event.
 *
 * Arguments:
 *   "event" is used to hold the received event.
 *   If "event" is NULL an event is received as normal
 *   however it is not placed in "event" (and thus no
 *   error occurs).
 *
 * Results:
 *   Returns TRUE if an event was received that we care about
 *   and FALSE otherwise. This function will also return
 *   before an event is received if the timeout interval
 *   runs out.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_event_get (GdkEvent     *event,
	       GdkEventFunc  pred,
	       gpointer      data)
{
  GdkEvent *temp_event;
  GdkPredicate event_pred;
  GList *temp_list;
  XEvent xevent;

  /* If the last event we received was a destroy notify
   *  event then we will actually destroy the "gdk" data
   *  structures now. We don't want to destroy them at the
   *  time of receiving the event since the main program
   *  may try to access them and may need to destroy user
   *  data that has been attached to the window
   */
  if (received_destroy_notify)
    {
      if (gdk_show_events)
	g_print ("destroying window:\twindow: %ld\n",
		 ((GdkWindowPrivate*) window_to_destroy)->xwindow - base_id);

      gdk_window_real_destroy (window_to_destroy);
      received_destroy_notify = FALSE;
      window_to_destroy = NULL;
    }

  /* Initially we haven't received an event and want to
   *  return FALSE. If "event" is non-NULL, then initialize
   *  it to the nothing event.
   */
  if (event)
    {
      event->any.type = GDK_NOTHING;
      event->any.window = NULL;
      event->any.send_event = FALSE;
    }

  if (pred)
    {
      temp_list = putback_events;
      while (temp_list)
	{
	  temp_event = temp_list->data;

	  if ((* pred) (temp_event, data))
	    {
	      if (event)
		*event = *temp_event;
	      putback_events = g_list_remove_link (putback_events, temp_list);
	      g_list_free (temp_list);
	      return TRUE;
	    }

	  temp_list = temp_list->next;
	}

      event_pred.func = pred;
      event_pred.data = data;

      if (XCheckIfEvent (gdk_display, &xevent, gdk_event_get_type, (XPointer) &event_pred))
	if (event)
	  return gdk_event_translate (event, &xevent);
    }
  else
    {
      if (putback_events)
	{
	  temp_event = putback_events->data;
	  *event = *temp_event;

	  temp_list = putback_events;
	  putback_events = putback_events->next;
	  if (putback_events)
	    putback_events->prev = NULL;

	  temp_list->next = NULL;
	  temp_list->prev = NULL;
	  g_list_free (temp_list);
	  g_free (temp_event);

	  return TRUE;
	}

      /* Wait for an event to occur or the timeout to elapse.
       * If an event occurs "gdk_event_wait" will return TRUE.
       *  If the timeout elapses "gdk_event_wait" will return
       *  FALSE.
       */
      if (gdk_event_wait ())
	{
	  /* If we get here we can rest assurred that an event
	   *  has occurred. Read it.
	   */
	  XNextEvent (gdk_display, &xevent);

	  event->any.send_event = xevent.xany.send_event;

	  /* If "event" non-NULL.
	   */
	  if (event)
	    return gdk_event_translate (event, &xevent);
	}
    }

  return FALSE;
}

void
gdk_event_put (GdkEvent *event)
{
  GdkEvent *new_event;

  g_return_if_fail (event != NULL);

  new_event = g_new (GdkEvent, 1);
  *new_event = *event;

  putback_events = g_list_prepend (putback_events, new_event);
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

static GMemChunk *event_chunk;

GdkEvent*
gdk_event_copy (GdkEvent *event)
{
  GdkEvent *new_event;
  
  g_return_val_if_fail (event != NULL, NULL);

  if (event_chunk == NULL)
    event_chunk = g_mem_chunk_new ("events",
				   sizeof (GdkEvent),
				   4096,
				   G_ALLOC_AND_FREE);

  new_event = g_chunk_new (GdkEvent, event_chunk);
  *new_event = *event;
  gdk_window_ref (new_event->any.window);
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
  g_assert (event_chunk != NULL);
  g_return_if_fail (event != NULL);

  gdk_window_unref (event->any.window);
  g_mem_chunk_free (event_chunk, event);
}

/*
 *--------------------------------------------------------------
 * gdk_set_debug_level
 *
 *   Sets the debugging level.
 *
 * Arguments:
 *   "level" is the new debugging level.
 *
 * Results:
 *
 * Side effects:
 *   Other function calls to "gdk" use the debugging
 *   level to determine what kind of debugging information
 *   to print out.
 *
 *--------------------------------------------------------------
 */

void
gdk_set_debug_level (int level)
{
  gdk_debug_level = level;
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
gdk_set_show_events (int show_events)
{
  gdk_show_events = show_events;
}

void
gdk_set_use_xshm (gint use_xshm)
{
  gdk_use_xshm = use_xshm;
}

gint
gdk_get_debug_level ()
{
  return gdk_debug_level;
}

gint
gdk_get_show_events ()
{
  return gdk_show_events;
}

gint
gdk_get_use_xshm ()
{
  return gdk_use_xshm;
}

/*
 *--------------------------------------------------------------
 * gdk_time_get
 *
 *   Get the number of milliseconds since the library was
 *   initialized.
 *
 * Arguments:
 *
 * Results:
 *   The time since the library was initialized is returned.
 *   This time value is accurate to milliseconds even though
 *   a more accurate time down to the microsecond could be
 *   returned.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

guint32
gdk_time_get ()
{
  struct timeval end;
  struct timeval elapsed;
  guint32 milliseconds;

  X_GETTIMEOFDAY (&end);

  if (start.tv_usec > end.tv_usec)
    {
      end.tv_usec += 1000000;
      end.tv_sec--;
    }
  elapsed.tv_sec = end.tv_sec - start.tv_sec;
  elapsed.tv_usec = end.tv_usec - start.tv_usec;

  milliseconds = (elapsed.tv_sec * 1000) + (elapsed.tv_usec / 1000);

  return milliseconds;
}

/*
 *--------------------------------------------------------------
 * gdk_timer_get
 *
 *   Returns the current timer.
 *
 * Arguments:
 *
 * Results:
 *   Returns the current timer interval. This interval is
 *   in units of milliseconds.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

guint32
gdk_timer_get ()
{
  return timer_val;
}

/*
 *--------------------------------------------------------------
 * gdk_timer_set
 *
 *   Sets the timer interval.
 *
 * Arguments:
 *   "milliseconds" is the new value for the timer.
 *
 * Results:
 *
 * Side effects:
 *   Calls to "gdk_event_get" will last for a maximum
 *   of time of "milliseconds". However, a value of 0
 *   milliseconds will cause "gdk_event_get" to block
 *   indefinately until an event is received.
 *
 *--------------------------------------------------------------
 */

void
gdk_timer_set (guint32 milliseconds)
{
  timer_val = milliseconds;
  timer.tv_sec = milliseconds / 1000;
  timer.tv_usec = (milliseconds % 1000) * 1000;

}

void
gdk_timer_enable ()
{
  timerp = &timer;
}

void
gdk_timer_disable ()
{
  timerp = NULL;
}

gint
gdk_input_add (gint              source,
	       GdkInputCondition condition,
	       GdkInputFunction  function,
	       gpointer          data)
{
  static gint next_tag = 1;
  GList *list;
  GdkInput *input;
  gint tag;

  tag = 0;
  list = inputs;

  while (list)
    {
      input = list->data;
      list = list->next;

      if ((input->source == source) && (input->condition == condition))
	{
	  input->function = function;
	  input->data = data;
	  tag = input->tag;
	}
    }

  if (!tag)
    {
      input = g_new (GdkInput, 1);
      input->tag = next_tag++;
      input->source = source;
      input->condition = condition;
      input->function = function;
      input->data = data;
      tag = input->tag;

      inputs = g_list_prepend (inputs, input);
    }

  return tag;
}

void
gdk_input_remove (gint tag)
{
  GList *list;
  GList *temp_list;
  GdkInput *input;

  list = inputs;
  while (list)
    {
      input = list->data;

      if (input->tag == tag)
	{
	  temp_list = list;

	  if (list->next)
	    list->next->prev = list->prev;
	  if (list->prev)
	    list->prev->next = list->next;
	  if (inputs == list)
	    inputs = list->next;

	  temp_list->next = NULL;
	  temp_list->prev = NULL;

	  g_free (temp_list->data);
	  g_list_free (temp_list);
	  break;
	}

      list = list->next;
    }
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
gdk_pointer_grab (GdkWindow *     window,
		  gint            owner_events,
		  GdkEventMask    event_mask,
		  GdkWindow *     confine_to,
		  GdkCursor *     cursor,
		  guint32         time)
{
  /*  From gdkwindow.c  */
  extern int nevent_masks;
  extern int event_mask_table[];

  gint return_val;
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *confine_to_private;
  GdkCursorPrivate *cursor_private;
  guint xevent_mask;
  Window xwindow;
  Window xconfine_to;
  Cursor xcursor;
  int i;

  g_return_val_if_fail (window != NULL, 0);

  window_private = (GdkWindowPrivate*) window;
  confine_to_private = (GdkWindowPrivate*) confine_to;
  cursor_private = (GdkCursorPrivate*) cursor;

  xwindow = window_private->xwindow;

  if (!confine_to)
    xconfine_to = None;
  else
    xconfine_to = confine_to_private->xwindow;

  if (!cursor)
    xcursor = None;
  else
    xcursor = cursor_private->xcursor;


  xevent_mask = 0;
  for (i = 0; i < nevent_masks; i++)
    {
      if (event_mask & (1 << (i + 1)))
	xevent_mask |= event_mask_table[i];
    }

  if (((GdkWindowPrivate *)window)->extension_events &&
      gdk_input_vtable.grab_pointer)
    return_val = gdk_input_vtable.grab_pointer (window,
						owner_events,
						event_mask,
						confine_to,
						time);
  else
    return_val = Success;;

  if (return_val == Success)
    return_val = XGrabPointer (window_private->xdisplay,
			       xwindow,
			       owner_events,
			       xevent_mask,
			       GrabModeAsync, GrabModeAsync,
			       xconfine_to,
			       xcursor,
			       time);

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

  XUngrabPointer (gdk_display, time);
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
gdk_keyboard_grab (GdkWindow *     window,
                   gint            owner_events,
                   guint32         time)
{
  GdkWindowPrivate *window_private;
  Window xwindow;

  g_return_val_if_fail (window != NULL, 0);

  window_private = (GdkWindowPrivate*) window;
  xwindow = window_private->xwindow;

  return XGrabKeyboard (window_private->xdisplay,
			xwindow,
			owner_events,
			GrabModeAsync, GrabModeAsync,
			time);
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
  XUngrabKeyboard (gdk_display, time);
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width
 *
 *   Return the width of the screen.
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
gdk_screen_width ()
{
  gint return_val;

  return_val = DisplayWidth (gdk_display, gdk_screen);

  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen.
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
gdk_screen_height ()
{
  gint return_val;

  return_val = DisplayHeight (gdk_display, gdk_screen);

  return return_val;
}

void
gdk_key_repeat_disable ()
{
  XAutoRepeatOff (gdk_display);
}

void
gdk_key_repeat_restore ()
{
  if (autorepeat)
    XAutoRepeatOn (gdk_display);
  else
    XAutoRepeatOff (gdk_display);
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

void gdk_flush ()
{
  XSync (gdk_display, False);
}


void
gdk_beep ()
{
  XBell(gdk_display, 100);
}


/*
 *--------------------------------------------------------------
 * gdk_event_wait
 *
 *   Waits until an event occurs or the timer runs out.
 *
 * Arguments:
 *
 * Results:
 *   Returns TRUE if an event is ready to be read and FALSE
 *   if the timer ran out.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static gint
gdk_event_wait ()
{
  GList *list;
  GdkInput *input;
  GdkInputCondition condition;
  SELECT_MASK readfds;
  SELECT_MASK writefds;
  SELECT_MASK exceptfds;
  int max_input;
  int nfd;

  /* If there are no events pending we will wait for an event.
   * The time we wait is dependant on the "timer". If no timer
   *  has been specified then we'll block until an event arrives.
   *  If a timer has been specified we'll block until an event
   *  arrives or the timer expires. (This is all done using the
   *  "select" system call).
   */

  if (XPending (gdk_display) == 0)
    {
      FD_ZERO (&readfds);
      FD_ZERO (&writefds);
      FD_ZERO (&exceptfds);

      FD_SET (connection_number, &readfds);
      max_input = connection_number;

      list = inputs;
      while (list)
	{
	  input = list->data;
	  list = list->next;

	  if (input->condition & GDK_INPUT_READ)
	    FD_SET (input->source, &readfds);
	  if (input->condition & GDK_INPUT_WRITE)
	    FD_SET (input->source, &writefds);
	  if (input->condition & GDK_INPUT_EXCEPTION)
	    FD_SET (input->source, &exceptfds);

	  max_input = MAX (max_input, input->source);
	}

      nfd = select (max_input+1, &readfds, &writefds, &exceptfds, timerp);

      timerp = NULL;
      timer_val = 0;

      if (nfd > 0)
	{
	  if (FD_ISSET (connection_number, &readfds))
	    {
	      if (XPending (gdk_display) == 0)
		{
		  if (nfd == 1)
		    {
		      XNoOp (gdk_display);
		      XFlush (gdk_display);
		    }
		  return FALSE;
		}
	      else
		return TRUE;
	    }

	  list = inputs;
	  while (list)
	    {
	      input = list->data;
	      list = list->next;

	      condition = 0;
	      if (FD_ISSET (input->source, &readfds))
		condition |= GDK_INPUT_READ;
	      if (FD_ISSET (input->source, &writefds))
		condition |= GDK_INPUT_WRITE;
	      if (FD_ISSET (input->source, &exceptfds))
		condition |= GDK_INPUT_EXCEPTION;

	      if (condition && input->function)
		(* input->function) (input->data, input->source, condition);
	    }
	}
    }
  else
    return TRUE;

  return FALSE;
}

static gint
gdk_event_translate (GdkEvent *event,
		     XEvent   *xevent)
{

  GdkWindow *window;
  GdkWindowPrivate *window_private;
  XComposeStatus compose;
  int charcount;
  char buf[16];
  gint return_val;

  /* Are static variables used for this purpose thread-safe? */
  static GdkPoint dnd_drag_start = {0,0},
		  dnd_drag_oldpos = {0,0};
  static GdkRectangle dnd_drag_dropzone = {0,0,0,0};
  static gint dnd_drag_perhaps = 0;
  static GdkWindowPrivate *real_sw = NULL;
  static Window dnd_drag_curwin = None, dnd_drag_target = None;

  return_val = FALSE;

  /* Find the GdkWindow that this event occurred in.
   * All events occur in some GdkWindow (otherwise, why
   *  would we be receiving them). It really is an error
   *  to receive an event for which we cannot find the
   *  corresponding GdkWindow. We handle events with window=None
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

  /* We do a "manual" conversion of the XEvent to a
   *  GdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */
  /* Addendum:
   * During drag & drop you get events where the pointer is
   * in other windows. Need to just do finer-grained checking
   */
  switch (xevent->type)
    {
    case KeyPress:
      /* Lookup the string corresponding to the given keysym.
       */
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 (KeySym*) &event->key.keyval,
				 &compose);

      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("key press:\t\twindow: %ld  key: %12s  %d\n",
		 xevent->xkey.window - base_id,
		 XKeysymToString (event->key.keyval),
		 event->key.keyval);

      event->key.type = GDK_KEY_PRESS;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;

      return_val = !window_private->destroyed;
      break;

    case KeyRelease:
      /* Lookup the string corresponding to the given keysym.
       */
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 (KeySym*) &event->key.keyval,
				 &compose);

      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("key release:\t\twindow: %ld  key: %12s  %d\n",
		 xevent->xkey.window - base_id,
		 XKeysymToString (event->key.keyval),
		 event->key.keyval);

      event->key.type = GDK_KEY_RELEASE;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;

      return_val = !window_private->destroyed;
      break;

    case ButtonPress:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("button press[%d]:\t\twindow: %ld  x,y: %d %d  button: %d\n",
		 window_private?window_private->dnd_drag_enabled:0,
		 xevent->xbutton.window - base_id,
		 xevent->xbutton.x, xevent->xbutton.y,
		 xevent->xbutton.button);

      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	break;

      event->button.type = GDK_BUTTON_PRESS;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x;
      event->button.y = xevent->xbutton.y;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = (GdkModifierType) xevent->xbutton.state;
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
      if(window_private
	 && window_private->dnd_drag_enabled
	 && !dnd_drag_perhaps
	 && !gdk_dnd.drag_really)
	{
	  dnd_drag_perhaps = 1;
	  dnd_drag_start.x = xevent->xbutton.x_root;
	  dnd_drag_start.y = xevent->xbutton.y_root;
	  real_sw = window_private;
	  
	  if(gdk_dnd.drag_startwindows)
	    {
	      g_free(gdk_dnd.drag_startwindows);
	      gdk_dnd.drag_startwindows = NULL;
	    }
	  gdk_dnd.drag_numwindows = gdk_dnd.drag_really = 0;

	  {
	    /* Set motion mask for first DnD'd window, since it
	       will be the one that is actually dragged */
	    XWindowAttributes dnd_winattr;
	    XSetWindowAttributes dnd_setwinattr;
	    Status rv;

	    /* We need to get motion events while the button is down, so
	       we can know whether to really start dragging or not... */
	    XGetWindowAttributes(gdk_display, (Window)window_private->xwindow,
				 &dnd_winattr);
	    
	    window_private->dnd_drag_savedeventmask = dnd_winattr.your_event_mask;
	    dnd_setwinattr.event_mask = 
	      window_private->dnd_drag_eventmask = ButtonMotionMask;
	    XChangeWindowAttributes(gdk_display, window_private->xwindow,
				    CWEventMask, &dnd_setwinattr);
	}
      }
      return_val = window_private?(!window_private->destroyed):FALSE;
      break;

    case ButtonRelease:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("button release[%d]:\twindow: %ld  x,y: %d %d  button: %d\n",
		 window_private?window_private->dnd_drag_enabled:0,
		 xevent->xbutton.window - base_id,
		 xevent->xbutton.x, xevent->xbutton.y,
		 xevent->xbutton.button);

      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	break;

      event->button.type = GDK_BUTTON_RELEASE;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x;
      event->button.y = xevent->xbutton.y;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = (GdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;

      if(dnd_drag_perhaps)
	{
	if(gdk_dnd.drag_really)
	  {
	  GdkPoint foo = {xevent->xbutton.x_root,
			  xevent->xbutton.y_root};
	  XUngrabPointer(gdk_display, CurrentTime);

	  if(dnd_drag_target != None)
	    gdk_dnd_drag_end(dnd_drag_target, foo);
	  gdk_dnd.drag_really = 0;

	  if(gdk_dnd.drag_numwindows)
	    {
	      XSetWindowAttributes attrs;
	      /* Reset event mask to pre-drag value, assuming event_mask
		 doesn't change during drag */
	      attrs.event_mask = real_sw->dnd_drag_savedeventmask;
	      XChangeWindowAttributes(gdk_display, real_sw->xwindow,
				      CWEventMask, &attrs);
	    }

	  gdk_dnd.drag_numwindows = 0;
	  if(gdk_dnd.drag_startwindows)
	    {
	    g_free(gdk_dnd.drag_startwindows);
	    gdk_dnd.drag_startwindows = NULL;
	    }

	  real_sw = NULL;
	  }

	dnd_drag_perhaps = 0;
	dnd_drag_start.x = dnd_drag_start.y = 0;
	dnd_drag_dropzone.x = dnd_drag_dropzone.y = 0;
	dnd_drag_dropzone.width = dnd_drag_dropzone.height = 0;
	dnd_drag_curwin = None;
      }
      return_val = window ? (!window_private->destroyed) : FALSE;
      break;

    case MotionNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s d:%d r%d\n",
		 xevent->xmotion.window - base_id,
		 xevent->xmotion.x, xevent->xmotion.y,
		 (xevent->xmotion.is_hint) ? "true" : "false",
		 dnd_drag_perhaps, gdk_dnd.drag_really);

      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	break;

      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = xevent->xmotion.x;
      event->motion.y = xevent->xmotion.y;
      event->motion.pressure = 0.5;
      event->motion.xtilt = 0;
      event->motion.ytilt = 0;
      event->motion.state = (GdkModifierType) xevent->xmotion.state;
      event->motion.is_hint = xevent->xmotion.is_hint;
      event->motion.source = GDK_SOURCE_MOUSE;
      event->motion.deviceid = GDK_CORE_POINTER;

#define IS_IN_ZONE(cx, cy) (cx >= dnd_drag_dropzone.x \
     && cy >= dnd_drag_dropzone.y \
     && cx < (dnd_drag_dropzone.x + dnd_drag_dropzone.width) \
     && cy < (dnd_drag_dropzone.y + dnd_drag_dropzone.height))

      if(dnd_drag_perhaps && gdk_dnd.drag_really)
	{
	  /* First, we have to find what window the motion was in... */
	  /* XXX there has to be a better way to do this, perhaps with
	     XTranslateCoordinates or XQueryTree - I don't know how,
	     and this sort of works */
	  Window curwin, childwin = gdk_root_window, rootwinret;
	  int x, y;
	  unsigned int mask;
	  while(childwin != None)
	    {
	      curwin = childwin;
	      XQueryPointer(gdk_display, curwin, &rootwinret, &childwin,
			    &x, &y, &x, &y, &mask);
	    }
	  if(curwin != dnd_drag_curwin)
	    {
	      /* We have left one window and entered another
		 (do leave & enter bits) */
	      if(dnd_drag_curwin != real_sw->xwindow && dnd_drag_curwin != None)
		gdk_dnd_drag_leave(dnd_drag_curwin);
	      dnd_drag_curwin = curwin;
	      gdk_dnd_drag_enter(dnd_drag_curwin);
	      dnd_drag_dropzone.x = dnd_drag_dropzone.y = 0;
	      dnd_drag_dropzone.width = dnd_drag_dropzone.height = 0;
	      dnd_drag_target = None;
	      XChangeActivePointerGrab(gdk_display,
				       ButtonMotionMask |
				       ButtonPressMask | ButtonReleaseMask |
				       EnterWindowMask | LeaveWindowMask,
				       gdk_dnd.gdk_cursor_dragdefault,
				       CurrentTime);
	    }
	  else if(dnd_drag_dropzone.width > 0
		  && dnd_drag_dropzone.height > 0)
	    {
	      /* Handle all that dropzone stuff - thanks John ;-) */
	      if(dnd_drag_target != None
		 && IS_IN_ZONE(dnd_drag_oldpos.x, dnd_drag_oldpos.y)
		 && !IS_IN_ZONE(xevent->xmotion.x_root,
				xevent->xmotion.y_root))
		{
		  /* We were in the drop zone and moved out */
		  dnd_drag_target = None;
		  gdk_dnd_drag_leave(curwin);
		}
	      else
		{
		  /* We were outside drop zone but in the window
		     - have to send enter events */
		  gdk_dnd_drag_enter(curwin);
		  dnd_drag_curwin = curwin;
		  dnd_drag_dropzone.x = dnd_drag_dropzone.y = 0;
		  dnd_drag_target = None;
		}
	    } else
	      dnd_drag_curwin = None;
	  return_val = FALSE;
	}
      else
      return_val = window?(!window_private->destroyed):FALSE;
      break;

    case EnterNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld\n",
		 xevent->xcrossing.window - base_id,
		 xevent->xcrossing.detail,
		 xevent->xcrossing.subwindow - base_id);

      /* Tell XInput stuff about it if appropriate */
      if (window_private &&
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

      if(dnd_drag_perhaps
	 && gdk_dnd.drag_really
	 && xevent->xcrossing.window == real_sw->xwindow)
	{
	  gdk_dnd.drag_really = 0;
	  XUngrabPointer(gdk_display, CurrentTime);
	}

      return_val = (window ? !window_private->destroyed : FALSE);
      break;

    case LeaveNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld\n",
		 xevent->xcrossing.window - base_id,
		 xevent->xcrossing.detail, xevent->xcrossing.subwindow - base_id);

      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = window;

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
      if(dnd_drag_perhaps
	 && !gdk_dnd.drag_really)
	{
	  gdk_dnd_drag_addwindow((GdkWindow *) real_sw);
	  gdk_dnd_drag_begin((GdkWindow *) real_sw);
	  XGrabPointer(gdk_display, real_sw->xwindow, False,
		       ButtonMotionMask |
		       ButtonPressMask | ButtonReleaseMask |
		       EnterWindowMask | LeaveWindowMask,
		       GrabModeAsync, GrabModeAsync, gdk_root_window,
		       gdk_dnd.gdk_cursor_dragdefault, CurrentTime);
	  gdk_dnd.drag_really = 1;
      }
      return_val = window ? (!window_private->destroyed) : FALSE;
      break;

    case FocusIn:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("focus in:\t\twindow: %ld\n",
		 xevent->xfocus.window - base_id);

      event->focus_change.type = GDK_FOCUS_CHANGE;
      event->focus_change.window = window;
      event->focus_change.in = TRUE;

      return_val = !window_private->destroyed;
      break;

    case FocusOut:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("focus out:\t\twindow: %ld\n",
		 xevent->xfocus.window - base_id);

      event->focus_change.type = GDK_FOCUS_CHANGE;
      event->focus_change.window = window;
      event->focus_change.in = FALSE;

      return_val = !window_private->destroyed;
      break;

    case KeymapNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("keymap notify\n");

      /* Not currently handled */
      break;

    case Expose:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("expose:\t\twindow: %ld  %d  x,y: %d %d  w,h: %d %d\n",
		 xevent->xexpose.window - base_id, xevent->xexpose.count,
		 xevent->xexpose.x, xevent->xexpose.y,
		 xevent->xexpose.width, xevent->xexpose.height);

      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->xexpose.x;
      event->expose.area.y = xevent->xexpose.y;
      event->expose.area.width = xevent->xexpose.width;
      event->expose.area.height = xevent->xexpose.height;
      event->expose.count = xevent->xexpose.count;

      return_val = !window_private->destroyed;
      break;

    case GraphicsExpose:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("graphics expose:\tdrawable: %ld\n",
		 xevent->xgraphicsexpose.drawable - base_id);

      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->xgraphicsexpose.x;
      event->expose.area.y = xevent->xgraphicsexpose.y;
      event->expose.area.width = xevent->xgraphicsexpose.width;
      event->expose.area.height = xevent->xgraphicsexpose.height;
      event->expose.count = xevent->xexpose.count;

      return_val = !window_private->destroyed;
      break;

    case NoExpose:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("no expose:\t\tdrawable: %ld\n",
		 xevent->xnoexpose.drawable - base_id);

      /* Not currently handled */
      break;

    case VisibilityNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	switch (xevent->xvisibility.state)
	  {
	  case VisibilityFullyObscured:
	    g_print ("visibility notify:\twindow: %ld  none\n",
		     xevent->xvisibility.window - base_id);
	    break;
	  case VisibilityPartiallyObscured:
	    g_print ("visibility notify:\twindow: %ld  partial\n",
		     xevent->xvisibility.window - base_id);
	    break;
	  case VisibilityUnobscured:
	    g_print ("visibility notify:\twindow: %ld  full\n",
		     xevent->xvisibility.window - base_id);
	    break;
	  }

      /* Not currently handled */
      break;

    case CreateNotify:
      /* Not currently handled */
      break;

    case DestroyNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("destroy notify:\twindow: %ld\n",
		 xevent->xdestroywindow.window - base_id);

      event->any.type = GDK_DESTROY;
      event->any.window = window;

      /* Remeber which window received the destroy notify
       *  event so that we can destroy our associated
       *  data structures the next time the user asks
       *  us for an event.
       */
      received_destroy_notify = TRUE;
      window_to_destroy = window;

      return_val = !window_private->destroyed;
      break;

    case UnmapNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("unmap notify:\t\twindow: %ld\n",
		 xevent->xmap.window - base_id);

      event->any.type = GDK_UNMAP;
      event->any.window = window;

      return_val = !window_private->destroyed;
      break;

    case MapNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("map notify:\t\twindow: %ld\n",
		 xevent->xmap.window - base_id);

      event->any.type = GDK_MAP;
      event->any.window = window;

      return_val = !window_private->destroyed;
      break;

    case ReparentNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("reparent notify:\twindow: %ld\n",
		 xevent->xreparent.window - base_id);

      /* Not currently handled */
      break;

    case ConfigureNotify:
      /* Print debugging info.
       */
      while ((XPending(gdk_display) > 0) &&
		XCheckTypedWindowEvent(gdk_display, xevent->xany.window,
				       ConfigureNotify, xevent))
	  /*XSync(gdk_display, 0)*/;

      if (gdk_show_events)
	g_print ("configure notify:\twindow: %ld  x,y: %d %d  w,h: %d %d\n",
		 xevent->xconfigure.window - base_id,
		 xevent->xconfigure.x, xevent->xconfigure.y,
		 xevent->xconfigure.width, xevent->xconfigure.height);

      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_vtable.configure_event)
	gdk_input_vtable.configure_event (&xevent->xconfigure, window);

      if ((window_private->window_type != GDK_WINDOW_CHILD) &&
	  ((window_private->width != xevent->xconfigure.width) ||
	   (window_private->height != xevent->xconfigure.height)))
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.x = xevent->xconfigure.x;
	  event->configure.y = xevent->xconfigure.y;
	  event->configure.width = xevent->xconfigure.width;
	  event->configure.height = xevent->xconfigure.height;

	  window_private->x = xevent->xconfigure.x;
	  window_private->y = xevent->xconfigure.y;
	  window_private->width = xevent->xconfigure.width;
	  window_private->height = xevent->xconfigure.height;
	  if (window_private->resize_count > 1)
	    window_private->resize_count -= 1;

	  return_val = !window_private->destroyed;
	}
      break;

    case PropertyNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("property notify:\twindow: %ld\n",
		 xevent->xproperty.window - base_id);

      event->property.type = GDK_PROPERTY_NOTIFY;
      event->property.window = window;
      event->property.atom = xevent->xproperty.atom;
      event->property.time = xevent->xproperty.time;
      event->property.state = xevent->xproperty.state;

      return_val = !window_private->destroyed;
      break;

    case SelectionClear:
      if (gdk_show_events)
	g_print ("selection clear:\twindow: %ld\n",
		 xevent->xproperty.window - base_id);

      event->selection.type = GDK_SELECTION_CLEAR;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionclear.selection;
      event->selection.time = xevent->xselectionclear.time;

      return_val = !((GdkWindowPrivate*) window)->destroyed;
      break;

    case SelectionRequest:
      if (gdk_show_events)
	g_print ("selection request:\twindow: %ld\n",
		 xevent->xproperty.window - base_id);

      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionrequest.selection;
      event->selection.target = xevent->xselectionrequest.target;
      event->selection.property = xevent->xselectionrequest.property;
      event->selection.requestor = xevent->xselectionrequest.requestor;
      event->selection.time = xevent->xselectionrequest.time;

      return_val = !((GdkWindowPrivate*) window)->destroyed;
      break;

    case SelectionNotify:
      if (gdk_show_events)
	g_print ("selection notify:\twindow: %ld\n",
		 xevent->xproperty.window - base_id);


      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = xevent->xselection.selection;
      event->selection.target = xevent->xselection.target;
      event->selection.property = xevent->xselection.property;
      event->selection.time = xevent->xselection.time;

      return_val = !((GdkWindowPrivate*) window)->destroyed;
      break;

    case ColormapNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("colormap notify:\twindow: %ld\n",
		 xevent->xcolormap.window - base_id);

      /* Not currently handled */
      break;

    case ClientMessage:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("client message:\twindow: %ld\n",
		 xevent->xclient.window - base_id);

      /* Client messages are the means of the window manager
       *  communicating with a program. We'll first check to
       *  see if this is really the window manager talking
       *  to us.
       */
      if (xevent->xclient.message_type == gdk_wm_protocols)
	{
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

	      /* Print debugging info.
	       */
	      if (gdk_show_events)
		g_print ("delete window:\t\twindow: %ld\n",
			 xevent->xclient.window - base_id);

	      event->any.type = GDK_DELETE;
	      event->any.window = window;

	      return_val = !window_private->destroyed;
	    }
	  else if ((Atom) xevent->xclient.data.l[0] == gdk_wm_take_focus)
	    {
	    }
	}
      else if (xevent->xclient.message_type == gdk_dnd.gdk_XdeEnter)
	{
	  Atom reptype = 0;

	  event->dropenter.u.allflags = xevent->xclient.data.l[1];
	  if (gdk_show_events)
	    g_print ("GDK_DROP_ENTER\n");
	  return_val = FALSE;

	  /* Now figure out if we really want this drop...
	   * If someone is trying funky clipboard stuff, ignore 
	   */
	  if (window_private
	      && window_private->dnd_drop_enabled
	      && event->dropenter.u.flags.sendreply
	      && (reptype = gdk_dnd_check_types (window, xevent)))
	    {
	      XEvent replyev;

	      replyev.xclient.type = ClientMessage;
	      replyev.xclient.window = xevent->xclient.data.l[0];
	      replyev.xclient.format = 32;
	      replyev.xclient.message_type = gdk_dnd.gdk_XdeRequest;
	      replyev.xclient.data.l[0] = window_private->xwindow;

	      event->dragrequest.u.allflags = 0;
	      event->dragrequest.u.flags.protocol_version =
		DND_PROTOCOL_VERSION;
	      event->dragrequest.u.flags.willaccept = 1;
	      event->dragrequest.u.flags.delete_data =
		(window_private->dnd_drop_destructive_op) ? 1 : 0;

	      replyev.xclient.data.l[1] = event->dragrequest.u.allflags;
	      replyev.xclient.data.l[2] = replyev.xclient.data.l[3] = 0;
	      replyev.xclient.data.l[4] = reptype;

	      XSendEvent (gdk_display, replyev.xclient.window,
			  False, NoEventMask, &replyev);

	      event->any.type = GDK_DROP_ENTER;
	      event->dropenter.requestor = replyev.xclient.window;
	      event->dropenter.u.allflags = xevent->xclient.data.l[1];
	    }
	}
      else if (xevent->xclient.message_type == gdk_dnd.gdk_XdeLeave)
	{
	  if (gdk_show_events)
	    g_print ("GDK_DROP_LEAVE\n");
	  if (window_private && window_private->dnd_drop_enabled)
	    {
	      event->dropleave.type = GDK_DROP_LEAVE;
	      event->dropleave.window = window;
	      event->dropleave.requestor = xevent->xclient.data.l[0];
	      event->dropleave.u.allflags = xevent->xclient.data.l[1];
	      return_val = TRUE;
	    }
	  else
	    return_val = FALSE;
	}
      else if (xevent->xclient.message_type == gdk_dnd.gdk_XdeRequest)
	{
	  /* 
	   * make sure to only handle requests from the window the cursor is
	   * over 
	   */
	  if (gdk_show_events)
	    g_print ("GDK_DRAG_REQUEST\n");
	  event->dragrequest.u.allflags = xevent->xclient.data.l[1];
	  return_val = FALSE;

	  if (window && gdk_dnd.drag_really &&
	      xevent->xclient.data.l[0] == dnd_drag_curwin &&
	      event->dragrequest.u.flags.sendreply == 0)
	    {
	      /* Got request - do we need to ask user? */
	      if (!event->dragrequest.u.flags.willaccept
		  && event->dragrequest.u.flags.senddata)
		{
		  /* Yes we do :) */
		  event->dragrequest.type = GDK_DRAG_REQUEST;
		  event->dragrequest.window = window;
		  event->dragrequest.requestor = xevent->xclient.data.l[0];
		  event->dragrequest.isdrop = 0;
		  event->dragrequest.drop_coords.x =
		    event->dragrequest.drop_coords.y = 0;
		  return_val = TRUE;
		}
	      else if (event->dragrequest.u.flags.willaccept)
		{
		  window_private->dnd_drag_destructive_op =
		    event->dragrequest.u.flags.delete_data;
		  window_private->dnd_drag_accepted = 1;
		  window_private->dnd_drag_data_type =
		    xevent->xclient.data.l[4];

		  dnd_drag_target = dnd_drag_curwin;
		  XChangeActivePointerGrab (gdk_display,
					    ButtonMotionMask |
					    ButtonPressMask |
					    ButtonReleaseMask |
					    EnterWindowMask | LeaveWindowMask,
					    gdk_dnd.gdk_cursor_dragok,
					    CurrentTime);
		}
	      dnd_drag_dropzone.x = xevent->xclient.data.l[2] & 65535;
	      dnd_drag_dropzone.y =
		(xevent->xclient.data.l[2] >> 16) & 65535;
	      dnd_drag_dropzone.width = xevent->xclient.data.l[3] & 65535;
	      dnd_drag_dropzone.height =
		(xevent->xclient.data.l[3] >> 16) & 65535;
	    }
	}
      else if(xevent->xclient.message_type == gdk_dnd.gdk_XdeDataAvailable)
	  {
	    gint tmp_int; Atom tmp_atom;
	    gulong tmp_long;
	    guchar *tmp_charptr;
	    gpointer tmp_ptr;
	    
	    if(gdk_show_events)
	      g_print("GDK_DROP_DATA_AVAIL\n");
	    event->dropdataavailable.u.allflags = xevent->xclient.data.l[1];
	    if(window
	       /* No preview of data ATM */
	       && event->dropdataavailable.u.flags.isdrop)
	      {
		event->dropdataavailable.type = GDK_DROP_DATA_AVAIL;
		event->dropdataavailable.window = window;
		event->dropdataavailable.requestor = xevent->xclient.data.l[0];
		event->dropdataavailable.data_type =
			gdk_atom_name(xevent->xclient.data.l[2]);
		if(XGetWindowProperty (gdk_display,
				    event->dropdataavailable.requestor,
				    xevent->xclient.data.l[2],
				    0, LONG_MAX - 1,
				    False, XA_PRIMARY, &tmp_atom,
				    &tmp_int,
				    &event->dropdataavailable.data_numbytes,
				    &tmp_long,
				    &tmp_charptr)
		   != Success)
		  {
		    g_warning("XGetWindowProperty on %#x may have failed\n",
			    event->dropdataavailable.requestor);
			    event->dropdataavailable.data = NULL;
		  }
		else
		  {
		    g_print("XGetWindowProperty got us %d bytes\n",
			    event->dropdataavailable.data_numbytes);
		    event->dropdataavailable.data =
			g_malloc(event->dropdataavailable.data_numbytes);
		    memcpy(event->dropdataavailable.data,
			tmp_charptr, event->dropdataavailable.data_numbytes);
		    XFree(tmp_charptr);
		    return_val = TRUE;
		  }
	      return_val = TRUE;
	    }
	} else {
	  /* Send unknown ClientMessage's on to Gtk for it to use */
	  event->client.type = GDK_CLIENT_EVENT;
	  event->client.window = window;
	  event->client.message_type = xevent->xclient.message_type;
	  event->client.data_format = xevent->xclient.format;
	  memcpy(&event->client.data, &xevent->xclient.data,
		 sizeof(event->client.data));
	  return_val = TRUE;
	}
      return_val = return_val && !window_private->destroyed;
      break;
      
    case MappingNotify:
      /* Print debugging info.
       */
      if (gdk_show_events)
	g_print ("mapping notify\n");

      /* Let XLib know that there is a new keyboard mapping.
       */
      XRefreshKeyboardMapping (&xevent->xmapping);
      break;

    default:
      /* something else - (e.g., a Xinput event) */

      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_vtable.other_event)
	return_val = gdk_input_vtable.other_event(event, xevent, window);

      if (return_val < 0)	/* not an XInput event, convert */
	{
	  event->other.type = GDK_OTHER_EVENT;
	  event->other.window = window;
	  event->other.xevent = &other_xevent[other_xevent_i];
	  memcpy (&other_xevent[other_xevent_i], xevent, sizeof (XEvent));
	  other_xevent_i = (other_xevent_i+1) % OTHER_XEVENT_BUFSIZE;
	  return_val = TRUE;
	}

      return_val = return_val && !window_private->destroyed;
      break;
    }

  return return_val;
}

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

static void
gdk_synthesize_click (GdkEvent *event,
		      gint      nclicks)
{
  GdkEvent temp_event;

  g_return_if_fail (event != NULL);

  temp_event = *event;
  temp_event.type = (nclicks == 2) ? GDK_2BUTTON_PRESS : GDK_3BUTTON_PRESS;

  gdk_event_put (&temp_event);
}

/*
 *--------------------------------------------------------------
 * gdk_exit_func
 *
 *   This is the "atexit" function that makes sure the
 *   library gets a chance to cleanup.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *   The library is un-initialized and the program exits.
 *
 *--------------------------------------------------------------
 */

static void
gdk_exit_func ()
{
  if (initialized)
    {
      gdk_image_exit ();
      gdk_input_exit ();
      gdk_key_repeat_restore ();

      XCloseDisplay (gdk_display);
      initialized = 0;
    }
}

/*
 *--------------------------------------------------------------
 * gdk_x_error
 *
 *   The X error handling routine.
 *
 * Arguments:
 *   "display" is the X display the error orignated from.
 *   "error" is the XErrorEvent that we are handling.
 *
 * Results:
 *   Either we were expecting some sort of error to occur,
 *   in which case we set the "gdk_error_code" flag, or this
 *   error was unexpected, in which case we will print an
 *   error message and exit. (Since trying to continue will
 *   most likely simply lead to more errors).
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
gdk_x_error (Display     *display,
	     XErrorEvent *error)
{
  char buf[64];

  if (gdk_error_warnings)
    {
      XGetErrorText (display, error->error_code, buf, 63);
      g_error ("%s", buf);
    }

  gdk_error_code = -1;
  return 0;
}

/*
 *--------------------------------------------------------------
 * gdk_x_io_error
 *
 *   The X I/O error handling routine.
 *
 * Arguments:
 *   "display" is the X display the error orignated from.
 *
 * Results:
 *   An X I/O error basically means we lost our connection
 *   to the X server. There is not much we can do to
 *   continue, so simply print an error message and exit.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
gdk_x_io_error (Display *display)
{
  g_error ("an x io error occurred");
  return 0;
}

/*
 *--------------------------------------------------------------
 * gdk_signal
 *
 *   The signal handler.
 *
 * Arguments:
 *   "sig_num" is the number of the signal we received.
 *
 * Results:
 *   The signals we catch are all fatal. So we simply build
 *   up a nice little error message and print it and exit.
 *   If in the process of doing so another signal is received
 *   we notice that we are already exiting and simply kill
 *   our process.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static RETSIGTYPE
gdk_signal (int sig_num)
{
  static int caught_fatal_sig = 0;
  char *sig;

  if (caught_fatal_sig)
    kill (getpid (), sig_num);
  caught_fatal_sig = 1;

  switch (sig_num)
    {
    case SIGHUP:
      sig = "sighup";
      break;
    case SIGINT:
      sig = "sigint";
      break;
    case SIGQUIT:
      sig = "sigquit";
      break;
    case SIGBUS:
      sig = "sigbus";
      break;
    case SIGSEGV:
      sig = "sigsegv";
      break;
    case SIGPIPE:
      sig = "sigpipe";
      break;
    case SIGTERM:
      sig = "sigterm";
      break;
    default:
      sig = "unknown signal";
      break;
    }

  g_print ("\n** ERROR **: %s caught\n", sig);
  gdk_exit (1);
}

static void
gdk_dnd_drag_begin (GdkWindow *initial_window)
{
  GdkEventDragBegin tev;
  tev.type = GDK_DRAG_BEGIN;
  tev.window = initial_window;
  tev.u.allflags = 0;
  tev.u.flags.protocol_version = DND_PROTOCOL_VERSION;

  gdk_event_put ((GdkEvent *) &tev);
}

static void
gdk_dnd_drag_enter (Window dest)
{
  XEvent sev;
  GdkEventDropEnter tev;
  int i;
  GdkWindowPrivate *wp;
  
  sev.xclient.type = ClientMessage;
  sev.xclient.format = 32;
  sev.xclient.message_type = gdk_dnd.gdk_XdeEnter;
  sev.xclient.window = dest;

  tev.u.allflags = 0;
  tev.u.flags.protocol_version = DND_PROTOCOL_VERSION;
  tev.u.flags.sendreply = 1;
  for (i = 0; i < gdk_dnd.drag_numwindows; i++)
    {
      wp = (GdkWindowPrivate *) gdk_dnd.drag_startwindows[i];
      if (wp->dnd_drag_data_numtypesavail)
	{
	  sev.xclient.data.l[0] = wp->xwindow;
	  tev.u.flags.extended_typelist = (wp->dnd_drag_data_numtypesavail > 3)?1:0;
	  sev.xclient.data.l[1] = tev.u.allflags;
	  sev.xclient.data.l[2] = wp->dnd_drag_data_typesavail[0];
	  if (wp->dnd_drag_data_numtypesavail > 1)
	    {
	      sev.xclient.data.l[3] = wp->dnd_drag_data_typesavail[1];
	      if (wp->dnd_drag_data_numtypesavail > 2)
		{
		  sev.xclient.data.l[4] = wp->dnd_drag_data_typesavail[2];
		}
	      else
		sev.xclient.data.l[4] = None;
	    }
	  else
	    sev.xclient.data.l[3] = sev.xclient.data.l[4] = None;
	  XSendEvent (gdk_display, dest, False, NoEventMask, &sev);
	}

    }
}

static void
gdk_dnd_drag_leave (Window dest)
{
  XEvent sev;
  GdkEventDropLeave tev;
  int i;
  GdkWindowPrivate *wp;

  tev.u.allflags = 0;

  tev.u.flags.protocol_version = DND_PROTOCOL_VERSION;
  sev.xclient.type = ClientMessage;
  sev.xclient.window = dest;
  sev.xclient.format = 32;
  sev.xclient.message_type = gdk_dnd.gdk_XdeLeave;
  sev.xclient.data.l[1] = tev.u.allflags;
  for (i = 0; i < gdk_dnd.drag_numwindows; i++)
    {
      wp = (GdkWindowPrivate *) gdk_dnd.drag_startwindows[i];
      sev.xclient.data.l[0] = wp->xwindow;
      XSendEvent(gdk_display, dest, False, NoEventMask, &sev);
      wp->dnd_drag_accepted = 0;
    }
}

/* 
 * when a drop occurs, we go through the list of windows being dragged and
 * tell them that it has occurred, so that they can set things up and reply
 * to 'dest' window 
 */
static void
gdk_dnd_drag_end (Window     dest, 
		  GdkPoint   coords)
{
  GdkWindowPrivate *wp;
  GdkEventDragRequest tev;
  gchar *tmp_cptr;
  int i;

  tev.type = GDK_DRAG_REQUEST;
  tev.drop_coords = coords;
  tev.requestor = dest;
  tev.u.allflags = 0;
  tev.u.flags.protocol_version = DND_PROTOCOL_VERSION;
  tev.isdrop = 1;

  for (i = 0; i < gdk_dnd.drag_numwindows; i++)
    {
      wp = (GdkWindowPrivate *) gdk_dnd.drag_startwindows[i];
      if (wp->dnd_drag_accepted)
	{
	  tev.window = (GdkWindow *) wp;
	  tev.u.flags.delete_data = wp->dnd_drag_destructive_op;
	  tev.data_type = 
	  	gdk_atom_name(wp->dnd_drag_data_type);

	  gdk_event_put((GdkEvent *) &tev);
	}
    }
}

static GdkAtom
gdk_dnd_check_types (GdkWindow   *window, 
		     XEvent      *xevent)
{
  GdkWindowPrivate *wp = (GdkWindowPrivate *) window;
  int i, j;
  GdkEventDropEnter event;

  g_return_val_if_fail(window != NULL, 0);
  g_return_val_if_fail(xevent != NULL, 0);
  g_return_val_if_fail(xevent->type == ClientMessage, 0);
  g_return_val_if_fail(xevent->xclient.message_type == gdk_dnd.gdk_XdeEnter, 0);

  if(wp->dnd_drop_data_numtypesavail <= 0 ||
     !wp->dnd_drop_data_typesavail)
    return 0;

  for (i = 2; i <= 4; i++)
    {
      for (j = 0; j < wp->dnd_drop_data_numtypesavail; j++)
	{
	  if (xevent->xclient.data.l[i] == wp->dnd_drop_data_typesavail[j])
	    return xevent->xclient.data.l[i];
	}
    }

  /* Now we get the extended type list if it's available */
  event.u.allflags = xevent->xclient.data.l[1];
  if (event.u.flags.extended_typelist)
    {
      Atom *exttypes, realtype;
      gulong nitems, nbar;
      gint realfmt;

      if (XGetWindowProperty(gdk_display, xevent->xclient.data.l[0],
			     gdk_dnd.gdk_XdeTypelist, 0L, LONG_MAX - 1,
			     False, AnyPropertyType, &realtype, &realfmt,
			     &nitems, &nbar, (unsigned char **) &exttypes)
	 != Success)
	return 0;

      if (realfmt != (sizeof(Atom) * 8))
	{
	  g_warning("XdeTypelist property had format of %d instead of the expected %d, on window %#lx\n",
		    realfmt, sizeof(Atom) * 8, xevent->xclient.data.l[0]);
	  return 0;
	}

      for (i = 0; i <= nitems; i++)
	{
	  for (j = 0; j < wp->dnd_drop_data_numtypesavail; j++)
	    {
	      if (exttypes[i] == wp->dnd_drop_data_typesavail[j])
		{
		  XFree (exttypes);
		  return exttypes[i];
		}
	    }
	}
      XFree (exttypes);
    }
  return 0;
}

/* 
 * used for debugging only 
 */
static void
gdk_print_atom (GdkAtom anatom)
{
  gchar *tmpstr = NULL;
  tmpstr = (anatom!=None)?gdk_atom_name(anatom):"(none)";
  g_print("Atom %lu has name %s\n", anatom, tmpstr);
  if(tmpstr)
    g_free(tmpstr);
}

/* 
 * used only by below routine and itself 
 */
static Window 
getchildren (Display     *dpy, 
	     Window       win, 
	     Atom         WM_STATE)
{
  Window root, parent, *children, inf = 0;
  Atom type = None;
  unsigned int nchildren, i;
  int format;
  unsigned long nitems, after;
  unsigned char *data;

  if (XQueryTree(dpy, win, &root, &parent, &children, &nchildren) == 0)
    return 0;

  for (i = 0; !inf && (i < nchildren); i++)
    {
      XGetWindowProperty (dpy, children[i], WM_STATE, 0, 0, False,
			  AnyPropertyType, &type, &format, &nitems,
			  &after, &data);
      if (type != 0)
	inf = children[i];
    }

  for (i = 0; !inf && (i < nchildren); i++)
    inf = getchildren (dpy, children[i], WM_STATE);

  if (children != 0) 
    XFree ((char *) children);

  return inf;
}

/* 
 * find a window with WM_STATE, else return win itself, as per ICCCM
 *
 * modification of the XmuClientWindow() routine from X11R6.3
 */
Window
gdk_get_client_window (Display  *dpy, 
		       Window    win)
{
  Atom WM_STATE;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  Window inf;

  if (win == 0)
    return DefaultRootWindow(dpy);

  if ((WM_STATE = XInternAtom (dpy, "WM_STATE", True)) == 0)
    return win;

  XGetWindowProperty (dpy, win, WM_STATE, 0, 0, False, AnyPropertyType,
		      &type, &format, &nitems, &after, &data);
  if (type)
    return win;

  inf = getchildren (dpy, win, WM_STATE);

  if (inf == 0)
    return win;
  else
    return inf;
}

static GdkWindow *
gdk_drop_get_real_window (GdkWindow   *w, 
			  guint16     *x, 
			  guint16     *y)
{
  GdkWindow *retval = w;
  GdkWindowPrivate *awin;
  GList *children;
  gint16 myx = *x, myy = *y;

  g_return_val_if_fail(w != NULL && x != NULL && y != NULL, NULL);

  myx = *x; 
  myy = *y;

descend:
  for (children = gdk_window_get_children(retval); 
       children && children->next;
       children = children->next)
    {
      awin = (GdkWindowPrivate *) children->data;
      if ((myx >= awin->x) && (myy >= awin->y)
	  && (myx < (awin->x + awin->width))
	  && (myy < (awin->y + awin->height)))
	{
	  retval = (GdkWindow *) awin;
	  myx -= awin->x;
	  myy -= awin->y;
	  goto descend;
	}
    }

  *x = myx; 
  *y = myy;

  return retval;
}

/* Sends a ClientMessage to all toplevel client windows */
void
gdk_event_send_clientmessage_toall(GdkEvent *event)
{
  XEvent sev;
  Window *ret_children, ret_root, ret_parent, curwin;
  unsigned int ret_nchildren;
  int i;

  g_return_if_fail(event != NULL);

  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = gdk_display;
  sev.xclient.format = event->client.data_format;
  sev.xclient.serial = CurrentTime;
  memcpy(&sev.xclient.data, &event->client.data, sizeof(sev.xclient.data));
  sev.xclient.message_type = event->client.message_type;

  /* OK, we're all set, now let's find some windows to send this to */
  if(XQueryTree(gdk_display, gdk_root_window, &ret_root, &ret_parent,
		&ret_children, &ret_nchildren) != True)
    return;

  /* foreach true child window of the root window, send an event to it */
  for(i = 0; i < ret_nchildren; i++) {
    curwin = gdk_get_client_window(gdk_display, ret_children[i]);
    sev.xclient.window = curwin;
    XSendEvent(gdk_display, curwin, False, NoEventMask, &sev);
  }

  XFree(ret_children);
}
