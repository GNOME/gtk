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

#include <stdio.h>
#include "gtkhscale.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"


#define SCALE_CLASS(w)  GTK_SCALE_CLASS (GTK_OBJECT (w)->klass)
#define RANGE_CLASS(w)  GTK_RANGE_CLASS (GTK_OBJECT (w)->klass)

enum {
  ARG_0,
  ARG_ADJUSTMENT
};

static void gtk_hscale_class_init    (GtkHScaleClass *klass);
static void gtk_hscale_init          (GtkHScale      *hscale);
static void gtk_hscale_set_arg       (GtkObject      *object,
                                      GtkArg         *arg,
                                      guint           arg_id);
static void gtk_hscale_get_arg       (GtkObject      *object,
                                      GtkArg         *arg,
                                      guint           arg_id);
static void gtk_hscale_realize       (GtkWidget      *widget);
static void gtk_hscale_size_request  (GtkWidget      *widget,
                                      GtkRequisition *requisition);
static void gtk_hscale_size_allocate (GtkWidget      *widget,
                                      GtkAllocation  *allocation);
static void gtk_hscale_pos_trough    (GtkHScale      *hscale,
                                      gint           *x,
                                      gint           *y,
                                      gint           *w,
                                      gint           *h);
static void gtk_hscale_pos_background (GtkHScale     *hscale,
                                       gint          *x,
                                       gint          *y,
                                       gint          *w,
                                       gint          *h);
static void gtk_hscale_draw_slider   (GtkRange       *range);
static void gtk_hscale_draw_value    (GtkScale       *scale);
static void gtk_hscale_draw          (GtkWidget      *widget,
                                      GdkRectangle   *area);
static gint gtk_hscale_trough_keys   (GtkRange *range,
                                      GdkEventKey *key,
                                      GtkScrollType *scroll,
                                      GtkTroughType *pos);
static void gtk_hscale_clear_background (GtkRange    *range);

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
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;
  GtkScaleClass *scale_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  range_class = (GtkRangeClass*) class;
  scale_class = (GtkScaleClass*) class;
  
  gtk_object_add_arg_type ("GtkHScale::adjustment",
                           GTK_TYPE_ADJUSTMENT,
                           GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
                           ARG_ADJUSTMENT);
  
  object_class->set_arg = gtk_hscale_set_arg;
  object_class->get_arg = gtk_hscale_get_arg;
  
  widget_class->realize = gtk_hscale_realize;
  widget_class->size_request = gtk_hscale_size_request;
  widget_class->size_allocate = gtk_hscale_size_allocate;
  widget_class->draw = gtk_hscale_draw;
  
  range_class->slider_update = gtk_range_default_hslider_update;
  range_class->trough_click = gtk_range_default_htrough_click;
  range_class->motion = gtk_range_default_hmotion;
  range_class->draw_slider = gtk_hscale_draw_slider;
  range_class->trough_keys = gtk_hscale_trough_keys;
  range_class->clear_background = gtk_hscale_clear_background;
  
  scale_class->draw_value = gtk_hscale_draw_value;
}

