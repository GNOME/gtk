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

#include <stdio.h>
#include "gtkmain.h"
#include "gtkrange.h"
#include "gtksignal.h"


#define SCROLL_TIMER_LENGTH  20
#define SCROLL_INITIAL_DELAY 250  /* must hold button this long before ... */
#define SCROLL_LATER_DELAY   100  /* ... it starts repeating at this rate  */
#define SCROLL_DELAY_LENGTH  300

#define RANGE_CLASS(w)  GTK_RANGE_CLASS (GTK_OBJECT (w)->klass)

enum {
  ARG_0,
  ARG_UPDATE_POLICY
};

static void gtk_range_class_init               (GtkRangeClass    *klass);
static void gtk_range_init                     (GtkRange         *range);
static void gtk_range_set_arg		       (GtkObject        *object,
						GtkArg           *arg,
						guint             arg_id);
static void gtk_range_get_arg		       (GtkObject        *object,
						GtkArg           *arg,
						guint             arg_id);
static void gtk_range_destroy                  (GtkObject        *object);
static void gtk_range_finalize                 (GtkObject        *object);
static void gtk_range_draw                     (GtkWidget        *widget,
						GdkRectangle     *area);
static void gtk_range_draw_focus               (GtkWidget        *widget);
static void gtk_range_unrealize                (GtkWidget        *widget);
static gint gtk_range_expose                   (GtkWidget        *widget,
						GdkEventExpose   *event);
static gint gtk_range_button_press             (GtkWidget        *widget,
						GdkEventButton   *event);
static gint gtk_range_button_release           (GtkWidget        *widget,
						GdkEventButton   *event);
static gint gtk_range_motion_notify            (GtkWidget        *widget,
						GdkEventMotion   *event);
static gint gtk_range_key_press                (GtkWidget         *widget,
						GdkEventKey       *event);
static gint gtk_range_enter_notify             (GtkWidget        *widget,
						GdkEventCrossing *event);
static gint gtk_range_leave_notify             (GtkWidget        *widget,
						GdkEventCrossing *event);
static gint gtk_range_focus_in                 (GtkWidget        *widget,
						GdkEventFocus    *event);
static gint gtk_range_focus_out                (GtkWidget        *widget,
						GdkEventFocus    *event);
static void gtk_range_style_set                 (GtkWidget       *widget,
						 GtkStyle        *previous_style);

static void gtk_real_range_draw_trough         (GtkRange         *range);
static void gtk_real_range_draw_slider         (GtkRange         *range);
static gint gtk_real_range_timer               (GtkRange         *range);
static gint gtk_range_scroll                   (GtkRange         *range,
						gfloat            jump_perc);

static void gtk_range_add_timer                (GtkRange         *range);
static void gtk_range_remove_timer             (GtkRange         *range);

static void gtk_range_adjustment_changed       (GtkAdjustment    *adjustment,
						gpointer          data);
static void gtk_range_adjustment_value_changed (GtkAdjustment    *adjustment,
						gpointer          data);

static void gtk_range_trough_hdims             (GtkRange         *range,
						gint             *left,
						gint             *right);
static void gtk_range_trough_vdims             (GtkRange         *range,
						gint             *top,
						gint             *bottom);

static GtkWidgetClass *parent_class = NULL;


GtkType
gtk_range_get_type (void)
{
  static GtkType range_type = 0;

  if (!range_type)
    {
      static const GtkTypeInfo range_info =
      {
	"GtkRange",
	sizeof (GtkRange),
	sizeof (GtkRangeClass),
	(GtkClassInitFunc) gtk_range_class_init,
	(GtkObjectInitFunc) gtk_range_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      range_type = gtk_type_unique (GTK_TYPE_WIDGET, &range_info);
    }

  return range_type;
}

static void
gtk_range_class_init (GtkRangeClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_WIDGET);

  gtk_object_add_arg_type ("GtkRange::update_policy",
			   GTK_TYPE_UPDATE_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_UPDATE_POLICY);

  object_class->set_arg = gtk_range_set_arg;
  object_class->get_arg = gtk_range_get_arg;
  object_class->destroy = gtk_range_destroy;
  object_class->finalize = gtk_range_finalize;

  widget_class->draw = gtk_range_draw;
  widget_class->draw_focus = gtk_range_draw_focus;
  widget_class->unrealize = gtk_range_unrealize;
  widget_class->expose_event = gtk_range_expose;
  widget_class->button_press_event = gtk_range_button_press;
  widget_class->button_release_event = gtk_range_button_release;
  widget_class->motion_notify_event = gtk_range_motion_notify;
  widget_class->key_press_event = gtk_range_key_press;
  widget_class->enter_notify_event = gtk_range_enter_notify;
  widget_class->leave_notify_event = gtk_range_leave_notify;
  widget_class->focus_in_event = gtk_range_focus_in;
  widget_class->focus_out_event = gtk_range_focus_out;
  widget_class->style_set = gtk_range_style_set;

  class->slider_width = 11;
  class->stepper_size = 11;
  class->stepper_slider_spacing = 1;
  class->min_slider_size = 7;
  class->trough = 1;
  class->slider = 2;
  class->step_forw = 3;
  class->step_back = 4;
  class->draw_background = NULL;
  class->clear_background = NULL;
  class->draw_trough = gtk_real_range_draw_trough;
  class->draw_slider = gtk_real_range_draw_slider;
  class->draw_step_forw = NULL;
  class->draw_step_back = NULL;
  class->trough_click = NULL;
  class->trough_keys = NULL;
  class->motion = NULL;
  class->timer = gtk_real_range_timer;
}

