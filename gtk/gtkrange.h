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

#ifndef __GTK_RANGE_H__
#define __GTK_RANGE_H__


#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_RANGE            (gtk_range_get_type ())
#define GTK_RANGE(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_RANGE, GtkRange))
#define GTK_RANGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_RANGE, GtkRangeClass))
#define GTK_IS_RANGE(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_RANGE))
#define GTK_IS_RANGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_RANGE))


typedef struct _GtkRange        GtkRange;
typedef struct _GtkRangeClass   GtkRangeClass;

struct _GtkRange
{
  GtkWidget widget;

  GdkWindow *trough;
  GdkWindow *slider;
  GdkWindow *step_forw;
  GdkWindow *step_back;

  gint16 x_click_point;
  gint16 y_click_point;

  guint8 button;
  gint8 digits;
  guint policy : 2;
  guint scroll_type : 3;
  guint in_child : 3;
  guint click_child : 3;
  guint need_timer : 1;

  guint32 timer;

  gfloat old_value;
  gfloat old_lower;
  gfloat old_upper;
  gfloat old_page_size;

  GtkAdjustment *adjustment;
};

struct _GtkRangeClass
{
  GtkWidgetClass parent_class;

  gint slider_width;
  gint stepper_size;
  gint stepper_slider_spacing;
  gint min_slider_size;

  guint8 trough;
  guint8 slider;
  guint8 step_forw;
  guint8 step_back;

  void (* draw_background)  (GtkRange *range);
  void (* clear_background) (GtkRange *range);
  void (* draw_trough)     (GtkRange *range);
  void (* draw_slider)     (GtkRange *range);
  void (* draw_step_forw)  (GtkRange *range);
  void (* draw_step_back)  (GtkRange *range);
  void (* slider_update)   (GtkRange *range);
  gint (* trough_click)    (GtkRange *range,
			    gint      x,
			    gint      y,
			    gfloat   *jump_perc);
  gint (* trough_keys)     (GtkRange *range,
			    GdkEventKey *key,
			    GtkScrollType *scroll,
			    GtkTroughType *trough);
  void (* motion)          (GtkRange *range,
			    gint      xdelta,
			    gint      ydelta);
  gint (* timer)           (GtkRange *range);
};


GtkType        gtk_range_get_type               (void);
GtkAdjustment* gtk_range_get_adjustment         (GtkRange      *range);
void           gtk_range_set_update_policy      (GtkRange      *range,
						 GtkUpdateType  policy);
void           gtk_range_set_adjustment         (GtkRange      *range,
						 GtkAdjustment *adjustment);

void           gtk_range_draw_background        (GtkRange      *range);
void           gtk_range_clear_background        (GtkRange      *range);
void           gtk_range_draw_trough            (GtkRange      *range);
void           gtk_range_draw_slider            (GtkRange      *range);
void           gtk_range_draw_step_forw         (GtkRange      *range);
void           gtk_range_draw_step_back         (GtkRange      *range);
void           gtk_range_slider_update          (GtkRange      *range);
gint           gtk_range_trough_click           (GtkRange      *range,
						 gint           x,
						 gint           y,
						 gfloat	       *jump_perc);

void           gtk_range_default_hslider_update (GtkRange      *range);
void           gtk_range_default_vslider_update (GtkRange      *range);
gint           gtk_range_default_htrough_click  (GtkRange      *range,
						 gint           x,
						 gint           y,
						 gfloat	       *jump_perc);
gint           gtk_range_default_vtrough_click  (GtkRange      *range,
						 gint           x,
						 gint           y,
						 gfloat	       *jump_perc);
void           gtk_range_default_hmotion        (GtkRange      *range,
						 gint           xdelta,
						 gint           ydelta);
void           gtk_range_default_vmotion        (GtkRange      *range,
						 gint           xdelta,
						 gint           ydelta);

void _gtk_range_get_props (GtkRange *range,
			   gint     *slider_width,
			   gint     *trough_border,
			   gint     *stepper_size,
			   gint     *stepper_spacing);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_RANGE_H__ */
