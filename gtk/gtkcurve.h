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

#ifndef __GTK_CURVE_H__
#define __GTK_CURVE_H__


#include <gdk/gdk.h>
#include <gtk/gtkdrawingarea.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_CURVE                  (gtk_curve_get_type ())
#define GTK_CURVE(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_CURVE, GtkCurve))
#define GTK_CURVE_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CURVE, GtkCurveClass))
#define GTK_IS_CURVE(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_CURVE))
#define GTK_IS_CURVE_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CURVE))


typedef struct _GtkCurve	GtkCurve;
typedef struct _GtkCurveClass	GtkCurveClass;


struct _GtkCurve
{
  GtkDrawingArea graph;

  gint cursor_type;
  gfloat min_x;
  gfloat max_x;
  gfloat min_y;
  gfloat max_y;
  GdkPixmap *pixmap;
  GtkCurveType curve_type;
  gint height;                  /* (cached) graph height in pixels */
  gint grab_point;              /* point currently grabbed */
  gint last;

  /* (cached) curve points: */
  gint num_points;
  GdkPoint *point;

  /* control points: */
  gint num_ctlpoints;           /* number of control points */
  gfloat (*ctlpoint)[2];        /* array of control points */
};

struct _GtkCurveClass
{
  GtkDrawingAreaClass parent_class;

  void (* curve_type_changed) (GtkCurve *curve);
};


GtkType		gtk_curve_get_type	(void);
GtkWidget*	gtk_curve_new		(void);
void		gtk_curve_reset		(GtkCurve *curve);
void		gtk_curve_set_gamma	(GtkCurve *curve, gfloat gamma);
void		gtk_curve_set_range	(GtkCurve *curve,
					 gfloat min_x, gfloat max_x,
					 gfloat min_y, gfloat max_y);
void		gtk_curve_get_vector	(GtkCurve *curve,
					 int veclen, gfloat vector[]);
void		gtk_curve_set_vector	(GtkCurve *curve,
					 int veclen, gfloat vector[]);
void		gtk_curve_set_curve_type (GtkCurve *curve, GtkCurveType type);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_CURVE_H__ */
