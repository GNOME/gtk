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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gtkvruler.h"


#define RULER_WIDTH           14
#define MINIMUM_INCR          5
#define MAXIMUM_SUBDIVIDE     5
#define MAXIMUM_SCALES        10

#define ROUND(x) ((int) ((x) + 0.5))


static void gtk_vruler_class_init    (GtkVRulerClass *klass);
static void gtk_vruler_init          (GtkVRuler      *vruler);
static gint gtk_vruler_motion_notify (GtkWidget      *widget,
				      GdkEventMotion *event);
static void gtk_vruler_draw_ticks    (GtkRuler       *ruler);
static void gtk_vruler_draw_pos      (GtkRuler       *ruler);


guint
gtk_vruler_get_type (void)
{
  static guint vruler_type = 0;

  if (!vruler_type)
    {
      static const GtkTypeInfo vruler_info =
      {
	"GtkVRuler",
	sizeof (GtkVRuler),
	sizeof (GtkVRulerClass),
	(GtkClassInitFunc) gtk_vruler_class_init,
	(GtkObjectInitFunc) gtk_vruler_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      vruler_type = gtk_type_unique (gtk_ruler_get_type (), &vruler_info);
    }

  return vruler_type;
}

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
  widget->requisition.width = widget->style->klass->xthickness * 2 + RULER_WIDTH;
  widget->requisition.height = widget->style->klass->ythickness * 2 + 1;
}

GtkWidget*
gtk_vruler_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_vruler_get_type ()));
}


static gint
gtk_vruler_motion_notify (GtkWidget      *widget,
			  GdkEventMotion *event)
{
  GtkRuler *ruler;
  gint y;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_VRULER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ruler = GTK_RULER (widget);

  if (event->is_hint)
    gdk_window_get_pointer (widget->window, NULL, &y, NULL);
  else
    y = event->y;

  ruler->position = ruler->lower + ((ruler->upper - ruler->lower) * y) / widget->allocation.height;

  /*  Make sure the ruler has been allocated already  */
  if (ruler->backing_store != NULL)
    gtk_ruler_draw_pos (ruler);

  return FALSE;
}

static void
gtk_vruler_draw_ticks (GtkRuler *ruler)
{
  GtkWidget *widget;
  GdkGC *gc, *bg_gc;
  GdkFont *font;
  gint i, j;
  gint width, height;
  gint xthickness;
  gint ythickness;
  gint length, ideal_length;
  gfloat lower, upper;		/* Upper and lower limits, in ruler units */
  gfloat increment;		/* Number of pixels per unit */
  gint scale;			/* Number of units per major unit */
  gfloat subd_incr;
  gfloat start, end, cur;
  gchar unit_str[32];
  gchar digit_str[2] = { '\0', '\0' };
  gint digit_height;
  gint text_height;
  gint pos;

  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_VRULER (ruler));

  if (!GTK_WIDGET_DRAWABLE (ruler)) 
    return;

  widget = GTK_WIDGET (ruler);

  gc = widget->style->fg_gc[GTK_STATE_NORMAL];
  bg_gc = widget->style->bg_gc[GTK_STATE_NORMAL];
  font = widget->style->font;
  xthickness = widget->style->klass->xthickness;
  ythickness = widget->style->klass->ythickness;
  digit_height = font->ascent; /* assume descent == 0 ? */

  width = widget->allocation.height;
  height = widget->allocation.width - ythickness * 2;

   gtk_paint_box (widget->style, ruler->backing_store,
		  GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
		  NULL, widget, "vruler",
		  0, 0, 
		  widget->allocation.width, widget->allocation.height);

   gdk_draw_line (ruler->backing_store, gc,
		 height + xthickness,
		 ythickness,
		 height + xthickness,
		 widget->allocation.height - ythickness);

  upper = ruler->upper / ruler->metric->pixels_per_unit;
  lower = ruler->lower / ruler->metric->pixels_per_unit;

  if ((upper - lower) == 0)
    return;
  increment = (gfloat) width / (upper - lower);

  /* determine the scale
   *   use the maximum extents of the ruler to determine the largest
   *   possible number to be displayed.  Calculate the height in pixels
   *   of this displayed text. Use this height to find a scale which
   *   leaves sufficient room for drawing the ruler.  
   */
  scale = ceil (ruler->max_size / ruler->metric->pixels_per_unit);
  sprintf (unit_str, "%d", scale);
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
      subd_incr = (gfloat) ruler->metric->ruler_scale[scale] / 
	          (gfloat) ruler->metric->subdivide[i];
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

	  gdk_draw_line (ruler->backing_store, gc,
			 height + xthickness - length, pos,
			 height + xthickness, pos);

	  /* draw label */
	  if (i == 0)
	    {
	      sprintf (unit_str, "%d", (int) cur);
	      for (j = 0; j < (int) strlen (unit_str); j++)
		{
		  digit_str[0] = unit_str[j];
		  gdk_draw_string (ruler->backing_store, font, gc,
				   xthickness + 1,
				   pos + digit_height * (j + 1) + 1,
				   digit_str);
		}
	    }
	}
    }
}


static void
gtk_vruler_draw_pos (GtkRuler *ruler)
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
  g_return_if_fail (GTK_IS_VRULER (ruler));

  if (GTK_WIDGET_DRAWABLE (ruler))
    {
      widget = GTK_WIDGET (ruler);

      gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      xthickness = widget->style->klass->xthickness;
      ythickness = widget->style->klass->ythickness;
      width = widget->allocation.width - xthickness * 2;
      height = widget->allocation.height;

      bs_height = width / 2;
      bs_height |= 1;  /* make sure it's odd */
      bs_width = bs_height / 2 + 1;

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

	  increment = (gfloat) height / (ruler->upper - ruler->lower);

	  x = (width + bs_width) / 2 + xthickness;
	  y = ROUND ((ruler->position - ruler->lower) * increment) + (ythickness - bs_height) / 2 - 1;

	  for (i = 0; i < bs_width; i++)
	    gdk_draw_line (widget->window, gc,
			   x + i, y + i,
			   x + i, y + bs_height - 1 - i);

	  ruler->xsrc = x;
	  ruler->ysrc = y;
	}
    }
}