static void
gtk_hscale_set_arg (GtkObject          *object,
                    GtkArg             *arg,
                    guint               arg_id)
{
  GtkHScale *hscale;
  
  hscale = GTK_HSCALE (object);
  
  switch (arg_id)
    {
    case ARG_ADJUSTMENT:
      gtk_range_set_adjustment (GTK_RANGE (hscale), GTK_VALUE_POINTER (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_hscale_get_arg (GtkObject          *object,
                    GtkArg             *arg,
                    guint               arg_id)
{
  GtkHScale *hscale;
  
  hscale = GTK_HSCALE (object);
  
  switch (arg_id)
    {
    case ARG_ADJUSTMENT:
      GTK_VALUE_POINTER (*arg) = GTK_RANGE (hscale);
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_hscale_init (GtkHScale *hscale)
{
  GTK_WIDGET_SET_FLAGS (hscale, GTK_NO_WINDOW);
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


static void
gtk_hscale_realize (GtkWidget *widget)
{
  GtkRange *range;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint x, y, w, h;
  gint slider_width;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HSCALE (widget));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  range = GTK_RANGE (widget);

  _gtk_range_get_props (range, &slider_width, NULL, NULL, NULL);
  
  widget->window = gtk_widget_get_parent_window (widget);
  gdk_window_ref (widget->window);
  
  gtk_hscale_pos_trough (GTK_HSCALE (widget), &x, &y, &w, &h);

  attributes.x = x;
  attributes.y = y;
  attributes.width = w;
  attributes.height = h;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  
  attributes.event_mask = gtk_widget_get_events (widget) | 
    (GDK_EXPOSURE_MASK |
     GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK |
     GDK_ENTER_NOTIFY_MASK |
     GDK_LEAVE_NOTIFY_MASK);
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  
  range->trough = gdk_window_new (widget->window, &attributes, attributes_mask);
  
  attributes.width = SCALE_CLASS (range)->slider_length;
  attributes.height = slider_width;
  attributes.event_mask |= (GDK_BUTTON_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK);
  
  range->slider = gdk_window_new (range->trough, &attributes, attributes_mask);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  gdk_window_set_user_data (range->trough, widget);
  gdk_window_set_user_data (range->slider, widget);
  
  gtk_style_set_background (widget->style, range->trough, GTK_STATE_ACTIVE);
  gtk_style_set_background (widget->style, range->slider, GTK_STATE_NORMAL);
  
  gtk_range_slider_update (GTK_RANGE (widget));
  
  gdk_window_show (range->slider);
}

static void
gtk_hscale_draw (GtkWidget    *widget,
                 GdkRectangle *area)
{
  GtkRange *range;
  GdkRectangle tmp_area;
  GdkRectangle child_area;
  gint x, y, width, height;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_RANGE (widget));
  g_return_if_fail (area != NULL);
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      range = GTK_RANGE (widget);
      
      gtk_hscale_pos_background (GTK_HSCALE (widget), &x, &y, &width, &height);
      
      tmp_area.x = x;
      tmp_area.y = y;
      tmp_area.width = width;
      tmp_area.height = height;
      
      if (gdk_rectangle_intersect (area, &tmp_area, &child_area))
        gtk_range_draw_background (range);
      
      gtk_hscale_pos_trough (GTK_HSCALE (widget), &x, &y, &width, &height);
      
      tmp_area.x = x;
      tmp_area.y = y;
      tmp_area.width = width;
      tmp_area.height = height;
      
      if (gdk_rectangle_intersect (area, &tmp_area, &child_area))
        {
          gtk_range_draw_trough (range);
          gtk_range_draw_slider (range);
          gtk_range_draw_step_forw (range);
          gtk_range_draw_step_back (range);
        }
    }
}

static void 
gtk_hscale_clear_background (GtkRange    *range)
{
  GtkWidget *widget;
  gint x, y, width, height;
  
  g_return_if_fail (range != NULL);
  
  widget = GTK_WIDGET (range);
  
  gtk_hscale_pos_background (GTK_HSCALE (range), &x, &y, &width, &height);
  
  gtk_widget_queue_clear_area (GTK_WIDGET (range),
                               x, y, width, height);
}

static void
gtk_hscale_size_request (GtkWidget      *widget,
                         GtkRequisition *requisition)
{
  GtkScale *scale;
  gint value_width;
  gint slider_width;
  gint trough_border;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HSCALE (widget));
  g_return_if_fail (requisition != NULL);
  
  scale = GTK_SCALE (widget);

  _gtk_range_get_props (GTK_RANGE (scale),
			&slider_width, &trough_border, NULL, NULL);
  
  requisition->width = (SCALE_CLASS (scale)->slider_length + trough_border) * 2;
  requisition->height = (slider_width + trough_border * 2);
  
  if (scale->draw_value)
    {
      value_width = gtk_scale_get_value_width (scale);
      
      if ((scale->value_pos == GTK_POS_LEFT) ||
          (scale->value_pos == GTK_POS_RIGHT))
        {
          requisition->width += value_width + SCALE_CLASS (scale)->value_spacing;
          if (requisition->height < (widget->style->font->ascent + widget->style->font->descent))
            requisition->height = widget->style->font->ascent + widget->style->font->descent;
        }
      else if ((scale->value_pos == GTK_POS_TOP) ||
               (scale->value_pos == GTK_POS_BOTTOM))
        {
          if (requisition->width < value_width)
            requisition->width = value_width;
          requisition->height += widget->style->font->ascent + widget->style->font->descent;
        }
    }
}

static void
gtk_hscale_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
  GtkRange *range;
  GtkScale *scale;
  gint width, height;
  gint x, y;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HSCALE (widget));
  g_return_if_fail (allocation != NULL);
  
  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      range = GTK_RANGE (widget);
      scale = GTK_SCALE (widget);
      
      gtk_hscale_pos_trough (GTK_HSCALE (widget), &x, &y, &width, &height);
      
      gdk_window_move_resize (range->trough, 
                              x, y, width, height);
      gtk_range_slider_update (GTK_RANGE (widget));
    }
}

