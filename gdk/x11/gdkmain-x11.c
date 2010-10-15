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

#include "gdkx.h"
#include "gdkasync.h"
#include "gdkdisplay-x11.h"
#include "gdkinternals.h"
#include "gdkprivate-x11.h"
#include "gdkintl.h"
#include "gdkdeviceprivate.h"

#include <glib/gprintf.h>
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


typedef struct _GdkPredicate        GdkPredicate;
typedef struct _GdkGlobalErrorTrap  GdkGlobalErrorTrap;

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

/* non-GDK previous error handler */
typedef int (*GdkXErrorHandler) (Display *, XErrorEvent *);
static GdkXErrorHandler _gdk_old_error_handler;
/* number of times we've pushed the GDK error handler */
static int _gdk_error_handler_push_count = 0;

struct _GdkGlobalErrorTrap
{
  GSList *displays;
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
static GQueue gdk_error_traps;

const GOptionEntry _gdk_windowing_args[] = {
  { "sync", 0, 0, G_OPTION_ARG_NONE, &_gdk_synchronize, 
    /* Description of --sync in --help output */ N_("Make X calls synchronous"), NULL },
  { NULL }
};

void
_gdk_windowing_init (void)
{
  _gdk_x11_initialize_locale ();

  g_queue_init (&gdk_error_traps);
  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);

  _gdk_selection_property = gdk_atom_intern_static_string ("GDK_SELECTION");
}

GdkGrabStatus
_gdk_x11_convert_grab_status (gint status)
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

static void
has_pointer_grab_callback (GdkDisplay *display,
			   gpointer data,
			   gulong serial)
{
  GdkDevice *device = data;

  _gdk_display_device_grab_update (display, device, serial);
}

GdkGrabStatus
_gdk_windowing_device_grab (GdkDevice    *device,
                            GdkWindow    *window,
                            GdkWindow    *native,
                            gboolean      owner_events,
                            GdkEventMask  event_mask,
                            GdkWindow    *confine_to,
                            GdkCursor    *cursor,
                            guint32       time)
{
  GdkDisplay *display;
  GdkGrabStatus status = GDK_GRAB_SUCCESS;

  if (!window || GDK_WINDOW_DESTROYED (window))
    return GDK_GRAB_NOT_VIEWABLE;

  display = gdk_device_get_display (device);

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_NOGRABS)
    status = GrabSuccess;
  else
#endif
    status = GDK_DEVICE_GET_CLASS (device)->grab (device,
                                                  native,
                                                  owner_events,
                                                  event_mask,
                                                  confine_to,
                                                  cursor,
                                                  time);
  if (status == GDK_GRAB_SUCCESS)
    _gdk_x11_roundtrip_async (display,
			      has_pointer_grab_callback,
                              device);
  return status;
}

/**
 * _gdk_xgrab_check_unmap:
 * @window: a #GdkWindow
 * @serial: serial from Unmap event (or from NextRequest(display)
 *   if the unmap is being done by this client.)
 * 
 * Checks to see if an unmap request or event causes the current
 * grab window to become not viewable, and if so, clear the
 * the pointer we keep to it.
 **/
void
_gdk_xgrab_check_unmap (GdkWindow *window,
			gulong     serial)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkDeviceManager *device_manager;
  GList *devices, *d;

  device_manager = gdk_display_get_device_manager (display);

  /* Get all devices */
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
  devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_SLAVE));
  devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING));

  /* End all grabs on the newly hidden window */
  for (d = devices; d; d = d->next)
    _gdk_display_end_device_grab (display, d->data, serial, window, TRUE);

  g_list_free (devices);
}

/**
 * _gdk_xgrab_check_destroy:
 * @window: a #GdkWindow
 * 
 * Checks to see if window is the current grab window, and if
 * so, clear the current grab window.
 **/
