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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_DRAW_WINDOW_H__
#define __GTK_DRAW_WINDOW_H__


#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_DRAW_WINDOW                  (gtk_draw_window_get_type ())
#define GTK_DRAW_WINDOW(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_DRAW_WINDOW, GtkDrawWindow))
#define GTK_DRAW_WINDOW_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_DRAW_WINDOW, GtkDrawWindowClass))
#define GTK_IS_DRAW_WINDOW(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_DRAW_WINDOW))
#define GTK_IS_DRAW_WINDOW_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_DRAW_WINDOW))


typedef struct _GtkDrawWindow        GtkDrawWindow;
typedef struct _GtkDrawWindowClass   GtkDrawWindowClass;
typedef struct _GtkDrawWindowButton  GtkDrawWindowButton;


struct _GtkDrawWindow
{
  GtkWindow window;
};

struct _GtkDrawWindowClass
{
  GtkWindowClass parent_class;
};


GtkType    gtk_draw_window_get_type (void);
GtkWidget* gtk_draw_window_new      (GtkWindowType        type);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_DRAW_WINDOW_H__ */
