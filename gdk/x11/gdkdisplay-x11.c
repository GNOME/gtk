/*
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

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib.h>
#include "gdkx.h"
#include "gdkdisplay.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen.h"
#include "gdkscreen-x11.h"
#include "gdkinternals.h"
#include "gdkinputprivate.h"
#include "xsettings-client.h"
#ifdef HAS_SOLARIS_XINERAMA
#include <X11/extensions/xinerama.h>
#endif
#ifdef HAS_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

static void gdk_x11_display_impl_class_init (GdkDisplayImplX11Class * klass);
static gint gdk_x11_display_impl_get_n_screens (GdkDisplay * display);
static GdkScreen *gdk_x11_display_impl_get_screen (GdkDisplay * display,
						   gint screen_num);
G_CONST_RETURN static char *gdk_x11_display_impl_get_display_name (GdkDisplay
								   * display);
static GdkScreen *gdk_x11_display_impl_get_default_screen (GdkDisplay *
							   display);
static void gdk_x11_display_impl_finalize (GObject * object);


extern void _gdk_xsettings_watch_cb (Display * display,
				     Window window,
				     Bool is_start, long mask, void *cb_data);
extern void _gdk_xsettings_notify_cb (Display * display,
				      Window root_window,
				      const char *name,
				      XSettingsAction action,
				      XSettingsSetting * setting, void *data);
static gpointer parent_class = NULL;
GType
gdk_x11_display_impl_get_type ()
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
	sizeof (GdkDisplayImplX11Class),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) gdk_x11_display_impl_finalize,
	(GClassInitFunc) gdk_x11_display_impl_class_init,
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (GdkDisplayImplX11),
	0,			/* n_preallocs */
	(GInstanceInitFunc) NULL,
      };
      object_type = g_type_register_static (GDK_TYPE_DISPLAY,
					    "GdkDisplayImplX11",
					    &object_info, 0);
    }
  return object_type;
}

static void
gdk_x11_display_impl_class_init (GdkDisplayImplX11Class * klass)
{
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  display_class->get_display_name = gdk_x11_display_impl_get_display_name;
  display_class->get_n_screens = gdk_x11_display_impl_get_n_screens;
  display_class->get_screen = gdk_x11_display_impl_get_screen;
  display_class->get_default_screen = gdk_x11_display_impl_get_default_screen;
  G_OBJECT_CLASS (klass)->finalize = gdk_x11_display_impl_finalize;
  parent_class = g_type_class_peek_parent (klass);
}