static void
gtk_range_set_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkRange *range;

  range = GTK_RANGE (object);

  switch (arg_id)
    {
    case ARG_UPDATE_POLICY:
      gtk_range_set_update_policy (range, GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_range_get_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkRange *range;

  range = GTK_RANGE (object);

  switch (arg_id)
    {
    case ARG_UPDATE_POLICY:
      GTK_VALUE_ENUM (*arg) = range->policy;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_range_init (GtkRange *range)
{
  range->trough = NULL;
  range->slider = NULL;
  range->step_forw = NULL;
  range->step_back = NULL;

  range->x_click_point = 0;
  range->y_click_point = 0;
  range->button = 0;
  range->digits = -1;
  range->policy = GTK_UPDATE_CONTINUOUS;
  range->scroll_type = GTK_SCROLL_NONE;
  range->in_child = 0;
  range->click_child = 0;
  range->need_timer = FALSE;
  range->timer = 0;
  range->old_value = 0.0;
  range->old_lower = 0.0;
  range->old_upper = 0.0;
  range->old_page_size = 0.0;
  range->adjustment = NULL;
}

GtkAdjustment*
gtk_range_get_adjustment (GtkRange *range)
{
  g_return_val_if_fail (range != NULL, NULL);
  g_return_val_if_fail (GTK_IS_RANGE (range), NULL);

  return range->adjustment;
}

void
gtk_range_set_update_policy (GtkRange      *range,
			     GtkUpdateType  policy)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  range->policy = policy;
}

void
gtk_range_set_adjustment (GtkRange      *range,
			  GtkAdjustment *adjustment)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));
  
  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  else
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (range->adjustment != adjustment)
    {
      if (range->adjustment)
	{
	  gtk_signal_disconnect_by_data (GTK_OBJECT (range->adjustment),
					 (gpointer) range);
	  gtk_object_unref (GTK_OBJECT (range->adjustment));
	}

      range->adjustment = adjustment;
      gtk_object_ref (GTK_OBJECT (adjustment));
      gtk_object_sink (GTK_OBJECT (adjustment));
      
      gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
			  (GtkSignalFunc) gtk_range_adjustment_changed,
			  (gpointer) range);
      gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			  (GtkSignalFunc) gtk_range_adjustment_value_changed,
			  (gpointer) range);
      
      range->old_value = adjustment->value;
      range->old_lower = adjustment->lower;
      range->old_upper = adjustment->upper;
      range->old_page_size = adjustment->page_size;
      
      gtk_range_adjustment_changed (adjustment, (gpointer) range);
    }
}

void
gtk_range_draw_background (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->trough && RANGE_CLASS (range)->draw_background)
    (* RANGE_CLASS (range)->draw_background) (range);
}

void
gtk_range_clear_background (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->trough && RANGE_CLASS (range)->clear_background)
    (* RANGE_CLASS (range)->clear_background) (range);
}

void
gtk_range_draw_trough (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->trough && RANGE_CLASS (range)->draw_trough)
    (* RANGE_CLASS (range)->draw_trough) (range);
}

void
gtk_range_draw_slider (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->slider && RANGE_CLASS (range)->draw_slider)
    (* RANGE_CLASS (range)->draw_slider) (range);
}

void
gtk_range_draw_step_forw (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->step_forw && RANGE_CLASS (range)->draw_step_forw)
    (* RANGE_CLASS (range)->draw_step_forw) (range);
}

void
gtk_range_draw_step_back (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->step_back && RANGE_CLASS (range)->draw_step_back)
    (* RANGE_CLASS (range)->draw_step_back) (range);
}

void
gtk_range_slider_update (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (RANGE_CLASS (range)->slider_update)
    (* RANGE_CLASS (range)->slider_update) (range);
}

gint
gtk_range_trough_click (GtkRange *range,
			gint      x,
			gint      y,
			gfloat   *jump_perc)
{
  g_return_val_if_fail (range != NULL, GTK_TROUGH_NONE);
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_TROUGH_NONE);

  if (RANGE_CLASS (range)->trough_click)
    return (* RANGE_CLASS (range)->trough_click) (range, x, y, jump_perc);

  return GTK_TROUGH_NONE;
}