static void
gtk_hscale_pos_trough (GtkHScale *hscale,
                       gint      *x,
                       gint      *y,
                       gint      *w,
                       gint      *h)
{
  GtkWidget *widget;
  GtkScale *scale;
  gint slider_width;
  gint trough_border;
  
  g_return_if_fail (hscale != NULL);
  g_return_if_fail (GTK_IS_HSCALE (hscale));
  g_return_if_fail ((x != NULL) && (y != NULL) && (w != NULL) && (h != NULL));
  
  widget = GTK_WIDGET (hscale);
  scale = GTK_SCALE (hscale);

  _gtk_range_get_props (GTK_RANGE (scale),
			&slider_width, &trough_border, NULL, NULL);

  *w = widget->allocation.width;
  *h = (slider_width + trough_border * 2);
  
  if (scale->draw_value)
    {
      *x = 0;
      *y = 0;
      
      switch (scale->value_pos)
        {
        case GTK_POS_LEFT:
          *x += gtk_scale_get_value_width (scale) + SCALE_CLASS (scale)->value_spacing;
          *y = (widget->allocation.height - *h) / 2;
          *w -= *x;
          break;
        case GTK_POS_RIGHT:
          *w -= gtk_scale_get_value_width (scale) + SCALE_CLASS (scale)->value_spacing;
          *y = (widget->allocation.height - *h) / 2;
          break;
        case GTK_POS_TOP:
          *y = (widget->style->font->ascent + widget->style->font->descent +
                (widget->allocation.height - widget->requisition.height) / 2);
          break;
        case GTK_POS_BOTTOM:
          *y = (widget->allocation.height - widget->requisition.height) / 2;
          break;
        }
    }
  else
    {
      *x = 0;
      *y = (widget->allocation.height - *h) / 2;
    }
  *x += 1;
  *w -= 2;
  
  *x += widget->allocation.x;
  *y += widget->allocation.y;
}

static void
gtk_hscale_pos_background (GtkHScale *hscale,
                           gint      *x,
                           gint      *y,
                           gint      *w,
                           gint      *h)
{
  GtkWidget *widget;
  GtkScale *scale;
  
  gint tx, ty, twidth, theight;
  
  g_return_if_fail (hscale != NULL);
  g_return_if_fail (GTK_IS_HSCALE (hscale));
  g_return_if_fail ((x != NULL) && (y != NULL) && (w != NULL) && (h != NULL));
  
  gtk_hscale_pos_trough (hscale, &tx, &ty, &twidth, &theight);
  
  widget = GTK_WIDGET (hscale);
  scale = GTK_SCALE (hscale);
  
  *x = widget->allocation.x;
  *y = widget->allocation.y;
  *w = widget->allocation.width;
  *h = widget->allocation.height;
  
  switch (scale->value_pos)
    {
    case GTK_POS_LEFT:
      *w -= twidth;
      break;
    case GTK_POS_RIGHT:
      *x += twidth;
      *w -= twidth;
      break;
    case GTK_POS_TOP:
      *h -= theight;
      break;
    case GTK_POS_BOTTOM:
      *y += theight;
      *h -= theight;
      break;
    }
  *w = MAX (*w, 0);
  *h = MAX (*h, 0);
}

static void
gtk_hscale_draw_slider (GtkRange *range)
{
  GtkStateType state_type;
  
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_HSCALE (range));
  
  if (range->slider)
    {
      if ((range->in_child == RANGE_CLASS (range)->slider) ||
          (range->click_child == RANGE_CLASS (range)->slider))
        state_type = GTK_STATE_PRELIGHT;
      else
        state_type = GTK_STATE_NORMAL;
      
      gtk_paint_slider (GTK_WIDGET (range)->style, range->slider, state_type, 
			GTK_SHADOW_OUT,
			NULL, GTK_WIDGET (range), "hscale",
			0, 0, -1, -1, 
			GTK_ORIENTATION_HORIZONTAL); 
    }
}

