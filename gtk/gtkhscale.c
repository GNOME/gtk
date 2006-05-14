/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 Red Hat, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include "gtkhscale.h"
#include "gtkintl.h"
#include "gtkalias.h"

static gboolean gtk_hscale_expose           (GtkWidget      *widget,
                                             GdkEventExpose *event);

static void     gtk_hscale_get_layout_offsets (GtkScale       *scale,
                                               gint           *x,
                                               gint           *y);

G_DEFINE_TYPE (GtkHScale, gtk_hscale, GTK_TYPE_SCALE)

static void
gtk_hscale_class_init (GtkHScaleClass *class)
{
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;
  GtkScaleClass *scale_class;
  
  widget_class = GTK_WIDGET_CLASS (class);
  range_class = GTK_RANGE_CLASS (class);
  scale_class = GTK_SCALE_CLASS (class);

  range_class->slider_detail = "hscale";

  scale_class->get_layout_offsets = gtk_hscale_get_layout_offsets;
  
  widget_class->expose_event = gtk_hscale_expose;
}

static void
gtk_hscale_init (GtkHScale *hscale)
{
  GtkRange *range;

  range = GTK_RANGE (hscale);

  range->orientation = GTK_ORIENTATION_HORIZONTAL;
  range->flippable = TRUE;
}

GtkWidget*
gtk_hscale_new (GtkAdjustment *adjustment)
{
  return g_object_new (GTK_TYPE_HSCALE, "adjustment", adjustment, NULL);
}

/**
 * gtk_hscale_new_with_range:
 * @min: minimum value
 * @max: maximum value
 * @step: step increment (tick size) used with keyboard shortcuts
 * 
 * Creates a new horizontal scale widget that lets the user input a
 * number between @min and @max (including @min and @max) with the
 * increment @step.  @step must be nonzero; it's the distance the
 * slider moves when using the arrow keys to adjust the scale value.
 * 
 * Note that the way in which the precision is derived works best if @step 
 * is a power of ten. If the resulting precision is not suitable for your 
 * needs, use gtk_scale_set_digits() to correct it.
 * 
 * Return value: a new #GtkHScale
 **/
GtkWidget*
gtk_hscale_new_with_range (gdouble min,
                           gdouble max,
                           gdouble step)
{
  GtkObject *adj;
  GtkScale *scale;
  gint digits;

  g_return_val_if_fail (min < max, NULL);
  g_return_val_if_fail (step != 0.0, NULL);

  adj = gtk_adjustment_new (min, min, max, step, 10 * step, 0);
  
  scale = g_object_new (GTK_TYPE_HSCALE,
                        "adjustment", adj,
                        NULL);

  if (fabs (step) >= 1.0 || step == 0.0)
    digits = 0;
  else {
    digits = abs ((gint) floor (log10 (fabs (step))));
    if (digits > 5)
      digits = 5;
  }

  gtk_scale_set_digits (scale, digits);
  
  return GTK_WIDGET (scale);
}

static gboolean
gtk_hscale_expose (GtkWidget      *widget,
                   GdkEventExpose *event)
{
  GtkScale *scale;
  
  scale = GTK_SCALE (widget);

  /* We need to chain up _first_ so the various geometry members of
   * GtkRange struct are updated.
   */
  if (GTK_WIDGET_CLASS (gtk_hscale_parent_class)->expose_event)
    GTK_WIDGET_CLASS (gtk_hscale_parent_class)->expose_event (widget, event);

  if (scale->draw_value)
    {
      PangoLayout *layout;
      gint x, y;
      GtkStateType state_type;

      layout = gtk_scale_get_layout (scale);
      gtk_scale_get_layout_offsets (scale, &x, &y);

      state_type = GTK_STATE_NORMAL;
      if (!GTK_WIDGET_IS_SENSITIVE (scale))
        state_type = GTK_STATE_INSENSITIVE;

      gtk_paint_layout (widget->style,
                        widget->window,
                        state_type,
			FALSE,
                        NULL,
                        widget,
                        "hscale",
                        x, y,
                        layout);

    }
  
  return FALSE;
}

static void     
gtk_hscale_get_layout_offsets (GtkScale *scale,
                               gint     *x,
                               gint     *y)
{
  GtkWidget *widget;
  GtkRange *range;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint value_spacing;

  widget = GTK_WIDGET (scale);
  layout = gtk_scale_get_layout (scale);
      
  if (!layout)    
    {
      *x = 0;
      *y = 0;

      return;
    }

  gtk_widget_style_get (widget, "value-spacing", &value_spacing, NULL);

  range = GTK_RANGE (widget);
  scale = GTK_SCALE (widget);

  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      
  switch (scale->value_pos)
    {
     case GTK_POS_LEFT:
       *x = range->range_rect.x - value_spacing - logical_rect.width;
       *y = range->range_rect.y + (range->range_rect.height - logical_rect.height) / 2;
       break;
          
    case GTK_POS_RIGHT:
      *x = range->range_rect.x + range->range_rect.width + value_spacing;
      *y = range->range_rect.y + (range->range_rect.height - logical_rect.height) / 2;
      break;
          
    case GTK_POS_TOP:
      *x = range->slider_start +
        (range->slider_end - range->slider_start - logical_rect.width) / 2;
      *x = CLAMP (*x, 0, widget->allocation.width - logical_rect.width);
      *y = range->range_rect.y - logical_rect.height - value_spacing;
      break;
          
    case GTK_POS_BOTTOM:
      *x = range->slider_start +
        (range->slider_end - range->slider_start - logical_rect.width) / 2;
      *x = CLAMP (*x, 0, widget->allocation.width - logical_rect.width);
      *y = range->range_rect.y + range->range_rect.height + value_spacing;
      break;

    default:
      g_return_if_reached ();
      *x = 0;
      *y = 0;
      break;
    }

  *x += widget->allocation.x;
  *y += widget->allocation.y;
}

#define __GTK_HSCALE_C__
#include "gtkaliasdef.c"
