/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
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


#define GTK_TYPE_PLUG            (gtk_plug_get_type ())
#define GTK_PLUG(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_PLUG, GtkPlug))
#define GTK_PLUG_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_PLUG, GtkPlugClass))
#define GTK_IS_PLUG(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_PLUG))
#define GTK_IS_PLUG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PLUG))
#define GTK_PLUG_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_PLUG, GtkPlugClass))


typedef struct _GtkPlug        GtkPlug;
typedef struct _GtkPlugClass   GtkPlugClass;


struct _GtkPlug
{
  GtkWindow window;

  GdkWindow *socket_window;
  GtkWidget *modality_window;
  GtkWindowGroup *modality_group;
  gboolean   same_app;
};

struct _GtkPlugClass
{
  GtkWindowClass parent_class;
};


GtkType    gtk_plug_get_type  (void) G_GNUC_CONST;
void       gtk_plug_construct (GtkPlug *plug, GdkNativeWindow socket_id);
GtkWidget* gtk_plug_new       (GdkNativeWindow socket_id);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PLUG_H__ */
