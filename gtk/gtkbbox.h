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

#ifndef __GTK_BUTTON_BOX_H__
#define __GTK_BUTTON_BOX_H__

#include <gtk/gtkbox.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
  

#define	GTK_TYPE_BUTTON_BOX		(gtk_button_box_get_type ())
#define GTK_BUTTON_BOX(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_BUTTON_BOX, GtkButtonBox))
#define GTK_BUTTON_BOX_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_BUTTON_BOX, GtkButtonBoxClass))
#define GTK_IS_BUTTON_BOX(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_BUTTON_BOX))
#define GTK_IS_BUTTON_BOX_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BUTTON_BOX))
  

#define GTK_BUTTONBOX_DEFAULT -1
 
typedef struct _GtkButtonBox       GtkButtonBox;
typedef struct _GtkButtonBoxClass  GtkButtonBoxClass;

struct _GtkButtonBox
{
  GtkBox box;
  gint spacing;
  gint child_min_width;
  gint child_min_height;
  gint child_ipad_x;
  gint child_ipad_y;
  GtkButtonBoxStyle layout_style;
};

struct _GtkButtonBoxClass
{
  GtkBoxClass parent_class;
};


GtkType gtk_button_box_get_type (void);

void gtk_button_box_get_child_size_default (gint *min_width, gint *min_height);
void gtk_button_box_get_child_ipadding_default (gint *ipad_x, gint *ipad_y);

void gtk_button_box_set_child_size_default (gint min_width, gint min_height);
void gtk_button_box_set_child_ipadding_default (gint ipad_x, gint ipad_y);

gint gtk_button_box_get_spacing (GtkButtonBox *widget);
GtkButtonBoxStyle gtk_button_box_get_layout (GtkButtonBox *widget);
void gtk_button_box_get_child_size (GtkButtonBox *widget,
				    gint *min_width, gint *min_height);
void gtk_button_box_get_child_ipadding (GtkButtonBox *widget, gint *ipad_x, gint *ipad_y);

void gtk_button_box_set_spacing (GtkButtonBox *widget, gint spacing);
void gtk_button_box_set_layout (GtkButtonBox *widget, 
				GtkButtonBoxStyle layout_style);
void gtk_button_box_set_child_size (GtkButtonBox *widget,
				    gint min_width, gint min_height);
void gtk_button_box_set_child_ipadding (GtkButtonBox *widget, gint ipad_x, gint ipad_y);


/* Internal method - do not use. */
void gtk_button_box_child_requisition (GtkWidget *widget,
				       int *nvis_children,
				       int *width,
				       int *height);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_BUTTON_BOX_H__ */


