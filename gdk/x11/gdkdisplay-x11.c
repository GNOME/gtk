/* GDK - The GIMP Drawing Kit
 * gdkdisplay-x11.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include "gdkx.h"
#include "gdkdisplay.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen.h"
#include "gdkscreen-x11.h"
#include "gdkinternals.h"
#include "gdkinputprivate.h"
#include "xsettings-client.h"

#include <X11/Xatom.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

static void                 gdk_display_x11_class_init         (GdkDisplayX11Class *class);
static void                 gdk_display_x11_dispose            (GObject            *object);
static void                 gdk_display_x11_finalize           (GObject            *object);

static void gdk_internal_connection_watch (Display  *display,
					   XPointer  arg,
					   gint      fd,
					   gboolean  opening,
					   XPointer *watch_data);

static gpointer parent_class = NULL;

GType
_gdk_display_x11_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (GdkDisplayX11Class),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gdk_display_x11_class_init,
	  NULL,			/* class_finalize */
	  NULL,			/* class_data */
	  sizeof (GdkDisplayX11),
	  0,			/* n_preallocs */
	  (GInstanceInitFunc) NULL,
	};
      
      object_type = g_type_register_static (GDK_TYPE_DISPLAY,
					    "GdkDisplayX11",
					    &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_display_x11_class_init (GdkDisplayX11Class * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  
  object_class->dispose = gdk_display_x11_dispose;
  object_class->finalize = gdk_display_x11_finalize;
  
  parent_class = g_type_class_peek_parent (class);
}

/**
 * gdk_display_open:
 * @display_name: the name of the display to open
 * @returns: a #GdkDisplay, or %NULL if the display
 *  could not be opened.
 *
 * Opens a display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  Display *xdisplay;
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  gint argc;
  gchar **argv;
  const char *sm_client_id;
  
  XClassHint *class_hint;
  XKeyboardState keyboard_state;
  gulong pid;
  gint i;

  xdisplay = XOpenDisplay (display_name);
  if (!xdisplay)
    return NULL;
  
  display = g_object_new (GDK_TYPE_DISPLAY_X11, NULL);
  display_x11 = GDK_DISPLAY_X11 (display);

  display_x11->use_xft = -1;
  display_x11->use_xshm = TRUE;
  display_x11->xdisplay = xdisplay;

  /* Set up handlers for Xlib internal connections */
  XAddConnectionWatch (xdisplay, gdk_internal_connection_watch, NULL);
  
  /* initialize the display's screens */ 
  display_x11->screens = g_new (GdkScreen *, ScreenCount (display_x11->xdisplay));
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    display_x11->screens[i] = _gdk_x11_screen_new (display, i);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    _gdk_x11_events_init_screen (display_x11->screens[i]);
  
  /*set the default screen */
  display_x11->default_screen = display_x11->screens[DefaultScreen (display_x11->xdisplay)];
  display_x11->leader_window = XCreateSimpleWindow (display_x11->xdisplay,
						    GDK_SCREEN_X11 (display_x11->default_screen)->xroot_window,
						    10, 10, 10, 10, 0, 0, 0);
  display_x11->leader_window_title_set = FALSE;

  display_x11->have_shape = GDK_UNKNOWN;
  display_x11->gravity_works = GDK_UNKNOWN;
  display_x11->have_render = GDK_UNKNOWN;

  if (_gdk_synchronize)
    XSynchronize (display_x11->xdisplay, True);
  
  class_hint = XAllocClassHint();
  class_hint->res_name = g_get_prgname ();
  
  class_hint->res_class = (char *)gdk_get_program_class ();
  _gdk_get_command_line_args (&argc, &argv);
  XmbSetWMProperties (display_x11->xdisplay,
		      display_x11->leader_window,
		      NULL, NULL, argv, argc, NULL, NULL,
		      class_hint);
  XFree (class_hint);

  sm_client_id = _gdk_get_sm_client_id ();
  if (sm_client_id)
    _gdk_windowing_display_set_sm_client_id (display, sm_client_id);

  pid = getpid ();
  XChangeProperty (display_x11->xdisplay,
		   display_x11->leader_window,
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PID"),
		   XA_CARDINAL, 32, PropModeReplace, (guchar *) & pid, 1);
  
  XGetKeyboardControl (display_x11->xdisplay, &keyboard_state);

#ifdef HAVE_XKB
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion (&xkb_major, &xkb_minor))
      {
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;
	    
        if (XkbQueryExtension (display_x11->xdisplay, 
			       NULL, &display_x11->xkb_event_type, NULL,
                               &xkb_major, &xkb_minor))
          {
	    Bool detectable_autorepeat_supported;
	    
	    display_x11->use_xkb = TRUE;

            XkbSelectEvents (display_x11->xdisplay,
                             XkbUseCoreKbd,
                             XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask,
                             XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask);

	    XkbSetDetectableAutoRepeat (display_x11->xdisplay,
					True,
					&detectable_autorepeat_supported);

	    GDK_NOTE (MISC, g_message ("Detectable autorepeat %s.",
				       detectable_autorepeat_supported ? 
				       "supported" : "not supported"));
	    
	    display_x11->have_xkb_autorepeat = detectable_autorepeat_supported;
          }
      }
  }
