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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "../config.h"

/* If you don't want to use gdk's signal handlers define this */
/* #define I_NEED_TO_ACTUALLY_DEBUG_MY_PROGRAMS 1 */

#include <X11/Xlocale.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef USE_XIM
#include <stdarg.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H_ */

#define XLIB_ILLEGAL_ACCESS
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/Xmu/WinUtil.h>
#ifdef USE_XIM
#include <X11/Xresource.h>
#endif
#include <X11/cursorfont.h>
#include "gdk.h"
#include "gdkprivate.h"
#include "gdkinput.h"
#ifdef USE_XIM
#include "gdkx.h"
#endif
#include "gdkkeysyms.h"
#include "gdki18n.h"

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
  GdkDestroyNotify destroy;
};

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

/* 
 * Private function declarations
 */

static GdkEvent *gdk_event_new          (void);
static gint      gdk_event_wait         (void);
static gint      gdk_event_apply_filters (XEvent *xevent,
					  GdkEvent *event,
					  GList *filters);
static gint      gdk_event_translate    (GdkEvent     *event, 
				         XEvent       *xevent);
#if 0
static Bool      gdk_event_get_type     (Display      *display, 
					 XEvent       *xevent, 
					 XPointer      arg);
#endif
static void      gdk_synthesize_click   (GdkEvent     *event, 
					 gint          nclicks);

static void      gdk_dnd_drag_begin     (GdkWindow    *initial_window);
static void      gdk_dnd_drag_enter     (Window        dest);
static void      gdk_dnd_drag_leave     (Window        dest);
static void      gdk_dnd_drag_end       (Window        dest,
					 GdkPoint      coords);
static GdkAtom   gdk_dnd_check_types    (GdkWindow    *window,
					 XEvent       *xevent);
#ifdef DEBUG_DND
static void      gdk_print_atom         (GdkAtom       anatom);
#endif

/* 
 * old junk from offix, we might use it though so leave it 
 */
Window       gdk_get_client_window   (Display     *dpy, 
				      Window       win);
#ifdef WE_HAVE_MOTIF_DROPS_DONE
static GdkWindow *  gdk_drop_get_real_window     (GdkWindow   *w, 
						  guint16     *x,
						  guint16     *y);
#endif
static void         gdk_exit_func                (void);
static int          gdk_x_error                  (Display     *display, 
						  XErrorEvent *error);
static int          gdk_x_io_error               (Display     *display);
static RETSIGTYPE   gdk_signal                   (int          signum);


#ifdef USE_XIM
static guint         gdk_im_va_count     (va_list list);
static XVaNestedList gdk_im_va_to_nested (va_list list, 
                                          guint   count);

static GdkIM  gdk_im_get		(void);
static gint   gdk_im_open		(XrmDatabase db,
					 gchar* res_name,
					 gchar* rec_class);
static void   gdk_im_close		(void);
static void   gdk_ic_cleanup 		(void);

#endif /* USE_XIM */

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
static GdkWindowPrivate *xgrab_window = NULL;	    /* Window that currently holds the
						     *  x pointer grab
						     */

#ifdef USE_XIM
static gint xim_using;				/* using XIM Protocol if TRUE */
static GdkIM xim_im;				/* global IM */
static XIMStyles* xim_styles;			/* im supports these styles */
static XIMStyle xim_best_allowed_style;
static GdkICPrivate *xim_ic;			/* currently using IC */
static GdkWindow* xim_window;			/* currently using Widow */
static GList* xim_ic_list;

#endif

static GList *putback_events = NULL;

static gulong base_id;
static gint autorepeat;

#ifdef G_ENABLE_DEBUG
static GDebugKey gdk_debug_keys[] = {
  {"events",        GDK_DEBUG_EVENTS},
  {"misc",          GDK_DEBUG_MISC},
  {"dnd",           GDK_DEBUG_DND},
  {"color-context", GDK_DEBUG_COLOR_CONTEXT},
  {"xim",           GDK_DEBUG_XIM}
};

static const int gdk_ndebug_keys = sizeof(gdk_debug_keys)/sizeof(GDebugKey);

#endif /* G_ENABLE_DEBUG */

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
  gint synchronize;
  gint i, j, k;
  XClassHint *class_hint;
  gint argc_orig = *argc;
  gchar **argv_orig;

  argv_orig = g_malloc ((argc_orig + 1) * sizeof (char*));
  for (i = 0; i < argc_orig; i++)
    argv_orig[i] = g_strdup ((*argv)[i]);
  argv_orig[argc_orig] = NULL;

  X_GETTIMEOFDAY (&start);

#ifndef I_NEED_TO_ACTUALLY_DEBUG_MY_PROGRAMS
  signal (SIGHUP, gdk_signal);
  signal (SIGINT, gdk_signal);
  signal (SIGQUIT, gdk_signal);
  signal (SIGBUS, gdk_signal);
  signal (SIGSEGV, gdk_signal);
  signal (SIGPIPE, gdk_signal);
  signal (SIGTERM, gdk_signal);
#endif

  gdk_display_name = NULL;

  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);

  synchronize = FALSE;

#ifdef G_ENABLE_DEBUG
  {
    gchar *debug_string = getenv("GDK_DEBUG");
    if (debug_string != NULL)
      gdk_debug_flags = g_parse_debug_string (debug_string,
					      gdk_debug_keys,
					      gdk_ndebug_keys);
  }
#endif	/* G_ENABLE_DEBUG */

  if (argc && argv)
    {
      if (*argc > 0)
	{
	  gchar *d;

	  d = strrchr((*argv)[0],'/');
	  if (d != NULL)
	    gdk_progname = g_strdup (d + 1);
	  else
	    gdk_progname = g_strdup ((*argv)[0]);
	  GDK_NOTE (MISC,
		    g_print ("Gdk: progname: \"%s\"\n", gdk_progname));
	}

      for (i = 1; i < *argc;)
	{
#ifdef G_ENABLE_DEBUG	  
	  if ((strcmp ("--gdk-debug", (*argv)[i]) == 0) ||
	      (strncmp ("--gdk-debug=", (*argv)[i], 12) == 0))
	    {
	      gchar *equal_pos = strchr ((*argv)[i], '=');
	      
	      if (equal_pos != NULL)
		{
		  gdk_debug_flags |= g_parse_debug_string (equal_pos+1,
							   gdk_debug_keys,
							   gdk_ndebug_keys);
		}
	      else if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  gdk_debug_flags |= g_parse_debug_string ((*argv)[i+1],
							   gdk_debug_keys,
							   gdk_ndebug_keys);
		  (*argv)[i] = NULL;
		  i += 1;
		}
	      (*argv)[i] = NULL;
	    }
	  else if ((strcmp ("--gdk-no-debug", (*argv)[i]) == 0) ||
		   (strncmp ("--gdk-no-debug=", (*argv)[i], 15) == 0))
	    {
	      gchar *equal_pos = strchr ((*argv)[i], '=');

	      if (equal_pos != NULL)
		{
		  gdk_debug_flags &= ~g_parse_debug_string (equal_pos+1,
							    gdk_debug_keys,
							    gdk_ndebug_keys);
		}
	      else if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  gdk_debug_flags &= ~g_parse_debug_string ((*argv)[i+1],
							    gdk_debug_keys,
							    gdk_ndebug_keys);
		  (*argv)[i] = NULL;
		  i += 1;
		}
	      (*argv)[i] = NULL;
	    }
	  else 
