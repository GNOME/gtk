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
#ifndef __GTK_VSEPARATOR_H__
#define __GTK_VSEPARATOR_H__


#include <gdk/gdk.h>
#include <gtk/gtkseparator.h>


#define GTK_VSEPARATOR(obj)          GTK_CHECK_CAST (obj, gtk_vseparator_get_type (), GtkVSeparator)
#define GTK_VSEPARATOR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_vseparator_get_type (), GtkVSeparatorClass)
#define GTK_IS_VSEPARATOR(obj)       GTK_CHECK_TYPE (obj, gtk_vseparator_get_type ())


typedef struct _GtkVSeparator       GtkVSeparator;
typedef struct _GtkVSeparatorClass  GtkVSeparatorClass;

struct _GtkVSeparator
{
  GtkSeparator separator;
};

struct _GtkVSeparatorClass
{
  GtkSeparatorClass parent_class;
};


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


guint      gtk_vseparator_get_type (void);
GtkWidget* gtk_vseparator_new      (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SEPARATOR_H__ */