#endif

  _gdk_windowing_image_init (display);
  _gdk_events_init (display);
  _gdk_input_init (display);
  _gdk_dnd_init (display);

  g_signal_emit_by_name (gdk_display_manager_get(),
			 "display_opened", display);

  return display;
}

/*
 * XLib internal connection handling
 */
typedef struct _GdkInternalConnection GdkInternalConnection;

struct _GdkInternalConnection
{
  gint	         fd;
  GSource	*source;
  Display	*display;
};

static gboolean
process_internal_connection (GIOChannel  *gioc,
			     GIOCondition cond,
			     gpointer     data)
{
  GdkInternalConnection *connection = (GdkInternalConnection *)data;

  GDK_THREADS_ENTER ();

  XProcessInternalConnection ((Display*)connection->display, connection->fd);

  GDK_THREADS_LEAVE ();

  return TRUE;
}

static GdkInternalConnection *
gdk_add_connection_handler (Display *display,
			    guint    fd)
{
  GIOChannel *io_channel;
  GdkInternalConnection *connection;

  connection = g_new (GdkInternalConnection, 1);

  connection->fd = fd;
  connection->display = display;
  
  io_channel = g_io_channel_unix_new (fd);
  
  connection->source = g_io_create_watch (io_channel, G_IO_IN);
  g_source_set_callback (connection->source,
			 (GSourceFunc)process_internal_connection, connection, NULL);
  g_source_attach (connection->source, NULL);
  
  g_io_channel_unref (io_channel);
  
  return connection;
}

static void
gdk_remove_connection_handler (GdkInternalConnection *connection)
{
  g_source_destroy (connection->source);
  g_free (connection);
}

static void
gdk_internal_connection_watch (Display  *display,
			       XPointer  arg,
			       gint      fd,
			       gboolean  opening,
			       XPointer *watch_data)
{
  if (opening)
    *watch_data = (XPointer)gdk_add_connection_handler (display, fd);
  else
    gdk_remove_connection_handler ((GdkInternalConnection *)*watch_data);
}

/**
 * gdk_display_get_name:
 * @display: a #GdkDisplay
 *
 * Gets the name of the display.
 * 
 * Returns: a string representing the display name. This string is owned
 * by GDK and should not be modified or freed.
 * 
 * Since: 2.2
 */
G_CONST_RETURN gchar *
gdk_display_get_name (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  return (gchar *) DisplayString (GDK_DISPLAY_X11 (display)->xdisplay);
}

/**
 * gdk_display_get_n_screens:
 * @display: a #GdkDisplay
 *
 * Gets the number of screen managed by the @display.
 * 
 * Returns: number of screens.
 * 
 * Since: 2.2
 */
gint
gdk_display_get_n_screens (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  
  return ScreenCount (GDK_DISPLAY_X11 (display)->xdisplay);
}

/**
 * gdk_display_get_screen:
 * @display: a #GdkDisplay
 * @screen_num: the screen number
 *
 * Returns a screen object for one of the screens of the display.
 *
 * Returns: the #GdkScreen object
 *
 * Since: 2.2
 */
GdkScreen *
gdk_display_get_screen (GdkDisplay * display, gint screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (ScreenCount (GDK_DISPLAY_X11 (display)->xdisplay) > screen_num, NULL);
  
  return GDK_DISPLAY_X11 (display)->screens[screen_num];
}