#endif /* G_ENABLE_DEBUG */
	    if (strcmp ("--display", (*argv)[i]) == 0)
	    {
	      (*argv)[i] = NULL;

	      if ((i + 1) < *argc && (*argv)[i + 1])
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
	  else if (strcmp ("--no-xshm", (*argv)[i]) == 0)
	    {
	      (*argv)[i] = NULL;
	      gdk_use_xshm = FALSE;
	    }
	  else if (strcmp ("--name", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  (*argv)[i++] = NULL;
		  gdk_progname = (*argv)[i];
		  (*argv)[i] = NULL;
		}
	    }
	  else if (strcmp ("--class", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  (*argv)[i++] = NULL;
		  gdk_progclass = (*argv)[i];
		  (*argv)[i] = NULL;
		}
	    }
#ifdef XINPUT_GXI
	  else if (strcmp ("--gxid_host", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  (*argv)[i++] = NULL;
		  gdk_input_gxid_host = ((*argv)[i]);
		  (*argv)[i] = NULL;
		}
	    }
	  else if (strcmp ("--gxid_port", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  (*argv)[i++] = NULL;
		  gdk_input_gxid_port = atoi ((*argv)[i]);
		  (*argv)[i] = NULL;
		}
	    }
#endif
#ifdef USE_XIM
	  else if (strcmp ("--xim-preedit", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  (*argv)[i++] = NULL;
		  if (strcmp ("none", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMPreeditNone);
		  else if (strcmp ("nothing", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMPreeditNothing);
		  else if (strcmp ("area", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMPreeditArea);
		  else if (strcmp ("position", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMPreeditPosition);
		  else if (strcmp ("callbacks", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMPreeditCallbacks);
		}
	    }
	  else if (strcmp ("--xim-status", (*argv)[i]) == 0)
	    {
	      if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  (*argv)[i++] = NULL;
		  if (strcmp ("none", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMStatusNone);
		  else if (strcmp ("nothing", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMStatusNothing);
		  else if (strcmp ("area", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMStatusArea);
		  else if (strcmp ("callbacks", (*argv)[i]) == 0)
		    gdk_im_set_best_style (GdkIMStatusCallbacks);
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
    {
      g_warning ("cannot open display: %s", XDisplayName (gdk_display_name));
      exit(1);
    }

  /* This is really crappy. We have to look into the display structure
   *  to find the base resource id. This is only needed for recording
   *  and playback of events.
   */
  /* base_id = RESOURCE_BASE; */
  base_id = 0;
  GDK_NOTE (EVENTS, g_print ("base id: %lu\n", base_id));

  connection_number = ConnectionNumber (gdk_display);
  GDK_NOTE (MISC,
    g_print ("Gdk: connection number: %d\n", connection_number));

  if (synchronize)
    XSynchronize (gdk_display, True);

  gdk_screen = DefaultScreen (gdk_display);
  gdk_root_window = RootWindow (gdk_display, gdk_screen);

  gdk_leader_window = XCreateSimpleWindow(gdk_display, gdk_root_window,
					  10, 10, 10, 10, 0, 0 , 0);
  class_hint = XAllocClassHint();
  class_hint->res_name = gdk_progname;
  if (gdk_progclass == NULL)
    {
      gdk_progclass = g_strdup (gdk_progname);
      gdk_progclass[0] = toupper (gdk_progclass[0]);
    }
  class_hint->res_class = gdk_progclass;
  XSetClassHint(gdk_display, gdk_leader_window, class_hint);
  XSetCommand(gdk_display, gdk_leader_window, argv_orig, argc_orig);
  XFree (class_hint);
  
  for (i = 0; i < argc_orig; i++)
    g_free(argv_orig[i]);
  g_free(argv_orig);
  
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
  gdk_dnd.c->gdk_cursor_dragdefault = XCreateFontCursor(gdk_display, XC_bogosity);
  gdk_dnd.c->gdk_cursor_dragok = XCreateFontCursor(gdk_display, XC_heart);

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

#ifdef USE_XIM
  /* initialize XIM Protocol variables */
  xim_using = FALSE;
  xim_im = NULL;
  xim_styles = NULL;
  if (!(xim_best_allowed_style & GdkIMPreeditMask))
    gdk_im_set_best_style (GdkIMPreeditCallbacks);
  if (!(xim_best_allowed_style & GdkIMStatusMask))
    gdk_im_set_best_style (GdkIMStatusCallbacks);
  xim_ic = NULL;
  xim_window = (GdkWindow*)NULL;

  gdk_im_open (NULL, NULL, NULL);
#endif

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
gdk_set_locale (void)
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
gdk_events_pending (void)
{
  gint result;
  GList *tmp_list;

  result = XPending (gdk_display);

  tmp_list = putback_events;
  while (tmp_list)
    {
      result++;
      tmp_list = tmp_list->next;
    }
  
  return result;
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
graphics_expose_predicate  (Display  *display,
			    XEvent   *xevent,
			    XPointer  arg)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)arg;
  
  g_return_val_if_fail (private != NULL, False);

  if ((xevent->xany.window == private->xwindow) &&
      ((xevent->xany.type == GraphicsExpose) ||
       (xevent->xany.type == NoExpose)))
    return True;
  else
    return False;
}

GdkEvent *
gdk_event_get_graphics_expose (GdkWindow *window)
{
  XEvent xevent;
  GdkEvent *event;
  
  g_return_val_if_fail (window != NULL, NULL);

  XIfEvent (gdk_display, &xevent, graphics_expose_predicate, (XPointer)window);

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

/*
 *--------------------------------------------------------------
 * gdk_event_get
 *
 *   Gets the next event.
 *
 * Arguments:
 *
 * Results:
 *   If an event was received that we care about, returns 
 *   a pointer to that event, to be freed with gdk_event_free.
 *   Otherwise, returns NULL. This function will also return
 *   before an event is received if the timeout interval
 *   runs out.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

GdkEvent *
gdk_event_get (void)
{
  GdkEvent *event;
  GList *temp_list;
  XEvent xevent;

#if 0
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

      if (XCheckIfEvent (gdk_display, &xevent, gdk_event_get_type, (XPointer) & event_pred))
	if (event)
	  return gdk_event_translate (event, &xevent);
    }
  else
#endif
  if (putback_events)
    {
      event = putback_events->data;
      
      temp_list = putback_events;
      putback_events = g_list_remove_link (putback_events, temp_list);
      g_list_free_1 (temp_list);
      
      return event;
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
#ifdef USE_XIM
      gint filter_status;
      if (xim_using && xim_window)
	do
	  {		/* don't dispatch events used by IM */
	    XNextEvent (gdk_display, &xevent);
	    filter_status = XFilterEvent (&xevent, 
					  GDK_WINDOW_XWINDOW (xim_window));
	  } while (filter_status == True);
      else
	XNextEvent (gdk_display, &xevent);
#else
      XNextEvent (gdk_display, &xevent);
#endif
      
      event = gdk_event_new ();

      event->any.type = GDK_NOTHING;
      event->any.window = NULL;
      event->any.send_event = FALSE;
      event->any.send_event = xevent.xany.send_event;
      
      if (gdk_event_translate (event, &xevent))
	return event;
      else
	gdk_event_free (event);
    }

  return NULL;
}

void
gdk_event_put (GdkEvent *event)
{
  GdkEvent *new_event;

  g_return_if_fail (event != NULL);

  new_event = gdk_event_copy (event);

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

static GdkEvent*
gdk_event_new (void)
{
  GdkEvent *new_event;
  
  if (event_chunk == NULL)
    event_chunk = g_mem_chunk_new ("events",
				   sizeof (GdkEvent),
				   4096,
				   G_ALLOC_AND_FREE);

  new_event = g_chunk_new (GdkEvent, event_chunk);

  return new_event;
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

    case GDK_DROP_DATA_AVAIL:
      new_event->dropdataavailable.data_type = g_strdup (event->dropdataavailable.data_type);
      new_event->dropdataavailable.data = g_malloc (event->dropdataavailable.data_numbytes);
      memcpy (new_event->dropdataavailable.data,
	      event->dropdataavailable.data, 
	      event->dropdataavailable.data_numbytes);
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
  g_assert (event_chunk != NULL);
  g_return_if_fail (event != NULL);

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

    case GDK_DROP_DATA_AVAIL:
      g_free (event->dropdataavailable.data_type);
      g_free (event->dropdataavailable.data);
      break;

    case GDK_DRAG_REQUEST:
      g_free (event->dragrequest.data_type);
      break;

    default:
      break;
    }

  g_mem_chunk_free (event_chunk, event);
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
  if (show_events)
    gdk_debug_flags |= GDK_DEBUG_EVENTS;
  else
    gdk_debug_flags &= ~GDK_DEBUG_EVENTS;
}

void
gdk_set_use_xshm (gint use_xshm)
{
  gdk_use_xshm = use_xshm;
}

gint
gdk_get_show_events (void)
{
  return gdk_debug_flags & GDK_DEBUG_EVENTS;
}

gint
gdk_get_use_xshm (void)
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
gdk_time_get (void)
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
gdk_timer_get (void)
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
gdk_timer_enable (void)
{
  timerp = &timer;
}

void
gdk_timer_disable (void)
{
  timerp = NULL;
}

gint
gdk_input_add_full (gint              source,
		    GdkInputCondition condition,
		    GdkInputFunction  function,
		    gpointer          data,
		    GdkDestroyNotify  destroy)
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
	  if (input->destroy)
	    (input->destroy) (input->data);
	  input->function = function;
	  input->data = data;
	  input->destroy = destroy;
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
      input->destroy = destroy;
      tag = input->tag;

      inputs = g_list_prepend (inputs, input);
    }

  return tag;
}

gint
gdk_input_add (gint              source,
	       GdkInputCondition condition,
	       GdkInputFunction  function,
	       gpointer          data)
{
  return gdk_input_add_interp (source, condition, function, data, NULL);
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
	  if (input->destroy)
	    (input->destroy) (input->data);

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

  if (!confine_to || confine_to_private->destroyed)
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
	return_val = XGrabPointer (window_private->xdisplay,
				   xwindow,
				   owner_events,
				   xevent_mask,
				   GrabModeAsync, GrabModeAsync,
				   xconfine_to,
				   xcursor,
				   time);
      else
	return_val = AlreadyGrabbed;
    }

  if (return_val == GrabSuccess)
    xgrab_window = window_private;
  
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
  xgrab_window = NULL;
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
  return xgrab_window != NULL;
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

  if (!window_private->destroyed)
    return XGrabKeyboard (window_private->xdisplay,
			  xwindow,
			  owner_events,
			  GrabModeAsync, GrabModeAsync,
			  time);
  else
    return AlreadyGrabbed;
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
gdk_screen_width (void)
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
gdk_screen_height (void)
{
  gint return_val;

  return_val = DisplayHeight (gdk_display, gdk_screen);

  return return_val;
}

void
gdk_key_repeat_disable (void)
{
  XAutoRepeatOff (gdk_display);
}

void
gdk_key_repeat_restore (void)
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

void gdk_flush (void)
{
  XSync (gdk_display, False);
}


void
gdk_beep (void)
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
gdk_event_wait (void)
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
gdk_event_apply_filters (XEvent *xevent,
			 GdkEvent *event,
			 GList *filters)
{
  GdkEventFilter *filter;
  GList *tmp_list;
  GdkFilterReturn result;

  tmp_list = filters;
  
  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;

      result = (*filter->function)(xevent, event, filter->data);
      if (result !=  GDK_FILTER_CONTINUE)
	return result;
	
      tmp_list = tmp_list->next;
    }

  return GDK_FILTER_CONTINUE;
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

  /* We need to play catch-up with the dnd motion events */
  if(gdk_dnd.drag_really && xevent->type == MotionNotify)
	while (XCheckTypedEvent(xevent->xany.display,MotionNotify,xevent));

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

  if (window != NULL)
    gdk_window_ref (window);
#ifdef USE_XIM
  else if (XFilterEvent(xevent, None)) /* for xlib XIM handling */
    return FALSE;
#endif
  else if(gdk_null_window_warnings) /* Special purpose programs that
				       get events for other windows may
				       want to disable this */
    g_warning ("%#lx -> NULL\n", xevent->xany.window);

  /* Check for filters for this window */

  {
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
#ifdef USE_XIM
      if (buf_len == 0) 
        {
	  buf_len = 128;
	  buf = g_new (gchar, buf_len);
	}
      keysym = GDK_VoidSymbol;

      if (xim_using == TRUE && xim_ic)
	{
	  Status status;
	  
	  /* Clear keyval. Depending on status, may not be set */
	  charcount = XmbLookupString(xim_ic->xic,
				      &xevent->xkey, buf, buf_len-1,
				      &keysym, &status);
	  if (status == XBufferOverflow)
	    {                     /* retry */
	      /* alloc adequate size of buffer */
	      GDK_NOTE (XIM,
		g_print("XIM: overflow (required %i)\n", charcount));

	      while (buf_len <= charcount)
		buf_len *= 2;
	      buf = (gchar *) g_realloc (buf, buf_len);
	     
	      charcount = XmbLookupString (xim_ic->xic,
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

      /* Print debugging info.
       */
#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_EVENTS)
	{
	  g_print ("key press:\twindow: %ld  key: %12s  %d\n",
		   xevent->xkey.window - base_id,
		   event->key.keyval ? XKeysymToString (event->key.keyval) : "(none)",
		   event->key.keyval);
	  if (charcount > 0)
	    g_print ("\t\tlength: %4d string: \"%s\"\n",
		     charcount, buf);
	}
#endif /* G_ENABLE_DEBUG */

      event->key.type = GDK_KEY_PRESS;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;
      event->key.string = g_strdup (buf);
      event->key.length = charcount;

      return_val = window_private && !window_private->destroyed;
      
      if (!return_val)
	g_free (event->key.string);
      
      break;

    case KeyRelease:
      /* Lookup the string corresponding to the given keysym.
       */
      keysym = GDK_VoidSymbol;
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 &keysym, &compose);
      event->key.keyval = keysym;      

      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
	g_print ("key release:\t\twindow: %ld  key: %12s  %d\n",
		 xevent->xkey.window - base_id,
		 XKeysymToString (event->key.keyval),
		 event->key.keyval));

      event->key.type = GDK_KEY_RELEASE;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;
      event->key.length = 0;
      event->key.string = NULL;

      return_val = window_private && !window_private->destroyed;
      break;

    case ButtonPress:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
	g_print ("button press[%d]:\t\twindow: %ld  x,y: %d %d  button: %d\n",
		 window_private?window_private->dnd_drag_enabled:0,
		 xevent->xbutton.window - base_id,
		 xevent->xbutton.x, xevent->xbutton.y,
		 xevent->xbutton.button));

      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	break;

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
	 && !gdk_dnd.drag_perhaps
	 && event->button.button == 1
	 && !gdk_dnd.drag_really)
	{
	  gdk_dnd.drag_perhaps = 1;
	  gdk_dnd.dnd_drag_start.x = xevent->xbutton.x_root;
	  gdk_dnd.dnd_drag_start.y = xevent->xbutton.y_root;
	  gdk_dnd.real_sw = window_private;
	  
	  if(gdk_dnd.drag_startwindows)
	    {
	      g_free(gdk_dnd.drag_startwindows);
	      gdk_dnd.drag_startwindows = NULL;
	    }
	  gdk_dnd.drag_numwindows = gdk_dnd.drag_really = 0;
	  gdk_dnd.dnd_grabbed = FALSE;

	  {
	    /* Set motion mask for first DnD'd window, since it
	       will be the one that is actually dragged */
	    XWindowAttributes dnd_winattr;
	    XSetWindowAttributes dnd_setwinattr;

	    /* We need to get motion events while the button is down, so
	       we can know whether to really start dragging or not... */
	    XGetWindowAttributes(gdk_display, (Window)window_private->xwindow,
				 &dnd_winattr);
	    
	    window_private->dnd_drag_savedeventmask = dnd_winattr.your_event_mask;
	    dnd_setwinattr.event_mask = 
	      window_private->dnd_drag_eventmask = ButtonMotionMask | ButtonPressMask | ButtonReleaseMask |
			EnterWindowMask | LeaveWindowMask | ExposureMask;
	    XChangeWindowAttributes(gdk_display, window_private->xwindow,
				    CWEventMask, &dnd_setwinattr);
	}
      }
      return_val = window_private && !window_private->destroyed;
      break;

    case ButtonRelease:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
	g_print ("button release[%d]:\twindow: %ld  x,y: %d %d  button: %d\n",
		 window_private?window_private->dnd_drag_enabled:0,
		 xevent->xbutton.window - base_id,
		 xevent->xbutton.x, xevent->xbutton.y,
		 xevent->xbutton.button));

      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	break;

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
      event->button.state = (GdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;

      gdk_dnd.last_drop_time = xevent->xbutton.time;
      if(gdk_dnd.drag_perhaps)
	{
	  {
	    XSetWindowAttributes attrs;
	    /* Reset event mask to pre-drag value, assuming event_mask
	       doesn't change during drag */
	    attrs.event_mask = gdk_dnd.real_sw->dnd_drag_savedeventmask;
	    XChangeWindowAttributes(gdk_display, gdk_dnd.real_sw->xwindow,
				      CWEventMask, &attrs);
	  }
	  
	  if (gdk_dnd.dnd_grabbed) 
	    {
	      gdk_dnd_display_drag_cursor(-2,
					  -2,
					  FALSE, TRUE);
	      XUngrabPointer(gdk_display, CurrentTime);
	      gdk_dnd.dnd_grabbed = FALSE;
	    }
	  
	if(gdk_dnd.drag_really)
	  {
	  GdkPoint foo;
	  foo.x = xevent->xbutton.x_root;
	  foo.y = xevent->xbutton.y_root;
			  
	  if(gdk_dnd.dnd_drag_target != None)
	    gdk_dnd_drag_end(gdk_dnd.dnd_drag_target, foo);
	  gdk_dnd.drag_really = 0;

	  gdk_dnd.drag_numwindows = 0;
	  if(gdk_dnd.drag_startwindows)
	    {
	    g_free(gdk_dnd.drag_startwindows);
	    gdk_dnd.drag_startwindows = NULL;
	    }

	  gdk_dnd.real_sw = NULL;
	  }

	gdk_dnd.drag_perhaps = 0;
	gdk_dnd.dnd_drag_start.x = gdk_dnd.dnd_drag_start.y = 0;
	gdk_dnd.dnd_drag_dropzone.x = gdk_dnd.dnd_drag_dropzone.y = 0;
	gdk_dnd.dnd_drag_dropzone.width = gdk_dnd.dnd_drag_dropzone.height = 0;
	gdk_dnd.dnd_drag_curwin = None;
	return_val = window_private?TRUE:FALSE;
      } else
	return_val = window_private && !window_private->destroyed;
      break;

    case MotionNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s d:%d r%d\n",
		 xevent->xmotion.window - base_id,
		 xevent->xmotion.x, xevent->xmotion.y,
		 (xevent->xmotion.is_hint) ? "true" : "false",
		 gdk_dnd.drag_perhaps, gdk_dnd.drag_really));

      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	break;

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
      event->motion.state = (GdkModifierType) xevent->xmotion.state;
      event->motion.is_hint = xevent->xmotion.is_hint;
      event->motion.source = GDK_SOURCE_MOUSE;
      event->motion.deviceid = GDK_CORE_POINTER;

#define IS_IN_ZONE(cx, cy) (cx >= gdk_dnd.dnd_drag_dropzone.x \
     && cy >= gdk_dnd.dnd_drag_dropzone.y \
     && cx < (gdk_dnd.dnd_drag_dropzone.x + gdk_dnd.dnd_drag_dropzone.width) \
     && cy < (gdk_dnd.dnd_drag_dropzone.y + gdk_dnd.dnd_drag_dropzone.height))

      if(gdk_dnd.drag_perhaps && gdk_dnd.drag_really
	 /* && event->motion.is_hint */ /* HINTME */)
	{
	  /* First, we have to find what window the motion was in... */
	  /* XXX there has to be a better way to do this, perhaps with
	     XTranslateCoordinates or XQueryTree - I don't know how,
	     and this sort of works */
	  static Window lastwin = None, curwin = None;
#if 0
	  Window twin;
#endif
	  Window childwin = gdk_root_window;
	  int x, y, ox, oy;

	  /* Interlude - display cursor for the drag ASAP */
	  gdk_dnd_display_drag_cursor(xevent->xmotion.x_root,
				      xevent->xmotion.y_root,
				      gdk_dnd.dnd_drag_target?TRUE:FALSE,
				      FALSE);

	  lastwin = curwin;
	  curwin = gdk_root_window;
	  ox = x = xevent->xmotion.x_root;
	  oy = y = xevent->xmotion.y_root;
#if 1
	  curwin = gdk_window_xid_at_coords(xevent->xmotion.x_root,
					    xevent->xmotion.y_root,
					    gdk_dnd.c->xids,TRUE);
	  XTranslateCoordinates(gdk_display, gdk_root_window, curwin,
				x, y, &x, &y, &childwin);
#else
	  while(childwin != None)
	    {
	      ox = x; oy = y;
	      curwin = childwin;
	      XTranslateCoordinates(gdk_display, curwin, curwin,
				    x, y, &x, &y, &childwin);
	      if(childwin != None) 
		{
		  XTranslateCoordinates(gdk_display, curwin, childwin,
					x, y, &x, &y, &twin);
		}
	    }
#endif
	  GDK_NOTE (DND,
	    g_print("Drag is now in window %#lx, lastwin was %#lx, ddc = %#lx\n",
		    curwin, lastwin, gdk_dnd.dnd_drag_curwin));
	  if(curwin != gdk_dnd.dnd_drag_curwin && curwin != lastwin)
	    {
	      /* We have left one window and entered another
		 (do leave & enter bits) */
	      if(gdk_dnd.dnd_drag_curwin != None)
		  gdk_dnd_drag_leave(gdk_dnd.dnd_drag_curwin);
	      gdk_dnd.dnd_drag_curwin = curwin;
	      gdk_dnd_drag_enter(gdk_dnd.dnd_drag_curwin);
	      gdk_dnd.dnd_drag_dropzone.x = gdk_dnd.dnd_drag_dropzone.y = 0;
	      gdk_dnd.dnd_drag_dropzone.width = gdk_dnd.dnd_drag_dropzone.height = 0;
	      gdk_dnd.dnd_drag_target = None;
	      GDK_NOTE (DND,
	        g_print("curwin = %#lx, lastwin = %#lx, dnd_drag_curwin = %#lx\n",
		        curwin, lastwin, gdk_dnd.dnd_drag_curwin));

	      gdk_dnd_display_drag_cursor(xevent->xmotion.x_root,
					  xevent->xmotion.y_root,
					  FALSE, TRUE);
	    }
	  else if(gdk_dnd.dnd_drag_dropzone.width > 0
		  && gdk_dnd.dnd_drag_dropzone.height > 0
		  && curwin == gdk_dnd.dnd_drag_curwin)
	    {
	      /* Handle all that dropzone stuff - thanks John ;-) */
	      if (gdk_dnd.dnd_drag_target != None)
		{
		  gboolean in_zone = IS_IN_ZONE(xevent->xmotion.x_root,
						xevent->xmotion.y_root);
		  gboolean old_in_zone = IS_IN_ZONE(gdk_dnd.dnd_drag_oldpos.x, 
						    gdk_dnd.dnd_drag_oldpos.y);

		  if (!in_zone && old_in_zone)
		    {
		      /* We were in the drop zone and moved out */
		      gdk_dnd.dnd_drag_target = None;
		      gdk_dnd_drag_leave(curwin);
		      gdk_dnd_display_drag_cursor(xevent->xmotion.x_root,
						  xevent->xmotion.y_root,
						  FALSE, TRUE);
		    }
		  else if (!in_zone && !old_in_zone)
		    {
		      /* We were outside drop zone but in the window
			 - have to send enter events */
		      gdk_dnd_drag_enter(curwin);
		      gdk_dnd.dnd_drag_curwin = curwin;
		      gdk_dnd.dnd_drag_dropzone.x = gdk_dnd.dnd_drag_dropzone.y = 0;
		      gdk_dnd.dnd_drag_target = None;
		    }
		}
	    } /* else
	      dnd_drag_curwin = None; */
	  return_val = FALSE;
	}
      else
	return_val = window_private && !window_private->destroyed;
      break;

    case EnterNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld\n",
		 xevent->xcrossing.window - base_id,
		 xevent->xcrossing.detail,
		 xevent->xcrossing.subwindow - base_id));

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