void
gtk_range_default_hslider_update (GtkRange *range)
{
  gint left;
  gint right;
  gint x;
  gint trough_border;

  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  _gtk_range_get_props (range, NULL, &trough_border, NULL, NULL);

  if (GTK_WIDGET_REALIZED (range))
    {
      gtk_range_trough_hdims (range, &left, &right);
      x = left;

      if (range->adjustment->value < range->adjustment->lower)
	{
	  range->adjustment->value = range->adjustment->lower;
	  gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	}
      else if (range->adjustment->value > range->adjustment->upper)
	{
	  range->adjustment->value = range->adjustment->upper;
	  gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	}

      if (range->adjustment->lower != (range->adjustment->upper - range->adjustment->page_size))
	x += ((right - left) * (range->adjustment->value - range->adjustment->lower) /
	      (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));

      if (x < left)
	x = left;
      else if (x > right)
	x = right;

      gdk_window_move (range->slider, x, trough_border);
    }
}

void
gtk_range_default_vslider_update (GtkRange *range)
{
  gint top;
  gint bottom;
  gint y;
  gint trough_border;

  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  _gtk_range_get_props (range, NULL, &trough_border, NULL, NULL);

  if (GTK_WIDGET_REALIZED (range))
    {
      gtk_range_trough_vdims (range, &top, &bottom);
      y = top;

      if (range->adjustment->value < range->adjustment->lower)
	{
	  range->adjustment->value = range->adjustment->lower;
	  gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	}
      else if (range->adjustment->value > range->adjustment->upper)
	{
	  range->adjustment->value = range->adjustment->upper;
	  gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	}

      if (range->adjustment->lower != (range->adjustment->upper - range->adjustment->page_size))
	y += ((bottom - top) * (range->adjustment->value - range->adjustment->lower) /
	      (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));

      if (y < top)
	y = top;
      else if (y > bottom)
	y = bottom;

      gdk_window_move (range->slider, trough_border, y);
    }
}

gint
gtk_range_default_htrough_click (GtkRange *range,
				 gint      x,
				 gint      y,
				 gfloat	  *jump_perc)
{
  gint trough_border;
  gint trough_width;
  gint trough_height;
  gint slider_x;
  gint slider_length;
  gint left, right;

  g_return_val_if_fail (range != NULL, GTK_TROUGH_NONE);
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_TROUGH_NONE);

  _gtk_range_get_props (range, NULL, &trough_border, NULL, NULL);

  gtk_range_trough_hdims (range, &left, &right);
  gdk_window_get_size (range->slider, &slider_length, NULL);
  right += slider_length;
	      
  if ((x > left) && (y > trough_border))
    {
      gdk_window_get_size (range->trough, &trough_width, &trough_height);

      if ((x < right) && (y < (trough_height - trough_border)))
	{
	  if (jump_perc)
	    {
	      *jump_perc = ((gdouble) (x - left)) / ((gdouble) (right - left));
	      return GTK_TROUGH_JUMP;
	    }
	  
	  gdk_window_get_position (range->slider, &slider_x, NULL);
	  
	  if (x < slider_x)
	    return GTK_TROUGH_START;
	  else
	    return GTK_TROUGH_END;
	}
    }

  return GTK_TROUGH_NONE;
}

gint
gtk_range_default_vtrough_click (GtkRange *range,
				 gint      x,
				 gint      y,
				 gfloat   *jump_perc)
{
  gint trough_width;
  gint trough_height;
  gint slider_y;
  gint top, bottom;
  gint slider_length;
  gint trough_border;

  g_return_val_if_fail (range != NULL, GTK_TROUGH_NONE);
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_TROUGH_NONE);

  _gtk_range_get_props (range, NULL, &trough_border, NULL, NULL);

  gtk_range_trough_vdims (range, &top, &bottom);
  gdk_window_get_size (range->slider, NULL, &slider_length);
  bottom += slider_length;
	      
  if ((x > trough_border) && (y > top))
    {
      gdk_window_get_size (range->trough, &trough_width, &trough_height);

      if ((x < (trough_width - trough_border) && (y < bottom)))
	{
	  if (jump_perc)
	    {
	      *jump_perc = ((gdouble) (y - top)) / ((gdouble) (bottom - top));

	      return GTK_TROUGH_JUMP;
	    }
	  
	  gdk_window_get_position (range->slider, NULL, &slider_y);
	  
	  if (y < slider_y)
	    return GTK_TROUGH_START;
	  else
	    return GTK_TROUGH_END;
	}
    }

  return GTK_TROUGH_NONE;
}

void
gtk_range_default_hmotion (GtkRange *range,
			   gint      xdelta,
			   gint      ydelta)
{
  gdouble old_value;
  gint left, right;
  gint slider_x, slider_y;
  gint new_pos;

  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  range = GTK_RANGE (range);

  gdk_window_get_position (range->slider, &slider_x, &slider_y);
  gtk_range_trough_hdims (range, &left, &right);

  if (left == right)
    return;

  new_pos = slider_x + xdelta;

  if (new_pos < left)
    new_pos = left;
  else if (new_pos > right)
    new_pos = right;

  old_value = range->adjustment->value;
  range->adjustment->value = ((range->adjustment->upper -
			       range->adjustment->lower -
			       range->adjustment->page_size) *
			      (new_pos - left) / (right - left) +
			      range->adjustment->lower);

  if (range->digits >= 0)
    {
      char buffer[64];

      sprintf (buffer, "%0.*f", range->digits, range->adjustment->value);
      sscanf (buffer, "%f", &range->adjustment->value);
    }

  if (old_value != range->adjustment->value)
    {
      if (range->policy == GTK_UPDATE_CONTINUOUS)
	{
	  gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	}
      else
	{
	  gtk_range_slider_update (range);
	  gtk_range_clear_background (range);

	  if (range->policy == GTK_UPDATE_DELAYED)
	    {
	      gtk_range_remove_timer (range);
	      range->timer = gtk_timeout_add (SCROLL_DELAY_LENGTH,
					      (GtkFunction) RANGE_CLASS (range)->timer,
					      (gpointer) range);
	    }
	}
    }
}