/**
 * gdk_display_get_default_screen:
 * @display: a #GdkDisplay
 *
 * Get the default #GdkScreen for @display.
 * 
 * Returns: the default #GdkScreen object for @display
 *
 * Since: 2.2
 */
GdkScreen *
gdk_display_get_default_screen (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  return GDK_DISPLAY_X11 (display)->default_screen;
}

gboolean
_gdk_x11_display_is_root_window (GdkDisplay *display,
				 Window      xroot_window)
{
  GdkDisplayX11 *display_x11;
  gint i;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  
  display_x11 = GDK_DISPLAY_X11 (display);
  
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    {
      if (GDK_SCREEN_XROOTWIN (display_x11->screens[i]) == xroot_window)
	return TRUE;
    }
  return FALSE;
}

/**
 * gdk_display_pointer_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g. GDK_CURRENT_TIME).
 *
 * Release any pointer grab.
 *
 * Since: 2.2
 */
void
gdk_display_pointer_ungrab (GdkDisplay *display,
			    guint32     time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  _gdk_input_ungrab_pointer (display, time);
  XUngrabPointer (GDK_DISPLAY_XDISPLAY (display), time);
  GDK_DISPLAY_X11 (display)->pointer_xgrab_window = NULL;
}

/**
 * gdk_display_pointer_is_grabbed:
 * @display: a #GdkDisplay
 *
 * Test if the pointer is grabbed.
 *
 * Returns: %TRUE if an active X pointer grab is in effect
 *
 * Since: 2.2
 */
gboolean
gdk_display_pointer_is_grabbed (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);
  
  return (GDK_DISPLAY_X11 (display)->pointer_xgrab_window != NULL);
}

/**
 * gdk_display_keyboard_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g #GDK_CURRENT_TIME).
 *
 * Release any keyboard grab
 *
 * Since: 2.2
 */
void
gdk_display_keyboard_ungrab (GdkDisplay *display,
			     guint32     time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  XUngrabKeyboard (GDK_DISPLAY_XDISPLAY (display), time);
  GDK_DISPLAY_X11 (display)->keyboard_xgrab_window = NULL;
}

/**
 * gdk_display_beep:
 * @display: a #GdkDisplay
 *
 * Emits a short beep on @display
 *
 * Since: 2.2
 */
void
gdk_display_beep (GdkDisplay * display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
}

/**
 * gdk_display_sync:
 * @display: a #GdkDisplay
 *
 * Flushes any requests queued for the windowing system and waits until all
 * requests have been handled. This is often used for making sure that the
 * display is synchronized with the current state of the program. Calling
 * gdk_display_sync() before gdk_error_trap_pop() makes sure that any errors
 * generated from earlier requests are handled before the error trap is 
 * removed.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 *
 * Since: 2.2
 */
void
gdk_display_sync (GdkDisplay * display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
}

/**
 * gdk_x11_display_grab:
 * @display: a #GdkDisplay 
 * 
 * Call XGrabServer() on @display. 
 * To ungrab the display again, use gdk_x11_display_ungrab(). 
 *
 * gdk_x11_display_grab()/gdk_x11_display_ungrab() calls can be nested.
 *
 * Since: 2.2
 **/
void
gdk_x11_display_grab (GdkDisplay * display)
{
  GdkDisplayX11 *display_x11;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  display_x11 = GDK_DISPLAY_X11 (display);
  
  if (display_x11->grab_count == 0)
    XGrabServer (display_x11->xdisplay);
  display_x11->grab_count++;
}

/**
 * gdk_x11_display_ungrab:
 * @display: a #GdkDisplay
 * 
 * Ungrab @display after it has been grabbed with 
 * gdk_x11_display_grab(). 
 *
 * Since: 2.2
 **/
void
gdk_x11_display_ungrab (GdkDisplay * display)
{
  GdkDisplayX11 *display_x11;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  display_x11 = GDK_DISPLAY_X11 (display);;
  g_return_if_fail (display_x11->grab_count > 0);
  
  display_x11->grab_count--;
  if (display_x11->grab_count == 0)
    {
      XUngrabServer (display_x11->xdisplay);
      XFlush (display_x11->xdisplay);
    }
}

