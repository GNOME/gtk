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

#ifndef __GTK_BUTTON_H__
#define __GTK_BUTTON_H__


#include <gdk/gdk.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkenums.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_BUTTON			(gtk_button_get_type ())
#define GTK_BUTTON(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_BUTTON, GtkButton))
#define GTK_BUTTON_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_BUTTON, GtkButtonClass))
#define GTK_IS_BUTTON(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_BUTTON))
#define GTK_IS_BUTTON_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BUTTON))


typedef struct _GtkButton       GtkButton;
typedef struct _GtkButtonClass  GtkButtonClass;

struct _GtkButton
{
  GtkBin bin;

  GtkWidget *child /* deprecapted field,
		    * use GTK_BIN (button)->child instead
		    */;

  guint in_button : 1;
  guint button_down : 1;
  guint relief : 2;
};

struct _GtkButtonClass
{
  GtkBinClass        parent_class;
  
  void (* pressed)  (GtkButton *button);
  void (* released) (GtkButton *button);
  void (* clicked)  (GtkButton *button);
  void (* enter)    (GtkButton *button);
  void (* leave)    (GtkButton *button);
};


GtkType        gtk_button_get_type       (void);
GtkWidget*     gtk_button_new            (void);
GtkWidget*     gtk_button_new_with_label (const gchar *label);
void           gtk_button_pressed        (GtkButton *button);
void           gtk_button_released       (GtkButton *button);
void           gtk_button_clicked        (GtkButton *button);
void           gtk_button_enter          (GtkButton *button);
void           gtk_button_leave          (GtkButton *button);
void           gtk_button_set_relief     (GtkButton *button,
					  GtkReliefStyle newstyle);
GtkReliefStyle gtk_button_get_relief      (GtkButton *button);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_BUTTON_H__ */
