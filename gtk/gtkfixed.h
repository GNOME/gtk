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
#ifndef __GTK_FIXED_H__
#define __GTK_FIXED_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_FIXED(obj)          GTK_CHECK_CAST (obj, gtk_fixed_get_type (), GtkFixed)
#define GTK_FIXED_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_fixed_get_type (), GtkFixedClass)
#define GTK_IS_FIXED(obj)       GTK_CHECK_TYPE (obj, gtk_fixed_get_type ())


typedef struct _GtkFixed        GtkFixed;
typedef struct _GtkFixedClass   GtkFixedClass;
typedef struct _GtkFixedChild   GtkFixedChild;

struct _GtkFixed
{
  GtkContainer container;

  GList *children;
};

struct _GtkFixedClass
{
  GtkContainerClass parent_class;
};

struct _GtkFixedChild
{
  GtkWidget *widget;
  gint16 x;
  gint16 y;
};


guint      gtk_fixed_get_type          (void);
GtkWidget* gtk_fixed_new               (void);
void       gtk_fixed_put               (GtkFixed       *fixed,
                                        GtkWidget      *widget,
                                        gint16         x,
                                        gint16         y);
void       gtk_fixed_move              (GtkFixed       *fixed,
                                        GtkWidget      *widget,
                                        gint16         x,
                                        gint16         y);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_FIXED_H__ */