void
_gdk_xgrab_check_destroy (GdkWindow *window)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkDeviceManager *device_manager;
  GdkDeviceGrabInfo *grab;
  GList *devices, *d;

  device_manager = gdk_display_get_device_manager (display);

  /* Get all devices */
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
  devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_SLAVE));
  devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING));

  for (d = devices; d; d = d->next)
    {
      /* Make sure there is no lasting grab in this native window */
      grab = _gdk_display_get_last_device_grab (display, d->data);

      if (grab && grab->native_window == window)
        {
          /* We don't know the actual serial to end, but it
             doesn't really matter as this only happens
             after we get told of the destroy from the
             server so we know its ended in the server,
             just make sure its ended. */
          grab->serial_end = grab->serial_start;
          grab->implicit_ungrab = TRUE;
        }
    }

  g_list_free (devices);
}

void
_gdk_windowing_display_set_sm_client_id (GdkDisplay  *display,
					 const gchar *sm_client_id)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  if (display->closed)
    return;

  if (sm_client_id && strcmp (sm_client_id, ""))
    {
      XChangeProperty (display_x11->xdisplay, display_x11->leader_window,
		       gdk_x11_get_xatom_by_name_for_display (display, "SM_CLIENT_ID"),
		       XA_STRING, 8, PropModeReplace, (guchar *)sm_client_id,
		       strlen (sm_client_id));
    }
  else
    XDeleteProperty (display_x11->xdisplay, display_x11->leader_window,
		     gdk_x11_get_xatom_by_name_for_display (display, "SM_CLIENT_ID"));
}

/* Close all open displays
 */
void
_gdk_windowing_exit (void)
{
  GSList *tmp_list = _gdk_displays;
    
  while (tmp_list)
    {
      XCloseDisplay (GDK_DISPLAY_XDISPLAY (tmp_list->data));
      
      tmp_list = tmp_list->next;
  }
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
      g_warning ("The application '%s' lost its connection to the display %s;\n"
                 "most likely the X server was shut down or you killed/destroyed\n"
                 "the application.\n",
                 g_get_prgname (),
                 display ? DisplayString (display) : gdk_get_display_arg_name ());
    }
  else
    {
      g_warning ("%s: Fatal IO error %d (%s) on X server %s.\n",
                 g_get_prgname (),
                 errno, g_strerror (errno),
                 display ? DisplayString (display) : gdk_get_display_arg_name ());
    }

  exit(1);
}

/* X error handler. Keep the name the same because people are used to
 * breaking on it in the debugger.
 */
static int
gdk_x_error (Display	 *xdisplay,
	     XErrorEvent *error)
{
  if (error->error_code)
    {
      GdkDisplay *error_display;
      GdkDisplayManager *manager;
      GSList *displays;

      /* Figure out which GdkDisplay if any got the error. */
      error_display = NULL;
      manager = gdk_display_manager_get ();
      displays = gdk_display_manager_list_displays (manager);
      while (displays != NULL)
        {
          GdkDisplayX11 *gdk_display = displays->data;

          if (xdisplay == gdk_display->xdisplay)
            {
              error_display = GDK_DISPLAY_OBJECT (gdk_display);
              g_slist_free (displays);
              displays = NULL;
            }
          else
            {
              displays = g_slist_delete_link (displays, displays);
            }
        }

      if (error_display == NULL)
        {
          /* Error on an X display not opened by GDK. Ignore. */

          return 0;
        }
      else
        {
          _gdk_x11_display_error_event (error_display, error);
        }
    }

  return 0;
}

void
_gdk_x11_error_handler_push (void)
{
  GdkXErrorHandler previous;

  previous = XSetErrorHandler (gdk_x_error);

  if (_gdk_error_handler_push_count > 0)
    {
      if (previous != gdk_x_error)
        g_warning ("XSetErrorHandler() called with a GDK error trap pushed. Don't do that.");
    }
  else
    {
      _gdk_old_error_handler = previous;
    }

  _gdk_error_handler_push_count += 1;
}

void
_gdk_x11_error_handler_pop  (void)
{
  g_return_if_fail (_gdk_error_handler_push_count > 0);

  _gdk_error_handler_push_count -= 1;

  if (_gdk_error_handler_push_count == 0)
    {
      XSetErrorHandler (_gdk_old_error_handler);
      _gdk_old_error_handler = NULL;
    }
}

