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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gtkhruler.h"


#define RULER_HEIGHT          14
#define MINIMUM_INCR          5
#define MAXIMUM_SUBDIVIDE     5
#define MAXIMUM_SCALES        10

#define ROUND(x) ((int) ((x) + 0.5))


static void gtk_hruler_class_init    (GtkHRulerClass *klass);
static void gtk_hruler_init          (GtkHRuler      *hruler);
static gint gtk_hruler_motion_notify (GtkWidget      *widget,
				      GdkEventMotion *event);
static void gtk_hruler_draw_ticks    (GtkRuler       *ruler);
static void gtk_hruler_draw_pos      (GtkRuler       *ruler);


guint
gtk_hruler_get_type ()
{
  static guint hruler_type = 0;

  if (!hruler_type)
    {
      GtkTypeInfo hruler_info =
      {
	"GtkHRuler",
	sizeof (GtkHRuler),
	sizeof (GtkHRulerClass),
	(GtkClassInitFunc) gtk_hruler_class_init,
	(GtkObjectInitFunc) gtk_hruler_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      hruler_type = gtk_type_unique (gtk_ruler_get_type (), &hruler_info);
    }

  return hruler_type;
}

static void
gtk_hruler_class_init (GtkHRulerClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkRulerClass *ruler_class;

  widget_class = (GtkWidgetClass*) klass;
  ruler_class = (GtkRulerClass*) klass;

  widget_class->motion_notify_event = gtk_hruler_motion_notify;

  ruler_class->draw_ticks = gtk_hruler_draw_ticks;
  ruler_class->draw_pos = gtk_hruler_draw_pos;
}

static void
gtk_hruler_init (GtkHRuler *hruler)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (hruler);
  widget->requisition.width = widget->style->klass->xthickness * 2 + 1;
  widget->requisition.height = widget->style->klass->ythickness * 2 + RULER_HEIGHT;
}


GtkWidget*
gtk_hruler_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_hruler_get_type ()));
}

static gint
gtk_hruler_motion_notify (GtkWidget      *widget,
			  GdkEventMotion *event)
{
  GtkRuler *ruler;
  gint x;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HRULER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ruler = GTK_RULER (widget);

  if (event->is_hint)
    gdk_window_get_pointer (widget->window, &x, NULL, NULL);
  else
    x = event->x;

  ruler->position = ruler->lower + ((ruler->upper - ruler->lower) * x) / widget->allocation.width;

  /*  Make sure the ruler has been allocated already  */
  if (ruler->backing_store != NULL)
    gtk_ruler_draw_pos (ruler);

  return FALSE;
}

