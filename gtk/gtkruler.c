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

#include "config.h"

#include <math.h>
#include <string.h>

#include "gtkorientable.h"
#include "gtkruler.h"
#include "gtktypeutils.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#define RULER_WIDTH           14
#define MINIMUM_INCR          5
#define MAXIMUM_SUBDIVIDE     5
#define MAXIMUM_SCALES        10

#define ROUND(x) ((int) ((x) + 0.5))

struct _GtkRulerPrivate
{
  GtkOrientation        orientation;
  GtkRulerMetric       *metric;

  cairo_surface_t      *backing_store;

  gint                  slider_size;
  gint                  xsrc;
  gint                  ysrc;

  gdouble               lower;          /* The upper limit of the ruler (in points) */
  gdouble               max_size;       /* The maximum size of the ruler */
  gdouble               position;       /* The position of the mark on the ruler */
  gdouble               upper;          /* The lower limit of the ruler */
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_LOWER,
  PROP_UPPER,
  PROP_POSITION,
  PROP_MAX_SIZE,
  PROP_METRIC
};


static void     gtk_ruler_set_property    (GObject        *object,
                                           guint            prop_id,
                                           const GValue   *value,
                                           GParamSpec     *pspec);
static void     gtk_ruler_get_property    (GObject        *object,
                                           guint           prop_id,
                                           GValue         *value,
                                           GParamSpec     *pspec);
static void     gtk_ruler_realize         (GtkWidget      *widget);
static void     gtk_ruler_unrealize       (GtkWidget      *widget);
static void     gtk_ruler_get_preferred_width
                                          (GtkWidget      *widget,
                                           gint           *minimum,
                                           gint           *natural);
static void     gtk_ruler_get_preferred_height
                                          (GtkWidget        *widget,
                                           gint             *minimum,
                                           gint             *natural);
static void     gtk_ruler_size_allocate   (GtkWidget      *widget,
                                           GtkAllocation  *allocation);
static gboolean gtk_ruler_motion_notify   (GtkWidget      *widget,
                                           GdkEventMotion *event);
static gboolean gtk_ruler_draw            (GtkWidget      *widget,
                                           cairo_t        *cr);
static void     gtk_ruler_make_pixmap     (GtkRuler       *ruler);
static void     gtk_ruler_draw_ticks      (GtkRuler       *ruler);
static void     gtk_ruler_real_draw_ticks (GtkRuler       *ruler,
                                           cairo_t        *cr);
static void     gtk_ruler_real_draw_pos   (GtkRuler       *ruler,
                                           cairo_t        *cr);


static const GtkRulerMetric ruler_metrics[] =
{
  { "Pixel", "Pi", 1.0, { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000 }, { 1, 5, 10, 50, 100 }},
  { "Inches", "In", 72.0, { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 }, { 1, 2, 4, 8, 16 }},
  { "Centimeters", "Cn", 28.35, { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000 }, { 1, 5, 10, 50, 100 }},
};


G_DEFINE_TYPE_WITH_CODE (GtkRuler, gtk_ruler, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                NULL))


static void
gtk_ruler_class_init (GtkRulerClass *class)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (class);

  gobject_class->set_property = gtk_ruler_set_property;
  gobject_class->get_property = gtk_ruler_get_property;

  widget_class->realize = gtk_ruler_realize;
  widget_class->unrealize = gtk_ruler_unrealize;
  widget_class->get_preferred_width = gtk_ruler_get_preferred_width;
  widget_class->get_preferred_height = gtk_ruler_get_preferred_height;
  widget_class->size_allocate = gtk_ruler_size_allocate;
  widget_class->motion_notify_event = gtk_ruler_motion_notify;
  widget_class->draw = gtk_ruler_draw;

  class->draw_ticks = gtk_ruler_real_draw_ticks;
  class->draw_pos = gtk_ruler_real_draw_pos;

  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

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

  g_type_class_add_private (gobject_class, sizeof (GtkRulerPrivate));
}

static void
gtk_ruler_init (GtkRuler *ruler)
{
  GtkRulerPrivate *priv;

  ruler->priv = G_TYPE_INSTANCE_GET_PRIVATE (ruler,
                                             GTK_TYPE_RULER,
                                             GtkRulerPrivate);
  priv = ruler->priv;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  priv->backing_store = NULL;
  priv->xsrc = 0;
  priv->ysrc = 0;
  priv->slider_size = 0;
  priv->lower = 0;
  priv->upper = 0;
  priv->position = 0;
  priv->max_size = 0;

  gtk_ruler_set_metric (ruler, GTK_PIXELS);
}

