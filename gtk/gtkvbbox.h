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
#ifndef __GTK_VBUTTON_BOX_H__
#define __GTK_VBUTTON_BOX_H__


#include <gtk/gtkbbox.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_VBUTTON_BOX(obj)          GTK_CHECK_CAST (obj, gtk_vbutton_box_get_type (), GtkVButtonBox)
#define GTK_VBUTTON_BOX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_vbutton_box_get_type (), GtkVButtonBoxClass)
#define GTK_IS_VBUTTON_BOX(obj)       GTK_CHECK_TYPE (obj, gtk_vbutton_box_get_type ())


typedef struct _GtkVButtonBox       GtkVButtonBox;
typedef struct _GtkVButtonBoxClass  GtkVButtonBoxClass;

struct _GtkVButtonBox
{
  GtkButtonBox button_box;
};

struct _GtkVButtonBoxClass
{
  GtkButtonBoxClass parent_class;
};


guint      gtk_vbutton_box_get_type (void);
GtkWidget *gtk_vbutton_box_new      (void);

/* buttons can be added by gtk_container_add() */

gint gtk_vbutton_box_get_spacing_default (void);
void gtk_vbutton_box_set_spacing_default (gint spacing);

GtkButtonBoxStyle gtk_vbutton_box_get_layout_default (void);
void gtk_vbutton_box_set_layout_default (GtkButtonBoxStyle layout);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_VBUTTON_BOX_H__ */
