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
#ifndef __GTK_BUTTON_BOX_H__
#define __GTK_BUTTON_BOX_H__

#include <gtk/gtkbox.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
  

#define GTK_BUTTON_BOX(obj)          GTK_CHECK_CAST (obj, gtk_button_box_get_type (), GtkButtonBox)
#define GTK_BUTTON_BOX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_button_box_get_type (), GtkButtonBoxClass)
#define GTK_IS_BUTTON_BOX(obj)       GTK_CHECK_TYPE (obj, gtk_button_box_get_type ())
  

#define GTK_BUTTONBOX_DEFAULT -1
 
typedef enum {
  GTK_BUTTONBOX_DEFAULT_STYLE,
  GTK_BUTTONBOX_SPREAD,
  GTK_BUTTONBOX_EDGE,
  GTK_BUTTONBOX_START,
  GTK_BUTTONBOX_END
} GtkButtonBoxStyle;

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


guint gtk_button_box_get_type (void);

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