static void
gdk_display_x11_dispose (GObject *object)
{
  GdkDisplayX11 *display_x11;
  gint i;
  
  display_x11 = GDK_DISPLAY_X11 (object);
  
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    _gdk_screen_close (display_x11->screens[i]);

  g_source_destroy (display_x11->event_source);

  XCloseDisplay (display_x11->xdisplay);
  display_x11->xdisplay = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gdk_display_x11_finalize (GObject *object)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (object);
  int i;
  GList *tmp;
  /* FIXME need to write GdkKeymap finalize fct 
     g_object_unref (display_x11->keymap);
   */
  /* Free motif Dnd */
  if (display_x11->motif_target_lists)
    {
      for (i = 0; i < display_x11->motif_n_target_lists; i++)
        g_list_free (display_x11->motif_target_lists[i]);
      g_free (display_x11->motif_target_lists);
    }

  /* Atom Hashtable */
  g_hash_table_destroy (display_x11->atom_from_virtual);
  g_hash_table_destroy (display_x11->atom_to_virtual);
  /* Leader Window */
  XDestroyWindow (display_x11->xdisplay, display_x11->leader_window);
  /* list of filters for client messages */
  g_list_free (display_x11->client_filters);
  /* X ID hashtable */
  g_hash_table_destroy (display_x11->xid_ht);
  /* input GdkDevice list */
  /* FIXME need to write finalize fct */
  for (tmp = display_x11->input_devices; tmp; tmp = tmp->next)
    g_object_unref (tmp->data);
  g_list_free (display_x11->input_devices);
  /* input GdkWindow list */
  for (tmp = display_x11->input_windows; tmp; tmp = tmp->next)
    g_object_unref (tmp->data);
  g_list_free (display_x11->input_windows);
  /* Free all GdkScreens */
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    g_object_unref (display_x11->screens[i]);
  g_free (display_x11->screens);
  g_free (display_x11->startup_notification_id);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gdk_x11_lookup_xdisplay:
 * @xdisplay: a pointer to an X Display
 * 
 * Find the #GdkDisplay corresponding to @display, if any exists.
 * 
 * Return value: the #GdkDisplay, if found, otherwise %NULL.
 *
 * Since: 2.2
 **/
GdkDisplay *
gdk_x11_lookup_xdisplay (Display *xdisplay)
{
  GSList *tmp_list;

  for (tmp_list = _gdk_displays; tmp_list; tmp_list = tmp_list->next)
    {
      if (GDK_DISPLAY_XDISPLAY (tmp_list->data) == xdisplay)
	return tmp_list->data;
    }
  
  return NULL;
}

/**
 * _gdk_x11_display_screen_for_xrootwin:
 * @display: a #Display
 * @xrootwin: window ID for one of of the screen's of the display.
 * 
 * Given the root window ID of one of the screen's of a #GdkDisplay,
 * finds the screen.
 * 
 * Return value: the #GdkScreen corresponding to @xrootwin, or %NULL.
 **/
GdkScreen *
_gdk_x11_display_screen_for_xrootwin (GdkDisplay *display,
				      Window      xrootwin)
{
  gint n_screens, i;

  n_screens = gdk_display_get_n_screens (display);
  for (i = 0; i < n_screens; i++)
    {
      GdkScreen *screen = gdk_display_get_screen (display, i);
      if (GDK_SCREEN_XROOTWIN (screen) == xrootwin)
	return screen;
    }

  return NULL;
}

/**
 * gdk_x11_display_get_xdisplay:
 * @display: a #GdkDisplay
 * @returns: an X display.
 *
 * Returns the X display of a #GdkDisplay.
 *
 * Since: 2.2
 */
Display *
gdk_x11_display_get_xdisplay (GdkDisplay  *display)
{
  return GDK_DISPLAY_X11 (display)->xdisplay;
}

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  const gchar *startup_id;
  
  if (display)
    gdk_display = GDK_DISPLAY_XDISPLAY (display);
  else
    gdk_display = NULL;

  g_free (display_x11->startup_notification_id);
  display_x11->startup_notification_id = NULL;
  
  startup_id = g_getenv ("DESKTOP_STARTUP_ID");
  if (startup_id && *startup_id != '\0')
    {
      if (!g_utf8_validate (startup_id, -1, NULL))
	g_warning ("DESKTOP_STARTUP_ID contains invalid UTF-8");
      else
	display_x11->startup_notification_id = g_strdup (startup_id);
      
      /* Clear the environment variable so it won't be inherited by
       * child processes and confuse things.  unsetenv isn't portable,
       * right...
       */
      putenv ("DESKTOP_STARTUP_ID=");

      /* Set the startup id on the leader window so it
       * applies to all windows we create on this display
       */
      XChangeProperty (display_x11->xdisplay,
		       display_x11->leader_window,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_STARTUP_ID"),
		       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		       PropModeReplace,
		       startup_id, strlen (startup_id));
    }
}

