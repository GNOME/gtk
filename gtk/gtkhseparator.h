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
#ifndef __GTK_HSEPARATOR_H__
#define __GTK_HSEPARATOR_H__


#include <gdk/gdk.h>
#include <gtk/gtkseparator.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_HSEPARATOR(obj)          GTK_CHECK_CAST (obj, gtk_hseparator_get_type (), GtkHSeparator)
#define GTK_HSEPARATOR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_hseparator_get_type (), GtkHSeparatorClass)
#define GTK_IS_HSEPARATOR(obj)       GTK_CHECK_TYPE (obj, gtk_hseparator_get_type ())


typedef struct _GtkHSeparator       GtkHSeparator;
typedef struct _GtkHSeparatorClass  GtkHSeparatorClass;

struct _GtkHSeparator
{
  GtkSeparator separator;
};

struct _GtkHSeparatorClass
{
  GtkSeparatorClass parent_class;
};


guint      gtk_hseparator_get_type (void);
GtkWidget* gtk_hseparator_new      (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_HSEPARATOR_H__ */
