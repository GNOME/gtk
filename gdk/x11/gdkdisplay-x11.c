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

#ifdef HAVE_SOLARIS_XINERAMA
#include <X11/extensions/xinerama.h>
#endif
#ifdef HAVE_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

static void                 gdk_display_x11_class_init         (GdkDisplayX11Class *class);
static gint                 gdk_display_x11_get_n_screens      (GdkDisplay         *display);
static GdkScreen *          gdk_display_x11_get_screen         (GdkDisplay         *display,
						                gint                screen_num);
G_CONST_RETURN static char *gdk_display_x11_get_display_name   (GdkDisplay         *display);
static GdkScreen *          gdk_display_x11_get_default_screen (GdkDisplay         *display);
static void                 gdk_display_x11_finalize           (GObject            *object);

static gpointer parent_class = NULL;

GType
gdk_display_x11_get_type ()
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (GdkDisplayX11Class),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) gdk_display_x11_finalize,
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
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  display_class->get_display_name = gdk_display_x11_get_display_name;
  display_class->get_n_screens = gdk_display_x11_get_n_screens;
  display_class->get_screen = gdk_display_x11_get_screen;
  display_class->get_default_screen = gdk_display_x11_get_default_screen;
  
  G_OBJECT_CLASS (class)->finalize = gdk_display_x11_finalize;
  
  parent_class = g_type_class_peek_parent (class);
}

#ifdef HAVE_XINERAMA
static gboolean
check_solaris_xinerama (GdkScreen *screen)
{
#ifdef HAVE_SOLARIS_XINERAMA
  
  if (XineramaGetState (GDK_SCREEN_XDISPLAY (screen),
			gdk_screen_get_number (screen)))
    {
      XRectangle monitors[MAXFRAMEBUFFERS];
      char hints[16];
      gint result;
      GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);

      result = XineramaGetInfo (GDK_SCREEN_XDISPLAY (screen),
				gdk_screen_get_number (screen),
				monitors, hints,
				&screen_x11->num_monitors);
      /* Yes I know it should be Success but the current implementation 
          returns the num of monitor*/
      if (result == 0)
	{
	  /* FIXME: We need to trap errors, since XINERAMA isn't always XINERAMA.
	   */ 
	  g_error ("error while retrieving Xinerama information");
	}
      else
	{
	  int i;
	  screen_x11->use_virtual_screen = TRUE;
	  screen_x11->monitors = g_new0 (GdkRectangle, screen_x11->num_monitors);
	  
	  for (i = 0; i < screen_x11->num_monitors; i++)
	    {
	      screen_x11->monitors[i].x = monitors[i].x;
	      screen_x11->monitors[i].y = monitors[i].y;
	      screen_x11->monitors[i].width = monitors[i].width;
	      screen_x11->monitors[i].height = monitors[i].height;
	    }

	  return TRUE;
	}
    }
#endif /* HAVE_SOLARIS_XINERAMA */
  
  return FALSE;
}

static gboolean
check_xfree_xinerama (GdkScreen *screen)
{
#ifdef HAVE_XFREE_XINERAMA
  if (XineramaIsActive (GDK_SCREEN_XDISPLAY (screen)))
    {
      XineramaScreenInfo *monitors = XineramaQueryScreens (GDK_SCREEN_XDISPLAY (screen),
							   &screen_x11->num_monitors);
      if (screen_x11->num_monitors <= 0)
	{
	  /* FIXME: We need to trap errors, since XINERAMA isn't always XINERAMA.
	   *        I don't think the num_monitors <= 0 check has any validity.
	   */ 
	  g_error ("error while retrieving Xinerama information");
	}
      else
	{
	  int i;
	  screen_x11->use_virtual_screen = TRUE;
	  screen_x11->monitors = g_new0 (GdkRectangle, screen_x11->num_monitors);
	  
	  for (i = 0; i < screen_x11->num_monitors; i++)
	    {
	      screen_x11->monitors[i].x = monitors[i].x_org;
	      screen_x11->monitors[i].y = monitors[i].y_org;
	      screen_x11->monitors[i].width = monitors[i].width;
	      screen_x11->monitors[i].height = monitors[i].height;
	    }

	  XFree (monitors);

	  return TRUE;
	}
    }
#endif /* HAVE_XFREE_XINERAMA */
  
  return FALSE;
}
#endif /* HAVE_XINERAMA */