void
gtk_range_default_vmotion (GtkRange *range,
			   gint      xdelta,
			   gint      ydelta)
{
  gdouble old_value;
  gint top, bottom;
  gint slider_x, slider_y;
  gint new_pos;

  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  range = GTK_RANGE (range);

  gdk_window_get_position (range->slider, &slider_x, &slider_y);
  gtk_range_trough_vdims (range, &top, &bottom);

  if (bottom == top)
    return;

  new_pos = slider_y + ydelta;

  if (new_pos < top)
    new_pos = top;
  else if (new_pos > bottom)
    new_pos = bottom;

  old_value = range->adjustment->value;
  range->adjustment->value = ((range->adjustment->upper -
			       range->adjustment->lower -
			       range->adjustment->page_size) *
			      (new_pos - top) / (bottom - top) +
			      range->adjustment->lower);

  if (range->digits >= 0)
    {
      char buffer[64];

      sprintf (buffer, "%0.*f", range->digits, range->adjustment->value);
      sscanf (buffer, "%f", &range->adjustment->value);
    }

  if (old_value != range->adjustment->value)
    {
      if (range->policy == GTK_UPDATE_CONTINUOUS)
	{
	  gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	}
      else
	{
	  gtk_range_slider_update (range);
	  gtk_range_clear_background (range);

	  if (range->policy == GTK_UPDATE_DELAYED)
	    {
	      gtk_range_remove_timer (range);
	      range->timer = gtk_timeout_add (SCROLL_DELAY_LENGTH,
					      (GtkFunction) RANGE_CLASS (range)->timer,
					      (gpointer) range);
	    }
	}
    }
}


static void
gtk_range_destroy (GtkObject *object)
{
  GtkRange *range;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_RANGE (object));

  range = GTK_RANGE (object);

  if (range->adjustment)
    gtk_signal_disconnect_by_data (GTK_OBJECT (range->adjustment),
				   (gpointer) range);

  (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_range_finalize (GtkObject *object)
{
  GtkRange *range;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_RANGE (object));

  range = GTK_RANGE (object);

  if (range->adjustment)
    gtk_object_unref (GTK_OBJECT (range->adjustment));

  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_range_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  GtkRange *range;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_RANGE (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      range = GTK_RANGE (widget);

      gtk_range_draw_background (range);
      gtk_range_draw_trough (range);
      gtk_range_draw_slider (range);
      gtk_range_draw_step_forw (range);
      gtk_range_draw_step_back (range);
    }
}

