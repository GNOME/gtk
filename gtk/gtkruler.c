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

#include "gtkruler.h"

enum {
  ARG_0,
  ARG_LOWER,
  ARG_UPPER,
  ARG_POSITION,
  ARG_MAX_SIZE
};

static void gtk_ruler_class_init    (GtkRulerClass  *klass);
static void gtk_ruler_init          (GtkRuler       *ruler);
static void gtk_ruler_realize       (GtkWidget      *widget);
static void gtk_ruler_unrealize     (GtkWidget      *widget);
static void gtk_ruler_size_allocate (GtkWidget      *widget,
				     GtkAllocation  *allocation);
static gint gtk_ruler_expose        (GtkWidget      *widget,
				     GdkEventExpose *event);
static void gtk_ruler_make_pixmap   (GtkRuler       *ruler);
static void gtk_ruler_set_arg       (GtkObject      *object,
				     GtkArg         *arg,
				     guint           arg_id);
static void gtk_ruler_get_arg       (GtkObject      *object,
				     GtkArg         *arg,
				     guint           arg_id);

static GtkWidgetClass *parent_class;

static const GtkRulerMetric ruler_metrics[] =
{
  {"Pixels", "Pi", 1.0, { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000 }, { 1, 5, 10, 50, 100 }},
  {"Inches", "In", 72.0, { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 }, { 1, 2, 4, 8, 16 }},
  {"Centimeters", "Cn", 28.35, { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000 }, { 1, 5, 10, 50, 100 }},
};


GtkType
gtk_ruler_get_type (void)
{
  static GtkType ruler_type = 0;

  if (!ruler_type)
    {
      static const GtkTypeInfo ruler_info =
      {
	"GtkRuler",
	sizeof (GtkRuler),
	sizeof (GtkRulerClass),
	(GtkClassInitFunc) gtk_ruler_class_init,
	(GtkObjectInitFunc) gtk_ruler_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      ruler_type = gtk_type_unique (GTK_TYPE_WIDGET, &ruler_info);
    }

  return ruler_type;
}

static void
gtk_ruler_class_init (GtkRulerClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_WIDGET);
  
  object_class->set_arg = gtk_ruler_set_arg;
  object_class->get_arg = gtk_ruler_get_arg;

  widget_class->realize = gtk_ruler_realize;
  widget_class->unrealize = gtk_ruler_unrealize;
  widget_class->size_allocate = gtk_ruler_size_allocate;
  widget_class->expose_event = gtk_ruler_expose;

  class->draw_ticks = NULL;
  class->draw_pos = NULL;

  gtk_object_add_arg_type ("GtkRuler::lower", GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE, ARG_LOWER);
  gtk_object_add_arg_type ("GtkRuler::upper", GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE, ARG_UPPER);
  gtk_object_add_arg_type ("GtkRuler::position", GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE, ARG_POSITION);
  gtk_object_add_arg_type ("GtkRuler::max_size", GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE, ARG_MAX_SIZE);
}

static void
gtk_ruler_init (GtkRuler *ruler)
{
  ruler->backing_store = NULL;
  ruler->non_gr_exp_gc = NULL;
  ruler->xsrc = 0;
  ruler->ysrc = 0;
  ruler->slider_size = 0;
  ruler->lower = 0;
  ruler->upper = 0;
  ruler->position = 0;
  ruler->max_size = 0;

  gtk_ruler_set_metric (ruler, GTK_PIXELS);
}

static void
gtk_ruler_set_arg (GtkObject  *object,
		   GtkArg     *arg,
		   guint       arg_id)
{
  GtkRuler *ruler = GTK_RULER (object);

  switch (arg_id)
    {
    case ARG_LOWER:
      gtk_ruler_set_range (ruler, GTK_VALUE_FLOAT (*arg), ruler->upper,
			   ruler->position, ruler->max_size);
      break;
    case ARG_UPPER:
      gtk_ruler_set_range (ruler, ruler->lower, GTK_VALUE_FLOAT (*arg),
			   ruler->position, ruler->max_size);
      break;
    case ARG_POSITION:
      gtk_ruler_set_range (ruler, ruler->lower, ruler->upper,
			   GTK_VALUE_FLOAT (*arg), ruler->max_size);
      break;
    case ARG_MAX_SIZE:
      gtk_ruler_set_range (ruler, ruler->lower, ruler->upper,
			   ruler->position,  GTK_VALUE_FLOAT (*arg));
      break;
    }
}

