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

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H_ */

#define XLIB_ILLEGAL_ACCESS
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "gdk.h"
#include "gdkprivate.h"
#include "gdkinput.h"
#include "gdkx.h"
#include "gdki18n.h"
#include "gdkkeysyms.h"

#ifndef X_GETTIMEOFDAY
#define X_GETTIMEOFDAY(tv)  gettimeofday (tv, NULL)
#endif /* X_GETTIMEOFDAY */


typedef struct _GdkPredicate  GdkPredicate;
typedef struct _GdkErrorTrap  GdkErrorTrap;

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

struct _GdkErrorTrap
{
  gint error_warnings;
  gint error_code;
};

/* 
 * Private function declarations
 */

#ifndef HAVE_XCONVERTCASE
static void	 gdkx_XConvertCase	(KeySym	       symbol,
					 KeySym	      *lower,
					 KeySym	      *upper);
#define XConvertCase gdkx_XConvertCase
#endif

static void	    gdk_exit_func		 (void);
static int	    gdk_x_error			 (Display     *display, 
						  XErrorEvent *error);
static int	    gdk_x_io_error		 (Display     *display);

GdkFilterReturn gdk_wm_protocols_filter (GdkXEvent *xev,
					 GdkEvent  *event,
					 gpointer   data);

/* Private variable declarations
 */
static int gdk_initialized = 0;			    /* 1 if the library is initialized,
						     * 0 otherwise.
						     */

static struct timeval start;			    /* The time at which the library was
						     *	last initialized.
						     */
static struct timeval timer;			    /* Timeout interval to use in the call
						     *	to "select". This is used in
						     *	conjunction with "timerp" to create
						     *	a maximum time to wait for an event
						     *	to arrive.
						     */
static struct timeval *timerp;			    /* The actual timer passed to "select"
						     *	This may be NULL, in which case
						     *	"select" will block until an event
						     *	arrives.
						     */
static guint32 timer_val;			    /* The timeout length as specified by
						     *	the user in milliseconds.
						     */
static gint autorepeat;

static GSList *gdk_error_traps = NULL;               /* List of error traps */
static GSList *gdk_error_trap_free_list = NULL;      /* Free list */

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"events",	    GDK_DEBUG_EVENTS},
  {"misc",	    GDK_DEBUG_MISC},
  {"dnd",	    GDK_DEBUG_DND},
  {"color-context", GDK_DEBUG_COLOR_CONTEXT},
  {"xim",	    GDK_DEBUG_XIM}
};

static const int gdk_ndebug_keys = sizeof(gdk_debug_keys)/sizeof(GDebugKey);

#endif /* G_ENABLE_DEBUG */

static const char *
get_option (char ***argv,
	    gint    argc,
	    int    *i_inout)
{
  gchar *equal_pos;
  const gchar *result = NULL;
  gint i = *i_inout;
  const gchar *orig = (*argv)[i];

  equal_pos = strchr ((*argv)[i], '=');
  (*argv)[i] = NULL;

  if (equal_pos)
    {
      result = equal_pos + 1;
    }
  else
    {
      if ((i + 1) < argc && (*argv)[i + 1])
	{
	  i++;
	  result = (*argv)[i];
	  (*argv)[i] = NULL;
	}
      else
	{
	  g_warning ("Option '%s' requires an argument.", orig);
	}
    }

  *i_inout = i;
  return result;
}

/*
 *--------------------------------------------------------------
 * gdk_init_heck
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
 *   be handled by the application or dismissed). If initialization
 *   fails, returns FALSE, otherwise TRUE.
 *
 * Side effects:
 *   The library is initialized.
 *
 *--------------------------------------------------------------
 */

