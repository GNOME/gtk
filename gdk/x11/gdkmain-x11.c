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
#include "gdkdisplaymgr-x11.h"

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

static gboolean gdk_synchronize = FALSE;

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"events",	    GDK_DEBUG_EVENTS},
  {"misc",	    GDK_DEBUG_MISC},
  {"dnd",	    GDK_DEBUG_DND},
  {"multihead",	    GDK_DEBUG_MULTIHEAD},
  {"xim",	    GDK_DEBUG_XIM}
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
  XClassHint *class_hint;
  guint pid;
  GdkDisplay *display;
  GdkDisplayImplX11 *display_impl;
  
  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);

  if (!_gdk_display_manager)
    _gdk_display_manager = g_object_new (GDK_TYPE_DISPLAY_MANAGER, NULL);

  display = gdk_display_manager_open_display (_gdk_display_manager,
					      display_name);
  if (!display)
    return NULL;
  display_impl = GDK_DISPLAY_IMPL_X11 (display);

  if (gdk_synchronize)
    XSynchronize (display_impl->xdisplay, True);
  
  class_hint = XAllocClassHint();
  class_hint->res_name = g_get_prgname ();
  if (gdk_progclass == NULL)
    {
      gdk_progclass = g_strdup (g_get_prgname ());
      gdk_progclass[0] = toupper (gdk_progclass[0]);
    }
  class_hint->res_class = gdk_progclass;
  XmbSetWMProperties (display_impl->xdisplay,
		      display_impl->leader_window,
		      NULL, NULL, argv, argc, NULL, NULL,
		      class_hint);
  XFree (class_hint);

  pid = getpid ();
  XChangeProperty (display_impl->xdisplay,
		   display_impl->leader_window,
		   gdk_x11_get_real_atom_by_name (display, "_NET_WM_PID"),
		   XA_CARDINAL, 32, PropModeReplace, (guchar *) & pid, 1);

  display_impl->gdk_wm_delete_window =
    gdk_x11_get_real_atom_by_name (display, "WM_DELETE_WINDOW");

  display_impl->gdk_wm_take_focus =
    gdk_x11_get_real_atom_by_name (display, "WM_TAKE_FOCUS");

  display_impl->gdk_wm_protocols =
    gdk_x11_get_real_atom_by_name (display, "WM_PROTOCOLS");

  display_impl->gdk_wm_window_protocols[0] = display_impl->gdk_wm_delete_window;
  display_impl->gdk_wm_window_protocols[1] = display_impl->gdk_wm_take_focus;
  display_impl->gdk_wm_window_protocols[2] =
    gdk_x11_get_real_atom_by_name (display, "_NET_WM_PING");

  display_impl->gdk_selection_property =
    gdk_x11_get_real_atom_by_name (display, "GDK_SELECTION");

  display_impl->wm_state_atom =
    gdk_x11_get_real_atom_by_name (display, "_NET_WM_STATE");

  display_impl->wm_desktop_atom =
    gdk_x11_get_real_atom_by_name (display, "_NET_WM_DESKTOP");

  display_impl->timestamp_prop_atom =
    gdk_x11_get_real_atom_by_name (display, "GDK_TIMESTAMP_PROP");

  display_impl->wmspec_check_atom =
    gdk_x11_get_real_atom_by_name (display, "_NET_SUPPORTING_WM_CHECK");

  display_impl->wmspec_supported_atom =
    gdk_x11_get_real_atom_by_name (display, "_NET_SUPPORTED");


#ifdef HAVE_XKB
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion (&xkb_major, &xkb_minor))
      {
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;
	    
        if (XkbQueryExtension (display_impl->xdisplay, NULL, &display_impl->xkb_event_type, NULL,
                               &xkb_major, &xkb_minor))
          {
	    Bool detectable_autorepeat_supported;
	    
	    display_impl->use_xkb = TRUE;

            XkbSelectEvents (display_impl->xdisplay,
                             XkbUseCoreKbd,
                             XkbMapNotifyMask | XkbStateNotifyMask,
                             XkbMapNotifyMask | XkbStateNotifyMask);

	    XkbSetDetectableAutoRepeat (display_impl->xdisplay,
					True,
					&detectable_autorepeat_supported);

	    GDK_NOTE (MISC, g_message ("Detectable autorepeat %s.",
				       detectable_autorepeat_supported ? "supported" : "not supported"));
	    
	    display_impl->have_xkb_autorepeat = detectable_autorepeat_supported;
          }
      }
  }