static void
gtk_ruler_set_property (GObject      *object,
 			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkRuler *ruler = GTK_RULER (object);
  GtkRulerPrivate *priv = ruler->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      priv->orientation = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (ruler));
      break;
    case PROP_LOWER:
      gtk_ruler_set_range (ruler, g_value_get_double (value), priv->upper,
                           priv->position, priv->max_size);
      break;
    case PROP_UPPER:
      gtk_ruler_set_range (ruler, priv->lower, g_value_get_double (value),
                           priv->position, priv->max_size);
      break;
    case PROP_POSITION:
      gtk_ruler_set_range (ruler, priv->lower, priv->upper,
                           g_value_get_double (value), priv->max_size);
      break;
    case PROP_MAX_SIZE:
      gtk_ruler_set_range (ruler, priv->lower, priv->upper,
                           priv->position,  g_value_get_double (value));
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
  GtkRulerPrivate *priv = ruler->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_LOWER:
      g_value_set_double (value, priv->lower);
      break;
    case PROP_UPPER:
      g_value_set_double (value, priv->upper);
      break;
    case PROP_POSITION:
      g_value_set_double (value, priv->position);
      break;
    case PROP_MAX_SIZE:
      g_value_set_double (value, priv->max_size);
      break;
    case PROP_METRIC:
      g_value_set_enum (value, gtk_ruler_get_metric (ruler));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_ruler_new:
 * @orientation: the ruler's orientation.
 *
 * Creates a new #GtkRuler with the given orientation.
 *
 * Return value: a new #GtkRuler.
 *
 * Since: 3.0
 **/
GtkWidget *
gtk_ruler_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_RULER,
                       "orientation", orientation,
                       NULL);
}

/**
 * gtk_ruler_invalidate_ticks:
 * @ruler: the ruler to invalidate
 *
 * For performance reasons, #GtkRuler keeps a backbuffer containing the
 * prerendered contents of the ticks. To cause a repaint this buffer,
 * call this function instead of gtk_widget_queue_draw().
 **/
static void
gtk_ruler_invalidate_ticks (GtkRuler *ruler)
{
  g_return_if_fail (GTK_IS_RULER (ruler));

  if (ruler->priv->backing_store == NULL)
    return;

  gtk_ruler_draw_ticks (ruler);
  gtk_widget_queue_draw (GTK_WIDGET (ruler));
}

void
gtk_ruler_set_metric (GtkRuler      *ruler,
		      GtkMetricType  metric)
{
  GtkRulerPrivate *priv;

  g_return_if_fail (GTK_IS_RULER (ruler));

  priv = ruler->priv;

  priv->metric = (GtkRulerMetric *) &ruler_metrics[metric];

  g_object_notify (G_OBJECT (ruler), "metric");

  gtk_ruler_invalidate_ticks (ruler);
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
  GtkRulerPrivate *priv;
  gint i;

  g_return_val_if_fail (GTK_IS_RULER (ruler), 0);

  priv = ruler->priv;

  for (i = 0; i < G_N_ELEMENTS (ruler_metrics); i++)
    if (priv->metric == &ruler_metrics[i])
      return i;

  g_assert_not_reached ();

  return 0;
}

/**
 * gtk_ruler_set_range:
 * @ruler: the gtkruler
 * @lower: the lower limit of the ruler
 * @upper: the upper limit of the ruler
 * @position: the mark on the ruler
 * @max_size: the maximum size of the ruler used when calculating the space to
 * leave for the text
 *
 * This sets the range of the ruler. 
 */
void
gtk_ruler_set_range (GtkRuler *ruler,
		     gdouble   lower,
		     gdouble   upper,
		     gdouble   position,
		     gdouble   max_size)
{
  GtkRulerPrivate *priv;

  g_return_if_fail (GTK_IS_RULER (ruler));

  priv = ruler->priv;

  g_object_freeze_notify (G_OBJECT (ruler));
  if (priv->lower != lower)
    {
      priv->lower = lower;
      g_object_notify (G_OBJECT (ruler), "lower");
    }
  if (priv->upper != upper)
    {
      priv->upper = upper;
      g_object_notify (G_OBJECT (ruler), "upper");
    }
  if (priv->position != position)
    {
      priv->position = position;
      g_object_notify (G_OBJECT (ruler), "position");
    }
  if (priv->max_size != max_size)
    {
      priv->max_size = max_size;
      g_object_notify (G_OBJECT (ruler), "max-size");
    }
  g_object_thaw_notify (G_OBJECT (ruler));

  gtk_ruler_invalidate_ticks (ruler);
}