#ifdef G_ENABLE_DEBUG
        if ((gdk_debug_flags & GDK_DEBUG_DND) & gdk_dnd.drag_perhaps)
	  {
	    g_print("We may[%d] have a drag into %#lx = %#lx\n",
		    gdk_dnd.drag_really,
		    xevent->xcrossing.window, gdk_dnd.real_sw->xwindow);
	  }
#endif /* G_ENABLE_DEBUG */

	if (gdk_dnd.drag_perhaps && gdk_dnd.drag_really && 
	    (xevent->xcrossing.window == gdk_dnd.real_sw->xwindow))
	  {
	    gdk_dnd.drag_really = 0;

	    GDK_NOTE (DND, g_print("Ungrabbed\n"));

	    gdk_dnd.drag_numwindows = 0;
	    g_free(gdk_dnd.drag_startwindows);
	    gdk_dnd.drag_startwindows = NULL;
	    /* We don't want to ungrab the pointer here, or we'll
	     * start getting spurious enter/leave events */
#if 0
	    XChangeActivePointerGrab (gdk_display, 0, None, CurrentTime);
#endif
	  }
	
	return_val = window_private && !window_private->destroyed;
	break;

    case LeaveNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
	g_print ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld\n",
		 xevent->xcrossing.window - base_id,
		 xevent->xcrossing.detail, xevent->xcrossing.subwindow - base_id));

      event->crossing.type = GDK_LEAVE_NOTIFY;
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
#ifdef G_ENABLE_DEBUG
      if ((gdk_debug_flags & GDK_DEBUG_DND) & gdk_dnd.drag_perhaps)
	{
	  g_print("We may[%d] have a drag out of %#lx = %#lx\n",
		  gdk_dnd.drag_really,
		  xevent->xcrossing.window, gdk_dnd.real_sw->xwindow);
	}