static void
gtk_range_draw_focus (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_RANGE (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_range_draw_trough (GTK_RANGE (widget));
}

static void
gtk_range_unrealize (GtkWidget *widget)
{
  GtkRange *range;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_RANGE (widget));

  range = GTK_RANGE (widget);

  if (range->slider)
    {
      gdk_window_set_user_data (range->slider, NULL);
      gdk_window_destroy (range->slider);
      range->slider = NULL;
    }
  if (range->trough)
    {
      gdk_window_set_user_data (range->trough, NULL);
      gdk_window_destroy (range->trough);
      range->trough = NULL;
    }
  if (range->step_forw)
    {
      gdk_window_set_user_data (range->step_forw, NULL);
      gdk_window_destroy (range->step_forw);
      range->step_forw = NULL;
    }
  if (range->step_back)
    {
      gdk_window_set_user_data (range->step_back, NULL);
      gdk_window_destroy (range->step_back);
      range->step_back = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static gint
gtk_range_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkRange *range;
  gint trough_border;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  _gtk_range_get_props (range, NULL, &trough_border, NULL, NULL);

  if (event->window == range->trough)
    {
      /* Don't redraw if we are only exposing the literal trough region.
       * this may not work correctly if someone overrides the default
       * trough-drawing handler. (Probably should really pass another
       * argument - the redrawn area to all the drawing functions)
       */
      if (!((event->area.x >= trough_border) &&
	    (event->area.y >= trough_border) &&
	    (event->area.x + event->area.width <= 
	     widget->allocation.width - trough_border) &&
	    (event->area.y + event->area.height <= 
	     widget->allocation.height - trough_border)))
	gtk_range_draw_trough (range);
    }
  else if (event->window == widget->window)
    {
      gtk_range_draw_background (range); 
    }
  else if (event->window == range->slider)
    {
      gtk_range_draw_slider (range);
    }
  else if (event->window == range->step_forw)
    {
      gtk_range_draw_step_forw (range);
    }
  else if (event->window == range->step_back)
    {
      gtk_range_draw_step_back (range);
    }
  return FALSE;
}

static gint
gtk_range_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkRange *range;
  gint trough_part;
  gfloat jump_perc;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  jump_perc = -1;
  range = GTK_RANGE (widget);
  if (range->button == 0)
    {
      gtk_grab_add (widget);

      range->button = event->button;
      range->x_click_point = event->x;
      range->y_click_point = event->y;

      if (event->window == range->trough)
	{
	  range->click_child = RANGE_CLASS (range)->trough;
	  
	  if (range->button == 2)
	    trough_part = gtk_range_trough_click (range, event->x, event->y, &jump_perc);
	  else
	    trough_part = gtk_range_trough_click (range, event->x, event->y, NULL);
	  
	  range->scroll_type = GTK_SCROLL_NONE;
	  if (trough_part == GTK_TROUGH_START)
	    range->scroll_type = GTK_SCROLL_PAGE_BACKWARD;
	  else if (trough_part == GTK_TROUGH_END)
	    range->scroll_type = GTK_SCROLL_PAGE_FORWARD;
	  else if (trough_part == GTK_TROUGH_JUMP &&
		   jump_perc >= 0 && jump_perc <= 1)
	    range->scroll_type = GTK_SCROLL_JUMP;
	  
	  if (range->scroll_type != GTK_SCROLL_NONE)
	    {
	      gtk_range_scroll (range, jump_perc);
	      gtk_range_add_timer (range);
	    }
	}
      else if (event->window == range->slider)
	{
	  range->click_child = RANGE_CLASS (range)->slider;
	  range->scroll_type = GTK_SCROLL_NONE;
	}
      else if (event->window == range->step_forw)
	{
	  range->click_child = RANGE_CLASS (range)->step_forw;
	  range->scroll_type = GTK_SCROLL_STEP_FORWARD;

	  gtk_range_scroll (range, -1);
	  gtk_range_add_timer (range);
	  gtk_range_draw_step_forw (range);
	}
      else if (event->window == range->step_back)
	{
	  range->click_child = RANGE_CLASS (range)->step_back;
	  range->scroll_type = GTK_SCROLL_STEP_BACKWARD;

	  gtk_range_scroll (range, -1);
	  gtk_range_add_timer (range);
	  gtk_range_draw_step_back (range);
	}
    }
  
  return TRUE;
}

static gint
gtk_range_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkRange *range;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  if (range->button == event->button)
    {
      gtk_grab_remove (widget);

      range->button = 0;
      range->x_click_point = -1;
      range->y_click_point = -1;

      if (range->click_child == RANGE_CLASS (range)->slider)
	{
	  if (range->policy == GTK_UPDATE_DELAYED)
	    gtk_range_remove_timer (range);

	  if ((range->policy != GTK_UPDATE_CONTINUOUS) &&
	      (range->old_value != range->adjustment->value))
	    gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	}
      else if ((range->click_child == RANGE_CLASS (range)->trough) ||
	       (range->click_child == RANGE_CLASS (range)->step_forw) ||
	       (range->click_child == RANGE_CLASS (range)->step_back))
	{
	  gtk_range_remove_timer (range);

	  if ((range->policy != GTK_UPDATE_CONTINUOUS) &&
	      (range->old_value != range->adjustment->value))
	    gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");

	  if (range->click_child == RANGE_CLASS (range)->step_forw)
	    {
	      range->click_child = 0;
	      gtk_range_draw_step_forw (range);
	    }
	  else if (range->click_child == RANGE_CLASS (range)->step_back)
	    {
	      range->click_child = 0;
	      gtk_range_draw_step_back (range);
	    }
	}

      range->click_child = 0;
    }

  return TRUE;
}

static gint
gtk_range_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GtkRange *range;
  GdkModifierType mods;
  gint x, y, mask;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  if (range->click_child == RANGE_CLASS (range)->slider)
    {
      x = event->x;
      y = event->y;

      if (event->is_hint || (event->window != range->slider))
	gdk_window_get_pointer (range->slider, &x, &y, &mods);

      switch (range->button)
	{
	case 1:
	  mask = GDK_BUTTON1_MASK;
	  break;
	case 2:
	  mask = GDK_BUTTON2_MASK;
	  break;
	case 3:
	  mask = GDK_BUTTON3_MASK;
	  break;
	default:
	  mask = 0;
	  break;
	}

      if (mods & mask)
	{
	  if (RANGE_CLASS (range)->motion)
	    (* RANGE_CLASS (range)->motion) (range, x - range->x_click_point, y - range->y_click_point);
	}
    }

  return TRUE;
}