static void
init_xinerama_support (GdkScreen * screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  
#ifdef HAVE_XINERAMA
  int opcode, firstevent, firsterror;
  gint result;
  
  if (XQueryExtension (GDK_SCREEN_XDISPLAY (screen), "XINERAMA",
		       &opcode, &firstevent, &firsterror))
    {
      if (check_solaris_xinerama (screen) ||
	  check_xfree_xinerama (screen))
	return;
    }
#endif /* HAVE_XINERAMA */

  /* No Xinerama
   */
  screen_x11->use_virtual_screen = FALSE;
  screen_x11->num_monitors = 1;
  screen_x11->monitors = g_new0 (GdkRectangle, 1);
  screen_x11->monitors[0].x = 0;
  screen_x11->monitors[0].y = 0;
  screen_x11->monitors[0].width = WidthOfScreen (screen_x11->xscreen);
  screen_x11->monitors[0].height = HeightOfScreen (screen_x11->xscreen);
}

GdkDisplay *
gdk_open_display (const gchar *display_name)
{
  Display *xdisplay;
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  gint argc;
  gchar **argv;
  
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
  display_x11->xdisplay = xdisplay;
  
  /* populate the screen list and set default */
  for (i = 0; i < ScreenCount (display_x11->xdisplay); i++)
    {
      GdkScreen *screen;
      GdkScreenX11 *screen_x11;
      
      screen = g_object_new (GDK_TYPE_SCREEN_X11, NULL);
      
      screen_x11 = GDK_SCREEN_X11 (screen);
      screen_x11->display = display;
      screen_x11->xdisplay = display_x11->xdisplay;
      screen_x11->xscreen = ScreenOfDisplay (display_x11->xdisplay, i);
      screen_x11->screen_num = i;
      screen_x11->xroot_window = (Window) RootWindow (display_x11->xdisplay, i);
      screen_x11->wmspec_check_window = None;
      screen_x11->visual_initialised = FALSE;
      screen_x11->colormap_initialised = FALSE;

      
      init_xinerama_support (screen);
      if (screen_x11->xscreen == DefaultScreenOfDisplay (display_x11->xdisplay))
        {
          display_x11->default_screen = screen;
          display_x11->leader_window = XCreateSimpleWindow (display_x11->xdisplay,
							     screen_x11->xroot_window,
							     10, 10, 10, 10, 0, 0, 0);
        }
      
      display_x11->screen_list = g_slist_prepend (display_x11->screen_list, screen);
    }

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
                             XkbMapNotifyMask | XkbStateNotifyMask,
                             XkbMapNotifyMask | XkbStateNotifyMask);

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

  _gdk_visual_init (display_x11->default_screen);
  _gdk_windowing_window_init (display_x11->default_screen);
  _gdk_windowing_image_init (display);
  _gdk_events_init (display);
  _gdk_input_init (display);
  _gdk_dnd_init (display);

  return display;
}

static G_CONST_RETURN gchar*
gdk_display_x11_get_display_name (GdkDisplay * display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  return (gchar *) DisplayString (display_x11->xdisplay);
}

static gint
gdk_display_x11_get_n_screens (GdkDisplay * display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  return ScreenCount (display_x11->xdisplay);
}

static GdkScreen *
gdk_display_x11_get_screen (GdkDisplay *display,
			    gint        screen_num)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  Screen *desired_screen;
  GSList *tmp_list;

  g_return_val_if_fail (ScreenCount (display_x11->xdisplay) > screen_num, NULL);
  
  desired_screen = ScreenOfDisplay (display_x11->xdisplay, screen_num);
  
  tmp_list = display_x11->screen_list;
  while (tmp_list)
    {
      GdkScreenX11 *screen_x11 = tmp_list->data;
      GdkScreen *screen = tmp_list->data;
      
      if (screen_x11->xscreen == desired_screen)
        {
          if (!screen_x11->visual_initialised)
            _gdk_visual_init (screen);
          if (!screen_x11->colormap_initialised)
            _gdk_windowing_window_init (screen);
	  
          if (!screen_x11->xsettings_client)
	    _gdk_x11_events_init_screen (screen);
	  
	  return screen;
        }
      
      tmp_list = tmp_list->next;
    }

  g_assert_not_reached ();
  return NULL;
}

