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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#include "gdk.h"

#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkregion-generic.h"
#include "gdkinputprivate.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"



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


/* Private variable declarations
 */
static int gdk_initialized = 0;			    /* 1 if the library is initialized,
						     * 0 otherwise.
						     */

/*static gint autorepeat;*/
gboolean gdk_synchronize = FALSE;

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"events",	    GDK_DEBUG_EVENTS},
  {"misc",	    GDK_DEBUG_MISC},
  {"dnd",	    GDK_DEBUG_DND},
  {"color-context", GDK_DEBUG_COLOR_CONTEXT},
  {"xim",	    GDK_DEBUG_XIM},
  {"multihead",	    GDK_DEBUG_MULTIHEAD}
};

static const int gdk_ndebug_keys = sizeof(gdk_debug_keys)/sizeof(GDebugKey);

#endif /* G_ENABLE_DEBUG */

GdkArgDesc _gdk_windowing_args[] = {
  { "display",     GDK_ARG_STRING,   &gdk_display_name,    (GdkArgFunc)NULL   },
  { "sync",        GDK_ARG_BOOL,     &gdk_synchronize,     (GdkArgFunc)NULL   },
  { "no-xshm",     GDK_ARG_NOBOOL,   &gdk_use_xshm,        (GdkArgFunc)NULL   },
  { "class",       GDK_ARG_STRING,   &gdk_progclass,       (GdkArgFunc)NULL   },
  { "gxid-host",   GDK_ARG_STRING,   &gdk_input_gxid_host, (GdkArgFunc)NULL   },
  { "gxid-port",   GDK_ARG_INT,      &gdk_input_gxid_port, (GdkArgFunc)NULL   },
  { NULL }
};

GdkDisplay *
_gdk_windowing_init_check_for_display (int argc, char **argv, char *display_name)
{
  XKeyboardState keyboard_state;
  XClassHint *class_hint;
  guint pid;
  GdkDisplay *display;
  GdkDisplayImplX11 *dpy_impl;

  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);

  if (!dpy_mgr) {
    dpy_mgr = g_object_new (gdk_display_manager_type (), NULL);
  }
  display = GDK_DISPLAY_MGR_GET_CLASS (dpy_mgr)->open_display (dpy_mgr, display_name);
  if(!display)
    return NULL;
  dpy_impl = GDK_DISPLAY_IMPL_X11 (display);

  if (gdk_synchronize)
    XSynchronize (dpy_impl->xdisplay, True);

  class_hint = XAllocClassHint ();
  class_hint->res_name = g_get_prgname ();
  if (gdk_progclass == NULL) {
    gdk_progclass = g_strdup (g_get_prgname ());
    gdk_progclass[0] = toupper (gdk_progclass[0]);
  }
  class_hint->res_class = gdk_progclass;
  XmbSetWMProperties (dpy_impl->xdisplay,
		      DEFAULT_GDK_SCREEN_IMPL_X11_FOR_DISPLAY (display)->
		      leader_window, NULL, NULL, argv, argc, NULL, NULL,
		      class_hint);
  XFree (class_hint);

  pid = getpid ();
  XChangeProperty (dpy_impl->xdisplay,
		   DEFAULT_GDK_SCREEN_IMPL_X11_FOR_DISPLAY (display)->
		   leader_window, gdk_atom_intern_for_display ("_NET_WM_PID",
							       FALSE,
							       display),
		   XA_CARDINAL, 32, PropModeReplace, (guchar *) & pid, 1);

  dpy_impl->gdk_wm_delete_window =
    gdk_atom_intern_for_display ("WM_DELETE_WINDOW", FALSE, display);
  dpy_impl->gdk_wm_take_focus =
    gdk_atom_intern_for_display ("WM_TAKE_FOCUS", FALSE, display);
  dpy_impl->gdk_wm_protocols =
    gdk_atom_intern_for_display ("WM_PROTOCOLS", FALSE, display);
  dpy_impl->gdk_wm_window_protocols[0] = dpy_impl->gdk_wm_delete_window;
  dpy_impl->gdk_wm_window_protocols[1] = dpy_impl->gdk_wm_take_focus;
  dpy_impl->gdk_wm_window_protocols[2] =
    gdk_atom_intern_for_display ("_NET_WM_PING", FALSE, display);
  dpy_impl->gdk_selection_property =
    gdk_atom_intern_for_display ("GDK_SELECTION", FALSE, display);
  dpy_impl->wm_state_atom =
    gdk_atom_intern_for_display ("_NET_WM_STATE", FALSE, display);
  dpy_impl->wm_desktop_atom =
    gdk_atom_intern_for_display ("_NET_WM_DESKTOP", FALSE, display);
  dpy_impl->timestamp_prop_atom =
    gdk_atom_intern_for_display ("GDK_TIMESTAMP_PROP", FALSE, display);
  dpy_impl->wmspec_check_atom =
    gdk_atom_intern_for_display ("_NET_SUPPORTING_WM_CHECK", FALSE, display);

  dpy_impl->wmspec_supported_atom =
    gdk_atom_intern_for_display ("_NET_SUPPORTED", FALSE, display);

  XGetKeyboardControl (dpy_impl->xdisplay, &keyboard_state);
  dpy_impl->autorepeat = keyboard_state.global_auto_repeat;