#endif /* G_ENABLE_DEBUG */
      if (gdk_dnd.drag_perhaps && !gdk_dnd.drag_really &&
	  (xevent->xcrossing.window == gdk_dnd.real_sw->xwindow))
	{
	  gboolean xgpret;
	  gdk_dnd_drag_addwindow((GdkWindow *) gdk_dnd.real_sw);
	  gdk_dnd_drag_begin((GdkWindow *) gdk_dnd.real_sw);
	  xgpret = 
	  XGrabPointer(gdk_display, gdk_dnd.real_sw->xwindow, False,
		       ButtonMotionMask | PointerMotionMask |
		       /* PointerMotionHintMask | */ /* HINTME */
		       ButtonPressMask | ButtonReleaseMask,
		       GrabModeAsync, GrabModeAsync, None,
		       None, CurrentTime);
#ifdef G_ENABLE_DEBUG
	  GDK_NOTE(DND, g_print("xgpret = %d\n", xgpret));
#endif
	  gdk_dnd.dnd_grabbed = TRUE;
	  gdk_dnd.drag_really = 1;
	  gdk_dnd_display_drag_cursor(xevent->xmotion.x_root,
				      xevent->xmotion.y_root,
				      FALSE, TRUE);
	}

      return_val = window_private && !window_private->destroyed;
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
	    g_print ("focus %s:\t\twindow: %ld\n",
		     (xevent->xany.type == FocusIn) ? "in" : "out",
		     xevent->xfocus.window - base_id));
	  
	  event->focus_change.type = GDK_FOCUS_CHANGE;
	  event->focus_change.window = window;
	  event->focus_change.in = (xevent->xany.type == FocusIn);
	  
	  return_val = window_private && !window_private->destroyed;
	  break;
	default:
          ;
	}
      break;

    case KeymapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("keymap notify\n"));

      /* Not currently handled */
      break;

    case Expose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("expose:\t\twindow: %ld  %d  x,y: %d %d  w,h: %d %d\n",
		 xevent->xexpose.window - base_id, xevent->xexpose.count,
		 xevent->xexpose.x, xevent->xexpose.y,
		 xevent->xexpose.width, xevent->xexpose.height));

      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->xexpose.x;
      event->expose.area.y = xevent->xexpose.y;
      event->expose.area.width = xevent->xexpose.width;
      event->expose.area.height = xevent->xexpose.height;
      event->expose.count = xevent->xexpose.count;

      return_val = window_private && !window_private->destroyed;
      break;

    case GraphicsExpose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("graphics expose:\tdrawable: %ld\n",
		 xevent->xgraphicsexpose.drawable - base_id));

      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->xgraphicsexpose.x;
      event->expose.area.y = xevent->xgraphicsexpose.y;
      event->expose.area.width = xevent->xgraphicsexpose.width;
      event->expose.area.height = xevent->xgraphicsexpose.height;
      event->expose.count = xevent->xexpose.count;

      return_val = window_private && !window_private->destroyed;
      break;

    case NoExpose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("no expose:\t\tdrawable: %ld\n",
		 xevent->xnoexpose.drawable - base_id));

      event->no_expose.type = GDK_NO_EXPOSE;
      event->no_expose.window = window;

      return_val = window_private && !window_private->destroyed;
      break;

    case VisibilityNotify:
      /* Print debugging info.
       */
