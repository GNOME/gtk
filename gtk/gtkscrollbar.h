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
#ifndef __GTK_SCROLLBAR_H__
#define __GTK_SCROLLBAR_H__


#include <gdk/gdk.h>
#include <gtk/gtkrange.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_SCROLLBAR(obj)          GTK_CHECK_CAST (obj, gtk_scrollbar_get_type (), GtkScrollbar)
#define GTK_SCROLLBAR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_scrollbar_get_type (), GtkScrollbarClass)
#define GTK_IS_SCROLLBAR(obj)       GTK_CHECK_TYPE (obj, gtk_scrollbar_get_type ())


typedef struct _GtkScrollbar        GtkScrollbar;
typedef struct _GtkScrollbarClass   GtkScrollbarClass;

struct _GtkScrollbar
{
  GtkRange range;
};

struct _GtkScrollbarClass
{
  GtkRangeClass parent_class;
};


guint  gtk_scrollbar_get_type (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SCROLLBAR_H__ */
