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

#ifndef __GTK_VSCALE_H__
#define __GTK_VSCALE_H__


#include <gdk/gdk.h>
#include <gtk/gtkscale.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_VSCALE            (gtk_vscale_get_type ())
#define GTK_VSCALE(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_VSCALE, GtkVScale))
#define GTK_VSCALE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_VSCALE, GtkVScaleClass))
#define GTK_IS_VSCALE(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_VSCALE))
#define GTK_IS_VSCALE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VSCALE))


typedef struct _GtkVScale       GtkVScale;
typedef struct _GtkVScaleClass  GtkVScaleClass;

struct _GtkVScale
{
  GtkScale scale;
};

struct _GtkVScaleClass
{
  GtkScaleClass parent_class;
};


GtkType    gtk_vscale_get_type (void);
GtkWidget* gtk_vscale_new      (GtkAdjustment *adjustment);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_VSCALE_H__ */
