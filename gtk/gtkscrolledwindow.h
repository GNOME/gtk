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

#ifndef __GTK_SCROLLED_WINDOW_H__
#define __GTK_SCROLLED_WINDOW_H__


#include <gdk/gdk.h>
#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkviewport.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_SCROLLED_WINDOW            (gtk_scrolled_window_get_type ())
#define GTK_SCROLLED_WINDOW(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_SCROLLED_WINDOW, GtkScrolledWindow))
#define GTK_SCROLLED_WINDOW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SCROLLED_WINDOW, GtkScrolledWindowClass))
#define GTK_IS_SCROLLED_WINDOW(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_SCROLLED_WINDOW))
#define GTK_IS_SCROLLED_WINDOW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SCROLLED_WINDOW))


typedef struct _GtkScrolledWindow       GtkScrolledWindow;
typedef struct _GtkScrolledWindowClass  GtkScrolledWindowClass;

struct _GtkScrolledWindow
{
  GtkBin container;

  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;

  guint hscrollbar_policy      : 2;
  guint vscrollbar_policy      : 2;
  guint hscrollbar_visible     : 1;
  guint vscrollbar_visible     : 1;
  guint window_placement       : 2;
};

struct _GtkScrolledWindowClass
{
  GtkBinClass parent_class;
  
  gint scrollbar_spacing;
};


GtkType        gtk_scrolled_window_get_type          (void);
GtkWidget*     gtk_scrolled_window_new               (GtkAdjustment     *hadjustment,
						      GtkAdjustment     *vadjustment);
void           gtk_scrolled_window_set_hadjustment   (GtkScrolledWindow *scrolled_window,
						      GtkAdjustment     *hadjustment);
void           gtk_scrolled_window_set_vadjustment   (GtkScrolledWindow *scrolled_window,
						      GtkAdjustment     *hadjustment);
GtkAdjustment* gtk_scrolled_window_get_hadjustment   (GtkScrolledWindow *scrolled_window);
GtkAdjustment* gtk_scrolled_window_get_vadjustment   (GtkScrolledWindow *scrolled_window);
void           gtk_scrolled_window_set_policy        (GtkScrolledWindow *scrolled_window,
						      GtkPolicyType      hscrollbar_policy,
						      GtkPolicyType      vscrollbar_policy);
void           gtk_scrolled_window_set_placement     (GtkScrolledWindow *scrolled_window,
						      GtkCornerType      window_placement);
void	       gtk_scrolled_window_add_with_viewport (GtkScrolledWindow *scrolled_window,
						      GtkWidget		*child);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SCROLLED_WINDOW_H__ */