static gint
gtk_range_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  GtkRange *range;
  gint return_val;
  GtkScrollType scroll = GTK_SCROLL_NONE;
  GtkTroughType pos = GTK_TROUGH_NONE;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);
  return_val = FALSE;

  if (RANGE_CLASS (range)->trough_keys)
    return_val = (* RANGE_CLASS (range)->trough_keys) (range, event, &scroll, &pos);

  if (return_val)
    {
      if (scroll != GTK_SCROLL_NONE)
	{
	  range->scroll_type = scroll;
	  gtk_range_scroll (range, -1);
	  if (range->old_value != range->adjustment->value)
	    {
	      gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	      switch (range->scroll_type)
		{
		case GTK_SCROLL_STEP_BACKWARD:
		  gtk_range_draw_step_back (range);
		  break;
		case GTK_SCROLL_STEP_FORWARD:
		  gtk_range_draw_step_forw (range);
		  break;
		}
	    }
	}
      if (pos != GTK_TROUGH_NONE)
	{
	  if (pos == GTK_TROUGH_START)
	    range->adjustment->value = range->adjustment->lower;
	  else if (pos == GTK_TROUGH_END)
	    range->adjustment->value =
	      range->adjustment->upper - range->adjustment->page_size;

	  if (range->old_value != range->adjustment->value)
	    {
	      gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment),
				       "value_changed");

	      gtk_range_slider_update (range);
	      gtk_range_clear_background (range);
	    }
	}
    }
  return return_val;
}

static gint
gtk_range_enter_notify (GtkWidget        *widget,
			GdkEventCrossing *event)
{
  GtkRange *range;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  if (event->window == range->trough)
    {
      range->in_child = RANGE_CLASS (range)->trough;
    }
  else if (event->window == range->slider)
    {
      range->in_child = RANGE_CLASS (range)->slider;

      if ((range->click_child == 0) ||
	  (range->click_child == RANGE_CLASS (range)->trough))
	gtk_range_draw_slider (range);
    }
  else if (event->window == range->step_forw)
    {
      range->in_child = RANGE_CLASS (range)->step_forw;

      if ((range->click_child == 0) ||
	  (range->click_child == RANGE_CLASS (range)->trough))
	gtk_range_draw_step_forw (range);
    }
  else if (event->window == range->step_back)
    {
      range->in_child = RANGE_CLASS (range)->step_back;

      if ((range->click_child == 0) ||
	  (range->click_child == RANGE_CLASS (range)->trough))
	gtk_range_draw_step_back (range);
    }

  return TRUE;
}

static gint
gtk_range_leave_notify (GtkWidget        *widget,
			GdkEventCrossing *event)
{
  GtkRange *range;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  range->in_child = 0;

  if (event->window == range->trough)
    {
    }
  else if (event->window == range->slider)
    {
      if ((range->click_child == 0) ||
	  (range->click_child == RANGE_CLASS (range)->trough))
	gtk_range_draw_slider (range);
    }
  else if (event->window == range->step_forw)
    {
      if ((range->click_child == 0) ||
	  (range->click_child == RANGE_CLASS (range)->trough))
	gtk_range_draw_step_forw (range);
    }
  else if (event->window == range->step_back)
    {
      if ((range->click_child == 0) ||
	  (range->click_child == RANGE_CLASS (range)->trough))
	gtk_range_draw_step_back (range);
    }

  return TRUE;
}

static gint
gtk_range_focus_in (GtkWidget     *widget,
		    GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return TRUE;
}

static gint
gtk_range_focus_out (GtkWidget     *widget,
		     GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return TRUE;
}

static void
gtk_real_range_draw_trough (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->trough)
     {
	gtk_paint_box (GTK_WIDGET (range)->style, range->trough,
		       GTK_STATE_ACTIVE, GTK_SHADOW_IN,
		       NULL, GTK_WIDGET(range), "trough",
		       0, 0, -1, -1);
	if (GTK_WIDGET_HAS_FOCUS (range))
	  gtk_paint_focus (GTK_WIDGET (range)->style,
			  range->trough,
			   NULL, GTK_WIDGET(range), "trough",
			  0, 0, -1, -1);
    }
}

static void
gtk_real_range_draw_slider (GtkRange *range)
{
  GtkStateType state_type;
   
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));
   
  if (range->slider)
    {
      if ((range->in_child == RANGE_CLASS (range)->slider) ||
	  (range->click_child == RANGE_CLASS (range)->slider))
	state_type = GTK_STATE_PRELIGHT;
      else
	state_type = GTK_STATE_NORMAL;
      gtk_paint_box (GTK_WIDGET (range)->style, range->slider,
		     state_type, GTK_SHADOW_OUT,
		     NULL, GTK_WIDGET (range), "slider",
		     0, 0, -1, -1);
    }
}

