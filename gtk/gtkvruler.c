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
#include <math.h>
#include <string.h>
#include "gtkvruler.h"
#include "gtkintl.h"
#include "gtkalias.h"

#include <glib/gprintf.h>


#define RULER_WIDTH           14
#define MINIMUM_INCR          5
#define MAXIMUM_SUBDIVIDE     5
#define MAXIMUM_SCALES        10

#define ROUND(x) ((int) ((x) + 0.5))


static gint gtk_vruler_motion_notify (GtkWidget      *widget,
				      GdkEventMotion *event);
static void gtk_vruler_draw_ticks    (GtkRuler       *ruler);
static void gtk_vruler_draw_pos      (GtkRuler       *ruler);

G_DEFINE_TYPE (GtkVRuler, gtk_vruler, GTK_TYPE_RULER)

static void
gtk_vruler_class_init (GtkVRulerClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkRulerClass *ruler_class;

  widget_class = (GtkWidgetClass*) klass;
  ruler_class = (GtkRulerClass*) klass;

  widget_class->motion_notify_event = gtk_vruler_motion_notify;

  ruler_class->draw_ticks = gtk_vruler_draw_ticks;
  ruler_class->draw_pos = gtk_vruler_draw_pos;
}

static void
gtk_vruler_init (GtkVRuler *vruler)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (vruler);
  widget->requisition.width = widget->style->xthickness * 2 + RULER_WIDTH;
  widget->requisition.height = widget->style->ythickness * 2 + 1;
}

GtkWidget*
gtk_vruler_new (void)
{
  return g_object_new (GTK_TYPE_VRULER, NULL);
}


static gint
gtk_vruler_motion_notify (GtkWidget      *widget,
			  GdkEventMotion *event)
{
  GtkRuler *ruler;
  gint y;

  ruler = GTK_RULER (widget);

  if (event->is_hint)
    gdk_window_get_pointer (widget->window, NULL, &y, NULL);
  else
    y = event->y;

  ruler->position = ruler->lower + ((ruler->upper - ruler->lower) * y) / widget->allocation.height;
  g_object_notify (G_OBJECT (ruler), "position");

  /*  Make sure the ruler has been allocated already  */
  if (ruler->backing_store != NULL)
    gtk_ruler_draw_pos (ruler);

  return FALSE;
}

