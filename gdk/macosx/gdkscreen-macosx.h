/*
 * gdkscreen-macosx.h
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

#ifndef __GDK_SCREEN_MACOSX_H__
#define __GDK_SCREEN_MACOSX_H__

#include "gdkprivate-macosx.h"
//#include "xsettings-client.h"
#include <gdk/gdkscreen.h>
#include <gdk/gdkvisual.h>

G_BEGIN_DECLS
  
typedef struct _GdkScreenMacOSX GdkScreenMacOSX;
typedef struct _GdkScreenMacOSXClass GdkScreenMacOSXClass;

#define GDK_TYPE_SCREEN_MACOSX              (_gdk_screen_macosx_get_type ())
#define GDK_SCREEN_MACOSX(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SCREEN_MACOSX, GdkScreenMacOSX))
#define GDK_SCREEN_MACOSX_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SCREEN_MACOSX, GdkScreenMacOSXClass))
#define GDK_IS_SCREEN_MACOSX(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SCREEN_MACOSX))
#define GDK_IS_SCREEN_MACOSX_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SCREEN_MACOSX))
#define GDK_SCREEN_MACOSX_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SCREEN_MACOSX, GdkScreenMacOSXClass))

@class NSEvent;
@class NSScreen;

struct _GdkScreenMacOSX
{
	GdkScreen parent_instance;
	
	GdkDisplay *display;
	GdkWindow *root_window;
	GdkColormap *default_colormap;
};
  
struct _GdkScreenMacOSXClass
{
  GdkScreenClass parent_class;
};

GType       _gdk_screen_macosx_get_type (void);
GdkScreen * _gdk_macosx_screen_new      (GdkDisplay *display,
				      NSScreen	  *screen);

void _gdk_macosx_screen_size_changed           (GdkScreen *screen,
												NSEvent    *event);

G_END_DECLS

#endif /* __GDK_SCREEN_MacOSX_H__ */
