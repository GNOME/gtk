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
#ifndef __GTK_PANED_H__
#define __GTK_PANED_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_PANED(obj)          GTK_CHECK_CAST (obj, gtk_paned_get_type (), GtkPaned)
#define GTK_PANED_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_paned_get_type (), GtkPanedClass)
#define GTK_IS_PANED(obj)       GTK_CHECK_TYPE (obj, gtk_paned_get_type ())


typedef struct _GtkPaned       GtkPaned;
typedef struct _GtkPanedClass  GtkPanedClass;

struct _GtkPaned
{
  GtkContainer container;

  GtkWidget *child1;
  GtkWidget *child2;

  GdkWindow *handle;
  GdkCursor *cursor;
  GdkRectangle groove_rectangle;
  GdkGC *xor_gc;

  guint16 handle_size;
  guint16 gutter_size;
  
  gint child1_size;
  guint position_set : 1;
  guint in_drag : 1;

  gint16 handle_xpos;
  gint16 handle_ypos;
};

struct _GtkPanedClass
{
  GtkContainerClass parent_class;
};


guint gtk_paned_get_type   (void);
void gtk_paned_add1 (GtkPaned *paned, GtkWidget *child);
void gtk_paned_add2 (GtkPaned *paned, GtkWidget *child);
void gtk_paned_handle_size (GtkPaned *paned, guint16 size);
void gtk_paned_gutter_size (GtkPaned *paned, guint16 size);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PANED_H__ */
