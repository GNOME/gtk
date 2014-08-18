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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkdeviceprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkasync.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"

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

/**
 * SECTION:x_interaction
 * @Short_description: X backend-specific functions
 * @Title: X Window System Interaction
 *
 * The functions in this section are specific to the GDK X11 backend.
 * To use them, you need to include the `<gdk/gdkx.h>` header and use
 * the X11-specific pkg-config files to build your application (either
 * `gdk-x11-3.0` or `gtk+-x11-3.0`).
 *
 * To make your code compile with other GDK backends, guard backend-specific
 * calls by an ifdef as follows. Since GDK may be built with multiple
 * backends, you should also check for the backend that is in use (e.g. by
 * using the GDK_IS_X11_DISPLAY() macro).
 * |[
 * #ifdef GDK_WINDOWING_X11
 *   if (GDK_IS_X11_DISPLAY (display))
 *     {
 *       // make X11-specific calls here
 *     }
 *   else
 * #endif
 * #ifdef GDK_WINDOWING_QUARTZ
 *   if (GDK_IS_QUARTZ_DISPLAY (display))
 *     {
 *       // make Quartz-specific calls here
 *     }
 *   else
 * #endif
 *   g_error ("Unsupported GDK backend");
 * ]|
 */

typedef struct _GdkPredicate GdkPredicate;

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

/*
 * Private function declarations
 */

static int	    gdk_x_error			 (Display     *display, 
						  XErrorEvent *error);
static int	    gdk_x_io_error		 (Display     *display);

void
_gdk_x11_windowing_init (void)
{
  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);

  gdk_window_add_filter (NULL,
                         _gdk_wm_protocols_filter,
                         NULL);
  gdk_window_add_filter (NULL,
                         _gdk_x11_dnd_filter,
                         NULL);
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

/*
 * _gdk_x11_window_grab_check_unmap:
 * @window: a #GdkWindow
 * @serial: serial from Unmap event (or from NextRequest(display)
 *   if the unmap is being done by this client.)
 *
 * Checks to see if an unmap request or event causes the current
 * grab window to become not viewable, and if so, clear the
 * the pointer we keep to it.
 **/
void
_gdk_x11_window_grab_check_unmap (GdkWindow *window,
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

/*
 * _gdk_x11_window_grab_check_destroy:
 * @window: a #GdkWindow
 * 
 * Checks to see if window is the current grab window, and if
 * so, clear the current grab window.
 **/
void
_gdk_x11_window_grab_check_destroy (GdkWindow *window)
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

  _exit (1);
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
          GdkX11Display *gdk_display = displays->data;

          if (xdisplay == gdk_display->xdisplay)
            {
              error_display = GDK_DISPLAY (gdk_display);
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

gint
_gdk_x11_display_send_xevent (GdkDisplay *display,
                              Window      window,
                              gboolean    propagate,
                              glong       event_mask,
                              XEvent     *event_send)
{
  gboolean result;

  if (gdk_display_is_closed (display))
    return FALSE;

  gdk_x11_display_error_trap_push (display);
  result = XSendEvent (GDK_DISPLAY_XDISPLAY (display), window,
                       propagate, event_mask, event_send);
  XSync (GDK_DISPLAY_XDISPLAY (display), False);

  if (gdk_x11_display_error_trap_pop (display))
    return FALSE;

  return result;
}

void
_gdk_x11_region_get_xrectangles (const cairo_region_t *region,
                                 gint             x_offset,
                                 gint             y_offset,
                                 gint             scale,
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
      rectangles[i].x = CLAMP ((box.x + x_offset) * scale, G_MINSHORT, G_MAXSHORT);
      rectangles[i].y = CLAMP ((box.y + y_offset) * scale, G_MINSHORT, G_MAXSHORT);
      rectangles[i].width = CLAMP (box.width * scale, G_MINSHORT, G_MAXSHORT);
      rectangles[i].height = CLAMP (box.height * scale, G_MINSHORT, G_MAXSHORT);
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
 * Returns: returns the screen number specified by
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
 * Returns: an Xlib Window.
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
 * Returns: (transfer none): the Xlib Display* for
 * the display specified in the `--display` command
 * line option or the `DISPLAY` environment variable.
 **/
Display *
gdk_x11_get_default_xdisplay (void)
{
  return GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
}
