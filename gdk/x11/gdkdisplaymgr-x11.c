/*
 * gdkdisplaymgr-x11.c
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

#include <glib.h>
#include "gdkdisplaymgr.h"
#include "gdkdisplaymgr-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate.h"
#include "gdkscreen-x11.h"

static void gdk_display_manager_set_default_display (GdkDisplayManager *dpy_mgr,
						     GdkDisplay        *default_display);

GType
gdk_display_manager_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
	sizeof (GdkDisplayManagerClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) NULL,
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (GdkDisplayManager),
	0,			/* n_preallocs */
	(GInstanceInitFunc) NULL,
      };
      object_type = g_type_register_static (G_TYPE_OBJECT,
					    "GdkDisplayManager",
					    &object_info, 0);
    }
  return object_type;
}

GdkDisplay *
gdk_display_manager_open_display (GdkDisplayManager *dpy_mgr,
				  gchar             *display_name)
{
  GdkDisplay *dpy;
  dpy = _gdk_x11_display_impl_display_new (display_name);
  if (!dpy)
    return NULL;

  if (dpy_mgr->open_displays == NULL)
    gdk_display_manager_set_default_display (dpy_mgr, dpy);

  dpy_mgr->open_displays = g_slist_append (dpy_mgr->open_displays, dpy);
  return dpy;
}


static void
gdk_display_manager_set_default_display (GdkDisplayManager * dpy_mgr,
					 GdkDisplay * default_display)
{
  dpy_mgr->default_display = default_display;
}

GdkDisplay *
gdk_display_manager_get_default_display (GdkDisplayManager * dpy_mgr)
{
  g_assert (dpy_mgr->default_display != NULL);
  return dpy_mgr->default_display;
}

gint
gdk_display_manager_get_display_count (GdkDisplayManager * dpy_mgr)
{
  gint i = 0;
  GSList *tmp = dpy_mgr->open_displays;

  while (tmp != NULL)
    {
      tmp = g_slist_next (tmp);
      i++;
    }
  return i;
}

GdkScreen *
gdk_x11_display_manager_get_screen_for_root (GdkDisplayManager * dpy_mgr,
					     Window root)
{
  GdkDisplayImplX11 *tmp_dpy;
  GdkScreenImplX11 *tmp_scr;
  GSList *tmp_dpy_list = dpy_mgr->open_displays;
  GSList *tmp_scr_list;
  g_assert (tmp_dpy_list != NULL);
  while (tmp_dpy_list != NULL)
    {
      tmp_dpy = (GdkDisplayImplX11 *) tmp_dpy_list->data;
      tmp_scr_list = tmp_dpy->screen_list;
      g_assert (tmp_scr_list != NULL);
      while (tmp_scr_list != NULL)
	{
	  tmp_scr = (GdkScreenImplX11 *) tmp_scr_list->data;
	  if (tmp_scr->xroot_window == root)
	    return (GdkScreen *) tmp_scr;
	  tmp_scr_list = g_slist_next (tmp_scr_list);
	}
      tmp_dpy_list = g_slist_next (tmp_dpy_list);
    }
  return NULL;
}

GdkDisplay *
gdk_x11_display_manager_get_display (GdkDisplayManager * dpy_mgr,
				     Display * dpy)
{
  GdkDisplayImplX11 *tmp_dpy;
  GSList *tmp_dpy_list = dpy_mgr->open_displays;
  g_assert (tmp_dpy_list != NULL);
  while (tmp_dpy_list != NULL)
    {
      tmp_dpy = (GdkDisplayImplX11 *) tmp_dpy_list->data;
      if (tmp_dpy->xdisplay == dpy)
	return (GdkDisplay *) tmp_dpy;
      tmp_dpy_list = g_slist_next (tmp_dpy_list);
    }
  return NULL;
}

/* Help functions & Macros */


GdkDisplayImplX11 *
gdk_lookup_xdisplay (Display * dpy)
{
  return
    GDK_DISPLAY_IMPL_X11 (gdk_x11_display_manager_get_display
			  (gdk_display_manager, dpy));
}
