
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
#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include "gtkdial.h"

#define SCROLL_DELAY_LENGTH  300
#define DIAL_DEFAULT_SIZE 100

/* Forward declarations */

static void gtk_dial_class_init               (GtkDialClass     *klass);
static void gtk_dial_init                     (GtkDial          *dial);
static void gtk_dial_destroy                  (GtkObject        *object);
static void gtk_dial_realize                  (GtkWidget        *widget);
static void gtk_dial_size_request             (GtkWidget        *widget,
                                               GtkRequisition   *requisition);
static void gtk_dial_size_allocate            (GtkWidget        *widget,
                                               GtkAllocation    *allocation);
static gboolean gtk_dial_expose               (GtkWidget        *widget,
                                               GdkEventExpose   *event);
static gboolean gtk_dial_button_press         (GtkWidget        *widget,
                                               GdkEventButton   *event);
static gboolean gtk_dial_button_release       (GtkWidget        *widget,
                                               GdkEventButton   *event);
static gboolean gtk_dial_motion_notify        (GtkWidget        *widget,
                                               GdkEventMotion   *event);
static gboolean gtk_dial_timer                (GtkDial          *dial);

static void gtk_dial_update_mouse             (GtkDial *dial, gint x, gint y);
static void gtk_dial_update                   (GtkDial *dial);
static void gtk_dial_adjustment_changed       (GtkAdjustment    *adjustment,
						gpointer          data);
static void gtk_dial_adjustment_value_changed (GtkAdjustment    *adjustment,
						gpointer          data);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

GType
gtk_dial_get_type ()
{
  static GType dial_type = 0;

  if (!dial_type)
    {
      const GTypeInfo dial_info =
      {
	sizeof (GtkDialClass),
	NULL,
	NULL,
	(GClassInitFunc) gtk_dial_class_init,
	NULL,
	NULL,
	sizeof (GtkDial),
        0,
	(GInstanceInitFunc) gtk_dial_init,
      };

      dial_type = g_type_register_static (GTK_TYPE_WIDGET, "GtkDial", &dial_info, 0);
    }

  return dial_type;
}

static void
gtk_dial_class_init (GtkDialClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = g_type_class_peek_parent (class);

  object_class->destroy = gtk_dial_destroy;

  widget_class->realize = gtk_dial_realize;
  widget_class->expose_event = gtk_dial_expose;
  widget_class->size_request = gtk_dial_size_request;
  widget_class->size_allocate = gtk_dial_size_allocate;
  widget_class->button_press_event = gtk_dial_button_press;
  widget_class->button_release_event = gtk_dial_button_release;
  widget_class->motion_notify_event = gtk_dial_motion_notify;
}

static void
gtk_dial_init (GtkDial *dial)
{
  dial->button = 0;
  dial->policy = GTK_UPDATE_CONTINUOUS;
  dial->timer = 0;
  dial->radius = 0;
  dial->pointer_width = 0;
  dial->angle = 0.0;
  dial->old_value = 0.0;
  dial->old_lower = 0.0;
  dial->old_upper = 0.0;
  dial->adjustment = NULL;
}

GtkWidget*
gtk_dial_new (GtkAdjustment *adjustment)
{
  GtkDial *dial;

  dial = g_object_new (gtk_dial_get_type (), NULL);

  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  gtk_dial_set_adjustment (dial, adjustment);

  return GTK_WIDGET (dial);
}

static void
gtk_dial_destroy (GtkObject *object)
{
  GtkDial *dial;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_DIAL (object));

  dial = GTK_DIAL (object);

  if (dial->adjustment)
    {
      g_object_unref (GTK_OBJECT (dial->adjustment));
      dial->adjustment = NULL;
    }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkAdjustment*
gtk_dial_get_adjustment (GtkDial *dial)
{
  g_return_val_if_fail (dial != NULL, NULL);
  g_return_val_if_fail (GTK_IS_DIAL (dial), NULL);

  return dial->adjustment;
}