static void
init_xinerama_support (GdkScreen * screen)
{
  GdkScreenImplX11 *screen_impl = GDK_SCREEN_IMPL_X11 (screen);
#ifdef HAS_XINERAMA
#ifdef HAS_SOLARIS_XINERAMA
  XRectangle monitors[MAXFRAMEBUFFERS];
  int opcode, firstevent, firsterror;
  char hints[16];
  gint result;
#endif
  if (XQueryExtension (GDK_SCREEN_XDISPLAY (screen), "XINERAMA",
		       &opcode, &firstevent, &firsterror))
    {
#ifdef HAS_SOLARIS_XINERAMA
      if (XineramaGetState (GDK_SCREEN_XDISPLAY (screen),
			    gdk_screen_get_number (screen)))
	{
	  result = XineramaGetInfo (GDK_SCREEN_XDISPLAY (screen),
				    gdk_screen_get_number (screen),
				    monitors, hints,
				    &screen_impl->num_monitors);
/*	  if (result != Success)
	    {
	      g_error ("error while retrieving Xinerama information");
	    }
	  else*/
	    {
#endif
#ifdef HAS_XFREE_XINERAMA
      if (XineramaIsActive (GDK_SCREEN_XDISPLAY (screen)))
	{
	  XineramaScreenInfo *monitors =
	    XineramaQueryScreens (GDK_SCREEN_XDISPLAY (screen),
				  &screen_impl->num_monitors);
	  if (screen_impl->num_monitors <= 0)
	    {
	      g_error ("error while retrieving Xinerama information");
	    }
	  else
	    {
#endif
	      int i;
	      screen_impl->use_virtual_screen = TRUE;
	      screen_impl->monitors = g_new0 (GdkRectangle,
					      screen_impl->
					      num_monitors);
	      for (i = 0; i < screen_impl->num_monitors; i++)
		{
#ifdef HAS_SOLARIS_XINERAMA
		  screen_impl->monitors[i].x = monitors[i].x;
		  screen_impl->monitors[i].y = monitors[i].y;
#endif
#ifdef HAS_XFREE_XINERAMA
		  screen_impl->monitors[i].x = monitors[i].x_org;
		  screen_impl->monitors[i].y = monitors[i].y_org;
#endif
		  screen_impl->monitors[i].width = monitors[i].width;
		  screen_impl->monitors[i].height = monitors[i].height;
		}
#ifdef HAS_XFREE_XINERAMA
	      XFree (monitors);
#endif
	    }
	}
      }
      else
#endif
    {
      screen_impl->use_virtual_screen = FALSE;
      screen_impl->num_monitors = 1;
      screen_impl->monitors = g_new0 (GdkRectangle, 1);
      screen_impl->monitors[0].x = 0;
      screen_impl->monitors[0].y = 0;
      screen_impl->monitors[0].width =
        (gint) WidthOfScreen (screen_impl->xscreen);
      screen_impl->monitors[0].height =
        (gint) HeightOfScreen (screen_impl->xscreen);
    }
}

GdkDisplay *_gdk_x11_display_impl_display_new (gchar * display_name)
{
  GdkDisplay *display;
  GdkDisplayImplX11 *display_impl;
  Screen *default_screen;
  GdkScreen *screen;
  GdkScreenImplX11 *screen_impl;
  int i = 0;
  int screen_num;
  display = g_object_new (gdk_x11_display_impl_get_type (), NULL);
  display_impl = GDK_DISPLAY_IMPL_X11 (display);

  display_impl->use_xft = -1;
  if ((display_impl->xdisplay =
       XOpenDisplay (display_name)) == NULL)
    {
      return NULL;
    }

  screen_num = ScreenCount (display_impl->xdisplay);
  default_screen = DefaultScreenOfDisplay (display_impl->xdisplay);
  /* populate the screen list and set default */
  for (i = 0; i < screen_num; i++)
    {
      screen = g_object_new (gdk_X11_screen_impl_get_type (), NULL);
      screen_impl = GDK_SCREEN_IMPL_X11 (screen);
      screen_impl->display = display;
      screen_impl->xdisplay = display_impl->xdisplay;
      screen_impl->xscreen =
        ScreenOfDisplay (display_impl->xdisplay, i);
      screen_impl->screen_num = i;
      screen_impl->xroot_window =
        (Window) RootWindow (display_impl->xdisplay, i);
      screen_impl->wmspec_check_window = None;
      screen_impl->visual_initialised = FALSE;
      screen_impl->colormap_initialised = FALSE;
      init_xinerama_support (screen);
      if (screen_impl->xscreen == default_screen)
        {
          display_impl->default_screen = screen;
          display_impl->leader_window =
            XCreateSimpleWindow (display_impl->xdisplay,
      			   screen_impl->xroot_window,
      			   10, 10, 10, 10, 0, 0, 0);
        }
      display_impl->screen_list =
        g_slist_append (display_impl->screen_list, screen);
    }


  display_impl->dnd_default_screen = display_impl->default_screen;
  return display;
}


static G_CONST_RETURN gchar
  *gdk_x11_display_impl_get_display_name (GdkDisplay * display)
{
  GdkDisplayImplX11 *display_impl;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  return (gchar *) DisplayString (display_impl->xdisplay);
}

static gint
  gdk_x11_display_impl_get_n_screens (GdkDisplay * display)
{
  GdkDisplayImplX11 *display_impl;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  return (gint) ScreenCount (display_impl->xdisplay);
}


static GdkScreen *gdk_x11_display_impl_get_screen (GdkDisplay *
      					     display,
      					     gint screen_num)
{
  Screen *desired_screen;
  GSList *tmp_list;
  GdkDisplayImplX11 *display_impl;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  g_return_val_if_fail (ScreenCount (display_impl->xdisplay) >
      		  screen_num, NULL);
  tmp_list = display_impl->screen_list;
  desired_screen =
    ScreenOfDisplay (display_impl->xdisplay, screen_num);
  while (tmp_list)
    {
      GdkScreenImplX11 *screen_impl = tmp_list->data;
      GdkScreen *screen = tmp_list->data;
      if (screen_impl->xscreen == desired_screen)
        {
          if (!screen_impl->visual_initialised)
            _gdk_visual_init (screen);
          if (!screen_impl->colormap_initialised)
            _gdk_windowing_window_init (screen);
          if (!screen_impl->xsettings_client)
            screen_impl->xsettings_client =
      	xsettings_client_new (screen_impl->xdisplay,
      			      screen_impl->screen_num,
      			      _gdk_xsettings_notify_cb,
      			      _gdk_xsettings_watch_cb, NULL);
          return screen;
        }
      tmp_list = tmp_list->next;
    }

  g_assert_not_reached ();
  return NULL;
}

static GdkScreen
  *gdk_x11_display_impl_get_default_screen (GdkDisplay * display)
{
  GdkDisplayImplX11 *display_impl;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  return display_impl->default_screen;
}

gboolean
  gdk_x11_display_is_root_window (GdkDisplay * display,
      			    Window xroot_window)
{
  GdkDisplayImplX11 *display_impl;
  GSList *tmp_list;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  tmp_list = display_impl->screen_list;
  while (tmp_list)
    {
      GdkScreenImplX11 *screen_impl = tmp_list->data;
      if (screen_impl->xroot_window == xroot_window)
        return TRUE;
      tmp_list = tmp_list->next;
    }

  return FALSE;
}

/**
 * gdk_display_set_use_xshm:
 * @display : a #GdkDisplay.
 * @use_xshm : TRUE if use of the MIT shared memory extension should be 
 * attempted.
 *
 * Sets whether the use of the MIT shared memory extension should 
 * be attempted. This function is mainly for internal use. 
 * It is only safe for an application to set this to FALSE,
 * since if it is set to TRUE and the server does not support 
 * the extension it may cause warning messages to be output.
 *
 */
void
  gdk_display_set_use_xshm (GdkDisplay * display, gboolean use_xshm)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm = use_xshm;
}

/**
 * gdk_display_get_use_xshm:
 * @display : a #GdkDisplay.
 *
 * Returns TRUE if GDK will attempt to use the MIT-SHM
 * shared memory extension.
 *
 * The shared memory extension is used for GdkImage, and
 * consequently for GdkRGB. It enables much faster drawing 
 * by communicating with the X server through SYSV shared
 * memory calls. However, it can only be used if the X client 
 * and server are on the same machine and the server supports it.
 * Returns : TRUE if use of the MIT shared memory extension will be attempted.
 */
gboolean gdk_display_get_use_xshm (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  return GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm;
}
/**
 * gdk_display_pointer_ungrab:
 * @display : a #GdkDisplay.
 * @time : a timestap (e.g. GDK_CURRENT_TIME).
 *
 * Release any pointer grab.
 */

void gdk_display_pointer_ungrab (GdkDisplay * display, guint32 time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  _gdk_input_ungrab_pointer (display, time);
  XUngrabPointer (GDK_DISPLAY_XDISPLAY (display), time);
  GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window = NULL;
}
/**
 * gdk_display_pointer_is_grabbed :
 * @display : a #GdkDisplay
 *
 * Test if the pointer is grabbed.
 *
 * Returns : TRUE if an active X pointer grab is in effect
 */

gboolean gdk_display_pointer_is_grabbed (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);
  return (GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window != NULL);
}

