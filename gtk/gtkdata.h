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
#ifndef __GTK_DATA_H__
#define __GTK_DATA_H__


#include <gdk/gdk.h>
#include <gtk/gtkobject.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define GTK_TYPE_DATA		 (gtk_data_get_type ())
#define GTK_DATA(obj)		 (GTK_CHECK_CAST ((obj), GTK_TYPE_DATA, GtkData))
#define GTK_DATA_CLASS(klass)	 (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_DATA, GtkDataClass))
#define GTK_IS_DATA(obj)	 (GTK_CHECK_TYPE ((obj), GTK_TYPE_DATA))
#define GTK_IS_DATA_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_DATA))



typedef struct _GtkData       GtkData;
typedef struct _GtkDataClass  GtkDataClass;

struct _GtkData
{
  GtkObject object;
};

struct _GtkDataClass
{
  GtkObjectClass parent_class;

  void (* disconnect) (GtkData *data);
};


GtkType gtk_data_get_type (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_DATA_H__ */
