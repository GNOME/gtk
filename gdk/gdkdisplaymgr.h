/*
 * gdkdisplaymgr.h
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

#ifndef __GDK_DISPLAY_MANAGER__
#define __GDK_DISPLAY_MANAGER__

#include "gdkdisplay.h"

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GDK_TYPE_DISPLAY_MANAGER           (gdk_display_manager_get_type())
#define GDK_DISPLAY_MANAGER(object)        (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_MGR, GdkDisplayManager))
#define GDK_DISPLAY_MANAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY_MGR, GdkDisplayManagerClass))
#define GDK_IS_DISPLAY_MGR(object)         (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY_MGR))
#define GDK_IS_DISPLAY_MGR_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY_MGR))
#define GDK_DISPLAY_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY_MGR, GdkDisplayManagerClass))

typedef struct _GdkDisplayManager GdkDisplayManager;
typedef struct _GdkDisplayManagerClass GdkDisplayManagerClass;

GType              gdk_display_manager_get_type            (void);

GdkDisplay *       gdk_display_manager_get_default_display (GdkDisplayManager *display_mgr);
GdkDisplay *       gdk_display_manager_open_display        (GdkDisplayManager *display_mgr,
							    gchar             *display_name);
gint               gdk_display_manager_get_display_count   (GdkDisplayManager *display_mgr);
GdkDisplay *       gdk_get_default_display                 (void);
GdkScreen *        gdk_get_default_screen                  (void);
GdkDisplayManager *gdk_get_display_manager                 (void);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* __GDK_DISPLAY_MANAGER__ */
