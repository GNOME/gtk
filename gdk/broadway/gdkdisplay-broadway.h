/*
 * gdkdisplay-x11.h
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

#ifndef __GDK_DISPLAY_X11__
#define __GDK_DISPLAY_X11__

#include <gdk/gdkdisplay.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkinternals.h>
#include <gdk/gdkmain.h>

G_BEGIN_DECLS

typedef struct _GdkDisplayX11 GdkDisplayX11;
typedef struct _GdkDisplayX11Class GdkDisplayX11Class;

#define GDK_TYPE_DISPLAY_X11              (_gdk_display_x11_get_type())
#define GDK_DISPLAY_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_X11, GdkDisplayX11))
#define GDK_DISPLAY_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY_X11, GdkDisplayX11Class))
#define GDK_IS_DISPLAY_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY_X11))
#define GDK_IS_DISPLAY_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY_X11))
#define GDK_DISPLAY_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY_X11, GdkDisplayX11Class))

struct _GdkDisplayX11
{
  GdkDisplay parent_instance;
  GdkScreen *default_screen;
  GdkScreen **screens;

  GSource *event_source;

  gint grab_count;

  /* Keyboard related information */
  GdkKeymap *keymap;

  /* drag and drop information */
  GdkDragContext *current_dest_drag;

  /* Mapping to/from virtual atoms */

  GHashTable *atom_from_virtual;
  GHashTable *atom_to_virtual;

  /* Input device */
  /* input GdkDevice list */
  GList *input_devices;

  /* Time of most recent user interaction. */
  gulong user_time;

  /* The offscreen window that has the pointer in it (if any) */
  GdkWindow *active_offscreen_window;
};

struct _GdkDisplayX11Class
{
  GdkDisplayClass parent_class;
};

GType      _gdk_display_x11_get_type            (void);

G_END_DECLS

#endif				/* __GDK_DISPLAY_X11__ */
