/*
 * gdkdisplay.c
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
#include "gdkdisplay.h"
#include "gdkdisplaymgr.h"
#include "gdkinternals.h"


static void gdk_display_class_init (GdkDisplayClass * class);

GType
gdk_display_get_type (void)
{

  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
	sizeof (GdkDisplayClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gdk_display_class_init,
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (GdkDisplay),
	0,			/* n_preallocs */
	(GInstanceInitFunc) NULL,
      };
      object_type = g_type_register_static (G_TYPE_OBJECT,
					    "GdkDisplay", &object_info, 0);
    }

  return object_type;
}


static void
gdk_display_class_init (GdkDisplayClass * class)
{
/*   class->parent_class = g_type_class_peek_parent (class);*/
}
GdkDisplay *
gdk_display_new (gchar * display_name)
{
  return GDK_DISPLAY_MANAGER_GET_CLASS (gdk_display_manager)->
    open_display (gdk_display_manager, display_name);
}

gchar *
gdk_display_get_name (GdkDisplay * dpy)
{
  return GDK_DISPLAY_GET_CLASS (dpy)->get_display_name (dpy);
}

gint
gdk_display_get_n_screens (GdkDisplay * dpy)
{
  return GDK_DISPLAY_GET_CLASS (dpy)->get_n_screens (dpy);
}

GdkScreen *
gdk_display_get_screen (GdkDisplay * dpy, gint screen_num)
{
  return GDK_DISPLAY_GET_CLASS (dpy)->get_screen (dpy, screen_num);
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay * dpy)
{
  return GDK_DISPLAY_GET_CLASS (dpy)->get_default_screen (dpy);
}
