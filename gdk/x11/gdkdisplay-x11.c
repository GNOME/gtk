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

static void       gdk_x11_display_impl_class_init         (GdkDisplayImplX11Class *class);
static gint       gdk_x11_display_impl_get_n_screens      (GdkDisplay             *display);
static GdkScreen *gdk_x11_display_impl_get_screen         (GdkDisplay             *display,
							   gint                    screen_num);
static char *     gdk_x11_display_impl_get_display_name   (GdkDisplay             *display);
static GdkScreen *gdk_x11_display_impl_get_default_screen (GdkDisplay             *display);

GType
gdk_x11_display_impl_get_type ()
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
	sizeof (GdkDisplayImplX11Class),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
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
gdk_x11_display_impl_class_init (GdkDisplayImplX11Class * class)
{
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);
  
  display_class->get_display_name = gdk_x11_display_impl_get_display_name;
  display_class->get_n_screens = gdk_x11_display_impl_get_n_screens;
  display_class->get_screen = gdk_x11_display_impl_get_screen;
  display_class->get_default_screen = gdk_x11_display_impl_get_default_screen;
}

GdkDisplay *
_gdk_x11_display_impl_display_new (gchar * display_name)
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

  if ((display_impl->xdisplay = XOpenDisplay (display_name)) == NULL)
    {
      return NULL;
    }

  screen_num = ScreenCount (display_impl->xdisplay);
  default_screen = DefaultScreenOfDisplay (display_impl->xdisplay);
  /* populate the screen list and set default */
  for (i = 0; i < screen_num; i++)
    {
      screen= g_object_new (gdk_X11_screen_impl_get_type (), NULL);
      screen_impl = GDK_SCREEN_IMPL_X11 (screen);
      screen_impl->display = display;
      screen_impl->xdisplay = display_impl->xdisplay;
      screen_impl->xscreen = ScreenOfDisplay (display_impl->xdisplay, i);
      screen_impl->screen_num = i;
      screen_impl->xroot_window = (Window) RootWindow (display_impl->xdisplay, i);
      screen_impl->wmspec_check_window = None;
      screen_impl->visual_initialised = FALSE;
      screen_impl->colormap_initialised = FALSE;
      if (screen_impl->xscreen == default_screen)
	{
	  display_impl->default_screen = screen;
	  display_impl->leader_window = XCreateSimpleWindow (display_impl->xdisplay,
							     screen_impl->xroot_window,
							     10, 10, 10, 10, 0, 0, 0);
	}
      display_impl->screen_list = g_slist_append (display_impl->screen_list, screen);
    }
  display_impl->dnd_default_screen = display_impl->default_screen;
  return display;
}


static gchar *
gdk_x11_display_impl_get_display_name (GdkDisplay * display)
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


static GdkScreen *
gdk_x11_display_impl_get_screen (GdkDisplay * display, 
				 gint         screen_num)
{
  Screen *desired_screen;
  GSList *tmp_list;
  GdkDisplayImplX11 *display_impl;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  g_return_val_if_fail (ScreenCount (display_impl->xdisplay) > screen_num, NULL);

  tmp_list = display_impl->screen_list;
  desired_screen= ScreenOfDisplay (display_impl->xdisplay, screen_num);
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
	  return screen;
	}
      tmp_list = tmp_list->next;
    }

  g_assert_not_reached ();
  return NULL;
}

static GdkScreen *
gdk_x11_display_impl_get_default_screen (GdkDisplay * display)
{
  GdkDisplayImplX11 *display_impl;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  return display_impl->default_screen;
}

gboolean
gdk_x11_display_is_root_window (GdkDisplay *display, 
				Window      xroot_window)
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

void
gdk_display_set_use_xshm (GdkDisplay * display, gboolean use_xshm)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm = use_xshm;
}

gboolean
gdk_display_get_use_xshm (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  return GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm;
}

void
gdk_display_pointer_ungrab (GdkDisplay * display, guint32 time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  _gdk_input_ungrab_pointer (display, time);

  XUngrabPointer (GDK_DISPLAY_XDISPLAY (display), time);
  GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window = NULL;
}

gboolean
gdk_display_pointer_is_grabbed (GdkDisplay * display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);
  return (GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window != NULL);
}

void
gdk_display_keyboard_ungrab (GdkDisplay * display, guint32 time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  XUngrabKeyboard (GDK_DISPLAY_XDISPLAY (display), time);
}

void
gdk_display_beep (GdkDisplay * display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
}

void
gdk_display_sync (GdkDisplay * display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  XSync (GDK_DISPLAY_XDISPLAY (display), False);
}

void
gdk_x11_display_grab (GdkDisplay *display)
{ 
  GdkDisplayImplX11 *display_impl;
  g_return_if_fail (GDK_IS_DISPLAY (display));
  display_impl = GDK_DISPLAY_IMPL_X11 (display);
  
  if (display_impl->grab_count == 0)
    XGrabServer (display_impl->xdisplay);
  ++display_impl->grab_count;
}

void
gdk_x11_display_ungrab (GdkDisplay *display)
{
  GdkDisplayImplX11 *display_impl;
  g_return_if_fail (GDK_IS_DISPLAY (display));
  display_impl = GDK_DISPLAY_IMPL_X11 (display);;
  
  g_return_if_fail (display_impl->grab_count > 0);
  
  --display_impl->grab_count;
  if (display_impl->grab_count == 0)
    XUngrabServer (display_impl->xdisplay);
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
  _gdk_events_init (display);
  _gdk_input_init (display);
  gdk_dnd_init (display);
  
  return display;
}