#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_EVENTS)
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
      
      return_val = window_private && !window_private->destroyed;
      break;

    case CreateNotify:
      /* Not currently handled */
      break;

    case DestroyNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("destroy notify:\twindow: %ld\n",
		 xevent->xdestroywindow.window - base_id));

      event->any.type = GDK_DESTROY;
      event->any.window = window;

      return_val = window_private && !window_private->destroyed;

      gdk_window_destroy_notify (window);
      break;

    case UnmapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("unmap notify:\t\twindow: %ld\n",
		 xevent->xmap.window - base_id));

      event->any.type = GDK_UNMAP;
      event->any.window = window;

      if (xgrab_window == window_private)
	xgrab_window = NULL;

      return_val = window_private && !window_private->destroyed;
      break;

    case MapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("map notify:\t\twindow: %ld\n",
		 xevent->xmap.window - base_id));

      event->any.type = GDK_MAP;
      event->any.window = window;

      return_val = window_private && !window_private->destroyed;
      break;

    case ReparentNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("reparent notify:\twindow: %ld\n",
		 xevent->xreparent.window - base_id));

      /* Not currently handled */
      break;

    case ConfigureNotify:
      /* Print debugging info.
       */
      while ((XPending (gdk_display) > 0) &&
	     XCheckTypedWindowEvent(gdk_display, xevent->xany.window,
				    ConfigureNotify, xevent))
        {
	  GdkFilterReturn result;

	  GDK_NOTE (EVENTS, 
		    g_print ("configure notify discarded:\twindow: %ld\n",
			     xevent->xconfigure.window - base_id));

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
	g_print ("configure notify:\twindow: %ld  x,y: %d %d  w,h: %d %d  b-w: %d  above: %ld  ovr: %d\n",
		 xevent->xconfigure.window - base_id,
		 xevent->xconfigure.x,
		 xevent->xconfigure.y,
		 xevent->xconfigure.width,
		 xevent->xconfigure.height,
		 xevent->xconfigure.border_width,
		 xevent->xconfigure.above - base_id,
		 xevent->xconfigure.override_redirect));
      
      if (window_private)
	{
	  if (!window_private->destroyed &&
	      (window_private->extension_events != 0) &&
	      gdk_input_vtable.configure_event)
	    gdk_input_vtable.configure_event (&xevent->xconfigure, window);
	  
	  if (window_private->window_type != GDK_WINDOW_CHILD)
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

		  if (!XTranslateCoordinates (window_private->xdisplay,
					      window_private->xwindow,
					      gdk_root_window,
					      0, 0,
					      &tx, &ty,
					      &child_window))
		    g_warning ("GdkWindow %ld doesn't share root windows display?",
			       window_private->xwindow - base_id);
		  event->configure.x = tx;
		  event->configure.y = ty;
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
	      
	      return_val = !window_private->destroyed;
	    }
	}
      break;
      
    case PropertyNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("property notify:\twindow: %ld\n",
		 xevent->xproperty.window - base_id));

      event->property.type = GDK_PROPERTY_NOTIFY;
      event->property.window = window;
      event->property.atom = xevent->xproperty.atom;
      event->property.time = xevent->xproperty.time;
      event->property.state = xevent->xproperty.state;

      return_val = window_private && !window_private->destroyed;
      break;

    case SelectionClear:
      GDK_NOTE (EVENTS,
	g_print ("selection clear:\twindow: %ld\n",
		 xevent->xproperty.window - base_id));

      event->selection.type = GDK_SELECTION_CLEAR;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionclear.selection;
      event->selection.time = xevent->xselectionclear.time;

      return_val = window_private && !window_private->destroyed;
      break;

    case SelectionRequest:
      GDK_NOTE (EVENTS,
	g_print ("selection request:\twindow: %ld\n",
		 xevent->xproperty.window - base_id));

      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionrequest.selection;
      event->selection.target = xevent->xselectionrequest.target;
      event->selection.property = xevent->xselectionrequest.property;
      event->selection.requestor = xevent->xselectionrequest.requestor;
      event->selection.time = xevent->xselectionrequest.time;

      return_val = window_private && !window_private->destroyed;
      break;

    case SelectionNotify:
      GDK_NOTE (EVENTS,
	g_print ("selection notify:\twindow: %ld\n",
		 xevent->xproperty.window - base_id));


      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = xevent->xselection.selection;
      event->selection.target = xevent->xselection.target;
      event->selection.property = xevent->xselection.property;
      event->selection.time = xevent->xselection.time;

      return_val = window_private && !window_private->destroyed;
      break;

    case ColormapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("colormap notify:\twindow: %ld\n",
		 xevent->xcolormap.window - base_id));

      /* Not currently handled */
      break;

    case ClientMessage:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("client message:\twindow: %ld\n",
		 xevent->xclient.window - base_id));

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
	      GDK_NOTE (EVENTS,
		g_print ("delete window:\t\twindow: %ld\n",
			 xevent->xclient.window - base_id));

	      event->any.type = GDK_DELETE;
	      event->any.window = window;

	      return_val = window_private && !window_private->destroyed;
	    }
	  else if ((Atom) xevent->xclient.data.l[0] == gdk_wm_take_focus)
	    {
	    }
	}
      else if (xevent->xclient.message_type == gdk_dnd.gdk_XdeEnter)
	{
	  Atom reptype = 0;

	  event->dropenter.u.allflags = xevent->xclient.data.l[1];

	  GDK_NOTE (DND, g_print ("GDK_DROP_ENTER [%d][%d]\n",
		window_private->dnd_drop_enabled, event->dropenter.u.flags.sendreply));
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

	      if (!gdk_send_xevent (replyev.xclient.window, False, 
				    NoEventMask, &replyev))
		GDK_NOTE (DND, g_print("Sending XdeRequest to %#lx failed\n",
				       replyev.xclient.window));

	      event->any.type = GDK_DROP_ENTER;
	      event->any.window = window;
	      event->dropenter.requestor = replyev.xclient.window;
	      event->dropenter.u.allflags = xevent->xclient.data.l[1];

	      GDK_NOTE (DND, g_print("We sent a GDK_DROP_ENTER on to Gtk\n"));
	      return_val = TRUE;
	    }
	}
      else if (xevent->xclient.message_type == gdk_dnd.gdk_XdeLeave)
	{
#ifdef G_ENABLE_DEBUG	  
	  if (gdk_debug_flags & (GDK_DEBUG_EVENTS | GDK_DEBUG_DND))
	    g_print ("GDK_DROP_LEAVE\n");
#endif	  

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
#ifdef G_ENABLE_DEBUG	  
	  if (gdk_debug_flags & (GDK_DEBUG_EVENTS | GDK_DEBUG_DND))
	    g_print ("GDK_DRAG_REQUEST\n");
#endif	  
	  event->dragrequest.u.allflags = xevent->xclient.data.l[1];
	  return_val = FALSE;

	  if (window && gdk_dnd.drag_really &&
	      xevent->xclient.data.l[0] == gdk_dnd.dnd_drag_curwin &&
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

		  gdk_dnd.dnd_drag_target = gdk_dnd.dnd_drag_curwin;
		  gdk_dnd_display_drag_cursor(-1, -1, TRUE, TRUE);
		}
	      gdk_dnd.dnd_drag_dropzone.x = xevent->xclient.data.l[2] & 65535;
	      gdk_dnd.dnd_drag_dropzone.y =
		(xevent->xclient.data.l[2] >> 16) & 65535;
	      gdk_dnd.dnd_drag_dropzone.width = xevent->xclient.data.l[3] & 65535;
	      gdk_dnd.dnd_drag_dropzone.height =
		(xevent->xclient.data.l[3] >> 16) & 65535;
	    }
	}
      else if(xevent->xclient.message_type == gdk_dnd.gdk_XdeDataAvailable)
	  {
	    gint tmp_int; Atom tmp_atom;
	    gulong tmp_long;
	    guchar *tmp_charptr;

#ifdef G_ENABLE_DEBUG	  
	  if (gdk_debug_flags & (GDK_DEBUG_EVENTS | GDK_DEBUG_DND))
	    g_print("GDK_DROP_DATA_AVAIL\n");
#endif	  
	    event->dropdataavailable.u.allflags = xevent->xclient.data.l[1];
	    event->dropdataavailable.timestamp = xevent->xclient.data.l[4];
	    event->dropdataavailable.coords.x =
			xevent->xclient.data.l[3] & 0xffff;
	    event->dropdataavailable.coords.y =
			(xevent->xclient.data.l[3] >> 16) & 0xffff;
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
		    GDK_NOTE (DND, g_print("XGetWindowProperty got us %ld bytes\n",
			    event->dropdataavailable.data_numbytes));
		    event->dropdataavailable.data =
			g_malloc (event->dropdataavailable.data_numbytes);
		    memcpy (event->dropdataavailable.data,
			tmp_charptr, event->dropdataavailable.data_numbytes);
		    XFree(tmp_charptr);
		    return_val = TRUE;
		  }
	      return_val = TRUE;
	    }
	}
      else
	{
	  /* Send unknown ClientMessage's on to Gtk for it to use */
	  event->client.type = GDK_CLIENT_EVENT;
	  event->client.window = window;
	  event->client.message_type = xevent->xclient.message_type;
	  event->client.data_format = xevent->xclient.format;
	  memcpy(&event->client.data, &xevent->xclient.data,
		 sizeof(event->client.data));
	  if(window)
	    return_val = TRUE;
	  else	
	    return_val = FALSE;
	}
      if(window_private)
	return_val = return_val && !window_private->destroyed;
      break;
      
    case MappingNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
	g_print ("mapping notify\n"));

      /* Let XLib know that there is a new keyboard mapping.
       */
      XRefreshKeyboardMapping (&xevent->xmapping);
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

      return_val = return_val && !window_private->destroyed;

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
gdk_exit_func (void)
{
  static gboolean in_gdk_exit_func = FALSE;

  /* This is to avoid an infinite loop if a program segfaults in
     an atexit() handler (and yes, it does happen, especially if a program
     has trounced over memory too badly for even g_print to work) */
  if(in_gdk_exit_func == TRUE) return;
  in_gdk_exit_func = TRUE;

  if (initialized)
    {
#ifdef USE_XIM
      /* cleanup IC */
      gdk_ic_cleanup ();
      /* close IM */
      gdk_im_close ();
#endif
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
      g_error ("%s\n  serial %ld error_code %d request_code %d minor_code %d\n", 
	       buf, 
	       error->serial, 
	       error->error_code, 
	       error->request_code,
	       error->minor_code);
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
#ifdef G_ENABLE_DEBUG
  abort ();
#else /* !G_ENABLE_DEBUG */
  gdk_exit (1);
#endif /* !G_ENABLE_DEBUG */
}

static void
gdk_dnd_drag_begin (GdkWindow *initial_window)
{
  GdkEvent tev;

  GDK_NOTE(DND, g_print("------- STARTING DRAG from %p\n", initial_window));

  tev.type = GDK_DRAG_BEGIN;
  tev.dragbegin.window = initial_window;
  tev.dragbegin.u.allflags = 0;
  tev.dragbegin.u.flags.protocol_version = DND_PROTOCOL_VERSION;

  gdk_event_put (&tev);
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
	  if (!gdk_send_xevent (dest, False, StructureNotifyMask, &sev))
		GDK_NOTE (DND, g_print("Sending XdeEnter to %#lx failed\n",
				       dest));
	}

    }
}


