/*
 * gdkdisplaymgr.c
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

#include "gdkprivate.h"
#include "gdkdisplaymgr.h"

static void gdk_display_manager_class_init (GObjectClass * class);


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
	(GClassInitFunc) gdk_display_manager_class_init,
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

static void
gdk_display_manager_class_init (GObjectClass * class)
{

}


GdkDisplay *
gdk_display_manager_get_default_display (GdkDisplayManager * dpy_mgr)
{
  return GDK_DISPLAY_MANAGER_GET_CLASS (dpy_mgr)->
    get_default_display (dpy_mgr);
}

GdkDisplay *
gdk_display_manager_open_display (GdkDisplayManager * dpy_mgr,
				  gchar * display_name)
{
  return GDK_DISPLAY_MANAGER_GET_CLASS (dpy_mgr)->open_display (dpy_mgr,
								display_name);
}

gint
gdk_display_manager_get_display_count (GdkDisplayManager * dpy_mgr)
{
  return GDK_DISPLAY_MANAGER_GET_CLASS (dpy_mgr)->get_display_count (dpy_mgr);
}


GdkDisplay *
gdk_get_default_display (void)
{
  return GDK_DISPLAY_MANAGER_GET_CLASS (gdk_display_manager)->
    get_default_display (gdk_display_manager);
}

GdkScreen *
gdk_get_default_screen (void)
{
  GdkDisplay *dpy = gdk_get_default_display ();
  return GDK_DISPLAY_GET_CLASS (dpy)->get_default_screen (dpy);
}

GdkDisplayManager *
gdk_get_display_manager (void)
{
  return gdk_display_manager;
}