static gint
gtk_real_range_timer (GtkRange *range)
{
  gint return_val;

  GDK_THREADS_ENTER ();

  return_val = TRUE;
  if (range->click_child == RANGE_CLASS (range)->slider)
    {
      if (range->policy == GTK_UPDATE_DELAYED)
	gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
      return_val = FALSE;
    }
  else
    {
      GdkModifierType mods, mask;

      if (!range->timer)
	{
	  return_val = FALSE;
	  if (range->need_timer)
	    range->timer = gtk_timeout_add (SCROLL_TIMER_LENGTH,
					    (GtkFunction) RANGE_CLASS (range)->timer,
					    (gpointer) range);
	  else
	    {
	      GDK_THREADS_LEAVE ();
	      return FALSE;
	    }
	  range->need_timer = FALSE;
	}

      switch (range->button)
	{
	case 1:
	  mask = GDK_BUTTON1_MASK;
	  break;
	case 2:
	  mask = GDK_BUTTON2_MASK;
	  break;
	case 3:
	  mask = GDK_BUTTON3_MASK;
	  break;
	default:
	  mask = 0;
	  break;
	}

      gdk_window_get_pointer (range->slider, NULL, NULL, &mods);

      if (mods & mask)
	return_val = gtk_range_scroll (range, -1);
    }

  GDK_THREADS_LEAVE ();

  return return_val;
}

static gint
gtk_range_scroll (GtkRange *range,
		  gfloat    jump_perc)
{
  gfloat new_value;
  gint return_val;

  g_return_val_if_fail (range != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  new_value = range->adjustment->value;
  return_val = TRUE;

  switch (range->scroll_type)
    {
    case GTK_SCROLL_NONE:
      break;
      
    case GTK_SCROLL_JUMP:
      if (jump_perc >= 0 && jump_perc <= 1)
	{
	  new_value = (range->adjustment->lower +
		       (range->adjustment->upper - range->adjustment->page_size -
			range->adjustment->lower) * jump_perc);
	}
      break;
      
    case GTK_SCROLL_STEP_BACKWARD:
      new_value -= range->adjustment->step_increment;
      if (new_value <= range->adjustment->lower)
	{
	  new_value = range->adjustment->lower;
	  return_val = FALSE;
	  range->timer = 0;
	}
      break;

    case GTK_SCROLL_STEP_FORWARD:
      new_value += range->adjustment->step_increment;
      if (new_value >= (range->adjustment->upper - range->adjustment->page_size))
	{
	  new_value = range->adjustment->upper - range->adjustment->page_size;
	  return_val = FALSE;
	  range->timer = 0;
	}
      break;

    case GTK_SCROLL_PAGE_BACKWARD:
      new_value -= range->adjustment->page_increment;
      if (new_value <= range->adjustment->lower)
	{
	  new_value = range->adjustment->lower;
	  return_val = FALSE;
	  range->timer = 0;
	}
      break;

    case GTK_SCROLL_PAGE_FORWARD:
      new_value += range->adjustment->page_increment;
      if (new_value >= (range->adjustment->upper - range->adjustment->page_size))
	{
	  new_value = range->adjustment->upper - range->adjustment->page_size;
	  return_val = FALSE;
	  range->timer = 0;
	}
      break;
    }

  if (new_value != range->adjustment->value)
    {
      range->adjustment->value = new_value;

      if ((range->policy == GTK_UPDATE_CONTINUOUS) ||
	  (!return_val && (range->policy == GTK_UPDATE_DELAYED)))
	{
	  gtk_signal_emit_by_name (GTK_OBJECT (range->adjustment), "value_changed");
	}
      else
	{
	  gtk_range_slider_update (range);
	  gtk_range_clear_background (range);
	}
    }

  return return_val;
}


static gboolean
gtk_range_timer_1st_time (GtkRange *range)
{
  /*
   * If the real timeout function succeeds and the timeout is still set,
   * replace it with a quicker one so successive scrolling goes faster.
   */
  gtk_object_ref (GTK_OBJECT (range));

  if (RANGE_CLASS (range)->timer (range))
    {
      if (range->timer)
	{
	  /* We explicitely remove ourselves here in the paranoia
	   * that due to things happening above in the callback
	   * above, we might have been removed, and another added.
	   */
	  g_source_remove (range->timer);
	  range->timer = gtk_timeout_add (SCROLL_LATER_DELAY,
					  (GtkFunction) RANGE_CLASS (range)->timer,
					  range);
	}
    }
  
  gtk_object_unref (GTK_OBJECT (range));
  
  return FALSE;  /* don't keep calling this function */
}

static void
gtk_range_add_timer (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (!range->timer)
    {
      range->need_timer = TRUE;
      range->timer = gtk_timeout_add (SCROLL_INITIAL_DELAY,
				      (GtkFunction) gtk_range_timer_1st_time,
				      range);
    }
}

static void
gtk_range_remove_timer (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->timer)
    {
      gtk_timeout_remove (range->timer);
      range->timer = 0;
    }
  range->need_timer = FALSE;
}

