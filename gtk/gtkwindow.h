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
#ifndef __GTK_WINDOW_H__
#define __GTK_WINDOW_H__


#include <gdk/gdk.h>
#include <gtk/gtkaccelerator.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define GTK_TYPE_WINDOW			(gtk_window_get_type ())
#define GTK_WINDOW(obj)			(GTK_CHECK_CAST (obj, GTK_TYPE_WINDOW, GtkWindow))
#define GTK_WINDOW_CLASS(klass)		(GTK_CHECK_CLASS_CAST (klass, GTK_TYPE_WINDOW, GtkWindowClass))
#define GTK_IS_WINDOW(obj)		(GTK_CHECK_TYPE (obj, GTK_TYPE_WINDOW))
#define GTK_IS_WINDOW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE (klass, GTK_TYPE_WINDOW))


typedef struct _GtkWindow       GtkWindow;
typedef struct _GtkWindowClass  GtkWindowClass;

struct _GtkWindow
{
  GtkBin bin;

  gchar *title;
  gchar *wmclass_name;
  gchar *wmclass_class;
  GtkWindowType type;
  GList *accelerator_tables;

  GtkWidget *focus_widget;
  GtkWidget *default_widget;

  gushort resize_count;
  guint need_resize : 1;
  guint allow_shrink : 1;
  guint allow_grow : 1;
  guint auto_shrink : 1;
  guint handling_resize : 1;
  guint position : 2;
  guint use_uposition : 1;
};

struct _GtkWindowClass
{
  GtkBinClass parent_class;

  gint (* move_resize) (GtkWindow *window,
			gint      *x,
			gint      *y,
			gint       width,
			gint       height);
  void (* set_focus)   (GtkWindow *window,
			GtkWidget *focus);
};


GtkType    gtk_window_get_type                 (void);
GtkWidget* gtk_window_new                      (GtkWindowType        type);
void       gtk_window_set_title                (GtkWindow           *window,
						const gchar         *title);
void       gtk_window_set_wmclass              (GtkWindow           *window,
						const gchar         *wmclass_name,
						const gchar         *wmclass_class);
void       gtk_window_set_focus                (GtkWindow           *window,
						GtkWidget           *focus);
void       gtk_window_set_default              (GtkWindow           *window,
						GtkWidget           *defaultw);
void       gtk_window_set_policy               (GtkWindow           *window,
						gint                 allow_shrink,
						gint                 allow_grow,
						gint                 auto_shrink);
void       gtk_window_add_accelerator_table    (GtkWindow           *window,
						GtkAcceleratorTable *table);
void       gtk_window_remove_accelerator_table (GtkWindow           *window,
						GtkAcceleratorTable *table);
void       gtk_window_position                 (GtkWindow           *window,
						GtkWindowPosition    position);
gint	   gtk_window_activate_focus	       (GtkWindow           *window);
gint	   gtk_window_activate_default	       (GtkWindow           *window);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_WINDOW_H__ */
