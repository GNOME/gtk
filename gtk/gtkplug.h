/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_PLUG_H__
#define __GTK_PLUG_H__


#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_PLUG(obj)          GTK_CHECK_CAST (obj, gtk_plug_get_type (), GtkPlug)
#define GTK_PLUG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_plug_get_type (), GtkPlugClass)
#define GTK_IS_PLUG(obj)       GTK_CHECK_TYPE (obj, gtk_plug_get_type ())


typedef struct _GtkPlug        GtkPlug;
typedef struct _GtkPlugClass   GtkPlugClass;


struct _GtkPlug
{
  GtkWindow window;

  GdkWindow *socket_window;
  gint same_app;
};

struct _GtkPlugClass
{
  GtkWindowClass parent_class;
};


guint      gtk_plug_get_type  (void);
void       gtk_plug_construct (GtkPlug *plug, guint32 socket_id);
GtkWidget* gtk_plug_new       (guint32 socket_id);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PLUG_H__ */