/**
 * gdk_error_trap_push:
 *
 * This function allows X errors to be trapped instead of the normal
 * behavior of exiting the application. It should only be used if it
 * is not possible to avoid the X error in any other way. Errors are
 * ignored on all #GdkDisplay currently known to the
 * #GdkDisplayManager. If you don't care which error happens and just
 * want to ignore everything, pop with gdk_error_trap_pop_ignored().
 * If you need the error code, use gdk_error_trap_pop() which may have
 * to block and wait for the error to arrive from the X server.
 *
 * This API exists on all platforms but only does anything on X.
 *
 * You can use gdk_x11_display_error_trap_push() to ignore errors
 * on only a single display.
 *
 * <example>
 * <title>Trapping an X error</title>
 * <programlisting>
 * gdk_error_trap_push (<!-- -->);
 *
 *  // ... Call the X function which may cause an error here ...
 *
 *
 * if (gdk_error_trap_pop (<!-- -->))
 *  {
 *    // ... Handle the error here ...
 *  }
 * </programlisting>
 * </example>
 *
 */
void
gdk_error_trap_push (void)
{
  GdkGlobalErrorTrap *trap;
  GdkDisplayManager *manager;
  GSList *tmp_list;

  trap = g_slice_new (GdkGlobalErrorTrap);
  manager = gdk_display_manager_get ();
  trap->displays = gdk_display_manager_list_displays (manager);

  g_slist_foreach (trap->displays, (GFunc) g_object_ref, NULL);
  for (tmp_list = trap->displays;
       tmp_list != NULL;
       tmp_list = tmp_list->next)
    {
      gdk_x11_display_error_trap_push (tmp_list->data);
    }

  g_queue_push_head (&gdk_error_traps, trap);
}

static gint
gdk_error_trap_pop_internal (gboolean need_code)
{
  GdkGlobalErrorTrap *trap;
  gint result;
  GSList *tmp_list;

  trap = g_queue_pop_head (&gdk_error_traps);

  g_return_val_if_fail (trap != NULL, Success);

  result = Success;
  for (tmp_list = trap->displays;
       tmp_list != NULL;
       tmp_list = tmp_list->next)
    {
      gint code = Success;

      if (need_code)
        code = gdk_x11_display_error_trap_pop (tmp_list->data);
      else
        gdk_x11_display_error_trap_pop_ignored (tmp_list->data);

      /* we use the error on the last display listed, why not. */
      if (code != Success)
        result = code;
    }

  g_slist_foreach (trap->displays, (GFunc) g_object_unref, NULL);
  g_slist_free (trap->displays);

  g_slice_free (GdkGlobalErrorTrap, trap);

  return result;
}

/**
 * gdk_error_trap_pop_ignored:
 *
 * Removes an error trap pushed with gdk_error_trap_push(), but
 * without bothering to wait and see whether an error occurred.  If an
 * error arrives later asynchronously that was triggered while the
 * trap was pushed, that error will be ignored.
 *
 * Since: 3.0
 */
void
gdk_error_trap_pop_ignored (void)
{
  gdk_error_trap_pop_internal (FALSE);
}

/**
 * gdk_error_trap_pop:
 *
 * Removes an error trap pushed with gdk_error_trap_push().
 * May block until an error has been definitively received
 * or not received from the X server. gdk_error_trap_pop_ignored()
 * is preferred if you don't need to know whether an error
 * occurred, because it never has to block. If you don't
 * need the return value of gdk_error_trap_pop(), use
 * gdk_error_trap_pop_ignored().
 *
 * Prior to GDK 3.0, this function would not automatically
 * sync for you, so you had to gdk_flush() if your last
 * call to Xlib was not a blocking round trip.
 *
 * Return value: X error code or 0 on success
 */
gint
gdk_error_trap_pop (void)
{
  return gdk_error_trap_pop_internal (TRUE);
}

gchar *
gdk_get_display (void)
{
  return g_strdup (gdk_display_get_name (gdk_display_get_default ()));
}

