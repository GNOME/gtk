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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_SEPARATOR_H__
#define __GTK_SEPARATOR_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_SEPARATOR                  (gtk_separator_get_type ())
#define GTK_SEPARATOR(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_SEPARATOR, GtkSeparator))
#define GTK_SEPARATOR_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SEPARATOR, GtkSeparatorClass))
#define GTK_IS_SEPARATOR(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_SEPARATOR))
#define GTK_IS_SEPARATOR_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SEPARATOR))


typedef struct _GtkSeparator       GtkSeparator;
typedef struct _GtkSeparatorClass  GtkSeparatorClass;

struct _GtkSeparator
{
  GtkWidget widget;
};

struct _GtkSeparatorClass
{
  GtkWidgetClass parent_class;
};


GtkType  gtk_separator_get_type (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_SEPARATOR_H__ */