void
gtk_dial_set_update_policy (GtkDial      *dial,
			     GtkUpdateType  policy)
{
  g_return_if_fail (dial != NULL);
  g_return_if_fail (GTK_IS_DIAL (dial));

  dial->policy = policy;
}

void
gtk_dial_set_adjustment (GtkDial      *dial,
			  GtkAdjustment *adjustment)
{
  g_return_if_fail (dial != NULL);
  g_return_if_fail (GTK_IS_DIAL (dial));

  if (dial->adjustment)
    {
      g_signal_handlers_disconnect_by_func (GTK_OBJECT (dial->adjustment), NULL, (gpointer) dial);
      g_object_unref (GTK_OBJECT (dial->adjustment));
    }

  dial->adjustment = adjustment;
  g_object_ref (GTK_OBJECT (dial->adjustment));

  g_signal_connect (G_OBJECT (adjustment), "changed",
		    G_CALLBACK (gtk_dial_adjustment_changed),
		    (gpointer) dial);
  g_signal_connect (G_OBJECT (adjustment), "value_changed",
		    G_CALLBACK (gtk_dial_adjustment_value_changed),
		    (gpointer) dial);

  dial->old_value = adjustment->value;
  dial->old_lower = adjustment->lower;
  dial->old_upper = adjustment->upper;

  gtk_dial_update (dial);
}

static void
gtk_dial_realize (GtkWidget *widget)
{
  GtkDial *dial;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_DIAL (widget));

  gtk_widget_set_realized (widget, TRUE);
  dial = GTK_DIAL (widget);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget) | 
    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
    GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
    GDK_POINTER_MOTION_HINT_MASK;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_user_data (widget->window, widget);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);
}

static void 
gtk_dial_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  requisition->width = DIAL_DEFAULT_SIZE;
  requisition->height = DIAL_DEFAULT_SIZE;
}

static void
gtk_dial_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkDial *dial;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_DIAL (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  dial = GTK_DIAL (widget);

  if (gtk_widget_get_realized (widget))
    {

      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

    }
  dial->radius = MIN (allocation->width, allocation->height) * 0.45;
  dial->pointer_width = dial->radius / 5;
}