#ifdef USE_XIM

/* The following routines duplicate functionality in Xlib to
 * translate from varargs to X's internal opaque XVaNestedList.
 * 
 * If all vendors have stuck close to the reference implementation,
 * then we should hopefully be OK. 
 */

/* This needs to match XIMArg as defined in Xlcint.h exactly */
  
typedef struct {
  gchar   *name;
  gpointer value;
} GdkImArg;

/*************************************************************
 * gdk_im_va_count:
 *    Counts the number of name/value pairs in the vararg list
 *
 *   arguments:
 *     
 *   results:
 *************************************************************/

static guint 
gdk_im_va_count (va_list list)
{
  gint count = 0;
  gchar *name;

  name = va_arg (list, gchar *);
  while (name)
    {
      count++;
      (void)va_arg (list, gpointer);
      name = va_arg (list, gchar *);
    }

  return count;
}

/*************************************************************
 * gdk_im_va_to_nested:
 *     Given a varargs list and the result of gdk_im_va_count,
 *     create a XVaNestedList.
 *
 *   arguments:
 *     
 *   results:
 *************************************************************/

static XVaNestedList
gdk_im_va_to_nested (va_list list, guint count)
{
  GdkImArg *result;
  GdkImArg *arg;

  gchar *name;

  if (count == 0)
    return NULL;

  result = g_new (GdkImArg, count+1);
  arg = result;

  name = va_arg (list, gchar *);
  while (name)
    {
      arg->name = name;
      arg->value = va_arg (list, gpointer);
      arg++;
      name = va_arg (list, gchar *);
    }

  arg->name = NULL;

  return (XVaNestedList)result;
}