gboolean
gdk_init_check (int	 *argc,
		char ***argv)
{
  XKeyboardState keyboard_state;
  gint synchronize;
  gint i, j, k;
  XClassHint *class_hint;
  gchar **argv_orig = NULL;
  gint argc_orig = 0;
  const gchar *option;
  
  if (gdk_initialized)
    return TRUE;
  
  if (g_thread_supported ())
    gdk_threads_mutex = g_mutex_new ();
  
  if (argc && argv)
    {
      argc_orig = *argc;
      
      argv_orig = g_malloc ((argc_orig + 1) * sizeof (char*));
      for (i = 0; i < argc_orig; i++)
	argv_orig[i] = g_strdup ((*argv)[i]);
      argv_orig[argc_orig] = NULL;
    }
  
  X_GETTIMEOFDAY (&start);
  
  gdk_display_name = NULL;
  
  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);
  
  synchronize = FALSE;
  
#ifdef G_ENABLE_DEBUG
  {
    gchar *debug_string = getenv("GDK_DEBUG");
    if (debug_string != NULL)
      gdk_debug_flags = g_parse_debug_string (debug_string,
					      (GDebugKey *) gdk_debug_keys,
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
	    g_set_prgname (d + 1);
	  else
	    g_set_prgname ((*argv)[0]);
	}
      
      for (i = 1; i < *argc;)
	{
#ifdef G_ENABLE_DEBUG	  
	  if ((strcmp ("--gdk-debug", (*argv)[i]) == 0) ||
	      (strncmp ("--gdk-debug=", (*argv)[i], 12) == 0))
	    {
	      option = get_option (argv, *argc, &i);

	      if (option)
		gdk_debug_flags |= g_parse_debug_string (option,
							 (GDebugKey *) gdk_debug_keys,
							 gdk_ndebug_keys);
	    }
	  else if ((strcmp ("--gdk-no-debug", (*argv)[i]) == 0) ||
		   (strncmp ("--gdk-no-debug=", (*argv)[i], 15) == 0))
	    {
	      option = get_option (argv, *argc, &i);

	      if (option)
		gdk_debug_flags &= ~g_parse_debug_string (option,
							  (GDebugKey *) gdk_debug_keys,
							  gdk_ndebug_keys);
	    }
	  else 
#endif /* G_ENABLE_DEBUG */
	    if ((strcmp ("--display", (*argv)[i]) == 0) ||
	        (strncmp ("--display=", (*argv)[i], 10) == 0))
	      {
		option = get_option (argv, *argc, &i);

		if (option)
		  {
		    if (gdk_display_name)
		      g_free (gdk_display_name);
		      
		    gdk_display_name = g_strdup (option);
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
	    else if ((strcmp ("--name", (*argv)[i]) == 0) ||
		     (strncmp ("--name=", (*argv)[i], 7) == 0))
	      {
		option = get_option (argv, *argc, &i);

		if (option)
		  g_set_prgname (option);
	      }
	    else if ((strcmp ("--class", (*argv)[i]) == 0) ||
		     (strncmp ("--class=", (*argv)[i], 8) == 0))
	      {
	      	option = get_option (argv, *argc, &i);

		if (option)
		  {
		    if (gdk_progclass)
		      g_free (gdk_progclass);
		    
		    gdk_progclass = g_strdup (option);
		  }
	      }
#ifdef XINPUT_GXI
	    else if ((strcmp ("--gxid_host", (*argv)[i]) == 0) ||
		     (strncmp ("--gxid_host=", (*argv)[i], 12) == 0))
	      {
		option = get_option (argv, *argc, &i);

		if (option)
		  {
		    if (gdk_input_gxid_host)
		      g_free (gdk_input_gxid_host);
		    
		    gdk_input_gxid_host = g_strdup (option);
		  }
	      }
	    else if ((strcmp ("--gxid_port", (*argv)[i]) == 0) ||
		     (strncmp ("--gxid_port=", (*argv)[i], 12) == 0))
	      {
		option = get_option (argv, *argc, &i);

		if (option)
		  gdk_input_gxid_port = atoi (option);
	      }
#endif
#ifdef USE_XIM
	    else if ((strcmp ("--xim-preedit", (*argv)[i]) == 0) ||
		     (strncmp ("--xim-preedit=", (*argv)[i], 14) == 0))
	      {
		option = get_option (argv, *argc, &i);

		if (option)
		  {
		    if (strcmp ("none", option) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_NONE);
		    else if (strcmp ("nothing", option) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_NOTHING);
		    else if (strcmp ("area", option) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_AREA);
		    else if (strcmp ("position", option) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_POSITION);
		    else if (strcmp ("callbacks", option) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_CALLBACKS);
		    else
		      g_warning ("Argument '%s' for --xim-preedit not understood", option);
		      
		  }
	      }
	    else if ((strcmp ("--xim-status", (*argv)[i]) == 0) ||
		     (strncmp ("--xim-status=", (*argv)[i], 13) == 0))
	      {
		option = get_option (argv, *argc, &i);
		
		if (option)
		  {
		    if (strcmp ("none", option) == 0)
		      gdk_im_set_best_style (GDK_IM_STATUS_NONE);
		    else if (strcmp ("nothing", option) == 0)
		      gdk_im_set_best_style (GDK_IM_STATUS_NOTHING);
		    else if (strcmp ("area", option) == 0)
		      gdk_im_set_best_style (GDK_IM_STATUS_AREA);
		    else if (strcmp ("callbacks", option) == 0)
		      gdk_im_set_best_style (GDK_IM_STATUS_CALLBACKS);
		    else
		      g_warning ("Argumetn '%s' for --xim-status not understood", option);
		  }
	      }
#endif
	  
	  i++;
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
      g_set_prgname ("<unknown>");
    }
  
  GDK_NOTE (MISC, g_message ("progname: \"%s\"", g_get_prgname ()));
  
  gdk_display = XOpenDisplay (gdk_display_name);
  if (!gdk_display)
    return FALSE;
  
  if (synchronize)
    XSynchronize (gdk_display, True);
  
  gdk_screen = DefaultScreen (gdk_display);
  gdk_root_window = RootWindow (gdk_display, gdk_screen);
  
  gdk_leader_window = XCreateSimpleWindow(gdk_display, gdk_root_window,
					  10, 10, 10, 10, 0, 0 , 0);
  class_hint = XAllocClassHint();
  class_hint->res_name = g_get_prgname ();
  if (gdk_progclass == NULL)
    {
      gdk_progclass = g_strdup (g_get_prgname ());
      gdk_progclass[0] = toupper (gdk_progclass[0]);
    }
  class_hint->res_class = gdk_progclass;
  XmbSetWMProperties (gdk_display, gdk_leader_window,
                      NULL, NULL, argv_orig, argc_orig, 
                      NULL, NULL, class_hint);
  XFree (class_hint);
  
  for (i = 0; i < argc_orig; i++)
    g_free(argv_orig[i]);
  g_free(argv_orig);
  
  gdk_wm_delete_window = XInternAtom (gdk_display, "WM_DELETE_WINDOW", False);
  gdk_wm_take_focus = XInternAtom (gdk_display, "WM_TAKE_FOCUS", False);
  gdk_wm_protocols = XInternAtom (gdk_display, "WM_PROTOCOLS", False);
  gdk_wm_window_protocols[0] = gdk_wm_delete_window;
  gdk_wm_window_protocols[1] = gdk_wm_take_focus;
  gdk_selection_property = XInternAtom (gdk_display, "GDK_SELECTION", False);
  
  XGetKeyboardControl (gdk_display, &keyboard_state);
  autorepeat = keyboard_state.global_auto_repeat;
  
  timer.tv_sec = 0;
  timer.tv_usec = 0;
  timerp = NULL;
  
  g_atexit (gdk_exit_func);
  
  gdk_events_init ();
  gdk_visual_init ();
  gdk_window_init ();
  gdk_image_init ();
  gdk_input_init ();
  gdk_dnd_init ();

#ifdef USE_XIM
  gdk_im_open ();
#endif
  
  gdk_initialized = 1;

  return TRUE;
}

void
gdk_init (int *argc, char ***argv)
{
  if (!gdk_init_check (argc, argv))
    {
      g_warning ("cannot open display: %s", gdk_get_display ());
      exit(1);
    }
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

void
gdk_set_use_xshm (gboolean use_xshm)
{
  gdk_use_xshm = use_xshm;
}

gboolean
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
  /*  From gdkwindow.c	*/
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
  for (i = 0; i < gdk_nevent_masks; i++)
    {
      if (event_mask & (1 << (i + 1)))
	xevent_mask |= gdk_event_mask_table[i];
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
    gdk_xgrab_window = window_private;
  
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
  gdk_xgrab_window = NULL;
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

gboolean
gdk_pointer_is_grabbed (void)
{
  return gdk_xgrab_window != NULL;
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

/*
 *--------------------------------------------------------------
 * gdk_screen_width_mm
 *
 *   Return the width of the screen in millimeters.
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
gdk_screen_width_mm (void)
{
  gint return_val;
  
  return_val = DisplayWidthMM (gdk_display, gdk_screen);
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen in millimeters.
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
gdk_screen_height_mm (void)
{
  gint return_val;
  
  return_val = DisplayHeightMM (gdk_display, gdk_screen);
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_set_sm_client_id
 *
 *   Set the SM_CLIENT_ID property on the WM_CLIENT_LEADER window
 *   so that the window manager can save our state using the
 *   X11R6 ICCCM session management protocol. A NULL value should 
 *   be set following disconnection from the session manager to
 *   remove the SM_CLIENT_ID property.
 *
 * Arguments:
 * 
 *   "sm_client_id" specifies the client id assigned to us by the
 *   session manager or NULL to remove the property.
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_set_sm_client_id (const gchar* sm_client_id)
{
  if (sm_client_id && strcmp (sm_client_id, ""))
    {
      XChangeProperty (gdk_display, gdk_leader_window,
	   	       gdk_atom_intern ("SM_CLIENT_ID", FALSE),
		       XA_STRING, 8, PropModeReplace,
		       sm_client_id, strlen(sm_client_id));
    }
  else
     XDeleteProperty (gdk_display, gdk_leader_window,
	   	      gdk_atom_intern ("SM_CLIENT_ID", FALSE));
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


void
gdk_beep (void)
{
  XBell(gdk_display, 100);
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
     has trounced over memory too badly for even g_message to work) */
  if (in_gdk_exit_func == TRUE)
    return;
  in_gdk_exit_func = TRUE;
  
  if (gdk_initialized)
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
      gdk_initialized = 0;
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
gdk_x_error (Display	 *display,
	     XErrorEvent *error)
{
  if (error->error_code)
    {
      if (gdk_error_warnings)
	{
	  char buf[64];
	  
	  XGetErrorText (display, error->error_code, buf, 63);

#ifdef G_ENABLE_DEBUG	  
	  g_error ("%s\n  serial %ld error_code %d request_code %d minor_code %d\n", 
		   buf, 
		   error->serial, 
		   error->error_code, 
		   error->request_code,
		   error->minor_code);
#else /* !G_ENABLE_DEBUG */
	  fprintf (stderr, "Gdk-ERROR **: %s\n  serial %ld error_code %d request_code %d minor_code %d\n",
		   buf, 
		   error->serial, 
		   error->error_code, 
		   error->request_code,
		   error->minor_code);

	  exit(1);
#endif /* G_ENABLE_DEBUG */
	}
      gdk_error_code = error->error_code;
    }
  
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
  /* This is basically modelled after the code in XLib. We need
   * an explicit error handler here, so we can disable our atexit()
   * which would otherwise cause a nice segfault.
   * We fprintf(stderr, instead of g_warning() because g_warning()
   * could possibly be redirected to a dialog
   */
  if (errno == EPIPE)
    {
      fprintf (stderr, "Gdk-ERROR **: X connection to %s broken (explicit kill or server shutdown).\n", gdk_display ? DisplayString (gdk_display) : gdk_get_display());
    }
  else
    {
      fprintf (stderr, "Gdk-ERROR **: Fatal IO error %d (%s) on X server %s.\n",
	       errno, g_strerror (errno),
	       gdk_display ? DisplayString (gdk_display) : gdk_get_display());
    }

  /* Disable the atexit shutdown for GDK */
  gdk_initialized = 0;
  
  exit(1);
}

gchar *
gdk_get_display (void)
{
  return (gchar *)XDisplayName (gdk_display_name);
}

/*************************************************************
 * gdk_error_trap_push:
 *     Push an error trap. X errors will be trapped until
 *     the corresponding gdk_error_pop(), which will return
 *     the error code, if any.
 *   arguments:
 *     
 *   results:
 *************************************************************/

void
gdk_error_trap_push (void)
{
  GSList *node;
  GdkErrorTrap *trap;

  if (gdk_error_trap_free_list)
    {
      node = gdk_error_trap_free_list;
      gdk_error_trap_free_list = gdk_error_trap_free_list->next;
    }
  else
    {
      node = g_slist_alloc ();
      node->data = g_new (GdkErrorTrap, 1);
    }

  node->next = gdk_error_traps;
  gdk_error_traps = node;
  
  trap = node->data;
  trap->error_code = gdk_error_code;
  trap->error_warnings = gdk_error_warnings;

  gdk_error_code = 0;
  gdk_error_warnings = 0;
}

/*************************************************************
 * gdk_error_trap_pop:
 *     Pop an error trap added with gdk_error_push()
 *   arguments:
 *     
 *   results:
 *     0, if no error occured, otherwise the error code.
 *************************************************************/

gint
gdk_error_trap_pop (void)
{
  GSList *node;
  GdkErrorTrap *trap;
  gint result;

  g_return_val_if_fail (gdk_error_traps != NULL, 0);

  node = gdk_error_traps;
  gdk_error_traps = gdk_error_traps->next;

  node->next = gdk_error_trap_free_list;
  gdk_error_trap_free_list = node;
  
  result = gdk_error_code;
  
  trap = node->data;
  gdk_error_code = trap->error_code;
  gdk_error_warnings = trap->error_warnings;
  
  return result;
}

gint 
gdk_send_xevent (Window window, gboolean propagate, glong event_mask,
		 XEvent *event_send)
{
  Status result;
  gint old_warnings = gdk_error_warnings;
  
  gdk_error_code = 0;
  
  gdk_error_warnings = 0;
  result = XSendEvent (gdk_display, window, propagate, event_mask, event_send);
  XSync (gdk_display, False);
  gdk_error_warnings = old_warnings;
  
  return result && !gdk_error_code;
}

#ifndef HAVE_XCONVERTCASE
/* compatibility function from X11R6.3, since XConvertCase is not
 * supplied by X11R5.
 */
static void
gdkx_XConvertCase (KeySym symbol,
		   KeySym *lower,
		   KeySym *upper)
{
  register KeySym sym = symbol;
  
  g_return_if_fail (lower != NULL);
  g_return_if_fail (upper != NULL);
  
  *lower = sym;
  *upper = sym;
  
  switch (sym >> 8)
    {
#if	defined (GDK_A) && defined (GDK_Ooblique)
    case 0: /* Latin 1 */
      if ((sym >= GDK_A) && (sym <= GDK_Z))
	*lower += (GDK_a - GDK_A);
      else if ((sym >= GDK_a) && (sym <= GDK_z))
	*upper -= (GDK_a - GDK_A);
      else if ((sym >= GDK_Agrave) && (sym <= GDK_Odiaeresis))
	*lower += (GDK_agrave - GDK_Agrave);
      else if ((sym >= GDK_agrave) && (sym <= GDK_odiaeresis))
	*upper -= (GDK_agrave - GDK_Agrave);
      else if ((sym >= GDK_Ooblique) && (sym <= GDK_Thorn))
	*lower += (GDK_oslash - GDK_Ooblique);
      else if ((sym >= GDK_oslash) && (sym <= GDK_thorn))
	*upper -= (GDK_oslash - GDK_Ooblique);
      break;
#endif	/* LATIN1 */
      
#if	defined (GDK_Aogonek) && defined (GDK_tcedilla)
    case 1: /* Latin 2 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym == GDK_Aogonek)
	*lower = GDK_aogonek;
      else if (sym >= GDK_Lstroke && sym <= GDK_Sacute)
	*lower += (GDK_lstroke - GDK_Lstroke);
      else if (sym >= GDK_Scaron && sym <= GDK_Zacute)
	*lower += (GDK_scaron - GDK_Scaron);
      else if (sym >= GDK_Zcaron && sym <= GDK_Zabovedot)
	*lower += (GDK_zcaron - GDK_Zcaron);
      else if (sym == GDK_aogonek)
	*upper = GDK_Aogonek;
      else if (sym >= GDK_lstroke && sym <= GDK_sacute)
	*upper -= (GDK_lstroke - GDK_Lstroke);
      else if (sym >= GDK_scaron && sym <= GDK_zacute)
	*upper -= (GDK_scaron - GDK_Scaron);
      else if (sym >= GDK_zcaron && sym <= GDK_zabovedot)
	*upper -= (GDK_zcaron - GDK_Zcaron);
      else if (sym >= GDK_Racute && sym <= GDK_Tcedilla)
	*lower += (GDK_racute - GDK_Racute);
      else if (sym >= GDK_racute && sym <= GDK_tcedilla)
	*upper -= (GDK_racute - GDK_Racute);
      break;
#endif	/* LATIN2 */
      
#if	defined (GDK_Hstroke) && defined (GDK_Cabovedot)
    case 2: /* Latin 3 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym >= GDK_Hstroke && sym <= GDK_Hcircumflex)
	*lower += (GDK_hstroke - GDK_Hstroke);
      else if (sym >= GDK_Gbreve && sym <= GDK_Jcircumflex)
	*lower += (GDK_gbreve - GDK_Gbreve);
      else if (sym >= GDK_hstroke && sym <= GDK_hcircumflex)
	*upper -= (GDK_hstroke - GDK_Hstroke);
      else if (sym >= GDK_gbreve && sym <= GDK_jcircumflex)
	*upper -= (GDK_gbreve - GDK_Gbreve);
      else if (sym >= GDK_Cabovedot && sym <= GDK_Scircumflex)
	*lower += (GDK_cabovedot - GDK_Cabovedot);
      else if (sym >= GDK_cabovedot && sym <= GDK_scircumflex)
	*upper -= (GDK_cabovedot - GDK_Cabovedot);
      break;
#endif	/* LATIN3 */
      
#if	defined (GDK_Rcedilla) && defined (GDK_Amacron)
    case 3: /* Latin 4 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym >= GDK_Rcedilla && sym <= GDK_Tslash)
	*lower += (GDK_rcedilla - GDK_Rcedilla);
      else if (sym >= GDK_rcedilla && sym <= GDK_tslash)
	*upper -= (GDK_rcedilla - GDK_Rcedilla);
      else if (sym == GDK_ENG)
	*lower = GDK_eng;
      else if (sym == GDK_eng)
	*upper = GDK_ENG;
      else if (sym >= GDK_Amacron && sym <= GDK_Umacron)
	*lower += (GDK_amacron - GDK_Amacron);
      else if (sym >= GDK_amacron && sym <= GDK_umacron)
	*upper -= (GDK_amacron - GDK_Amacron);
      break;
#endif	/* LATIN4 */
      
#if	defined (GDK_Serbian_DJE) && defined (GDK_Cyrillic_yu)
    case 6: /* Cyrillic */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym >= GDK_Serbian_DJE && sym <= GDK_Serbian_DZE)
	*lower -= (GDK_Serbian_DJE - GDK_Serbian_dje);
      else if (sym >= GDK_Serbian_dje && sym <= GDK_Serbian_dze)
	*upper += (GDK_Serbian_DJE - GDK_Serbian_dje);
      else if (sym >= GDK_Cyrillic_YU && sym <= GDK_Cyrillic_HARDSIGN)
	*lower -= (GDK_Cyrillic_YU - GDK_Cyrillic_yu);
      else if (sym >= GDK_Cyrillic_yu && sym <= GDK_Cyrillic_hardsign)
	*upper += (GDK_Cyrillic_YU - GDK_Cyrillic_yu);
      break;
#endif	/* CYRILLIC */
      
#if	defined (GDK_Greek_ALPHAaccent) && defined (GDK_Greek_finalsmallsigma)
    case 7: /* Greek */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym >= GDK_Greek_ALPHAaccent && sym <= GDK_Greek_OMEGAaccent)
	*lower += (GDK_Greek_alphaaccent - GDK_Greek_ALPHAaccent);
      else if (sym >= GDK_Greek_alphaaccent && sym <= GDK_Greek_omegaaccent &&
	       sym != GDK_Greek_iotaaccentdieresis &&
	       sym != GDK_Greek_upsilonaccentdieresis)
	*upper -= (GDK_Greek_alphaaccent - GDK_Greek_ALPHAaccent);
      else if (sym >= GDK_Greek_ALPHA && sym <= GDK_Greek_OMEGA)
	*lower += (GDK_Greek_alpha - GDK_Greek_ALPHA);
      else if (sym >= GDK_Greek_alpha && sym <= GDK_Greek_omega &&
	       sym != GDK_Greek_finalsmallsigma)
	*upper -= (GDK_Greek_alpha - GDK_Greek_ALPHA);
      break;
#endif	/* GREEK */
    }
}
#endif

gchar*
gdk_keyval_name (guint	      keyval)
{
  return XKeysymToString (keyval);
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  g_return_val_if_fail (keyval_name != NULL, 0);
  
  return XStringToKeysym (keyval_name);
}

guint
gdk_keyval_to_upper (guint	  keyval)
{
  if (keyval)
    {
      KeySym lower_val = 0;
      KeySym upper_val = 0;
      
      XConvertCase (keyval, &lower_val, &upper_val);
      return upper_val;
    }
  return 0;
}

guint
gdk_keyval_to_lower (guint	  keyval)
{
  if (keyval)
    {
      KeySym lower_val = 0;
      KeySym upper_val = 0;
      
      XConvertCase (keyval, &lower_val, &upper_val);
      return lower_val;
    }
  return 0;
}

gboolean
gdk_keyval_is_upper (guint	  keyval)
{
  if (keyval)
    {
      KeySym lower_val = 0;
      KeySym upper_val = 0;
      
      XConvertCase (keyval, &lower_val, &upper_val);
      return upper_val == keyval;
    }
  return TRUE;
}

gboolean
gdk_keyval_is_lower (guint	  keyval)
{
  if (keyval)
    {
      KeySym lower_val = 0;
      KeySym upper_val = 0;
      
      XConvertCase (keyval, &lower_val, &upper_val);
      return lower_val == keyval;
    }
  return TRUE;
}

void
gdk_threads_enter ()
{
  GDK_THREADS_ENTER ();
}

void
gdk_threads_leave ()
{
  GDK_THREADS_LEAVE ();
}