/**
 * gtk_ruler_get_range:
 * @ruler: a #GtkRuler
 * @lower: (allow-none): location to store lower limit of the ruler, or %NULL
 * @upper: (allow-none): location to store upper limit of the ruler, or %NULL
 * @position: (allow-none): location to store the current position of the mark on the ruler, or %NULL
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
  GtkRulerPrivate *priv;

  g_return_if_fail (GTK_IS_RULER (ruler));

  priv = ruler->priv;

  if (lower)
    *lower = priv->lower;
  if (upper)
    *upper = priv->upper;
  if (position)
    *position = priv->position;
  if (max_size)
    *max_size = priv->max_size;
}

static void
gtk_ruler_draw_ticks (GtkRuler *ruler)
{
  GtkRulerPrivate *priv = ruler->priv;
  cairo_t *cr;

  g_return_if_fail (GTK_IS_RULER (ruler));

  if (priv->backing_store == NULL)
    return;
  
  cr = cairo_create (priv->backing_store);

  if (GTK_RULER_GET_CLASS (ruler)->draw_ticks)
    GTK_RULER_GET_CLASS (ruler)->draw_ticks (ruler, cr);

  cairo_destroy (cr);
}

static void
gtk_ruler_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GtkRuler *ruler;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  ruler = GTK_RULER (widget);

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_POINTER_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, ruler);

  gtk_widget_style_attach (widget);
  gtk_style_set_background (gtk_widget_get_style (widget),
                            window, GTK_STATE_ACTIVE);

  gtk_ruler_make_pixmap (ruler);
}

static void
gtk_ruler_unrealize (GtkWidget *widget)
{
  GtkRuler *ruler = GTK_RULER (widget);
  GtkRulerPrivate *priv = ruler->priv;

  if (priv->backing_store)
    {
      cairo_surface_destroy (priv->backing_store);
      priv->backing_store = NULL;
    }

  GTK_WIDGET_CLASS (gtk_ruler_parent_class)->unrealize (widget);
}

static void
gtk_ruler_get_preferred_size (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              gint           *minimum,
                              gint           *natural)
{
  GtkRuler *ruler = GTK_RULER (widget);
  GtkRulerPrivate *priv = ruler->priv;
  GtkStyle *style;
  gint thickness;

  style = gtk_widget_get_style (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    thickness = style->xthickness;
  else
    thickness = style->ythickness;

  if (priv->orientation == orientation)
    *minimum = *natural = thickness * 2 + 1;
  else
    *minimum = *natural = thickness * 2 + RULER_WIDTH;
}

static void
gtk_ruler_get_preferred_width (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  gtk_ruler_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum, natural);
}

static void
gtk_ruler_get_preferred_height (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  gtk_ruler_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, minimum, natural);
}

static void
gtk_ruler_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkRuler *ruler = GTK_RULER (widget);
  GtkAllocation old_allocation;
  gboolean resized;

  gtk_widget_get_allocation (widget, &old_allocation);
  resized = (old_allocation.width != allocation->width ||
             old_allocation.height != allocation->height);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      if (resized)
        gtk_ruler_make_pixmap (ruler);
    }
}

static gboolean
gtk_ruler_motion_notify (GtkWidget      *widget,
                         GdkEventMotion *event)
{
  GtkAllocation allocation;
  GtkRuler *ruler = GTK_RULER (widget);
  GtkRulerPrivate *priv = ruler->priv;
  gint x;
  gint y;

  gdk_event_request_motions (event);
  x = event->x;
  y = event->y;

  gtk_widget_get_allocation (widget, &allocation);
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    priv->position = priv->lower + ((priv->upper - priv->lower) * x) / allocation.width;
  else
    priv->position = priv->lower + ((priv->upper - priv->lower) * y) / allocation.height;

  g_object_notify (G_OBJECT (ruler), "position");

  gtk_widget_queue_draw (widget);

  return FALSE;
}

static gboolean
gtk_ruler_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkRuler *ruler = GTK_RULER (widget);
  GtkRulerPrivate *priv = ruler->priv;

  cairo_set_source_surface (cr, priv->backing_store, 0, 0);
  cairo_paint (cr);
  
  if (GTK_RULER_GET_CLASS (ruler)->draw_pos)
     GTK_RULER_GET_CLASS (ruler)->draw_pos (ruler, cr);

  return FALSE;
}

static void
gtk_ruler_make_pixmap (GtkRuler *ruler)
{
  GtkRulerPrivate *priv = ruler->priv;
  GtkAllocation allocation;
  GtkWidget *widget;

  widget = GTK_WIDGET (ruler);

  gtk_widget_get_allocation (widget, &allocation);

  if (priv->backing_store)
    cairo_surface_destroy (priv->backing_store);

  priv->backing_store = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                                           CAIRO_CONTENT_COLOR,
                                                           allocation.width,
                                                           allocation.height);

  priv->xsrc = 0;
  priv->ysrc = 0;

  gtk_ruler_draw_ticks (ruler);
}

static void
gtk_ruler_real_draw_ticks (GtkRuler *ruler,
                           cairo_t  *cr)
{
  GtkWidget *widget = GTK_WIDGET (ruler);
  GtkRulerPrivate *priv = ruler->priv;
  GtkStyle *style;
  gint i, j;
  gint w, h;
  gint width, height;
  gint xthickness;
  gint ythickness;
  gint length, ideal_length;
  gdouble lower, upper;		/* Upper and lower limits, in ruler units */
  gdouble increment;		/* Number of pixels per unit */
  gint scale;			/* Number of units per major unit */
  gdouble subd_incr;
  gdouble start, end, cur;
  gchar unit_str[32];
  gint digit_height;
  gint digit_offset;
  gint text_width;
  gint text_height;
  gint pos;
  PangoLayout *layout;
  PangoRectangle logical_rect, ink_rect;

  style = gtk_widget_get_style (widget);

  xthickness = style->xthickness;
  ythickness = style->ythickness;

  layout = gtk_widget_create_pango_layout (widget, "012456789");
  pango_layout_get_extents (layout, &ink_rect, &logical_rect);

  digit_height = PANGO_PIXELS (ink_rect.height) + 2;
  digit_offset = ink_rect.y;

  w = gtk_widget_get_allocated_width (widget);
  h = gtk_widget_get_allocated_height (widget);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      width = w;
      height = h - ythickness * 2;
    }
  else
    {
      width = h;
      height = w - ythickness * 2;
    }

