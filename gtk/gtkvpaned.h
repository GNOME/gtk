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
#ifndef __GTK_VPANED_H__
#define __GTK_VPANED_H__


#include <gdk/gdk.h>
#include <gtk/gtkpaned.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_VPANED(obj)          GTK_CHECK_CAST (obj, gtk_vpaned_get_type (), GtkVPaned)
#define GTK_VPANED_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_vpaned_get_type (), GtkVPanedClass)
#define GTK_IS_VPANED(obj)       GTK_CHECK_TYPE (obj, gtk_vpaned_get_type ())


typedef struct _GtkVPaned       GtkVPaned;
typedef struct _GtkVPanedClass  GtkVPanedClass;

struct _GtkVPaned
{
  GtkPaned paned;
};

struct _GtkVPanedClass
{
  GtkPanedClass parent_class;
};


guint      gtk_vpaned_get_type (void);
GtkWidget* gtk_vpaned_new      (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_VPANED_H__ */