static gboolean
gtk_dial_expose( GtkWidget      *widget,
		 GdkEventExpose *event )
{
  GtkDial *dial;
  GdkPoint points[6];
  gdouble s,c;
  gdouble theta, last, increment;
  GtkStyle      *blankstyle;
  gint xc, yc;
  gint upper, lower;
  gint tick_length;
  gint i, inc;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_DIAL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;
  
  dial = GTK_DIAL (widget);

/*  gdk_window_clear_area (widget->window,
			 0, 0,
			 widget->allocation.width,
			 widget->allocation.height);
*/
  xc = widget->allocation.width / 2;
  yc = widget->allocation.height / 2;

  upper = dial->adjustment->upper;
  lower = dial->adjustment->lower;

  /* Erase old pointer */

  s = sin (dial->last_angle);
  c = cos (dial->last_angle);
  dial->last_angle = dial->angle;

  points[0].x = xc + s*dial->pointer_width/2;
  points[0].y = yc + c*dial->pointer_width/2;
  points[1].x = xc + c*dial->radius;
  points[1].y = yc - s*dial->radius;
  points[2].x = xc - s*dial->pointer_width/2;
  points[2].y = yc - c*dial->pointer_width/2;
  points[3].x = xc - c*dial->radius/10;
  points[3].y = yc + s*dial->radius/10;
  points[4].x = points[0].x;
  points[4].y = points[0].y;

  blankstyle = gtk_style_new ();
  blankstyle->bg_gc[GTK_STATE_NORMAL] =
                widget->style->bg_gc[GTK_STATE_NORMAL];
  blankstyle->dark_gc[GTK_STATE_NORMAL] =
                widget->style->bg_gc[GTK_STATE_NORMAL];
  blankstyle->light_gc[GTK_STATE_NORMAL] =
                widget->style->bg_gc[GTK_STATE_NORMAL];
  blankstyle->black_gc =
                widget->style->bg_gc[GTK_STATE_NORMAL];
  blankstyle->depth =
                gdk_drawable_get_depth( GDK_DRAWABLE (widget->window));

  gtk_paint_polygon (blankstyle,
                    widget->window,
                    GTK_STATE_NORMAL,
                    GTK_SHADOW_OUT,
	            NULL,
                    widget,
                    NULL,
                    points, 5,
                    FALSE);

  g_object_unref (blankstyle);


  /* Draw ticks */

  if ((upper - lower) == 0)
    return FALSE;

  increment = (100*M_PI) / (dial->radius*dial->radius);

  inc = (upper - lower);

  while (inc < 100) inc *= 10;
  while (inc >= 1000) inc /= 10;
  last = -1;

  for (i = 0; i <= inc; i++)
    {
      theta = ((gfloat)i*M_PI / (18*inc/24.) - M_PI/6.);

      if ((theta - last) < (increment))
	continue;     
      last = theta;

      s = sin (theta);
      c = cos (theta);

      tick_length = (i%(inc/10) == 0) ? dial->pointer_width : dial->pointer_width / 2;

      gdk_draw_line (widget->window,
                     widget->style->fg_gc[widget->state],
                     xc + c*(dial->radius - tick_length),
                     yc - s*(dial->radius - tick_length),
                     xc + c*dial->radius,
                     yc - s*dial->radius);
    }

  /* Draw pointer */

  s = sin (dial->angle);
  c = cos (dial->angle);
  dial->last_angle = dial->angle;

  points[0].x = xc + s*dial->pointer_width/2;
  points[0].y = yc + c*dial->pointer_width/2;
  points[1].x = xc + c*dial->radius;
  points[1].y = yc - s*dial->radius;
  points[2].x = xc - s*dial->pointer_width/2;
  points[2].y = yc - c*dial->pointer_width/2;
  points[3].x = xc - c*dial->radius/10;
  points[3].y = yc + s*dial->radius/10;
  points[4].x = points[0].x;
  points[4].y = points[0].y;


  gtk_paint_polygon (widget->style,
		    widget->window,
		    GTK_STATE_NORMAL,
		    GTK_SHADOW_OUT,
	            NULL,
                    widget,
                    NULL,
		    points, 5,
		    TRUE);

  return FALSE;
}

static gboolean
gtk_dial_button_press( GtkWidget      *widget,
		       GdkEventButton *event )
{
  GtkDial *dial;
  gint dx, dy;
  double s, c;
  double d_parallel;
  double d_perpendicular;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_DIAL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  dial = GTK_DIAL (widget);

  /* Determine if button press was within pointer region - we 
     do this by computing the parallel and perpendicular distance of
     the point where the mouse was pressed from the line passing through
     the pointer */
  
  dx = event->x - widget->allocation.width / 2;
  dy = widget->allocation.height / 2 - event->y;
  
  s = sin (dial->angle);
  c = cos (dial->angle);
  
  d_parallel = s*dy + c*dx;
  d_perpendicular = fabs (s*dx - c*dy);
  
  if (!dial->button &&
      (d_perpendicular < dial->pointer_width/2) &&
      (d_parallel > - dial->pointer_width))
    {
      gtk_grab_add (widget);

      dial->button = event->button;

      gtk_dial_update_mouse (dial, event->x, event->y);
    }

  return FALSE;
}

static gboolean
gtk_dial_button_release( GtkWidget      *widget,
                         GdkEventButton *event )
{
  GtkDial *dial;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_DIAL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  dial = GTK_DIAL (widget);

  if (dial->button == event->button)
    {
      gtk_grab_remove (widget);

      dial->button = 0;

      if (dial->policy == GTK_UPDATE_DELAYED)
	g_source_remove (dial->timer);
      
      if ((dial->policy != GTK_UPDATE_CONTINUOUS) &&
	  (dial->old_value != dial->adjustment->value))
	g_signal_emit_by_name (GTK_OBJECT (dial->adjustment), "value_changed");
    }

  return FALSE;
}

