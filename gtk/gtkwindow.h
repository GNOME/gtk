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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_WINDOW_H__
#define __GTK_WINDOW_H__


#include <gdk/gdk.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
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

  GtkWidget *focus_widget;
  GtkWidget *default_widget;
  GtkWindow *transient_parent;

  gushort resize_count;
  guint allow_shrink : 1;
  guint allow_grow : 1;
  guint auto_shrink : 1;
  guint handling_resize : 1;
  guint position : 2;

  /* The following flag is initially TRUE when a window is mapped.
   * and will be set to FALSE after it is first positioned.
   * It is also temporarily reset when the window's size changes.
   * 
   * When TRUE, we move the window to the position the app set.
   */
  guint use_uposition : 1;
  guint modal : 1;

  /* Set if the window, or any descendent of it, has the focus
   */
  guint window_has_focus : 1;
  
  /* Set if !window_has_focus, but events are being sent to the
   * window because the pointer is in it. (Typically, no window
   * manager is running.
   */
  guint window_has_pointer_focus : 1;
};

struct _GtkWindowClass
{
  GtkBinClass parent_class;

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
void       gtk_window_set_policy               (GtkWindow           *window,
						gint                 allow_shrink,
						gint                 allow_grow,
						gint                 auto_shrink);
void       gtk_window_add_accel_group          (GtkWindow           *window,
						GtkAccelGroup	    *accel_group);
void       gtk_window_remove_accel_group       (GtkWindow           *window,
						GtkAccelGroup	    *accel_group);
void       gtk_window_set_position             (GtkWindow           *window,
						GtkWindowPosition    position);
gint	   gtk_window_activate_focus	       (GtkWindow           *window);
gint	   gtk_window_activate_default	       (GtkWindow           *window);

void       gtk_window_set_transient_for        (GtkWindow           *window, 
						GtkWindow           *parent);
void       gtk_window_set_geometry_hints       (GtkWindow           *window,
						GtkWidget           *geometry_widget,
						GdkGeometry         *geometry,
						GdkWindowHints       geom_mask);
/* The following differs from gtk_widget_set_usize, in that
 * gtk_widget_set_usize() overrides the requisition, so sets a minimum
 * size, while this only sets the size requested from the WM.
 */
void       gtk_window_set_default_size         (GtkWindow           *window,
						gint                 width,
						gint                 height);

/* If window is set modal, input will be grabbed when show and released when hide */
void       gtk_window_set_modal                (GtkWindow           *window,
                                                gboolean             modal);

/* --- internal functions --- */
void       gtk_window_set_focus                (GtkWindow           *window,
						GtkWidget           *focus);
void       gtk_window_set_default              (GtkWindow           *window,
						GtkWidget           *defaultw);
void       gtk_window_remove_embedded_xid      (GtkWindow           *window,
				                guint                xid);
void       gtk_window_add_embedded_xid         (GtkWindow           *window,
						guint                xid);
void       gtk_window_reposition               (GtkWindow           *window,
						gint                 x,
						gint                 y);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_WINDOW_H__ */