/*
 *--------------------------------------------------------------
 * gdk_im_begin
 *
 *   Begin using input method with XIM Protocol(X11R6 standard)
 *
 * Arguments:
 *   "ic" is the "Input Context" which is created by gtk_ic_new.
 *   The input area is specified with "window".
 *
 * Results:
 *   The gdk's event handling routine is switched to XIM based routine.
 *   XIM based routine uses XFilterEvent to get rid of events used by IM,
 *   and uses XmbLookupString instead of XLookupString.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void 
gdk_im_begin (GdkIC ic, GdkWindow* window)
{
  GdkICPrivate *private;
  Window xwin;

  g_return_if_fail (ic != NULL);
  g_return_if_fail (window);

  private = (GdkICPrivate *) ic;

  xim_using = TRUE;
  xim_ic = private;
  xim_window = window;
  if (gdk_im_ready())
    {
      XGetICValues (private->xic, XNFocusWindow, &xwin, NULL);
      if (xwin != GDK_WINDOW_XWINDOW(window))
        XSetICValues (private->xic, XNFocusWindow, 
		      GDK_WINDOW_XWINDOW(window), NULL);
      if (private != xim_ic)
	XSetICFocus (private->xic);
    }
}

/*
 *--------------------------------------------------------------
 * gdk_im_end
 *
 *   End using input method with XIM Protocol(X11R6 standard)
 *
 * Arguments:
 *
 * Results:
 *   The gdk's event handling routine is switched to normal routine.
 *   User should call this function before ic and window will be destroyed.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void 
gdk_im_end (void)
{
  xim_using = FALSE;
  xim_ic = NULL;
  xim_window = NULL;
}

static GdkIM 
gdk_im_get (void)
{
  return xim_im;
}

static GdkIMStyle 
gdk_im_choose_better_style (GdkIMStyle style1, GdkIMStyle style2) 
{
  GdkIMStyle s1, s2, u;

  if (style1 == 0) return style2;
  if (style2 == 0) return style1;
  if ((style1 & (GdkIMPreeditMask | GdkIMStatusMask))
    	== (style2 & (GdkIMPreeditMask | GdkIMStatusMask)))
    return style1;

  s1 = style1 & GdkIMPreeditMask;
  s2 = style2 & GdkIMPreeditMask;
  u = s1 | s2;
  if (s1 != s2) {
    if (u & GdkIMPreeditCallbacks)
      return (s1 == GdkIMPreeditCallbacks)? style1:style2;
    else if (u & GdkIMPreeditPosition)
      return (s1 == GdkIMPreeditPosition)? style1:style2;
    else if (u & GdkIMPreeditArea)
      return (s1 == GdkIMPreeditArea)? style1:style2;
    else if (u & GdkIMPreeditNothing)
      return (s1 == GdkIMPreeditNothing)? style1:style2;
  } else {
    s1 = style1 & GdkIMStatusMask;
    s2 = style2 & GdkIMStatusMask;
    u = s1 | s2;
    if ( u & GdkIMStatusCallbacks)
      return (s1 == GdkIMStatusCallbacks)? style1:style2;
    else if ( u & GdkIMStatusArea)
      return (s1 == GdkIMStatusArea)? style1:style2;
    else if ( u & GdkIMStatusNothing)
      return (s1 == GdkIMStatusNothing)? style1:style2;
    else if ( u & GdkIMStatusNone)
      return (s1 == GdkIMStatusNone)? style1:style2;
  }
  return 0; /* Get rid of stupid warning */
}

GdkIMStyle
gdk_im_decide_style (GdkIMStyle supported_style)
{
  gint i;
  GdkIMStyle style, tmp;

  g_return_val_if_fail (xim_styles != NULL, 0);

  style = 0;
  for (i=0; i<xim_styles->count_styles; i++)
    {
      tmp = xim_styles->supported_styles[i];
      if (tmp == (tmp & supported_style & xim_best_allowed_style))
	style = gdk_im_choose_better_style (style, tmp);
    }
  return style;
}

GdkIMStyle
gdk_im_set_best_style (GdkIMStyle style)
{
  if (style & GdkIMPreeditMask)
    {
      xim_best_allowed_style &= ~GdkIMPreeditMask;

      xim_best_allowed_style |= GdkIMPreeditNone;
      if (!(style & GdkIMPreeditNone))
	{
	  xim_best_allowed_style |= GdkIMPreeditNothing;
	  if (!(style & GdkIMPreeditNothing))
	    {
	      xim_best_allowed_style |= GdkIMPreeditArea;
	      if (!(style & GdkIMPreeditArea))
		{
		  xim_best_allowed_style |= GdkIMPreeditPosition;
		  if (!(style & GdkIMPreeditPosition))
		    xim_best_allowed_style |= GdkIMPreeditCallbacks;
		}
	    }
	}
    }
  if (style & GdkIMStatusMask)
    {
      xim_best_allowed_style &= ~GdkIMStatusMask;

      xim_best_allowed_style |= GdkIMStatusNone;
      if (!(style & GdkIMStatusNone))
	{
	  xim_best_allowed_style |= GdkIMStatusNothing;
	  if (!(style & GdkIMStatusNothing))
	    {
	      xim_best_allowed_style |= GdkIMStatusArea;
	      if (!(style & GdkIMStatusArea))
		xim_best_allowed_style |= GdkIMStatusCallbacks;
	    }
	}
    }
  
  return xim_best_allowed_style;
}

static gint 
gdk_im_open (XrmDatabase db, gchar* res_name, gchar* res_class)
{
  xim_im = XOpenIM (GDK_DISPLAY(), db, res_name, res_class);
  if (xim_im == NULL)
    {
      GDK_NOTE (XIM, g_warning ("Unable to open open IM."));
      return FALSE;
    }
  XGetIMValues (xim_im, XNQueryInputStyle, &xim_styles, NULL, NULL);

  return TRUE;
}

static void 
gdk_im_close (void)
{
  if (xim_im)
    {
      XCloseIM (xim_im);
      xim_im = NULL;
    }
  if (xim_styles)
    {
      XFree (xim_styles);
      xim_styles = NULL;
    }
}

gint 
gdk_im_ready (void)
{
  return (xim_im != NULL);
}

GdkIC 
gdk_ic_new (GdkWindow* client_window,
	    GdkWindow* focus_window,
	    GdkIMStyle style, ...)
{
  va_list list;
  GdkICPrivate *private;
  XVaNestedList preedit_attr = NULL;
  guint count;

  g_return_val_if_fail (client_window != NULL, NULL);
  g_return_val_if_fail (focus_window != NULL, NULL);
  g_return_val_if_fail (gdk_im_ready(), NULL);

  private = g_new (GdkICPrivate, 1);

  va_start (list, style);
  count = gdk_im_va_count (list);
  va_end (list);

  va_start (list, style);
  preedit_attr = gdk_im_va_to_nested (list, count);
  va_end (list);
  
  private->style = gdk_im_decide_style (style);
  if (private->style != style)
    {
      g_warning ("can not create input context with specified input style.");
      g_free (private);
      return NULL;
    }

  private->xic = XCreateIC(gdk_im_get (),
  		 	XNInputStyle,   style,
		      	XNClientWindow, GDK_WINDOW_XWINDOW (client_window),
		      	XNFocusWindow,  GDK_WINDOW_XWINDOW (focus_window),
		      	preedit_attr? XNPreeditAttributes : NULL, preedit_attr,
		      	NULL);

  g_free (preedit_attr);
  
  if (!private->xic)
    {
      g_free (private);
      return NULL;
    }

  xim_ic_list = g_list_append (xim_ic_list, private);
  return private;
}

void 
gdk_ic_destroy (GdkIC ic)
{
  GdkICPrivate *private;

  g_return_if_fail (ic != NULL);
  
  private = (GdkICPrivate *) ic;

  if (xim_ic == private)
    gdk_im_end ();

  XDestroyIC (private->xic);
  xim_ic_list = g_list_remove (xim_ic_list, private);
  g_free (private);
}

GdkIMStyle
gdk_ic_get_style (GdkIC ic)
{
  GdkICPrivate *private;

  g_return_val_if_fail (ic != NULL, 0);

  private = (GdkICPrivate *) ic;

  return private->style;
}

void 
gdk_ic_set_values (GdkIC ic, ...)
{
  va_list list;
  XVaNestedList args;
  GdkICPrivate *private;
  guint count;

  g_return_if_fail (ic != NULL);

  private = (GdkICPrivate *) ic;

  va_start (list, ic);
  count = gdk_im_va_count (list);
  va_end (list);

  va_start (list, ic);
  args = gdk_im_va_to_nested (list, count);
  va_end (list);

  XSetICValues (private->xic, XNVaNestedList, args, NULL);

  g_free (args);
}