static void
gtk_hruler_draw_ticks (GtkRuler *ruler)
{
  GtkWidget *widget;
  GdkGC *gc, *bg_gc;
  GdkFont *font;
  gint i;
  gint width, height;
  gint xthickness;
  gint ythickness;
  gint length, ideal_length;
  gfloat subd_incr;
  gfloat step_incr;
  gfloat increment;
  gfloat start, end, cur;
  gchar unit_str[12];
  gint text_height;
  gint digit_height;
  gint pos;
  gint scale;

  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_HRULER (ruler));

  if (GTK_WIDGET_DRAWABLE (ruler))
    {
      widget = GTK_WIDGET (ruler);

      gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      bg_gc = widget->style->bg_gc[GTK_STATE_NORMAL];
      font = widget->style->font;
      
      xthickness = widget->style->klass->xthickness;
      ythickness = widget->style->klass->ythickness;
      digit_height = font->ascent; /* assume descent == 0? */

      width = widget->allocation.width;
      height = widget->allocation.height - ythickness * 2;
      gdk_draw_line (ruler->backing_store, gc,
		     xthickness,
		     height + ythickness,
		     widget->allocation.width - xthickness,
		     height + ythickness);

      if ((ruler->upper - ruler->lower) == 0)
	return;

      increment = (gfloat) width * ruler->metric->pixels_per_unit / (ruler->upper - ruler->lower);

      /*  determine the scale
       *   use the maximum extents of the ruler to determine the largest possible
       *   number to be displayed.  calculate the height in pixels of this displayed
       *   text as for the vertical ruler case.  use this height to find a scale
       *   which leaves sufficient room for drawing the ruler.
       */
      scale = ceil (ruler->max_size / ruler->metric->pixels_per_unit);
      sprintf (unit_str, "%d", scale);
      text_height = strlen (unit_str) * digit_height + 1;

      for (scale = 0; scale < MAXIMUM_SCALES; scale++)
	if (ruler->metric->ruler_scale[scale] * increment > 2 * text_height)
	  break;

      if (scale == MAXIMUM_SCALES)
	scale = MAXIMUM_SCALES - 1;

      length = 0;
      for (i = MAXIMUM_SUBDIVIDE - 1; i >= 0; i--)
	{
	  subd_incr = (gfloat) ruler->metric->ruler_scale[scale] / (gfloat) ruler->metric->subdivide[i];
	  step_incr = subd_incr * increment;
	  if (step_incr <= MINIMUM_INCR)
	    continue;

	  start = floor ((ruler->lower / ruler->metric->pixels_per_unit) / subd_incr) * subd_incr;
	  end = ceil ((ruler->upper / ruler->metric->pixels_per_unit) / subd_incr) * subd_incr;

	  ideal_length = height / (i + 1) - 1;
	  if (ideal_length > ++length)
	      length = ideal_length;
	  
	  cur = start;
	  while (cur <= end)
	    {
	      pos = ROUND ((cur - (ruler->lower / ruler->metric->pixels_per_unit)) * increment);

	      gdk_draw_line (ruler->backing_store, gc,
			     pos, height + ythickness, pos,
			     height - length + ythickness);
	      if (i == 0)
		{
		  sprintf (unit_str, "%d", (int) cur);
		  gdk_draw_rectangle (ruler->backing_store,
				      bg_gc, TRUE,
				      pos + 1, ythickness,
				      gdk_string_width(font, unit_str) + 1,
				      digit_height);
		  gdk_draw_string (ruler->backing_store, font, gc,
				   pos + 2, ythickness + font->ascent - 1,
				   unit_str);
		}

	      cur += subd_incr;
	    }
	}
    }
}

static void
gtk_hruler_draw_pos (GtkRuler *ruler)
{
  GtkWidget *widget;
  GdkGC *gc;
  int i;
  gint x, y;
  gint width, height;
  gint bs_width, bs_height;
  gint xthickness;
  gint ythickness;
  gfloat increment;

  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_HRULER (ruler));

  if (GTK_WIDGET_DRAWABLE (ruler))
    {
      widget = GTK_WIDGET (ruler);

      gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      xthickness = widget->style->klass->xthickness;
      ythickness = widget->style->klass->ythickness;
      width = widget->allocation.width;
      height = widget->allocation.height - ythickness * 2;

      bs_width = height / 2;
      bs_width |= 1;  /* make sure it's odd */
      bs_height = bs_width / 2 + 1;

      if ((bs_width > 0) && (bs_height > 0))
	{
	  /*  If a backing store exists, restore the ruler  */
	  if (ruler->backing_store && ruler->non_gr_exp_gc)
	    gdk_draw_pixmap (ruler->widget.window,
			     ruler->non_gr_exp_gc,
			     ruler->backing_store,
			     ruler->xsrc, ruler->ysrc,
			     ruler->xsrc, ruler->ysrc,
			     bs_width, bs_height);

	  increment = (gfloat) width / (ruler->upper - ruler->lower);

	  x = ROUND ((ruler->position - ruler->lower) * increment) + (xthickness - bs_width) / 2 - 1;
	  y = (height + bs_height) / 2 + ythickness;

	  for (i = 0; i < bs_height; i++)
	    gdk_draw_line (widget->window, gc,
			   x + i, y + i,
			   x + bs_width - 1 - i, y + i);


	  ruler->xsrc = x;
	  ruler->ysrc = y;
	}
    }
}
