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

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#include "gdk.h"

#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkinputprivate.h"

#include <pango/pangox.h>

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

static int	    gdk_x_error			 (Display     *display, 
						  XErrorEvent *error);
static int	    gdk_x_io_error		 (Display     *display);

/* Private variable declarations
 */
static int gdk_initialized = 0;			    /* 1 if the library is initialized,
						     * 0 otherwise.
						     */

static gint autorepeat;
static gboolean gdk_synchronize = FALSE;

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

static void
gdk_arg_xim_preedit_cb (const gchar *arg, const gchar *value, gpointer cb_data)
{
  if (strcmp ("none", value) == 0)
    gdk_im_set_best_style (GDK_IM_PREEDIT_NONE);
  else if (strcmp ("nothing", value) == 0)
    gdk_im_set_best_style (GDK_IM_PREEDIT_NOTHING);
  else if (strcmp ("area", value) == 0)
    gdk_im_set_best_style (GDK_IM_PREEDIT_AREA);
  else if (strcmp ("position", value) == 0)
    gdk_im_set_best_style (GDK_IM_PREEDIT_POSITION);
  else if (strcmp ("callbacks", value) == 0)
    gdk_im_set_best_style (GDK_IM_PREEDIT_CALLBACKS);
}

static void
gdk_arg_xim_status_cb (const gchar *arg, const gchar *value, gpointer cb_data)
{
  if (strcmp ("none", value) == 0)
    gdk_im_set_best_style (GDK_IM_STATUS_NONE);
  else if (strcmp ("nothing", value) == 0)
    gdk_im_set_best_style (GDK_IM_STATUS_NOTHING);
  else if (strcmp ("area", value) == 0)
    gdk_im_set_best_style (GDK_IM_STATUS_AREA);
  else if (strcmp ("callbacks", value) == 0)
    gdk_im_set_best_style (GDK_IM_STATUS_CALLBACKS);
}

GdkArgDesc _gdk_windowing_args[] = {
  { "display",     GDK_ARG_STRING,   &gdk_display_name,    (GdkArgFunc)NULL   },
  { "sync",        GDK_ARG_BOOL,     &gdk_synchronize,     (GdkArgFunc)NULL   },
  { "no-xshm",     GDK_ARG_NOBOOL,   &gdk_use_xshm,        (GdkArgFunc)NULL   },
  { "class",       GDK_ARG_STRING,   &gdk_progclass,       (GdkArgFunc)NULL   },
  { "gxid-host",   GDK_ARG_STRING,   &gdk_input_gxid_host, (GdkArgFunc)NULL   },
  { "gxid-port",   GDK_ARG_INT,      &gdk_input_gxid_port, (GdkArgFunc)NULL   },
  { "xim-preedit", GDK_ARG_CALLBACK, NULL,                 gdk_arg_xim_preedit_cb },
  { "xim-status",  GDK_ARG_CALLBACK, NULL,                 gdk_arg_xim_status_cb  },
  { NULL }
};

gboolean
_gdk_windowing_init_check (int argc, char **argv)
{
  XKeyboardState keyboard_state;
  XClassHint *class_hint;
  guint pid;
  
  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);
  
  gdk_display = XOpenDisplay (gdk_display_name);
  if (!gdk_display)
    return FALSE;
  
  if (gdk_synchronize)
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
                      NULL, NULL, argv, argc, 
                      NULL, NULL, class_hint);
  XFree (class_hint);

  pid = getpid();
  XChangeProperty (gdk_display, gdk_leader_window,
		   gdk_atom_intern ("_NET_WM_PID", FALSE),
		   XA_CARDINAL, 32,
		   PropModeReplace,
		   (guchar *)&pid, 1);
  
  gdk_wm_delete_window = gdk_atom_intern ("WM_DELETE_WINDOW", FALSE);
  gdk_wm_take_focus = gdk_atom_intern ("WM_TAKE_FOCUS", FALSE);
  gdk_wm_protocols = gdk_atom_intern ("WM_PROTOCOLS", FALSE);
  gdk_wm_window_protocols[0] = gdk_wm_delete_window;
  gdk_wm_window_protocols[1] = gdk_wm_take_focus;
  gdk_wm_window_protocols[2] = gdk_atom_intern ("_NET_WM_PING", FALSE);
  gdk_selection_property = gdk_atom_intern ("GDK_SELECTION", FALSE);
  
  XGetKeyboardControl (gdk_display, &keyboard_state);
  autorepeat = keyboard_state.global_auto_repeat;

#ifdef HAVE_XKB
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion (&xkb_major, &xkb_minor))
      {
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;
        if (XkbQueryExtension (gdk_display, NULL, NULL, NULL,
                               &xkb_major, &xkb_minor))
          {
            _gdk_use_xkb = TRUE;

            XkbSelectEvents (gdk_display,
                             XkbUseCoreKbd,
                             XkbMapNotifyMask,
                             XkbMapNotifyMask);
          }
      }
  }
#endif
  
  return TRUE;
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

static GdkGrabStatus
gdk_x11_convert_grab_status (gint status)
{
  switch (status)
    {
    case GrabSuccess:
      return GDK_GRAB_SUCCESS;
    case AlreadyGrabbed:
      return GDK_GRAB_ALREADY_GRABBED;
    case GrabInvalidTime:
      return GDK_GRAB_INVALID_TIME;
    case GrabNotViewable:
      return GDK_GRAB_NOT_VIEWABLE;
    case GrabFrozen:
      return GDK_GRAB_FROZEN;
    }

  g_assert_not_reached();

  return 0;
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

GdkGrabStatus
gdk_pointer_grab (GdkWindow *	  window,
		  gboolean	  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
  gint return_val;
  GdkCursorPrivate *cursor_private;
  guint xevent_mask;
  Window xwindow;
  Window xconfine_to;
  Cursor xcursor;
  int i;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);
  
  cursor_private = (GdkCursorPrivate*) cursor;
  
  xwindow = GDK_WINDOW_XID (window);
  
  if (!confine_to || GDK_WINDOW_DESTROYED (confine_to))
    xconfine_to = None;
  else
    xconfine_to = GDK_WINDOW_XID (confine_to);
  
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
  
  return_val = _gdk_input_grab_pointer (window,
					owner_events,
					event_mask,
					confine_to,
					time);

  if (return_val == GrabSuccess)
    {
      if (!GDK_WINDOW_DESTROYED (window))
	return_val = XGrabPointer (GDK_WINDOW_XDISPLAY (window),
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
    gdk_xgrab_window = (GdkWindowObject *)window;

  return gdk_x11_convert_grab_status (return_val);
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
  _gdk_input_ungrab_pointer (time);
  
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

GdkGrabStatus
gdk_keyboard_grab (GdkWindow *	   window,
		   gboolean	   owner_events,
		   guint32	   time)
{
  gint return_val;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  
  if (!GDK_WINDOW_DESTROYED (window))
    return_val = XGrabKeyboard (GDK_WINDOW_XDISPLAY (window),
				GDK_WINDOW_XID (window),
				owner_events,
				GrabModeAsync, GrabModeAsync,
				time);
  else
    return_val = AlreadyGrabbed;

  return gdk_x11_convert_grab_status (return_val);
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
gdk_beep (void)
{
  XBell(gdk_display, 0);
}

void
gdk_windowing_exit (void)
{
  pango_x_shutdown_display (gdk_display);
  
  XCloseDisplay (gdk_display);
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

