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
#ifndef __GTK_HSCROLLBAR_H__
#define __GTK_HSCROLLBAR_H__


#include <gdk/gdk.h>
#include <gtk/gtkscrollbar.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_HSCROLLBAR(obj)          GTK_CHECK_CAST (obj, gtk_hscrollbar_get_type (), GtkHScrollbar)
#define GTK_HSCROLLBAR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_hscrollbar_get_type (), GtkHScrollbarClass)
#define GTK_IS_HSCROLLBAR(obj)       GTK_CHECK_TYPE (obj, gtk_hscrollbar_get_type ())


typedef struct _GtkHScrollbar       GtkHScrollbar;
typedef struct _GtkHScrollbarClass  GtkHScrollbarClass;

struct _GtkHScrollbar
{
  GtkScrollbar scrollbar;
};

struct _GtkHScrollbarClass
{
  GtkScrollbarClass parent_class;
};


guint      gtk_hscrollbar_get_type (void);
GtkWidget* gtk_hscrollbar_new      (GtkAdjustment *adjustment);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_HSCROLLBAR_H__ */
