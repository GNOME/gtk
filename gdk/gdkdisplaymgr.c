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

GdkDisplay *
gdk_get_default_display (void)
{
  return gdk_display_manager_get_default_display (gdk_display_manager);
}

GdkScreen *
gdk_get_default_screen (void)
{
  GdkDisplay *dpy = gdk_get_default_display ();
  return gdk_display_get_default_screen (dpy);
}

GdkDisplayManager *
gdk_get_display_manager (void)
{
  return gdk_display_manager;
}