static void
gtk_hscale_draw_value (GtkScale *scale)
{
  GtkStateType state_type;
  GtkWidget *widget;
  gchar buffer[32];
  gint text_width;
  gint width, height;
  gint x, y;
  
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_HSCALE (scale));
  
  widget = GTK_WIDGET (scale);
  
  if (scale->draw_value)
    {
      sprintf (buffer, "%0.*f", GTK_RANGE (scale)->digits, GTK_RANGE (scale)->adjustment->value);
      text_width = gdk_string_measure (GTK_WIDGET (scale)->style->font, buffer);
      
      switch (scale->value_pos)
        {
        case GTK_POS_LEFT:
          gdk_window_get_position (GTK_RANGE (scale)->trough, &x, &y);
          gdk_window_get_size (GTK_RANGE (scale)->trough, &width, &height);
          
          x -= SCALE_CLASS (scale)->value_spacing + text_width;
          y += ((height -
                 (GTK_WIDGET (scale)->style->font->ascent +
                  GTK_WIDGET (scale)->style->font->descent)) / 2 +
                GTK_WIDGET (scale)->style->font->ascent);
          break;
        case GTK_POS_RIGHT:
          gdk_window_get_position (GTK_RANGE (scale)->trough, &x, &y);
          gdk_window_get_size (GTK_RANGE (scale)->trough, &width, &height);
          
          x += width + SCALE_CLASS (scale)->value_spacing;
          y += ((height -
                 (GTK_WIDGET (scale)->style->font->ascent +
                  GTK_WIDGET (scale)->style->font->descent)) / 2 +
                GTK_WIDGET (scale)->style->font->ascent);
          break;
        case GTK_POS_TOP:
          gdk_window_get_position (GTK_RANGE (scale)->slider, &x, NULL);
          gdk_window_get_position (GTK_RANGE (scale)->trough, NULL, &y);
          gdk_window_get_size (GTK_RANGE (scale)->slider, &width, NULL);
          gdk_window_get_size (GTK_RANGE (scale)->trough, NULL, &height);
          
          x += widget->allocation.x + (width - text_width) / 2;
	  x = CLAMP (x, widget->allocation.x,
		     widget->allocation.x + widget->allocation.width - text_width);
          y -= GTK_WIDGET (scale)->style->font->descent;
          break;
        case GTK_POS_BOTTOM:
          gdk_window_get_position (GTK_RANGE (scale)->slider, &x, NULL);
          gdk_window_get_position (GTK_RANGE (scale)->trough, NULL, &y);
          gdk_window_get_size (GTK_RANGE (scale)->slider, &width, NULL);
          gdk_window_get_size (GTK_RANGE (scale)->trough, NULL, &height);
          
          x += widget->allocation.x + (width - text_width) / 2;
	  x = CLAMP (x, widget->allocation.x,
		     widget->allocation.x + widget->allocation.width - text_width);
          y += height + GTK_WIDGET (scale)->style->font->ascent;
          break;
        }
      
      state_type = GTK_STATE_NORMAL;
      if (!GTK_WIDGET_IS_SENSITIVE (scale))
        state_type = GTK_STATE_INSENSITIVE;
      
      gtk_paint_string (GTK_WIDGET (scale)->style,
                        GTK_WIDGET (scale)->window,
                        state_type, 
                        NULL, GTK_WIDGET (scale), "hscale", 
                        x, y, buffer);
    }
}

static gint
gtk_hscale_trough_keys (GtkRange *range,
			GdkEventKey *key,
			GtkScrollType *scroll,
			GtkTroughType *pos)
{
  gint return_val = FALSE;
  switch (key->keyval)
    {
    case GDK_Left:
      return_val = TRUE;
      if (key->state & GDK_CONTROL_MASK)
        *scroll = GTK_SCROLL_PAGE_BACKWARD;
      else
        *scroll = GTK_SCROLL_STEP_BACKWARD;
      break;
    case GDK_Right:
      return_val = TRUE;
      if (key->state & GDK_CONTROL_MASK)
        *scroll = GTK_SCROLL_PAGE_FORWARD;
      else
        *scroll = GTK_SCROLL_STEP_FORWARD;
      break;
    case GDK_Home:
      return_val = TRUE;
      *pos = GTK_TROUGH_START;
      break;
    case GDK_End:
      return_val = TRUE;
      *pos = GTK_TROUGH_END;
      break;
    }
  return return_val;
}
