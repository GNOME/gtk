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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "gtkvscale.h"
#include "gtksignal.h"
#include "gtkintl.h"

#define VALUE_SPACING 2

static gpointer parent_class;

static void     gtk_vscale_class_init       (GtkVScaleClass *klass);
static void     gtk_vscale_init             (GtkVScale      *vscale);
static gboolean gtk_vscale_expose           (GtkWidget      *widget,
                                             GdkEventExpose *event);

GtkType
gtk_vscale_get_type (void)
{
  static GtkType vscale_type = 0;
  
  if (!vscale_type)
    {
      static const GtkTypeInfo vscale_info =
      {
        "GtkVScale",
        sizeof (GtkVScale),
        sizeof (GtkVScaleClass),
        (GtkClassInitFunc) gtk_vscale_class_init,
        (GtkObjectInitFunc) gtk_vscale_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      vscale_type = gtk_type_unique (GTK_TYPE_SCALE, &vscale_info);
    }
  
  return vscale_type;
}

static void
gtk_vscale_class_init (GtkVScaleClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  range_class = GTK_RANGE_CLASS (class); 

  parent_class = g_type_class_peek_parent (class);

  range_class->slider_detail = "vscale";
  
  widget_class->expose_event = gtk_vscale_expose;
}

static void
gtk_vscale_init (GtkVScale *vscale)
{
  GtkRange *range;

  range = GTK_RANGE (vscale);
  
  range->orientation = GTK_ORIENTATION_VERTICAL;
}

GtkWidget*
gtk_vscale_new (GtkAdjustment *adjustment)
{
  GtkWidget *vscale;
  
  vscale = gtk_widget_new (GTK_TYPE_VSCALE,
			   "adjustment", adjustment,
			   NULL);
  
  return vscale;
}


/**
 * gtk_vscale_new_with_range:
 * @min: minimum value
 * @max: maximum value
 * @step: step increment (tick size) used with keyboard shortcuts
 * 
 * Creates a new vertical scale widget that lets the user
 * input a number between @min and @max with the increment @step.
 * @step must be nonzero; it's the distance the slider moves when
 * using the arrow keys to adjust the scale value.
 * 
 * Return value: a new #GtkVScale
 **/
GtkWidget*
gtk_vscale_new_with_range (gdouble min,
                           gdouble max,
                           gdouble step)
{
  GtkObject *adj;
  GtkScale *scale;
  gint digits;

  g_return_val_if_fail (min < max, NULL);
  g_return_val_if_fail (step != 0.0, NULL);

  adj = gtk_adjustment_new (min, min, max, step, 10 * step, step);
  
  scale = g_object_new (GTK_TYPE_VSCALE,
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
gtk_vscale_expose (GtkWidget      *widget,
                   GdkEventExpose *event)
{
  GtkRange *range;
  GtkVScale *vscale;
  GtkScale *scale;
  
  range = GTK_RANGE (widget);
  scale = GTK_SCALE (widget);
  vscale = GTK_VSCALE (widget);
  
  if (scale->draw_value)
    {
      PangoLayout *layout;
      PangoRectangle logical_rect;
      gchar *txt;
      gint x, y;
      GtkStateType state_type;
      gint value_spacing;

      gtk_widget_style_get (widget, "value_spacing", &value_spacing, NULL);
      
      txt = _gtk_scale_format_value (scale,
                                     GTK_RANGE (scale)->adjustment->value);
      
      layout = gtk_widget_create_pango_layout (widget, txt);
      g_free (txt);
      
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      
      switch (scale->value_pos)
        {
        case GTK_POS_LEFT:
          x = range->range_rect.x - logical_rect.width - value_spacing;
          y = range->slider_start + (range->slider_end - range->slider_start - logical_rect.height) / 2;
          y = CLAMP (y, 0, widget->allocation.height - logical_rect.height);
          break;
          
        case GTK_POS_RIGHT:
          x = range->range_rect.x + range->range_rect.width + value_spacing;
          y = range->slider_start + (range->slider_end - range->slider_start - logical_rect.height) / 2;
          y = CLAMP (y, 0, widget->allocation.height - logical_rect.height);
          break;
          
        case GTK_POS_TOP:
          x = range->range_rect.x + (range->range_rect.width - logical_rect.width) / 2;
          y = range->range_rect.y - logical_rect.height - value_spacing;
          break;
          
        case GTK_POS_BOTTOM:
          x = range->range_rect.x + (range->range_rect.width - logical_rect.width) / 2;
          y = range->range_rect.y + range->range_rect.height + value_spacing;
          break;

        default:
          g_return_val_if_reached (FALSE);
          x = 0;
          y = 0;
          break;
        }
      
      state_type = GTK_STATE_NORMAL;
      if (!GTK_WIDGET_IS_SENSITIVE (scale))
        state_type = GTK_STATE_INSENSITIVE;

      gtk_paint_layout (widget->style,
                        widget->window,
                        state_type,
			FALSE,
                        NULL,
                        widget,
                        "vscale",
                        x, y,
                        layout);

      g_object_unref (G_OBJECT (layout));
    }
  
  return (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
}