#define DETAILE(private) (priv->orientation == GTK_ORIENTATION_HORIZONTAL ? "hruler" : "vruler");

  gdk_cairo_set_source_color (cr, &style->fg[gtk_widget_get_state (widget)]);

  gtk_paint_box (style, cr,
                       GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                       widget,
                       priv->orientation == GTK_ORIENTATION_HORIZONTAL ?
                       "hruler" : "vruler",
                       0, 0,
                       w, h);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      cairo_rectangle (cr,
                       xthickness,
                       height + ythickness,
                       w - 2 * xthickness,
                       1);
    }
  else
    {
      cairo_rectangle (cr,
                       height + xthickness,
                       ythickness,
                       1,
                       h - 2 * ythickness);
    }

  upper = priv->upper / priv->metric->pixels_per_unit;
  lower = priv->lower / priv->metric->pixels_per_unit;

  if ((upper - lower) == 0)
    goto out;

  increment = (gdouble) width / (upper - lower);

  /* determine the scale H
   *  We calculate the text size as for the vruler, so that the result
   *  for the scale looks consistent with an accompanying vruler
   */
  /* determine the scale V
   *   use the maximum extents of the ruler to determine the largest
   *   possible number to be displayed.  Calculate the height in pixels
   *   of this displayed text. Use this height to find a scale which
   *   leaves sufficient room for drawing the ruler.
   */
  scale = ceil (priv->max_size / priv->metric->pixels_per_unit);
  g_snprintf (unit_str, sizeof (unit_str), "%d", scale);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      text_width = strlen (unit_str) * digit_height + 1;

      for (scale = 0; scale < MAXIMUM_SCALES; scale++)
        if (priv->metric->ruler_scale[scale] * fabs(increment) > 2 * text_width)
          break;
    }
  else
    {
      text_height = strlen (unit_str) * digit_height + 1;

      for (scale = 0; scale < MAXIMUM_SCALES; scale++)
        if (priv->metric->ruler_scale[scale] * fabs(increment) > 2 * text_height)
          break;
    }

  if (scale == MAXIMUM_SCALES)
    scale = MAXIMUM_SCALES - 1;

  /* drawing starts here */
  length = 0;
  for (i = MAXIMUM_SUBDIVIDE - 1; i >= 0; i--)
    {
      subd_incr = (gdouble) priv->metric->ruler_scale[scale] /
	          (gdouble) priv->metric->subdivide[i];
      if (subd_incr * fabs(increment) <= MINIMUM_INCR)
	continue;

      /* Calculate the length of the tickmarks. Make sure that
       * this length increases for each set of ticks
       */
      ideal_length = height / (i + 1) - 1;
      if (ideal_length > ++length)
	length = ideal_length;

      if (lower < upper)
	{
	  start = floor (lower / subd_incr) * subd_incr;
	  end   = ceil  (upper / subd_incr) * subd_incr;
	}
      else
	{
	  start = floor (upper / subd_incr) * subd_incr;
	  end   = ceil  (lower / subd_incr) * subd_incr;
	}

      for (cur = start; cur <= end; cur += subd_incr)
	{
	  pos = ROUND ((cur - lower) * increment);

          if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              cairo_rectangle (cr,
                               pos, height + ythickness - length,
                               1,   length);
            }
          else
            {
              cairo_rectangle (cr,
                               height + xthickness - length, pos,
                               length,                       1);
            }
          cairo_fill (cr);

	  /* draw label */
	  if (i == 0)
	    {
	      g_snprintf (unit_str, sizeof (unit_str), "%d", (int) cur);

              if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
                {
                  pango_layout_set_text (layout, unit_str, -1);
                  pango_layout_get_extents (layout, &logical_rect, NULL);

                  gtk_paint_layout (style,
                                          cr,
                                          gtk_widget_get_state (widget),
                                          FALSE,
                                          widget,
                                          "hruler",
                                          pos + 2, ythickness + PANGO_PIXELS (logical_rect.y - digit_offset),
                                          layout);
                }
              else
                {
                  for (j = 0; j < (int) strlen (unit_str); j++)
                    {
                      pango_layout_set_text (layout, unit_str + j, 1);
                      pango_layout_get_extents (layout, NULL, &logical_rect);

                      gtk_paint_layout (style,
                                              cr,
                                              gtk_widget_get_state (widget),
                                              FALSE,
                                              widget,
                                              "vruler",
                                              xthickness + 1,
                                              pos + digit_height * j + 2 + PANGO_PIXELS (logical_rect.y - digit_offset),
                                              layout);
                    }
                }
	    }
	}
    }

  cairo_fill (cr);