/**
 * _gdk_send_xevent:
 * @display: #GdkDisplay which @window is on
 * @window: window ID to which to send the event
 * @propagate: %TRUE if the event should be propagated if the target window
 *             doesn't handle it.
 * @event_mask: event mask to match against, or 0 to send it to @window
 *              without regard to event masks.
 * @event_send: #XEvent to send
 * 
 * Send an event, like XSendEvent(), but trap errors and check
 * the result.
 * 
 * Return value: %TRUE if sending the event succeeded.
 **/
gint 
_gdk_send_xevent (GdkDisplay *display,
		  Window      window, 
		  gboolean    propagate, 
		  glong       event_mask,
		  XEvent     *event_send)
{
  gboolean result;

  if (display->closed)
    return FALSE;

  gdk_error_trap_push ();
  result = XSendEvent (GDK_DISPLAY_XDISPLAY (display), window, 
		       propagate, event_mask, event_send);
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
  
  if (gdk_error_trap_pop ())
    return FALSE;
 
  return result;
}

void
_gdk_region_get_xrectangles (const cairo_region_t *region,
                             gint             x_offset,
                             gint             y_offset,
                             XRectangle     **rects,
                             gint            *n_rects)
{
  XRectangle *rectangles;
  cairo_rectangle_int_t box;
  gint i, n;
  
  n = cairo_region_num_rectangles (region);
  rectangles = g_new (XRectangle, n);

  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (region, i, &box);
      rectangles[i].x = CLAMP (box.x + x_offset, G_MINSHORT, G_MAXSHORT);
      rectangles[i].y = CLAMP (box.y + y_offset, G_MINSHORT, G_MAXSHORT);
      rectangles[i].width = CLAMP (box.width, G_MINSHORT, G_MAXSHORT);
      rectangles[i].height = CLAMP (box.height, G_MINSHORT, G_MAXSHORT);
    }

  *n_rects = n;
  *rects = rectangles;
}

/**
 * gdk_x11_grab_server:
 * 
 * Call gdk_x11_display_grab() on the default display. 
 * To ungrab the server again, use gdk_x11_ungrab_server(). 
 *
 * gdk_x11_grab_server()/gdk_x11_ungrab_server() calls can be nested.
 **/ 
void
gdk_x11_grab_server (void)
{
  gdk_x11_display_grab (gdk_display_get_default ());
}

/**
 * gdk_x11_ungrab_server:
 *
 * Ungrab the default display after it has been grabbed with 
 * gdk_x11_grab_server(). 
 **/
void
gdk_x11_ungrab_server (void)
{
  gdk_x11_display_ungrab (gdk_display_get_default ());
}

/**
 * gdk_x11_get_default_screen:
 * 
 * Gets the default GTK+ screen number.
 * 
 * Return value: returns the screen number specified by
 *   the --display command line option or the DISPLAY environment
 *   variable when gdk_init() calls XOpenDisplay().
 **/
gint
gdk_x11_get_default_screen (void)
{
  return gdk_screen_get_number (gdk_screen_get_default ());
}

/**
 * gdk_x11_get_default_root_xwindow:
 * 
 * Gets the root window of the default screen 
 * (see gdk_x11_get_default_screen()).  
 * 
 * Return value: an Xlib <type>Window</type>.
 **/
Window
gdk_x11_get_default_root_xwindow (void)
{
  return GDK_SCREEN_XROOTWIN (gdk_screen_get_default ());
}

/**
 * gdk_x11_get_default_xdisplay:
 * 
 * Gets the default GTK+ display.
 * 
 * Return value: the Xlib <type>Display*</type> for the display
 * specified in the <option>--display</option> command line option 
 * or the <envar>DISPLAY</envar> environment variable.
 **/
Display *
gdk_x11_get_default_xdisplay (void)
{
  return GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
}

void
_gdk_windowing_event_data_copy (const GdkEvent *src,
                                GdkEvent       *dst)
{
}

void
_gdk_windowing_event_data_free (GdkEvent *event)
{
}