static void
gtk_range_adjustment_changed (GtkAdjustment *adjustment,
			      gpointer       data)
{
  GtkRange *range;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  range = GTK_RANGE (data);

  if (((range->old_lower != adjustment->lower) ||
       (range->old_upper != adjustment->upper) ||
       (range->old_page_size != adjustment->page_size)) &&
      (range->old_value == adjustment->value))
    {
      if ((adjustment->lower == adjustment->upper) ||
	  (range->old_lower == (range->old_upper - range->old_page_size)))
	{
	  adjustment->value = adjustment->lower;
	  gtk_signal_emit_by_name (GTK_OBJECT (adjustment), "value_changed");
	}
    }

  if ((range->old_value != adjustment->value) ||
      (range->old_lower != adjustment->lower) ||
      (range->old_upper != adjustment->upper) ||
      (range->old_page_size != adjustment->page_size))
    {
      gtk_range_slider_update (range);
      gtk_range_clear_background (range);

      range->old_value = adjustment->value;
      range->old_lower = adjustment->lower;
      range->old_upper = adjustment->upper;
      range->old_page_size = adjustment->page_size;
    }
}

static void
gtk_range_adjustment_value_changed (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GtkRange *range;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  range = GTK_RANGE (data);

  if (range->old_value != adjustment->value)
    {
      gtk_range_slider_update (range);
      gtk_range_clear_background (range);

      range->old_value = adjustment->value;
    }
}


static void
gtk_range_trough_hdims (GtkRange *range,
			gint     *left,
			gint     *right)
{
  gint trough_width;
  gint slider_length;
  gint tmp_width;
  gint tleft;
  gint tright;
  gint stepper_spacing;
  gint trough_border;

  g_return_if_fail (range != NULL);

  gdk_window_get_size (range->trough, &trough_width, NULL);
  gdk_window_get_size (range->slider, &slider_length, NULL);

  _gtk_range_get_props (range, NULL, &trough_border, NULL, &stepper_spacing);
  
  tleft = trough_border;
  tright = trough_width - slider_length - trough_border;

  if (range->step_back)
    {
      gdk_window_get_size (range->step_back, &tmp_width, NULL);
      tleft += (tmp_width + stepper_spacing);
    }

  if (range->step_forw)
    {
      gdk_window_get_size (range->step_forw, &tmp_width, NULL);
      tright -= (tmp_width + stepper_spacing);
    }

  if (left)
    *left = tleft;
  if (right)
    *right = tright;
}

static void
gtk_range_trough_vdims (GtkRange *range,
			gint     *top,
			gint     *bottom)
{
  gint trough_height;
  gint slider_length;
  gint tmp_height;
  gint ttop;
  gint tbottom;
  gint trough_border;
  gint stepper_spacing;

  g_return_if_fail (range != NULL);

  _gtk_range_get_props (range, NULL, &trough_border, NULL, &stepper_spacing);
  
  gdk_window_get_size (range->trough, NULL, &trough_height);
  gdk_window_get_size (range->slider, NULL, &slider_length);

  ttop = trough_border;
  tbottom = trough_height - slider_length - trough_border;

  if (range->step_back)
    {
      gdk_window_get_size (range->step_back, NULL, &tmp_height);
      ttop += (tmp_height + stepper_spacing);
    }

  if (range->step_forw)
    {
      gdk_window_get_size (range->step_forw, NULL, &tmp_height);
      tbottom -= (tmp_height + stepper_spacing);
    }

  if (top)
    *top = ttop;
  if (bottom)
    *bottom = tbottom;
}

static void
gtk_range_style_set (GtkWidget *widget,
		      GtkStyle  *previous_style)
{
  GtkRange *range;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_RANGE (widget));

  range = GTK_RANGE (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      if (range->trough)
	gtk_style_set_background (widget->style, range->trough, GTK_STATE_ACTIVE);

      if (range->slider)
	gtk_style_set_background (widget->style, range->slider, GTK_STATE_NORMAL);
      
      /* The backgrounds of the step_forw and step_back never actually
       * get drawn in draw calls, so we call gdk_window_clear() here
       * so they get the correct colors. This is a hack. OWT.
       */

      if (range->step_forw)
	{
	  gtk_style_set_background (widget->style, range->step_forw, GTK_STATE_ACTIVE);
	  gdk_window_clear (range->step_forw);
	}

      if (range->step_back)
	{
	  gtk_style_set_background (widget->style, range->step_back, GTK_STATE_ACTIVE);
	  gdk_window_clear (range->step_back);
	}
    }
}

void
_gtk_range_get_props (GtkRange *range,
		      gint     *slider_width,
		      gint     *trough_border,
		      gint     *stepper_size,
		      gint     *stepper_spacing)
{
  GtkWidget *widget =  GTK_WIDGET (range);
  

  if (slider_width)
    *slider_width = gtk_style_get_prop_experimental (widget->style,
						     "GtkRange::slider_width",
						     RANGE_CLASS (widget)->slider_width);
  if (trough_border)
    *trough_border = gtk_style_get_prop_experimental (widget->style,
						      "GtkRange::trough_border",
						      widget->style->klass->xthickness);
  if (stepper_size)
    *stepper_size = gtk_style_get_prop_experimental (widget->style,
						     "GtkRange::stepper_size",
						     RANGE_CLASS (widget)->stepper_size);
  if (stepper_spacing)
    *stepper_spacing = gtk_style_get_prop_experimental (widget->style,
							"GtkRange::stepper_spacing",
							RANGE_CLASS (widget)->stepper_slider_spacing);
}

