/*
 * gdkscreen-x11.h
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

#ifndef __GDK_SCREEN_X11_H__
#define __GDK_SCREEN_X11_H__


#include <gdk/gdkscreen.h>
#include <gdk/gdkvisual.h>
#include <X11/X.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS
  
typedef struct _GdkScreenImplX11 GdkScreenImplX11;
typedef struct _GdkScreenImplX11Class GdkScreenImplX11Class;

typedef struct _GdkVisualPrivate       GdkVisualPrivate;

#define GDK_TYPE_SCREEN_IMPL_X11              (gdk_X11_screen_impl_get_type ())
#define GDK_SCREEN_IMPL_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SCREEN_IMPL_X11, GdkScreenImplX11))
#define GDK_SCREEN_IMPL_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SCREEN_IMPL_X11, GdkScreenImplX11Class))
#define GDK_IS_SCREEN_IMPL_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SCREEN_IMPL_X11))
#define GDK_IS_SCREEN_IMPL_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SCREEN_IMPL_X11))
#define GDK_SCREEN_IMPL_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SCREEN_IMPL_X11, GdkScreenImplX11Class))



struct _GdkScreenImplX11
{
  GdkScreen parent_instance;
  GdkDisplay *display;
  Display *xdisplay;
  Screen *xscreen;
  gint screen_num;
  Window xroot_window;
  GdkWindow *root_window;
  Window wmspec_check_window;
  /* Visual Part */
  gboolean visual_initialised;
  GdkVisualPrivate *system_visual;
  GdkVisualPrivate **visuals;
  gint nvisuals;
  gint available_depths[7];
  gint navailable_depths;
  GdkVisualType available_types[6];
  gint navailable_types;
  GHashTable *visual_hash;
  /* Colormap Part */
  gboolean colormap_initialised;
  GdkColormap *default_colormap;
  GdkColormap *system_colormap;
};
  
struct _GdkScreenImplX11Class
{
  GdkScreenClass parent_class;
};


GType gdk_X11_screen_impl_get_type ();


#define DEFAULT_X_SCREEN    GDK_SCREEN_IMPL_X11(gdk_get_default_screen())->xscreen
#define DEFAULT_GDK_SCREEN_IMPL_X11_FOR_DISPLAY(display)  GDK_SCREEN_IMPL_X11(GDK_DISPLAY_GET_CLASS(display)->get_default_screen(display))
#define GDK_SCREEN_XDISPLAY(screen)  (GDK_SCREEN_IMPL_X11(screen)->xdisplay)
#define GDK_SCREEN_XROOTWIN(screen)  (GDK_SCREEN_IMPL_X11(screen)->xroot_window)
#define GDK_SCREEN_DISPLAY(screen)   (GDK_SCREEN_IMPL_X11(screen)->display)
G_END_DECLS

#endif				/* _GDK_SCREEN_X11_H__ */
