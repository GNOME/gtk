/*
 * gdkdisplaymgr-x11.h
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

#ifndef __GDK_DISPLAY_MANAGER_H__
#define __GDK_DISPLAY_MANAGER_H__

#include <X11/Xlib.h>
#include "gdkdisplay-x11.h"

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

void               gdk_x11_display_manager_class_init          (GdkDisplayManagerClass *class);
GdkDisplay *       gdk_x11_display_manager_open_display        (GdkDisplayManager      *dpy_mgr,
								gchar                  *display_name);
void               gdk_x11_display_manager_set_default         (GdkDisplayManager      *dpy_mgr,
								GdkDisplay             *default_display);
GdkDisplay *       gdk_x11_display_manager_get_default         (GdkDisplayManager      *dpy_mgr);
gint               gdk_x11_display_manager_get_display_count   (GdkDisplayManager      *dpy_mgr);
GdkScreen *        gdk_x11_display_manager_get_screen_for_root (GdkDisplayManager      *dpy_mgr,
								Window                  root);
GdkDisplay *       gdk_x11_display_manager_get_display         (GdkDisplayManager      *dpy_mgr,
								Display                *dpy);
GdkDisplayImplX11 *gdk_lookup_xdisplay                         (Display                *dpy);


#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif /*__GDK_DISPLAY_MANAGER__*/
