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

void gdk_x11_display_impl_class_init (GdkDisplayImplX11Class * class);

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

void
gdk_x11_display_impl_class_init (GdkDisplayImplX11Class * class)
{
  GdkDisplayClass *dpy_class = GDK_DISPLAY_CLASS (class);
  dpy_class->new_display = gdk_x11_display_impl_display_new;
  dpy_class->get_display_name = gdk_x11_display_impl_get_display_name;
  dpy_class->get_n_screens = gdk_x11_display_impl_get_n_screens;
  dpy_class->get_screen = gdk_x11_display_impl_get_screen;
  dpy_class->get_default_screen = gdk_x11_display_impl_get_default_screen;
}




GdkDisplay *
gdk_x11_display_impl_display_new (gchar * display_name)
{
  GdkDisplay *dpy;
  GdkDisplayImplX11 *dpy_impl;
  Screen *default_scr;
  GdkScreen *scr;
  GdkScreenImplX11 *scr_impl;

  int i = 0;
  int scr_num;

  dpy = g_object_new (gdk_x11_display_impl_get_type (), NULL);
  dpy_impl = GDK_DISPLAY_IMPL_X11 (dpy);

  if ((dpy_impl->xdisplay = XOpenDisplay (display_name)) == NULL)
    {
      return NULL;
    }

  scr_num = ScreenCount (dpy_impl->xdisplay);
  default_scr = DefaultScreenOfDisplay (dpy_impl->xdisplay);
  /* populate the screen list and set default */
  for (i = 0; i < scr_num; i++)
    {
      scr = g_object_new (gdk_X11_screen_impl_get_type (), NULL);
      scr_impl = GDK_SCREEN_IMPL_X11 (scr);
      scr_impl->display = dpy;
      scr_impl->xdisplay = dpy_impl->xdisplay;
      scr_impl->xscreen = ScreenOfDisplay (dpy_impl->xdisplay, i);
      scr_impl->scr_num = i;
      scr_impl->xroot_window = (Window) RootWindow (dpy_impl->xdisplay, i);
      scr_impl->wmspec_check_window = None;
      scr_impl->leader_window = XCreateSimpleWindow (dpy_impl->xdisplay,
						     scr_impl->xroot_window,
						     10, 10, 10, 10, 0, 0, 0);
      scr_impl->visual_initialised = FALSE;
      scr_impl->colormap_initialised = FALSE;
      if (scr_impl->xscreen == default_scr)
	dpy_impl->default_screen = scr;
      dpy_impl->screen_list = g_slist_append (dpy_impl->screen_list, scr);
    }
  dpy_impl->dnd_default_screen = dpy_impl->default_screen;
  return dpy;
}


gchar *
gdk_x11_display_impl_get_display_name (GdkDisplay * dpy)
{
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11 (dpy);
  return (gchar *) DisplayString (dpy_impl->xdisplay);
}

gint
gdk_x11_display_impl_get_n_screens (GdkDisplay * dpy)
{
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11 (dpy);
  return (gint) ScreenCount (dpy_impl->xdisplay);
}


GdkScreen *
gdk_x11_display_impl_get_screen (GdkDisplay * dpy, gint screen_num)
{
  Screen *desired_scr;
  GSList *tmp;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11 (dpy);
  g_assert (ScreenCount (dpy_impl->xdisplay) > screen_num);
  tmp = dpy_impl->screen_list;
  g_assert (tmp != NULL);
  desired_scr = ScreenOfDisplay (dpy_impl->xdisplay, screen_num);
  while (tmp)
    {
      if ((((GdkScreenImplX11 *) tmp->data)->xscreen) == desired_scr)
	{
	  GdkScreenImplX11 *desired_screen = (GdkScreenImplX11 *) tmp->data;
	  if (!desired_screen->visual_initialised)
	    _gdk_visual_init ((GdkScreen *) tmp->data);
	  if (!desired_screen->colormap_initialised)
	    _gdk_windowing_window_init ((GdkScreen *) tmp->data);
	  return (GdkScreen *) tmp->data;
	}
      tmp = g_slist_next (tmp);
    }
  g_error ("Internal screen list is corrupted");
  exit (1);
}


GdkScreen *
gdk_x11_display_impl_get_default_screen (GdkDisplay * dpy)
{
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11 (dpy);
  return dpy_impl->default_screen;
}

gboolean
gdk_x11_display_impl_is_root_window (GdkDisplay * dpy, Window xroot_window)
{
  GSList *tmp;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11 (dpy);
  tmp = dpy_impl->screen_list;
  g_assert (tmp != NULL);
  while (tmp)
    {
      if ((((GdkScreenImplX11 *) tmp->data)->xroot_window) == xroot_window)
	return TRUE;
      tmp = g_slist_next (tmp);
    }
  return FALSE;
}

void
gdk_display_set_use_xshm (GdkDisplay * display, gboolean use_xshm)
{
  GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm = use_xshm;
}

gboolean
gdk_display_get_use_xshm (GdkDisplay * display)
{
  return GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm;
}


void
gdk_display_pointer_ungrab (GdkDisplay * display, guint32 time)
{
  _gdk_input_ungrab_pointer (time);

  XUngrabPointer (GDK_DISPLAY_XDISPLAY (display), time);
  GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window = NULL;
}


gboolean
gdk_display_pointer_is_grabbed (GdkDisplay * display)
{
  return (GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window != NULL);
}

void
gdk_display_keyboard_ungrab (GdkDisplay * display, guint32 time)
{
  XUngrabKeyboard (GDK_DISPLAY_XDISPLAY (display), time);
}

void
gdk_display_beep (GdkDisplay * display)
{
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
}

void
gdk_display_sync (GdkDisplay * display)
{
  XSync (GDK_DISPLAY_IMPL_X11 (display)->xdisplay, False);
}