#ifdef HAVE_XKB
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion (&xkb_major, &xkb_minor)) {
      xkb_major = XkbMajorVersion;
      xkb_minor = XkbMinorVersion;
      if (XkbQueryExtension (dpy_impl->xdisplay, NULL, NULL, NULL,
			     &xkb_major, &xkb_minor)) {
	Bool detectable_autorepeat_supported;

	dpy_impl->_gdk_use_xkb = TRUE;

	XkbSelectEvents (dpy_impl->xdisplay,
			 XkbUseCoreKbd, XkbMapNotifyMask, XkbMapNotifyMask);

	XkbSetDetectableAutoRepeat (dpy_impl->xdisplay,
				    True, &detectable_autorepeat_supported);

	GDK_NOTE (MISC, g_message ("Detectable autorepeat %s.",
				   detectable_autorepeat_supported ?
				   "supported" : "not supported"));

	dpy_impl->_gdk_have_xkb_autorepeat = detectable_autorepeat_supported;
      }
    }
  }
#endif
  return display;
}

/* This function is only used by gdk_init_check */

GdkDisplay *
_gdk_windowing_init_check (int argc, char **argv){
  /* This wrapper function is needed because of gdk_display_name exists only
   * in x11 implementation */
  return _gdk_windowing_init_check_for_display(argc,argv,gdk_display_name);
}


GdkDisplay *
gdk_display_init_new(int argc, char **argv, char *display_name){
  GdkDisplay *dpy = NULL;
  GdkScreen *scr = NULL;
  
  dpy = _gdk_windowing_init_check_for_display(argc,argv,display_name);
    if (!dpy)
      return NULL;
  scr = GDK_DISPLAY_GET_CLASS(dpy)->get_default_screen(dpy);
  
  _gdk_visual_init (scr);
  _gdk_windowing_window_init(scr);
  _gdk_windowing_image_init (dpy);
  gdk_events_init (dpy);
  gdk_dnd_init (dpy);
  return dpy;
}

void
gdk_set_use_xshm_for_display (GdkDisplay * display, gboolean use_xshm)
{
  GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm = use_xshm;
}

gboolean
gdk_get_use_xshm_for_display (GdkDisplay * display)
{
  return GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm;
}


void
gdk_set_use_xshm (gboolean use_xshm)
{
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_set_use_xshm_for_display instead\n"));
  gdk_set_use_xshm_for_display(DEFAULT_GDK_DISPLAY,use_xshm);
}

gboolean
gdk_get_use_xshm (void)
{
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_get_use_xshm_for_display instead\n"));
  return gdk_get_use_xshm_for_display(DEFAULT_GDK_DISPLAY);
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
    GDK_DISPLAY_IMPL_X11(GDK_WINDOW_DISPLAY(window))->gdk_xgrab_window = 
						    (GdkWindowObject *)window;

  return gdk_x11_convert_grab_status (return_val);
}

