/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gtkruler.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

enum {
  PROP_0,
  PROP_LOWER,
  PROP_UPPER,
  PROP_POSITION,
  PROP_MAX_SIZE,
  PROP_METRIC
};

static void gtk_ruler_realize       (GtkWidget      *widget);
static void gtk_ruler_unrealize     (GtkWidget      *widget);
static void gtk_ruler_size_allocate (GtkWidget      *widget,
				     GtkAllocation  *allocation);
static gint gtk_ruler_expose        (GtkWidget      *widget,
				     GdkEventExpose *event);
static void gtk_ruler_make_pixmap   (GtkRuler       *ruler);
static void gtk_ruler_set_property  (GObject        *object,
				     guint            prop_id,
				     const GValue   *value,
				     GParamSpec     *pspec);
static void gtk_ruler_get_property  (GObject        *object,
				     guint           prop_id,
				     GValue         *value,
				     GParamSpec     *pspec);

static const GtkRulerMetric ruler_metrics[] =
{
  { "Pixel", "Pi", 1.0, { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000 }, { 1, 5, 10, 50, 100 }},
  { "Inches", "In", 72.0, { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 }, { 1, 2, 4, 8, 16 }},
  { "Centimeters", "Cn", 28.35, { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000 }, { 1, 5, 10, 50, 100 }},
};

G_DEFINE_TYPE (GtkRuler, gtk_ruler, GTK_TYPE_WIDGET)