void 
gdk_ic_get_values (GdkIC ic, ...)
{
  va_list list;
  XVaNestedList args;
  GdkICPrivate *private;
  guint count;

  g_return_if_fail (ic != NULL);

  private = (GdkICPrivate *) ic;

  va_start (list, ic);
  count = gdk_im_va_count (list);
  va_end (list);

  va_start (list, ic);
  args = gdk_im_va_to_nested (list, count);
  va_end (list);

  XGetICValues (private->xic, XNVaNestedList, args, NULL);

  g_free (args);
}

void 
gdk_ic_set_attr (GdkIC ic, const char *target, ...)
{
  va_list list;
  XVaNestedList attr;
  GdkICPrivate *private;
  guint count;

  g_return_if_fail (ic != NULL);
  g_return_if_fail (target != NULL);

  private = (GdkICPrivate *) ic;

  va_start (list, target);
  count = gdk_im_va_count (list);
  va_end (list);

  va_start (list, target);
  attr = gdk_im_va_to_nested (list, count);
  va_end (list);

  XSetICValues (private->xic, target, attr, NULL);

  g_free (attr);
}

void 
gdk_ic_get_attr (GdkIC ic, const char *target, ...)
{
  va_list list;
  XVaNestedList attr;
  GdkICPrivate *private;
  guint count;

  g_return_if_fail (ic != NULL);
  g_return_if_fail (target != NULL);

  private = (GdkICPrivate *) ic;

  va_start (list, target);
  count = gdk_im_va_count (list);
  va_end (list);

  va_start (list, target);
  attr = gdk_im_va_to_nested (list, count);
  va_end (list);

  XGetICValues (private->xic, target, attr, NULL);

  g_free (attr);
}

GdkEventMask 
gdk_ic_get_events (GdkIC ic)
{
  GdkEventMask mask;
  glong xmask;
  glong bit;
  GdkICPrivate *private;
  gint i;

  /*  From gdkwindow.c  */
  extern int nevent_masks;
  extern int event_mask_table[];

  g_return_val_if_fail (ic != NULL, 0);

  private = (GdkICPrivate *) ic;

  if (XGetICValues (private->xic, XNFilterEvents, &xmask, NULL) != NULL)
    {
      GDK_NOTE (XIM, g_warning ("Call to XGetICValues: %s failed", XNFilterEvents));
      return 0;
    }

  mask = 0;
  for (i=0, bit=2; i < nevent_masks; i++, bit <<= 1)
    if (xmask & event_mask_table [i])
      {
	mask |= bit;
	xmask &= ~ event_mask_table [i];
      }

  if (xmask)
    g_warning ("ic requires events not supported by the application (%#04lx)", xmask);
  
  return mask;
}

static void 
gdk_ic_cleanup (void)
{
  GList* node;
  gint destroyed;
  GdkICPrivate *private;

  destroyed = 0;
  for (node = xim_ic_list; node != NULL; node = node->next)
    {
      if (node->data)
       {
	 private = (GdkICPrivate *) (node->data);
         XDestroyIC (private->xic);
	 g_free (private);
         destroyed++;
       }
    }
#ifdef G_ENABLE_DEBUG
  if ((gdk_debug_flags & GDK_DEBUG_XIM) && destroyed > 0)
    {
      g_warning ("Cleaned up %i IC(s)\n", destroyed);
    }
#endif /* G_ENABLE_DEBUG */
  g_list_free(xim_ic_list);
  xim_ic_list = NULL;
}

#else /* !USE_XIM */

void 
gdk_im_begin (GdkIC ic, GdkWindow* window)
{
}

void 
gdk_im_end (void)
{
}

GdkIMStyle
gdk_im_decide_style (GdkIMStyle supported_style)
{
  return GdkIMPreeditNone | GdkIMStatusNone;
}

GdkIMStyle
gdk_im_set_best_style (GdkIMStyle style)
{
  return GdkIMPreeditNone | GdkIMStatusNone;
}

gint 
gdk_im_ready (void)
{
  return FALSE;
}

GdkIC 
gdk_ic_new (GdkWindow* client_window,
	    GdkWindow* focus_window,
	    GdkIMStyle style, ...)
{
  return NULL;
}

void 
gdk_ic_destroy (GdkIC ic)
{
}

GdkIMStyle
gdk_ic_get_style (GdkIC ic)
{
  return GdkIMPreeditNone | GdkIMStatusNone;
}

void 
gdk_ic_set_values (GdkIC ic, ...)
{
}

void 
gdk_ic_get_values (GdkIC ic, ...)
{
}

void 
gdk_ic_set_attr (GdkIC ic, const char *target, ...)
{
}

void 
gdk_ic_get_attr (GdkIC ic, const char *target, ...)
{
}

GdkEventMask 
gdk_ic_get_events (GdkIC ic)
{
  return 0;
}

#endif /* USE_XIM */

#ifdef X_LOCALE

gint
_g_mbtowc (wchar_t *wstr, const char *str, size_t len)
{
  static wchar_t wcs[MB_CUR_MAX + 1];
  static gchar mbs[MB_CUR_MAX + 1];

  wcs[0] = (wchar_t) NULL;
  mbs[0] = '\0';

  /* The last argument isn't a mistake. The X locale code trims
   *  the input string to the length of the output string!
   */
  len = _Xmbstowcs (wcs, str, (len<MB_CUR_MAX)? len:MB_CUR_MAX);
  if (len < 1)
    return len;
  else if (wcs[0] == (wchar_t) NULL)
    return -1;

  len = _Xwctomb (mbs, wcs[0]);
  if (mbs[0] == '\0')
    return -1;
  if (wstr)
    *wstr = wcs[0];

  return len;
}

#endif /* X_LOCALE */

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
      if (!gdk_send_xevent (dest, False, StructureNotifyMask, &sev))
	GDK_NOTE (DND, g_print("Sending XdeLeave to %#lx failed\n",
			       dest));
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
  GdkEvent tev;
  int i;

  tev.dragrequest.type = GDK_DRAG_REQUEST;
  tev.dragrequest.drop_coords = coords;
  tev.dragrequest.requestor = dest;
  tev.dragrequest.u.allflags = 0;
  tev.dragrequest.u.flags.protocol_version = DND_PROTOCOL_VERSION;
  tev.dragrequest.isdrop = 1;

  for (i = 0; i < gdk_dnd.drag_numwindows; i++)
    {
      wp = (GdkWindowPrivate *) gdk_dnd.drag_startwindows[i];
      if (wp->dnd_drag_accepted)
	{
	  tev.dragrequest.window = (GdkWindow *) wp;
	  tev.dragrequest.u.flags.delete_data = wp->dnd_drag_destructive_op;
          tev.dragrequest.timestamp = gdk_dnd.last_drop_time;
	  tev.dragrequest.data_type = 
	  	gdk_atom_name(wp->dnd_drag_data_type);

	  gdk_event_put(&tev);
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
	  g_warning("XdeTypelist property had format of %d instead of the expected %ld, on window %#lx\n",
		    realfmt, (glong)sizeof(Atom) * 8, xevent->xclient.data.l[0]);
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
#ifdef DEBUG_DND
static void
gdk_print_atom (GdkAtom anatom)
{
  gchar *tmpstr = NULL;
  tmpstr = (anatom!=None)?gdk_atom_name(anatom):"(none)";
  g_print("Atom %lu has name %s\n", anatom, tmpstr);
  if(tmpstr)
    g_free(tmpstr);
}
#endif

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
      XFree(data);
    }

  for (i = 0; !inf && (i < nchildren); i++)
    inf = getchildren (dpy, children[i], WM_STATE);

  if (children != None)
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

#ifdef WE_HAVE_MOTIF_DROPS_DONE
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
#endif

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
    if (!gdk_send_xevent (curwin, False, NoEventMask, &sev))
      GDK_NOTE (MISC, g_print("Gdk: Sending client message %ld to %#lx failed\n",
			     event->client.message_type, curwin));
  }

  XFree(ret_children);
}

gchar *
gdk_get_display(void)
{
  return (gchar *)XDisplayName (gdk_display_name);
}

gint 
gdk_send_xevent (Window window, gboolean propagate, glong event_mask,
		 XEvent *event_send)
{
  Status result;
  
  gdk_error_code = 0;
  
  gdk_error_warnings = 0;
  result = XSendEvent (gdk_display, window, propagate, event_mask, event_send);
  XSync (gdk_display, False);
  gdk_error_warnings = 1;
    
  return result && (gdk_error_code != -1);
}