static void
gtk_ruler_get_arg (GtkObject  *object,
		   GtkArg     *arg,
		   guint       arg_id)
{
  GtkRuler *ruler = GTK_RULER (object);
  
  switch (arg_id)
    {
    case ARG_LOWER:
      GTK_VALUE_FLOAT (*arg) = ruler->lower;
      break;
    case ARG_UPPER:
      GTK_VALUE_FLOAT (*arg) = ruler->upper;
      break;
    case ARG_POSITION:
      GTK_VALUE_FLOAT (*arg) = ruler->position;
      break;
    case ARG_MAX_SIZE:
      GTK_VALUE_FLOAT (*arg) = ruler->max_size;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

void
gtk_ruler_set_metric (GtkRuler      *ruler,
		      GtkMetricType  metric)
{
  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_RULER (ruler));

  ruler->metric = (GtkRulerMetric *) &ruler_metrics[metric];

  if (GTK_WIDGET_DRAWABLE (ruler))
    gtk_widget_queue_draw (GTK_WIDGET (ruler));
}

void
gtk_ruler_set_range (GtkRuler *ruler,
		     gfloat    lower,
		     gfloat    upper,
		     gfloat    position,
		     gfloat    max_size)
{
  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_RULER (ruler));

  ruler->lower = lower;
  ruler->upper = upper;
  ruler->position = position;
  ruler->max_size = max_size;

  if (GTK_WIDGET_DRAWABLE (ruler))
    gtk_widget_queue_draw (GTK_WIDGET (ruler));
}

void
gtk_ruler_draw_ticks (GtkRuler *ruler)
{
  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_RULER (ruler));

  if (GTK_RULER_CLASS (GTK_OBJECT (ruler)->klass)->draw_ticks)
    (* GTK_RULER_CLASS (GTK_OBJECT (ruler)->klass)->draw_ticks) (ruler);
}

void
gtk_ruler_draw_pos (GtkRuler *ruler)
{
  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_RULER (ruler));

  if (GTK_RULER_CLASS (GTK_OBJECT (ruler)->klass)->draw_pos)
    (* GTK_RULER_CLASS (GTK_OBJECT (ruler)->klass)->draw_pos) (ruler);
}


static void
gtk_ruler_realize (GtkWidget *widget)
{
  GtkRuler *ruler;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_RULER (widget));

  ruler = GTK_RULER (widget);
  GTK_WIDGET_SET_FLAGS (ruler, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_POINTER_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, ruler);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);

  gtk_ruler_make_pixmap (ruler);
}

static void
gtk_ruler_unrealize (GtkWidget *widget)
{
  GtkRuler *ruler;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_RULER (widget));

  ruler = GTK_RULER (widget);

  if (ruler->backing_store)
    gdk_pixmap_unref (ruler->backing_store);
  if (ruler->non_gr_exp_gc)
    gdk_gc_destroy (ruler->non_gr_exp_gc);

  ruler->backing_store = NULL;
  ruler->non_gr_exp_gc = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_ruler_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkRuler *ruler;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_RULER (widget));

  ruler = GTK_RULER (widget);
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gtk_ruler_make_pixmap (ruler);
    }
}

static gint
gtk_ruler_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkRuler *ruler;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_RULER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      ruler = GTK_RULER (widget);

      gtk_ruler_draw_ticks (ruler);
      
      gdk_draw_pixmap (widget->window,
		       ruler->non_gr_exp_gc,
		       ruler->backing_store,
		       0, 0, 0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
      
      gtk_ruler_draw_pos (ruler);
    }

  return FALSE;
}

static void
gtk_ruler_make_pixmap (GtkRuler *ruler)
{
  GtkWidget *widget;
  gint width;
  gint height;

  widget = GTK_WIDGET (ruler);

  if (ruler->backing_store)
    {
      gdk_window_get_size (ruler->backing_store, &width, &height);
      if ((width == widget->allocation.width) &&
	  (height == widget->allocation.height))
	return;

      gdk_pixmap_unref (ruler->backing_store);
    }

  ruler->backing_store = gdk_pixmap_new (widget->window,
					 widget->allocation.width,
					 widget->allocation.height,
					 -1);

  ruler->xsrc = 0;
  ruler->ysrc = 0;

  if (!ruler->non_gr_exp_gc)
    {
      ruler->non_gr_exp_gc = gdk_gc_new (widget->window);
      gdk_gc_set_exposures (ruler->non_gr_exp_gc, FALSE);
    }
}
