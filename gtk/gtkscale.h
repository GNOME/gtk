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
#ifndef __GTK_SCALE_H__
#define __GTK_SCALE_H__


#include <gdk/gdk.h>
#include <gtk/gtkrange.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_SCALE(obj)          GTK_CHECK_CAST (obj, gtk_scale_get_type (), GtkScale)
#define GTK_SCALE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_scale_get_type (), GtkScaleClass)
#define GTK_IS_SCALE(obj)       GTK_CHECK_TYPE (obj, gtk_scale_get_type ())


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


guint  gtk_scale_get_type       (void);
void   gtk_scale_set_digits     (GtkScale        *scale,
				 gint             digits);
void   gtk_scale_set_draw_value (GtkScale        *scale,
				 gint             draw_value);
void   gtk_scale_set_value_pos  (GtkScale        *scale,
				 GtkPositionType  pos);
gint   gtk_scale_value_width    (GtkScale        *scale);

void   gtk_scale_draw_value     (GtkScale        *scale);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SCALE_H__ */