static GdkScreen *
gdk_display_x11_get_default_screen (GdkDisplay * display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  return display_x11->default_screen;
}

gboolean
_gdk_x11_display_is_root_window (GdkDisplay *display,
				 Window      xroot_window)
{
  GdkDisplayX11 *display_x11;
  GSList *tmp_list;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  
  display_x11 = GDK_DISPLAY_X11 (display);
  
  tmp_list = display_x11->screen_list;
  while (tmp_list)
    {
      GdkScreenX11 *screen_x11 = tmp_list->data;
      
      if (screen_x11->xroot_window == xroot_window)
        return TRUE;
      
      tmp_list = tmp_list->next;
    }

  return FALSE;
}

/**
 * gdk_display_pointer_ungrab:
 * @display : a #GdkDisplay.
 * @time: a timestap (e.g. GDK_CURRENT_TIME).
 *
 * Release any pointer grab.
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
 * gdk_display_pointer_is_grabbed :
 * @display : a #GdkDisplay
 *
 * Test if the pointer is grabbed.
 *
 * Returns : TRUE if an active X pointer grab is in effect
 */
gboolean
gdk_display_pointer_is_grabbed (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);
  
  return (GDK_DISPLAY_X11 (display)->pointer_xgrab_window != NULL);
}

/**
 * gdk_display_keyboard_ungrab:
 * @display : a #GdkDisplay.
 * @time : a timestap (e.g #GDK_CURRENT_TIME).
 *
 * Release any keyboard grab
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
 */
void
gdk_display_beep (GdkDisplay * display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
}

/**
 * gdk_display_sync :
 * @display : a #GdkDisplay
 *
 * Flushes any requests queued for the windowing system and waits until all
 * requests have been handled. This is often used for making sure that the
 * display is synchronized with the current state of the program. Calling
 * gdk_display_sync() before gdk_error_trap_pop() makes sure that any errors
 * generated from earlier requests are handled before the error trap is removed.
 *
 * This is most useful for X11. On windowing systems where requests handled
 * synchronously, this function will do nothing.
 */
void
gdk_display_sync (GdkDisplay * display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
}

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

void
gdk_x11_display_ungrab (GdkDisplay * display)
{
  GdkDisplayX11 *display_x11;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  display_x11 = GDK_DISPLAY_X11 (display);;
  g_return_if_fail (display_x11->grab_count > 0);
  
  display_x11->grab_count--;
  if (display_x11->grab_count == 0)
    XUngrabServer (display_x11->xdisplay);
}

static void
gdk_display_x11_finalize (GObject *object)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (object);
  int i;
  GList *tmp;
  GSList *stmp;
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
    g_object_unref (G_OBJECT (tmp->data));
  g_list_free (display_x11->input_devices);
  /* input GdkWindow list */
  for (tmp = display_x11->input_windows; tmp; tmp = tmp->next)
    g_object_unref (G_OBJECT (tmp->data));
  g_list_free (display_x11->input_windows);
  /* Free all GdkScreens */
  for (stmp = display_x11->screen_list; stmp; stmp = stmp->next)
    g_object_unref (G_OBJECT (stmp->data));
  g_slist_free (display_x11->screen_list);
  XCloseDisplay (display_x11->xdisplay);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gdk_x11_lookup_xdisplay:
 * @xdisplay: a pointer to an X Display
 * 
 * Find the #GdkDisplay corresponding to @display, if any exists.
 * 
 * Return value: the #GdkDisplay, if found, otherwise %NULL.
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

Display *
gdk_x11_display_get_xdisplay (GdkDisplay  *display)
{
  return GDK_DISPLAY_X11 (display)->xdisplay;
}