static void
gtk_vruler_draw_ticks (GtkRuler *ruler)
{
  GtkWidget *widget;
  cairo_t *cr;
  gint i, j;
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
  gint text_height;
  gint pos;
  PangoLayout *layout;
  PangoRectangle logical_rect, ink_rect;

  if (!GTK_WIDGET_DRAWABLE (ruler)) 
    return;

  widget = GTK_WIDGET (ruler);

  xthickness = widget->style->xthickness;
  ythickness = widget->style->ythickness;

  layout = gtk_widget_create_pango_layout (widget, "012456789");
  pango_layout_get_extents (layout, &ink_rect, &logical_rect);
  
  digit_height = PANGO_PIXELS (ink_rect.height) + 2;
  digit_offset = ink_rect.y;

  width = widget->allocation.height;
  height = widget->allocation.width - ythickness * 2;

  gtk_paint_box (widget->style, ruler->backing_store,
		 GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
		 NULL, widget, "vruler",
		 0, 0, 
		  widget->allocation.width, widget->allocation.height);
  
  cr = gdk_cairo_create (ruler->backing_store);
  gdk_cairo_set_source_color (cr, &widget->style->fg[widget->state]);
 
  cairo_rectangle (cr, 
		   height + xthickness,
		   ythickness,
		   1,
		   widget->allocation.height - 2 * ythickness);

  upper = ruler->upper / ruler->metric->pixels_per_unit;
  lower = ruler->lower / ruler->metric->pixels_per_unit;

  if ((upper - lower) == 0)
    goto out;

  increment = (gdouble) width / (upper - lower);

  /* determine the scale
   *   use the maximum extents of the ruler to determine the largest
   *   possible number to be displayed.  Calculate the height in pixels
   *   of this displayed text. Use this height to find a scale which
   *   leaves sufficient room for drawing the ruler.  
   */
  scale = ceil (ruler->max_size / ruler->metric->pixels_per_unit);
  g_snprintf (unit_str, sizeof (unit_str), "%d", scale);
  text_height = strlen (unit_str) * digit_height + 1;

  for (scale = 0; scale < MAXIMUM_SCALES; scale++)
    if (ruler->metric->ruler_scale[scale] * fabs(increment) > 2 * text_height)
      break;

  if (scale == MAXIMUM_SCALES)
    scale = MAXIMUM_SCALES - 1;

  /* drawing starts here */
  length = 0;
  for (i = MAXIMUM_SUBDIVIDE - 1; i >= 0; i--)
    {
      subd_incr = (gdouble) ruler->metric->ruler_scale[scale] / 
	          (gdouble) ruler->metric->subdivide[i];
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

	  cairo_rectangle (cr, 
			   height + xthickness - length, pos,
			   length,                       1);

	  /* draw label */
	  if (i == 0)
	    {
	      g_snprintf (unit_str, sizeof (unit_str), "%d", (int) cur);
	      
	      for (j = 0; j < (int) strlen (unit_str); j++)
		{
		  pango_layout_set_text (layout, unit_str + j, 1);
		  pango_layout_get_extents (layout, NULL, &logical_rect);

      
                  gtk_paint_layout (widget->style,
                                    ruler->backing_store,
                                    GTK_WIDGET_STATE (widget),
				    FALSE,
                                    NULL,
                                    widget,
                                    "vruler",
                                    xthickness + 1,
                                    pos + digit_height * j + 2 + PANGO_PIXELS (logical_rect.y - digit_offset),
                                    layout);
		}
	    }
	}
    }

  cairo_fill (cr);
out:
  cairo_destroy (cr);

  g_object_unref (layout);
}


static void
gtk_vruler_draw_pos (GtkRuler *ruler)
{
  GtkWidget *widget = GTK_WIDGET (ruler);
  gint x, y;
  gint width, height;
  gint bs_width, bs_height;
  gint xthickness;
  gint ythickness;
  gdouble increment;

  if (GTK_WIDGET_DRAWABLE (ruler))
    {
      xthickness = widget->style->xthickness;
      ythickness = widget->style->ythickness;
      width = widget->allocation.width - xthickness * 2;
      height = widget->allocation.height;

      bs_height = width / 2 + 2;
      bs_height |= 1;  /* make sure it's odd */
      bs_width = bs_height / 2 + 1;

      if ((bs_width > 0) && (bs_height > 0))
	{
	  cairo_t *cr = gdk_cairo_create (widget->window);
      
	  /*  If a backing store exists, restore the ruler  */
	  if (ruler->backing_store)
	    gdk_draw_drawable (widget->window,
			       widget->style->black_gc,
			       ruler->backing_store,
			       ruler->xsrc, ruler->ysrc,
			       ruler->xsrc, ruler->ysrc,
			       bs_width, bs_height);

	  increment = (gdouble) height / (ruler->upper - ruler->lower);

	  x = (width + bs_width) / 2 + xthickness;
	  y = ROUND ((ruler->position - ruler->lower) * increment) + (ythickness - bs_height) / 2 - 1;
	  
	  gdk_cairo_set_source_color (cr, &widget->style->fg[widget->state]);

	  cairo_move_to (cr, x,            y);
	  cairo_line_to (cr, x + bs_width, y + bs_height / 2.);
	  cairo_line_to (cr, x,            y + bs_height);
	  cairo_fill (cr);

	  cairo_destroy (cr);

	  ruler->xsrc = x;
	  ruler->ysrc = y;
	}
    }
}

#define __GTK_VRULER_C__
#include "gtkaliasdef.c"
