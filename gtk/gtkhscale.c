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

#include <stdio.h>
#include "gtkhscale.h"
#include "gtksignal.h"
#include "gtkintl.h"

static gpointer parent_class;

static void     gtk_hscale_class_init       (GtkHScaleClass *klass);
static void     gtk_hscale_init             (GtkHScale      *hscale);
static gboolean gtk_hscale_expose           (GtkWidget      *widget,
                                             GdkEventExpose *event);

GtkType
gtk_hscale_get_type (void)
{
  static GtkType hscale_type = 0;
  
  if (!hscale_type)
    {
      static const GtkTypeInfo hscale_info =
      {
        "GtkHScale",
        sizeof (GtkHScale),
        sizeof (GtkHScaleClass),
        (GtkClassInitFunc) gtk_hscale_class_init,
        (GtkObjectInitFunc) gtk_hscale_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      hscale_type = gtk_type_unique (GTK_TYPE_SCALE, &hscale_info);
    }
  
  return hscale_type;
}

static void
gtk_hscale_class_init (GtkHScaleClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  range_class = GTK_RANGE_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  range_class->slider_detail = "hscale";
  
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
  GtkWidget *hscale;
  
  hscale = gtk_widget_new (GTK_TYPE_HSCALE,
			   "adjustment", adjustment,
			   NULL);

  return hscale;
}

static gboolean
gtk_hscale_expose (GtkWidget      *widget,
                   GdkEventExpose *event)
{
  GtkRange *range;
  GtkHScale *hscale;
  GtkScale *scale;
  
  range = GTK_RANGE (widget);
  scale = GTK_SCALE (widget);
  hscale = GTK_HSCALE (widget);
  
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
          x = range->range_rect.x - value_spacing - logical_rect.width;
          y = range->range_rect.y + (range->range_rect.height - logical_rect.height) / 2;
          break;
          
        case GTK_POS_RIGHT:
          x = range->range_rect.x + range->range_rect.width + value_spacing;
          y = range->range_rect.y + (range->range_rect.height - logical_rect.height) / 2;
          break;
          
        case GTK_POS_TOP:
          x = range->slider_start +
            (range->slider_end - range->slider_start - logical_rect.width) / 2;
          x = CLAMP (x, 0, widget->allocation.width - logical_rect.width);
          y = range->range_rect.y - logical_rect.height - value_spacing;
          break;
          
        case GTK_POS_BOTTOM:
          x = range->slider_start +
            (range->slider_end - range->slider_start - logical_rect.width) / 2;
          x = CLAMP (x, 0, widget->allocation.width - logical_rect.width);
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
                        "hscale",
                        x, y,
                        layout);

      g_object_unref (G_OBJECT (layout));
    }

  return (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
}
