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
#include "gdkinternals.h"
#include "gdkscreen-x11.h"

static void gdk_display_manager_set_default_display (GdkDisplayManager *display_mgr,
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
gdk_display_manager_open_display (GdkDisplayManager *display_mgr,
				  gchar             *display_name)
{
  GdkDisplay *display;
  g_return_val_if_fail (GDK_IS_DISPLAY_MANAGER (display_mgr), NULL);
  display= _gdk_x11_display_impl_display_new (display_name);
  if (!display)
    return NULL;

  if (display_mgr->open_displays == NULL)
    gdk_display_manager_set_default_display (display_mgr, display);

  display_mgr->open_displays = g_slist_append (display_mgr->open_displays, display);
  return display;
}


static void
gdk_display_manager_set_default_display (GdkDisplayManager * display_mgr,
					 GdkDisplay * default_display)
{
  g_return_if_fail (GDK_IS_DISPLAY_MANAGER (display_mgr));
  g_return_if_fail (GDK_IS_DISPLAY (default_display));
  display_mgr->default_display = default_display;
}

GdkDisplay *
gdk_display_manager_get_default_display (GdkDisplayManager * display_mgr)
{
  g_return_val_if_fail (GDK_IS_DISPLAY_MANAGER (display_mgr), NULL);
  g_assert (display_mgr->default_display != NULL);
  return display_mgr->default_display;
}

gint
gdk_display_manager_get_display_count (GdkDisplayManager * display_mgr)
{
  gint i = 0;
  GSList *tmp;
  g_return_val_if_fail (GDK_IS_DISPLAY_MANAGER (display_mgr), 0);
  
  tmp = display_mgr->open_displays;

  while (tmp != NULL)
    {
      tmp = g_slist_next (tmp);
      i++;
    }
  return i;
}

GdkScreen *
gdk_x11_display_manager_get_screen_for_root (GdkDisplayManager * display_mgr,
					     Window root)
{
  GdkDisplayImplX11 *tmp_display;
  GdkScreenImplX11 *tmp_screen;
  GSList *tmp_display_list;
  GSList *tmp_screen_list;

  g_return_val_if_fail (GDK_IS_DISPLAY_MANAGER (display_mgr), NULL);
  
  tmp_display_list = display_mgr->open_displays;
  g_assert (tmp_display_list != NULL);
  
  while (tmp_display_list != NULL)
    {
      tmp_display= (GdkDisplayImplX11 *) tmp_display_list->data;
      tmp_screen_list = tmp_display->screen_list;
      g_assert (tmp_screen_list != NULL);
      while (tmp_screen_list != NULL)
	{
	  tmp_screen= (GdkScreenImplX11 *) tmp_screen_list->data;
	  if (tmp_screen->xroot_window == root)
	    return (GdkScreen *) tmp_screen;
	  tmp_screen_list = g_slist_next (tmp_screen_list);
	}
      tmp_display_list = g_slist_next (tmp_display_list);
    }
  return NULL;
}

GdkDisplay *
gdk_x11_display_manager_get_display (GdkDisplayManager * display_mgr,
				     Display * display)
{
  GdkDisplayImplX11 *tmp_display;
  GSList *tmp_display_list; 
  
  g_return_val_if_fail (GDK_IS_DISPLAY_MANAGER (display_mgr), NULL);

  tmp_display_list = display_mgr->open_displays;
  g_assert (tmp_display_list != NULL);
  while (tmp_display_list != NULL)
    {
      tmp_display= (GdkDisplayImplX11 *) tmp_display_list->data;
      if (tmp_display->xdisplay == display)
	return (GdkDisplay *) tmp_display;
      tmp_display_list = g_slist_next (tmp_display_list);
    }
  return NULL;
}

GdkDisplayImplX11 *
gdk_lookup_xdisplay (Display * display)
{
  return
    GDK_DISPLAY_IMPL_X11 (gdk_x11_display_manager_get_display (_gdk_display_manager, 
							       display));
}
GSList *
gdk_x11_display_manager_get_open_displays (GdkDisplayManager  *display_mgr)
{
  return _gdk_display_manager->open_displays;
}
