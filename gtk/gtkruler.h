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
#ifndef __GTK_RULER_H__
#define __GTK_RULER_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_RULER(obj)          GTK_CHECK_CAST (obj, gtk_ruler_get_type (), GtkRuler)
#define GTK_RULER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_ruler_get_type (), GtkRulerClass)
#define GTK_IS_RULER(obj)       GTK_CHECK_TYPE (obj, gtk_ruler_get_type ())


typedef struct _GtkRuler        GtkRuler;
typedef struct _GtkRulerClass   GtkRulerClass;
typedef struct _GtkRulerMetric  GtkRulerMetric;

struct _GtkRuler
{
  GtkWidget widget;

  GdkPixmap *backing_store;
  GdkGC *non_gr_exp_gc;
  GtkRulerMetric *metric;
  gint xsrc, ysrc;
  gint slider_size;

  gfloat lower;
  gfloat upper;
  gfloat position;
  gfloat max_size;
};

struct _GtkRulerClass
{
  GtkWidgetClass parent_class;

  void (* draw_ticks) (GtkRuler *ruler);
  void (* draw_pos)   (GtkRuler *ruler);
};

struct _GtkRulerMetric
{
  gchar *metric_name;
  gchar *abbrev;
  gfloat pixels_per_unit;
  gfloat ruler_scale[10];
  gint subdivide[5];        /* five possible modes of subdivision */
};


guint gtk_ruler_get_type   (void);
void  gtk_ruler_set_metric (GtkRuler       *ruler,
			    GtkMetricType   metric);
void  gtk_ruler_set_range  (GtkRuler       *ruler,
			    gfloat          lower,
			    gfloat          upper,
			    gfloat          position,
			    gfloat          max_size);
void  gtk_ruler_draw_ticks (GtkRuler       *ruler);
void  gtk_ruler_draw_pos   (GtkRuler       *ruler);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_RULER_H__ */