/**
 * gdk_display_keyboard_ungrab:
 * @display : a #GdkDisplay.
 * @time : a timestap (e.g #GDK_CURRENT_TIME).
 *
 * Release any keyboard grab
 */

void
  gdk_display_keyboard_ungrab (GdkDisplay * display, guint32 time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  XUngrabKeyboard (GDK_DISPLAY_XDISPLAY (display), time);
}
/**
 * gdk_display_beep:
 * @display: a #GdkDisplay
 *
 * Emits a short beep on @display
 *
 */
void gdk_display_beep (GdkDisplay * display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
}

/**
 * gdk_display_sync :
 * @display : a #GdkDisplay
 *
 * Flushes all the pending X calls
 */

void gdk_display_sync (GdkDisplay * display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
}

void gdk_x11_display_grab (GdkDisplay * display)
{
  GdkDisplayImplX11 *display_impl;
  g_return_if_fail (GDK_IS_DISPLAY (display));
  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  if (display_impl->grab_count == 0)
    XGrabServer (display_impl->xdisplay);
  ++display_impl->grab_count;
}

void gdk_x11_display_ungrab (GdkDisplay * display)
{
  GdkDisplayImplX11 *display_impl;
  g_return_if_fail (GDK_IS_DISPLAY (display));
  display_impl = GDK_DISPLAY_IMPL_X11 (display);;
  g_return_if_fail (display_impl->grab_count > 0);
  --display_impl->grab_count;
  if (display_impl->grab_count == 0)
    XUngrabServer (display_impl->xdisplay);
}

static void gdk_x11_display_impl_finalize (GObject * object)
{
  GdkDisplayImplX11 *display_impl = GDK_DISPLAY_IMPL_X11 (object);
  int i;
  GList *tmp;
  GSList *stmp;
  /* FIXME need to write GdkKeymap finalize fct 
     g_object_unref (display_impl->keymap);
   */
  /* Free motif Dnd */
  if (display_impl->motif_target_lists)
    {
      for (i = 0; i < display_impl->motif_n_target_lists; i++)
        g_list_free (display_impl->motif_target_lists[i]);
      g_free (display_impl->motif_target_lists);
    }

  /* Atom Hashtable */
  g_hash_table_destroy (display_impl->atom_from_virtual);
  g_hash_table_destroy (display_impl->atom_to_virtual);
  /* Leader Window */
  XDestroyWindow (display_impl->xdisplay,
      	    display_impl->leader_window);
  /* list of filters for client messages */
  g_list_free (display_impl->client_filters);
  /* X ID hashtable */
  g_hash_table_destroy (display_impl->xid_ht);
  /* input GdkDevice list */
  /* FIXME need to write finalize fct */
  for (tmp = display_impl->gdk_input_devices; tmp; tmp = tmp->next)
    g_object_unref (G_OBJECT (tmp->data));
  g_list_free (display_impl->gdk_input_devices);
  /* input GdkWindow list */
  for (tmp = display_impl->gdk_input_windows; tmp; tmp = tmp->next)
    g_object_unref (G_OBJECT (tmp->data));
  g_list_free (display_impl->gdk_input_windows);
  if (display_impl->gdk_input_gxid_host)
    g_free (display_impl->gdk_input_gxid_host);
  /* Free all GdkScreens */
  for (stmp = display_impl->screen_list; stmp; stmp = stmp->next)
    g_object_unref (G_OBJECT (stmp->data));
  g_slist_free (display_impl->screen_list);
  XCloseDisplay (display_impl->xdisplay);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}