static void
gtk_ruler_class_init (GtkRulerClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_ruler_set_property;
  gobject_class->get_property = gtk_ruler_get_property;

  widget_class->realize = gtk_ruler_realize;
  widget_class->unrealize = gtk_ruler_unrealize;
  widget_class->size_allocate = gtk_ruler_size_allocate;
  widget_class->expose_event = gtk_ruler_expose;

  class->draw_ticks = NULL;
  class->draw_pos = NULL;

  g_object_class_install_property (gobject_class,
                                   PROP_LOWER,
                                   g_param_spec_double ("lower",
							P_("Lower"),
							P_("Lower limit of ruler"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0.0,
							GTK_PARAM_READWRITE));  

  g_object_class_install_property (gobject_class,
                                   PROP_UPPER,
                                   g_param_spec_double ("upper",
							P_("Upper"),
							P_("Upper limit of ruler"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0.0,
							GTK_PARAM_READWRITE));  

  g_object_class_install_property (gobject_class,
                                   PROP_POSITION,
                                   g_param_spec_double ("position",
							P_("Position"),
							P_("Position of mark on the ruler"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0.0,
							GTK_PARAM_READWRITE));  

  g_object_class_install_property (gobject_class,
                                   PROP_MAX_SIZE,
                                   g_param_spec_double ("max-size",
							P_("Max Size"),
							P_("Maximum size of the ruler"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0.0,
							GTK_PARAM_READWRITE));  
  /**
   * GtkRuler:metric:
   *
   * The metric used for the ruler.
   *
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
                                   PROP_METRIC,
                                   g_param_spec_enum ("metric",
						      P_("Metric"),
						      P_("The metric used for the ruler"),
						      GTK_TYPE_METRIC_TYPE, 
						      GTK_PIXELS,
						      GTK_PARAM_READWRITE));  
}

static void
gtk_ruler_init (GtkRuler *ruler)
{
  ruler->backing_store = NULL;
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
gtk_ruler_set_property (GObject      *object,
 			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkRuler *ruler = GTK_RULER (object);

  switch (prop_id)
    {
    case PROP_LOWER:
      gtk_ruler_set_range (ruler, g_value_get_double (value), ruler->upper,
			   ruler->position, ruler->max_size);
      break;
    case PROP_UPPER:
      gtk_ruler_set_range (ruler, ruler->lower, g_value_get_double (value),
			   ruler->position, ruler->max_size);
      break;
    case PROP_POSITION:
      gtk_ruler_set_range (ruler, ruler->lower, ruler->upper,
			   g_value_get_double (value), ruler->max_size);
      break;
    case PROP_MAX_SIZE:
      gtk_ruler_set_range (ruler, ruler->lower, ruler->upper,
			   ruler->position,  g_value_get_double (value));
      break;
    case PROP_METRIC:
      gtk_ruler_set_metric (ruler, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_ruler_get_property (GObject      *object,
			guint         prop_id,
			GValue       *value,
			GParamSpec   *pspec)
{
  GtkRuler *ruler = GTK_RULER (object);
  
  switch (prop_id)
    {
    case PROP_LOWER:
      g_value_set_double (value, ruler->lower);
      break;
    case PROP_UPPER:
      g_value_set_double (value, ruler->upper);
      break;
    case PROP_POSITION:
      g_value_set_double (value, ruler->position);
      break;
    case PROP_MAX_SIZE:
      g_value_set_double (value, ruler->max_size);
      break;
    case PROP_METRIC:
      g_value_set_enum (value, gtk_ruler_get_metric (ruler));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_ruler_set_metric (GtkRuler      *ruler,
		      GtkMetricType  metric)
{
  g_return_if_fail (GTK_IS_RULER (ruler));

  ruler->metric = (GtkRulerMetric *) &ruler_metrics[metric];

  if (GTK_WIDGET_DRAWABLE (ruler))
    gtk_widget_queue_draw (GTK_WIDGET (ruler));

  g_object_notify (G_OBJECT (ruler), "metric");
}

/**
 * gtk_ruler_get_metric:
 * @ruler: a #GtkRuler
 *
 * Gets the units used for a #GtkRuler. See gtk_ruler_set_metric().
 *
 * Return value: the units currently used for @ruler
 **/
GtkMetricType
gtk_ruler_get_metric (GtkRuler *ruler)
{
  gint i;

  g_return_val_if_fail (GTK_IS_RULER (ruler), 0);

  for (i = 0; i < G_N_ELEMENTS (ruler_metrics); i++)
    if (ruler->metric == &ruler_metrics[i])
      return i;

  g_assert_not_reached ();

  return 0;
}

void
gtk_ruler_set_range (GtkRuler *ruler,
		     gdouble   lower,
		     gdouble   upper,
		     gdouble   position,
		     gdouble   max_size)
{
  g_return_if_fail (GTK_IS_RULER (ruler));

  g_object_freeze_notify (G_OBJECT (ruler));
  if (ruler->lower != lower)
    {
      ruler->lower = lower;
      g_object_notify (G_OBJECT (ruler), "lower");
    }
  if (ruler->upper != upper)
    {
      ruler->upper = upper;
      g_object_notify (G_OBJECT (ruler), "upper");
    }
  if (ruler->position != position)
    {
      ruler->position = position;
      g_object_notify (G_OBJECT (ruler), "position");
    }
  if (ruler->max_size != max_size)
    {
      ruler->max_size = max_size;
      g_object_notify (G_OBJECT (ruler), "max-size");
    }
  g_object_thaw_notify (G_OBJECT (ruler));

  if (GTK_WIDGET_DRAWABLE (ruler))
    gtk_widget_queue_draw (GTK_WIDGET (ruler));
}

/**
 * gtk_ruler_get_range:
 * @ruler: a #GtkRuler
 * @lower: location to store lower limit of the ruler, or %NULL
 * @upper: location to store upper limit of the ruler, or %NULL
 * @position: location to store the current position of the mark on the ruler, or %NULL
 * @max_size: location to store the maximum size of the ruler used when calculating
 *            the space to leave for the text, or %NULL.
 *
 * Retrieves values indicating the range and current position of a #GtkRuler.
 * See gtk_ruler_set_range().
 **/
void
gtk_ruler_get_range (GtkRuler *ruler,
		     gdouble  *lower,
		     gdouble  *upper,
		     gdouble  *position,
		     gdouble  *max_size)
{
  g_return_if_fail (GTK_IS_RULER (ruler));

  if (lower)
    *lower = ruler->lower;
  if (upper)
    *upper = ruler->upper;
  if (position)
    *position = ruler->position;
  if (max_size)
    *max_size = ruler->max_size;
}

void
gtk_ruler_draw_ticks (GtkRuler *ruler)
{
  g_return_if_fail (GTK_IS_RULER (ruler));

  if (GTK_RULER_GET_CLASS (ruler)->draw_ticks)
    GTK_RULER_GET_CLASS (ruler)->draw_ticks (ruler);
}

void
gtk_ruler_draw_pos (GtkRuler *ruler)
{
  g_return_if_fail (GTK_IS_RULER (ruler));

  if (GTK_RULER_GET_CLASS (ruler)->draw_pos)
     GTK_RULER_GET_CLASS (ruler)->draw_pos (ruler);
}


static void
gtk_ruler_realize (GtkWidget *widget)
{
  GtkRuler *ruler;
  GdkWindowAttr attributes;
  gint attributes_mask;

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
  GtkRuler *ruler = GTK_RULER (widget);

  if (ruler->backing_store)
    {
      g_object_unref (ruler->backing_store);
      ruler->backing_store = NULL;
    }

  if (ruler->non_gr_exp_gc)
    {
      g_object_unref (ruler->non_gr_exp_gc);
      ruler->non_gr_exp_gc = NULL;
    }

  if (GTK_WIDGET_CLASS (gtk_ruler_parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (gtk_ruler_parent_class)->unrealize) (widget);
}

static void
gtk_ruler_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkRuler *ruler = GTK_RULER (widget);

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

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      ruler = GTK_RULER (widget);

      gtk_ruler_draw_ticks (ruler);
      
      gdk_draw_drawable (widget->window,
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
      gdk_drawable_get_size (ruler->backing_store, &width, &height);
      if ((width == widget->allocation.width) &&
	  (height == widget->allocation.height))
	return;

      g_object_unref (ruler->backing_store);
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

#define __GTK_RULER_C__
#include "gtkaliasdef.c"