void
gdk_pointer_ungrab_for_display (GdkDisplay * display, guint32 time)
{
  _gdk_input_ungrab_pointer (time);


  XUngrabPointer (GDK_DISPLAY_XDISPLAY (display), time);
  GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window = NULL;
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
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_pointer_ungrab_for_display instead\n"));
  gdk_pointer_ungrab_for_display(DEFAULT_GDK_DISPLAY,time);
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
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_pointer_is_grabbed_for_display instead\n"));
  return gdk_pointer_is_grabbed_for_display(DEFAULT_GDK_DISPLAY);
}
gboolean
gdk_pointer_is_grabbed_for_display (GdkDisplay * display)
{
  return (GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window != NULL);
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

void
gdk_keyboard_ungrab_for_display (GdkDisplay * display, guint32 time)
{
  XUngrabKeyboard (GDK_DISPLAY_XDISPLAY (display), time);
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
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_keyboard_ungrab_for_display instead\n"));	 
  gdk_keyboard_ungrab_for_display(DEFAULT_GDK_DISPLAY,time);
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
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_screen_width_for_screen instead\n"));  
  return gdk_screen_width_for_screen (DEFAULT_GDK_SCREEN);
}
gint
gdk_screen_width_for_screen (GdkScreen * screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_width (screen);
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
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_screen_height_for_screen instead\n"));
  return gdk_screen_height_for_screen (DEFAULT_GDK_SCREEN);
}

gint
gdk_screen_height_for_screen (GdkScreen * screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_height (screen);
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
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_screen_width_mm_for_screen instead\n"));
  return gdk_screen_width_mm_for_screen(DEFAULT_GDK_SCREEN);
}
gint
gdk_screen_width_mm_for_screen (GdkScreen * screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_width_mm (screen);
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
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_screen_height_mm_for_screen instead\n"));
  return gdk_screen_height_mm_for_screen(DEFAULT_GDK_SCREEN);
}
gint
gdk_screen_height_mm_for_screen (GdkScreen * screen)
{
  return GDK_SCREEN_GET_CLASS(screen)->get_height_mm (screen);
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
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_set_sm_client_id_for_screen instead\n"));
  gdk_set_sm_client_id_for_screen(DEFAULT_GDK_SCREEN,sm_client_id);
}

void
gdk_set_sm_client_id_for_screen (GdkScreen * screen,
				 const gchar * sm_client_id)
{

  GdkScreenImplX11 *scr_impl = GDK_SCREEN_IMPL_X11 (screen);

  if (sm_client_id && strcmp (sm_client_id, "")) {
    XChangeProperty (scr_impl->xdisplay, scr_impl->leader_window,
		     gdk_atom_intern_for_display ("SM_CLIENT_ID", FALSE,
						  scr_impl->display),
		     XA_STRING, 8, PropModeReplace, sm_client_id,
		     strlen (sm_client_id));
  }
  else
    XDeleteProperty (scr_impl->xdisplay, scr_impl->leader_window,
		     gdk_atom_intern_for_display ("SM_CLIENT_ID", FALSE,
						  scr_impl->display));
}
/*

void
gdk_key_repeat_disable_for_display (GdkDisplay * dpy)
{
  XAutoRepeatOff (GDK_DISPLAY_XDISPLAY (dpy));
}

void
gdk_key_repeat_restore_for_display (GdkDisplay * dpy)
{
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11 (dpy);

  if (dpy_impl->autorepeat)
    XAutoRepeatOn (dpy_impl->xdisplay);
  else
    XAutoRepeatOff (dpy_impl->xdisplay);
}
  
void
gdk_key_repeat_disable (void)
{
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_key_repeat_disable_for_display instead\n"));
  gdk_key_repeat_disable_for_display (DEFAULT_GDK_DISPLAY);
}

void
gdk_key_repeat_restore (void)
{
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_key_repeat_restore_for_display instead\n"));	
  gdk_key_repeat_restore_for_display(DEFAULT_GDK_DISPLAY);
}

*/

void
gdk_beep_for_display (GdkDisplay * display)
{
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
}

void
gdk_beep (void)
{
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_beep_for_display instead\n"));
  gdk_beep_for_display(DEFAULT_GDK_DISPLAY);
}

/* close all open display */

void
gdk_windowing_exit (void)
{
  GSList * tmp = dpy_mgr->open_displays;
    
  while(tmp != NULL){
	  pango_x_shutdown_display(GDK_DISPLAY_XDISPLAY(tmp->data));
	  XCloseDisplay (GDK_DISPLAY_XDISPLAY (tmp->data));
	  tmp = tmp->next;
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

int
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

int
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
      fprintf (stderr, "Gdk-ERROR **: X connection to %s broken (explicit kill or server shutdown).\n", DisplayString (display));
    }
  else
    {
      fprintf (stderr, "Gdk-ERROR **: Fatal IO error %d (%s) on X server %s.\n",
	       errno, g_strerror (errno), DisplayString (display));
    }

  /* Disable the atexit shutdown for GDK */
  gdk_initialized = 0;
  
  exit(1);
}

gchar *
gdk_get_display (void)
{
  GDK_NOTE(MULTIHEAD,g_message("Use gdk_x11_display_impl_get_display_name instead\n"));
  return gdk_x11_display_impl_get_display_name(DEFAULT_GDK_DISPLAY);
}

gchar *
gdk_get_display_arg_name(void)
{
return gdk_display_name;
}

gint 
gdk_send_xevent (Window window, gboolean propagate, glong event_mask,
		 XEvent *event_send)
{
  Status result;
  gint old_warnings = gdk_error_warnings;
  
  gdk_error_code = 0;
  
  gdk_error_warnings = 0;
  result = XSendEvent (event_send->xany.display, window, propagate, event_mask, event_send);
  XSync (event_send->xany.display, False);
  gdk_error_warnings = old_warnings;
  
  return result && !gdk_error_code;
}

void
_gdk_region_get_xrectangles (GdkRegion   *region,
                             gint         x_offset,
                             gint         y_offset,
                             XRectangle **rects,
                             gint        *n_rects)
{
  XRectangle *rectangles = g_new (XRectangle, region->numRects);
  GdkRegionBox *boxes = region->rects;
  gint i;
  
  for (i = 0; i < region->numRects; i++)
    {
      rectangles[i].x = CLAMP (boxes[i].x1 + x_offset, G_MINSHORT, G_MAXSHORT);
      rectangles[i].y = CLAMP (boxes[i].y1 + y_offset, G_MINSHORT, G_MAXSHORT);
      rectangles[i].width = CLAMP (boxes[i].x2 + x_offset, G_MINSHORT, G_MAXSHORT) - rectangles[i].x;
      rectangles[i].height = CLAMP (boxes[i].y2 + y_offset, G_MINSHORT, G_MAXSHORT) - rectangles[i].y;
    }

  *rects = rectangles;
  *n_rects = region->numRects;
}
