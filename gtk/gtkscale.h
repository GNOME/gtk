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

#ifndef __GTK_SCALE_H__
#define __GTK_SCALE_H__


#include <gdk/gdk.h>
#include <gtk/gtkrange.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_SCALE            (gtk_scale_get_type ())
#define GTK_SCALE(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_SCALE, GtkScale))
#define GTK_SCALE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SCALE, GtkScaleClass))
#define GTK_IS_SCALE(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_SCALE))
#define GTK_IS_SCALE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SCALE))


typedef struct _GtkScale        GtkScale;
typedef struct _GtkScaleClass   GtkScaleClass;

struct _GtkScale
{
  GtkRange range;

  guint draw_value : 1;
  guint value_pos : 2;
};

struct _GtkScaleClass
{
  GtkRangeClass parent_class;

  gint slider_length;
  gint value_spacing;
  
  void (* draw_value) (GtkScale *scale);
};


GtkType gtk_scale_get_type        (void);
void    gtk_scale_set_digits      (GtkScale        *scale,
				   gint             digits);
void    gtk_scale_set_draw_value  (GtkScale        *scale,
				   gboolean         draw_value);
void    gtk_scale_set_value_pos   (GtkScale        *scale,
				   GtkPositionType  pos);
gint    gtk_scale_get_value_width (GtkScale        *scale);

void    gtk_scale_draw_value      (GtkScale        *scale);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SCALE_H__ */
