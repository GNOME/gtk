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

#ifndef __GTK_RULER_H__
#define __GTK_RULER_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_RULER            (gtk_ruler_get_type ())
#define GTK_RULER(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_RULER, GtkRuler))
#define GTK_RULER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_RULER, GtkRulerClass))
#define GTK_IS_RULER(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_RULER))
#define GTK_IS_RULER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_RULER))

typedef struct _GtkRuler        GtkRuler;
typedef struct _GtkRulerClass   GtkRulerClass;
typedef struct _GtkRulerMetric  GtkRulerMetric;

/* All distances below are in 1/72nd's of an inch. (According to
 * Adobe that's a point, but points are really 1/72.27 in.)
 */
struct _GtkRuler
{
  GtkWidget widget;

  GdkPixmap *backing_store;
  GdkGC *non_gr_exp_gc;
  GtkRulerMetric *metric;
  gint xsrc, ysrc;
  gint slider_size;

  /* The upper limit of the ruler (in points) */
  gfloat lower;
  /* The lower limit of the ruler */
  gfloat upper;
  /* The position of the mark on the ruler */
  gfloat position;
  /* The maximum size of the ruler */
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
  /* This should be points_per_unit. This is the size of the unit
   * in 1/72nd's of an inch and has nothing to do with screen pixels */
  gfloat pixels_per_unit;
  gfloat ruler_scale[10];
  gint subdivide[5];        /* five possible modes of subdivision */
};


GtkType gtk_ruler_get_type   (void);
void    gtk_ruler_set_metric (GtkRuler       *ruler,
			      GtkMetricType   metric);
void    gtk_ruler_set_range  (GtkRuler       *ruler,
			      gfloat          lower,
			      gfloat          upper,
			      gfloat          position,
			      gfloat          max_size);
void    gtk_ruler_draw_ticks (GtkRuler       *ruler);
void    gtk_ruler_draw_pos   (GtkRuler       *ruler);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_RULER_H__ */