out:
  g_object_unref (layout);
}

static void
gtk_ruler_real_draw_pos (GtkRuler *ruler,
                         cairo_t  *cr)
{
  GtkWidget *widget = GTK_WIDGET (ruler);
  GtkRulerPrivate *priv = ruler->priv;
  GtkStyle *style;
  gint x, y, width, height;
  gint bs_width, bs_height;
  gint xthickness;
  gint ythickness;
  gdouble increment;

  style = gtk_widget_get_style (widget);
  xthickness = style->xthickness;
  ythickness = style->ythickness;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      height -= ythickness * 2;

      bs_width = height / 2 + 2;
      bs_width |= 1;  /* make sure it's odd */
      bs_height = bs_width / 2 + 1;
    }
  else
    {
      width -= xthickness * 2;

      bs_height = width / 2 + 2;
      bs_height |= 1;  /* make sure it's odd */
      bs_width = bs_height / 2 + 1;
    }

  if ((bs_width > 0) && (bs_height > 0))
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          increment = (gdouble) width / (priv->upper - priv->lower);

          x = ROUND ((priv->position - priv->lower) * increment) + (xthickness - bs_width) / 2 - 1;
          y = (height + bs_height) / 2 + ythickness;
        }
      else
        {
          increment = (gdouble) height / (priv->upper - priv->lower);

          x = (width + bs_width) / 2 + xthickness;
          y = ROUND ((priv->position - priv->lower) * increment) + (ythickness - bs_height) / 2 - 1;
        }

      gdk_cairo_set_source_color (cr, &style->fg[gtk_widget_get_state (widget)]);

      cairo_move_to (cr, x, y);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          cairo_line_to (cr, x + bs_width / 2.0, y + bs_height);
          cairo_line_to (cr, x + bs_width,       y);
        }
      else
        {
          cairo_line_to (cr, x + bs_width, y + bs_height / 2.0);
          cairo_line_to (cr, x,            y + bs_height);
        }

      cairo_fill (cr);

      priv->xsrc = x;
      priv->ysrc = y;
    }
}