#endif

  return display;
}

/* This function is only used by gdk_init_check */

GdkDisplay *
_gdk_windowing_init_check (int argc, char **argv)
{
  /* This wrapper function is needed because of gdk_display_name exists only
   * in x11 implementation */
  return _gdk_windowing_init_check_for_display (argc, argv, gdk_display_name);
}

GdkDisplay *
gdk_display_init_new (int argc, char **argv, char *display_name)
{
  GdkDisplay *display = NULL;
  GdkScreen *screen = NULL;
  
  display = _gdk_windowing_init_check_for_display (argc,argv,display_name);
  if (!display)
    return NULL;
  
  screen = gdk_display_get_default_screen (display);
  
  _gdk_visual_init (screen);
  _gdk_windowing_window_init (screen);
  _gdk_windowing_image_init (display);
  gdk_events_init (display);
  gdk_dnd_init ();
  
  return display;
}

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_set_use_xshm (gboolean use_xshm)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_display_set_use_xshm instead\n"));
  gdk_display_set_use_xshm (gdk_get_default_display (), use_xshm);
}
#endif

#ifndef GDK_MULTIHEAD_SAFE
gboolean
gdk_get_use_xshm (void)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_display_get_use_xshm instead\n"));
  return gdk_display_get_use_xshm (gdk_get_default_display ());
}
#endif

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
    GDK_DISPLAY_IMPL_X11 (GDK_WINDOW_DISPLAY (window))->gdk_xgrab_window = 
      (GdkWindowObject *)window;

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

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_pointer_ungrab (guint32 time)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_display_pointer_ungrab instead\n"));
  gdk_display_pointer_ungrab (gdk_get_default_display (), time);
}
#endif

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

#ifndef GDK_MULTIHEAD_SAFE
gboolean
gdk_pointer_is_grabbed (void)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_display_pointer_is_grabbed instead\n"));
  return gdk_display_pointer_is_grabbed (gdk_get_default_display ());
}
#endif

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

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_keyboard_ungrab (guint32 time)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_display_keyboard_ungrab instead\n"));	 
  gdk_display_keyboard_ungrab (gdk_get_default_display (),time);
}
#endif

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

#ifndef GDK_MULTIHEAD_SAFE
gint
gdk_screen_width (void)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_screen_get_width instead\n"));  
  return gdk_screen_get_width (gdk_get_default_screen());
}
#endif

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

#ifndef GDK_MULTIHEAD_SAFE
gint
gdk_screen_height (void)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_screen_get_height instead\n"));
  return gdk_screen_get_height (gdk_get_default_screen());
}
#endif

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

#ifndef GDK_MULTIHEAD_SAFE
gint
gdk_screen_width_mm (void)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_screen_get_width_mm instead\n"));
  return gdk_screen_get_width_mm (gdk_get_default_screen());
}
#endif

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

#ifndef GDK_MULTIHEAD_SAFE
gint
gdk_screen_height_mm (void)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_screen_get_height_mm instead\n"));
  return gdk_screen_get_height_mm (gdk_get_default_screen ());
}
#endif

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

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_set_sm_client_id (const gchar* sm_client_id)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_set_sm_client_id_for_display instead\n"));
  gdk_set_sm_client_id_for_display (gdk_get_default_display (),sm_client_id);
}
#endif

void
gdk_set_sm_client_id_for_display (GdkDisplay *display,
				  const gchar * sm_client_id)
{
  GdkDisplayImplX11 *display_impl = GDK_DISPLAY_IMPL_X11 (display);

  if (sm_client_id && strcmp (sm_client_id, ""))
    {
      XChangeProperty (display_impl->xdisplay, display_impl->leader_window,
		       gdk_x11_get_real_atom_by_name (display, "SM_CLIENT_ID"),
		       XA_STRING, 8, PropModeReplace, sm_client_id,
		       strlen (sm_client_id));
    }
  else
    XDeleteProperty (display_impl->xdisplay, display_impl->leader_window,
		     gdk_x11_get_real_atom_by_name (display, "SM_CLIENT_ID"));
}

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_beep (void)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_display_beep instead\n"));
  gdk_display_beep (gdk_get_default_display ());
}
#endif

/* close all open display */

