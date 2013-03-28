/*
 * gdkdisplay-broadway.h
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_BROADWAY_DISPLAY__
#define __GDK_BROADWAY_DISPLAY__

#include "gdkdisplayprivate.h"
#include "gdkkeys.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"
#include "gdkbroadway-server.h"

G_BEGIN_DECLS

typedef struct _GdkBroadwayDisplay GdkBroadwayDisplay;
typedef struct _GdkBroadwayDisplayClass GdkBroadwayDisplayClass;

#define GDK_TYPE_BROADWAY_DISPLAY              (gdk_broadway_display_get_type())
#define GDK_BROADWAY_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_BROADWAY_DISPLAY, GdkBroadwayDisplay))
#define GDK_BROADWAY_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_BROADWAY_DISPLAY, GdkBroadwayDisplayClass))
#define GDK_IS_BROADWAY_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_BROADWAY_DISPLAY))
#define GDK_IS_BROADWAY_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_BROADWAY_DISPLAY))
#define GDK_BROADWAY_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_BROADWAY_DISPLAY, GdkBroadwayDisplayClass))

struct _GdkBroadwayDisplay
{
  GdkDisplay parent_instance;
  GdkScreen *default_screen;
  GdkScreen **screens;

  GHashTable *id_ht;
  GList *toplevels;

  GSource *event_source;

  /* Keyboard related information */
  GdkKeymap *keymap;

  /* drag and drop information */
  GdkDragContext *current_dest_drag;

  /* Input device */
  /* input GdkDevice list */
  GList *input_devices;

  /* The offscreen window that has the pointer in it (if any) */
  GdkWindow *active_offscreen_window;

  GdkBroadwayServer *server;

  gpointer move_resize_data;
};

struct _GdkBroadwayDisplayClass
{
  GdkDisplayClass parent_class;
};

GType      gdk_broadway_display_get_type            (void);

G_END_DECLS

#endif				/* __GDK_BROADWAY_DISPLAY__ */