char*
escape_for_xmessage (const char *str)
{
  GString *retval;
  const char *p;
  
  retval = g_string_new ("");

  p = str;
  while (*p)
    {
      switch (*p)
        {
        case ' ':
        case '"':
        case '\\':
          g_string_append_c (retval, '\\');
          break;
        }

      g_string_append_c (retval, *p);
      ++p;
    }

  return g_string_free (retval, FALSE);
}

static void
broadcast_xmessage   (GdkDisplay   *display,
                      const char   *message_type,
                      const char   *message_type_begin,
                      const char   *message)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  GdkScreen *screen = gdk_display_get_default_screen (display);
  GdkWindow *root_window = gdk_screen_get_root_window (screen);
  Window xroot_window = GDK_WINDOW_XID (root_window);
  
  Atom type_atom;
  Atom type_atom_begin;
  Window xwindow;

  {
    XSetWindowAttributes attrs;

    attrs.override_redirect = True;
    attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

    xwindow =
      XCreateWindow (xdisplay,
                     xroot_window,
                     -100, -100, 1, 1,
                     0,
                     CopyFromParent,
                     CopyFromParent,
                     CopyFromParent,
                     CWOverrideRedirect | CWEventMask,
                     &attrs);
  }

  type_atom = gdk_x11_get_xatom_by_name_for_display (display,
                                                     message_type);
  type_atom_begin = gdk_x11_get_xatom_by_name_for_display (display,
                                                           message_type_begin);
  
  {
    XEvent xevent;
    const char *src;
    const char *src_end;
    char *dest;
    char *dest_end;
    
    xevent.xclient.type = ClientMessage;
    xevent.xclient.message_type = type_atom_begin;
    xevent.xclient.display =xdisplay;
    xevent.xclient.window = xwindow;
    xevent.xclient.format = 8;

    src = message;
    src_end = message + strlen (message) + 1; /* +1 to include nul byte */
    
    while (src != src_end)
      {
        dest = &xevent.xclient.data.b[0];
        dest_end = dest + 20;        
        
        while (dest != dest_end &&
               src != src_end)
          {
            *dest = *src;
            ++dest;
            ++src;
          }
        
        XSendEvent (xdisplay,
                    xroot_window,
                    False,
                    PropertyChangeMask,
                    &xevent);

        xevent.xclient.message_type = type_atom;
      }
  }

  XDestroyWindow (xdisplay, xwindow);
  XFlush (xdisplay);
}

/**
 * gdk_notify_startup_complete:
 * 
 * Indicates to the GUI environment that the application has finished
 * loading. If the applications opens windows, this function is
 * normally called after opening the application's initial set of
 * windows.
 * 
 * GTK+ will call this function automatically after opening the first
 * #GtkWindow unless gtk_window_set_auto_startup_notification() is called 
 * to disable that feature.
 *
 * Since: 2.2
 **/
void
gdk_notify_startup_complete (void)
{
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  gchar *escaped_id;
  gchar *message;

  display = gdk_display_get_default ();
  if (!display)
    return;
  
  display_x11 = GDK_DISPLAY_X11 (display);

  if (display_x11->startup_notification_id == NULL)
    return;

  escaped_id = escape_for_xmessage (display_x11->startup_notification_id);
  message = g_strdup_printf ("remove: ID=%s", escaped_id);
  g_free (escaped_id);

  broadcast_xmessage (display,
		      "_NET_STARTUP_INFO",
                      "_NET_STARTUP_INFO_BEGIN",
                      message);

  g_free (message);
}
