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
#ifndef __GTK_VBOX_H__
#define __GTK_VBOX_H__


#include <gdk/gdk.h>
#include <gtk/gtkbox.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_VBOX(obj)          GTK_CHECK_CAST (obj, gtk_vbox_get_type (), GtkVBox)
#define GTK_VBOX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_vbox_get_type (), GtkVBoxClass)
#define GTK_IS_VBOX(obj)       GTK_CHECK_TYPE (obj, gtk_vbox_get_type ())


typedef struct _GtkVBox       GtkVBox;
typedef struct _GtkVBoxClass  GtkVBoxClass;

struct _GtkVBox
{
  GtkBox box;
};

struct _GtkVBoxClass
{
  GtkBoxClass parent_class;
};


guint      gtk_vbox_get_type (void);
GtkWidget* gtk_vbox_new      (gint homogeneous,
			      gint spacing);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_VBOX_H__ */