static gboolean
gtk_dial_motion_notify( GtkWidget      *widget,
                        GdkEventMotion *event )
{
  GtkDial *dial;
  GdkModifierType mods;
  gint x, y, mask;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_DIAL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  dial = GTK_DIAL (widget);

  if (dial->button != 0)
    {
      x = event->x;
      y = event->y;

      if (event->is_hint || (event->window != widget->window))
	gdk_window_get_pointer (widget->window, &x, &y, &mods);

      switch (dial->button)
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
	gtk_dial_update_mouse (dial, x,y);
    }

  return FALSE;
}

static gboolean
gtk_dial_timer( GtkDial *dial )
{
  g_return_val_if_fail (dial != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_DIAL (dial), FALSE);

  if (dial->policy == GTK_UPDATE_DELAYED)
    g_signal_emit_by_name (GTK_OBJECT (dial->adjustment), "value_changed");

  return FALSE;
}

static void
gtk_dial_update_mouse( GtkDial *dial, gint x, gint y )
{
  gint xc, yc;
  gfloat old_value;

  g_return_if_fail (dial != NULL);
  g_return_if_fail (GTK_IS_DIAL (dial));

  xc = GTK_WIDGET(dial)->allocation.width / 2;
  yc = GTK_WIDGET(dial)->allocation.height / 2;

  old_value = dial->adjustment->value;
  dial->angle = atan2(yc-y, x-xc);

  if (dial->angle < -M_PI/2.)
    dial->angle += 2*M_PI;

  if (dial->angle < -M_PI/6)
    dial->angle = -M_PI/6;

  if (dial->angle > 7.*M_PI/6.)
    dial->angle = 7.*M_PI/6.;

  dial->adjustment->value = dial->adjustment->lower + (7.*M_PI/6 - dial->angle) *
    (dial->adjustment->upper - dial->adjustment->lower) / (4.*M_PI/3.);

  if (dial->adjustment->value != old_value)
    {
      if (dial->policy == GTK_UPDATE_CONTINUOUS)
	{
	  g_signal_emit_by_name (GTK_OBJECT (dial->adjustment), "value_changed");
	}
      else
	{
	  gtk_widget_queue_draw (GTK_WIDGET (dial));

	  if (dial->policy == GTK_UPDATE_DELAYED)
	    {
	      if (dial->timer)
		g_source_remove (dial->timer);

	      dial->timer = gdk_threads_add_timeout (SCROLL_DELAY_LENGTH,
					   (GSourceFunc) gtk_dial_timer,
					   (gpointer) dial);
	    }
	}
    }
}

static void
gtk_dial_update (GtkDial *dial)
{
  gfloat new_value;
  
  g_return_if_fail (dial != NULL);
  g_return_if_fail (GTK_IS_DIAL (dial));

  new_value = dial->adjustment->value;
  
  if (new_value < dial->adjustment->lower)
    new_value = dial->adjustment->lower;

  if (new_value > dial->adjustment->upper)
    new_value = dial->adjustment->upper;

  if (new_value != dial->adjustment->value)
    {
      dial->adjustment->value = new_value;
      g_signal_emit_by_name (GTK_OBJECT (dial->adjustment), "value_changed");
    }

  dial->angle = 7.*M_PI/6. - (new_value - dial->adjustment->lower) * 4.*M_PI/3. /
    (dial->adjustment->upper - dial->adjustment->lower);

  gtk_widget_queue_draw (GTK_WIDGET (dial));
}

static void
gtk_dial_adjustment_changed (GtkAdjustment *adjustment,
			      gpointer       data)
{
  GtkDial *dial;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  dial = GTK_DIAL (data);

  if ((dial->old_value != adjustment->value) ||
      (dial->old_lower != adjustment->lower) ||
      (dial->old_upper != adjustment->upper))
    {
      gtk_dial_update (dial);

      dial->old_value = adjustment->value;
      dial->old_lower = adjustment->lower;
      dial->old_upper = adjustment->upper;
    }
}

static void
gtk_dial_adjustment_value_changed (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GtkDial *dial;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  dial = GTK_DIAL (data);

  if (dial->old_value != adjustment->value)
    {
      gtk_dial_update (dial);

      dial->old_value = adjustment->value;
    }
}