void
gdk_windowing_exit (void)
{
  GSList *tmp_list = _gdk_display_manager->open_displays;
    
  while (tmp_list)
    {
      pango_x_shutdown_display (GDK_DISPLAY_XDISPLAY (tmp_list->data));
      XCloseDisplay (GDK_DISPLAY_XDISPLAY (tmp_list->data));
      
      tmp_list = tmp_list->next;
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
	  gchar buf[64];
          gchar *msg;
          
	  XGetErrorText (display, error->error_code, buf, 63);

          msg =
            g_strdup_printf ("The program '%s' received an X Window System error.\n"
                             "This probably reflects a bug in the program.\n"
                             "The error was '%s'.\n"
                             "  (Details: serial %ld error_code %d request_code %d minor_code %d)\n"
                             "  (Note to programmers: normally, X errors are reported asynchronously;\n"
                             "   that is, you will receive the error a while after causing it.\n"
                             "   To debug your program, run it with the --sync command line\n"
                             "   option to change this behavior. You can then get a meaningful\n"
                             "   backtrace from your debugger if you break on the gdk_x_error() function.)",
                             g_get_prgname (),
                             buf,
                             error->serial, 
                             error->error_code, 
                             error->request_code,
                             error->minor_code);
          
#ifdef G_ENABLE_DEBUG	  
	  g_error ("%s", msg);
#else /* !G_ENABLE_DEBUG */
	  fprintf (stderr, "%s\n", msg);

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
   * We fprintf (stderr, instead of g_warning() because g_warning()
   * could possibly be redirected to a dialog
   */
  if (errno == EPIPE)
    {
      fprintf (stderr,
               "The application '%s' lost its connection to the display %s;\n"
               "most likely the X server was shut down or you killed/destroyed\n"
               "the application.\n",
               g_get_prgname (),
               display ? DisplayString (display) : gdk_get_display_arg_name ());
    }
  else
    {
      fprintf (stderr, "%s: Fatal IO error %d (%s) on X server %s.\n",
               g_get_prgname (),
	       errno, g_strerror (errno),
	       display ? DisplayString (display) : gdk_get_display_arg_name ());
    }

  /* Disable the atexit shutdown for GDK */
  gdk_initialized = 0;
  
  exit(1);
}

#ifndef GDK_MULTIHEAD_SAFE
gchar *
gdk_get_display (void)
{
  GDK_NOTE (MULTIHEAD, g_message ("Use gdk_display_get_name instead\n"));
  return gdk_display_get_name (gdk_get_default_display ());
}
#endif

gchar *
gdk_get_display_arg_name (void)
{
  return gdk_display_name;
}

#ifndef GDK_MULTIHEAD_SAFE
gint 
gdk_send_xevent (Window window, gboolean propagate, glong event_mask,
		 XEvent *event_send)
{
  Status result;
  gint old_warnings = gdk_error_warnings;
  gdk_error_code = 0;
  
  gdk_error_warnings = 0;
    
  result = XSendEvent (gdk_get_default_display (), window, propagate, 
		       event_mask, event_send);
  XSync (event_send->xany.display, False);
  gdk_error_warnings = old_warnings;
  
  return result && !gdk_error_code;
}
#endif
gint 
gdk_send_xevent_for_display (GdkDisplay *display,
			     Window window, 
			     gboolean propagate, 
			     glong event_mask,
			     XEvent *event_send)
{
  Status result;
  gint old_warnings = gdk_error_warnings;
  
  gdk_error_code = 0;
  
  gdk_error_warnings = 0;
  result = XSendEvent (GDK_DISPLAY_XDISPLAY (display), window, 
		       propagate, event_mask, event_send);
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
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

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_x11_grab_server ()
{ 
  GdkDisplayImplX11 *display_impl = gdk_get_default_display ();
  
  if (display_impl->grab_count == 0)
    XGrabServer (display_impl->xdisplay);
  ++display_impl->grab_count;
}
#endif

#ifndef GDK_MULTIHEAD_SAFE
void
gdk_x11_ungrab_server ()
{
  GdkDisplayImplX11 *display_impl = gdk_get_default_display ();
  
  g_return_if_fail (display_impl->grab_count > 0);
  
  --display_impl->grab_count;
  if (display_impl->grab_count == 0)
    XUngrabServer (display_impl->xdisplay);
}
#endif
