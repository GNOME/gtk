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

#include <stdio.h>
#include "gtkvscale.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"
#include "gtkintl.h"
#include "gtkbindings.h"


#define SCALE_CLASS(w)  GTK_SCALE_GET_CLASS (w)
#define RANGE_CLASS(w)  GTK_RANGE_GET_CLASS (w)

enum {
  PROP_0,
  PROP_ADJUSTMENT
};

static void     gtk_vscale_class_init       (GtkVScaleClass *klass);
static void     gtk_vscale_init             (GtkVScale      *vscale);
static void     gtk_vscale_set_property     (GObject        *object,
					     guint           prop_id,
					     const GValue   *value,
					     GParamSpec     *pspec);
static void     gtk_vscale_get_property     (GObject        *object,
					     guint           prop_id,
					     GValue         *value,
					     GParamSpec     *pspec);
static void     gtk_vscale_realize          (GtkWidget      *widget);
static void     gtk_vscale_size_request     (GtkWidget      *widget,
					     GtkRequisition *requisition);
static void     gtk_vscale_size_allocate    (GtkWidget      *widget,
					     GtkAllocation  *allocation);
static void     gtk_vscale_pos_trough       (GtkVScale      *vscale,
					     gint           *x,
					     gint           *y,
					     gint           *w,
					     gint           *h);
static void     gtk_vscale_pos_background   (GtkVScale      *vscale,
					     gint           *x,
					     gint           *y,
					     gint           *w,
					     gint           *h);
static void     gtk_vscale_draw_slider      (GtkRange       *range);
static void     gtk_vscale_draw_value       (GtkScale       *scale);
static void     gtk_vscale_clear_background (GtkRange       *range);

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

#define add_slider_binding(binding_set, keyval, mask, scroll, trough) \
  gtk_binding_entry_add_signal (binding_set, keyval, mask,             \
                                "move_slider", 2,                      \
                                GTK_TYPE_SCROLL_TYPE, scroll,          \
                                GTK_TYPE_TROUGH_TYPE, trough)

static void
gtk_vscale_class_init (GtkVScaleClass *class)
{
  GtkObjectClass *object_class;
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;
  GtkScaleClass *scale_class;
  GtkBindingSet *binding_set;
  
  object_class = (GtkObjectClass*) class;
  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  range_class = (GtkRangeClass*) class;
  scale_class = (GtkScaleClass*) class;
  
  gobject_class->set_property = gtk_vscale_set_property;
  gobject_class->get_property = gtk_vscale_get_property;
  
  widget_class->realize = gtk_vscale_realize;
  widget_class->size_request = gtk_vscale_size_request;
  widget_class->size_allocate = gtk_vscale_size_allocate;
  
  range_class->slider_update = _gtk_range_default_vslider_update;
  range_class->trough_click = _gtk_range_default_vtrough_click;
  range_class->motion = _gtk_range_default_vmotion;
  range_class->draw_slider = gtk_vscale_draw_slider;
  range_class->clear_background = gtk_vscale_clear_background;
  
  scale_class->draw_value = gtk_vscale_draw_value;

  g_object_class_install_property (gobject_class,
				   PROP_ADJUSTMENT,
				   g_param_spec_object ("adjustment",
							_("Adjustment"),
							_("The GtkAdjustment that determines the values to use for this VScale."),
							GTK_TYPE_ADJUSTMENT,
							G_PARAM_READWRITE));

  binding_set = gtk_binding_set_by_class (object_class);

  add_slider_binding (binding_set, GDK_Up, 0,
                      GTK_SCROLL_STEP_UP, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_UP, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_KP_Up, 0,
                      GTK_SCROLL_STEP_UP, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_KP_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_UP, GTK_TROUGH_NONE);


  add_slider_binding (binding_set, GDK_Down, 0,
                      GTK_SCROLL_STEP_DOWN, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_DOWN, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_KP_Down, 0,
                      GTK_SCROLL_STEP_DOWN, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_KP_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_DOWN, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_Page_Up, 0,
                      GTK_SCROLL_PAGE_BACKWARD, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_KP_Page_Up, 0,
                      GTK_SCROLL_PAGE_BACKWARD, GTK_TROUGH_NONE);  

  add_slider_binding (binding_set, GDK_Page_Down, 0,
                      GTK_SCROLL_PAGE_FORWARD, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_KP_Page_Down, 0,
                      GTK_SCROLL_PAGE_FORWARD, GTK_TROUGH_NONE);

  add_slider_binding (binding_set, GDK_Home, 0,
                      GTK_SCROLL_NONE, GTK_TROUGH_START);

  add_slider_binding (binding_set, GDK_KP_Home, 0,
                      GTK_SCROLL_NONE, GTK_TROUGH_START);


  add_slider_binding (binding_set, GDK_End, 0,
                      GTK_SCROLL_NONE, GTK_TROUGH_END);

  add_slider_binding (binding_set, GDK_KP_End, 0,
                      GTK_SCROLL_NONE, GTK_TROUGH_END);
}

static void
gtk_vscale_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  GtkVScale *vscale;
  
  vscale = GTK_VSCALE (object);
  
  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      gtk_range_set_adjustment (GTK_RANGE (vscale), g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_vscale_get_property (GObject         *object,
			 guint            prop_id,
			 GValue          *value,
			 GParamSpec      *pspec)
{
  GtkVScale *vscale;
  
  vscale = GTK_VSCALE (object);
  
  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value,
			  G_OBJECT (gtk_range_get_adjustment (GTK_RANGE (vscale))));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_vscale_init (GtkVScale *vscale)
{
  GTK_WIDGET_SET_FLAGS (vscale, GTK_NO_WINDOW);
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


static void
gtk_vscale_realize (GtkWidget *widget)
{
  GtkRange *range;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint x, y, w, h;
  gint slider_width, slider_length;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VSCALE (widget));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  range = GTK_RANGE (widget);

  _gtk_range_get_props (range, &slider_width, NULL, NULL, NULL);
  gtk_widget_style_get (widget, "slider_length", &slider_length, NULL);
  
  widget->window = gtk_widget_get_parent_window (widget);
  gdk_window_ref (widget->window);
  
  gtk_vscale_pos_trough (GTK_VSCALE (widget), &x, &y, &w, &h);
  
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
  
  attributes.width = slider_width;
  attributes.height = slider_length;
  attributes.event_mask |= (GDK_BUTTON_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK);
  
  range->slider = gdk_window_new (range->trough, &attributes, attributes_mask);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  gdk_window_set_user_data (range->trough, widget);
  gdk_window_set_user_data (range->slider, widget);
  
  gtk_style_set_background (widget->style, range->trough, GTK_STATE_ACTIVE);
  gtk_style_set_background (widget->style, range->slider, GTK_STATE_NORMAL);
  
  _gtk_range_slider_update (GTK_RANGE (widget));
  
  gdk_window_show (range->slider);
}

static void
gtk_vscale_clear_background (GtkRange    *range)
{
  GtkWidget *widget;
  GtkScale *scale;
  gint x, y, width, height;
  
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_SCALE (range));
  
  widget = GTK_WIDGET (range);
  scale = GTK_SCALE (range);
  
  gtk_vscale_pos_background (GTK_VSCALE (widget), &x, &y, &width, &height);
  
  gtk_widget_queue_clear_area (GTK_WIDGET (range),
                               x, y, width, height);
}

static void
gtk_vscale_size_request (GtkWidget      *widget,
                         GtkRequisition *requisition)
{
  GtkScale *scale = GTK_SCALE (widget);
  gint slider_width, slider_length, trough_border;
  
  _gtk_range_get_props (GTK_RANGE (scale),
 			&slider_width, &trough_border, NULL, NULL);
  gtk_widget_style_get (widget, "slider_length", &slider_length, NULL);
    
  requisition->width = (slider_width + trough_border * 2);
  requisition->height = (slider_length + trough_border) * 2;
  
  if (scale->draw_value)
    {
      gint value_width, value_height;
      gtk_scale_get_value_size (scale, &value_width, &value_height);
      
      if ((scale->value_pos == GTK_POS_LEFT) ||
          (scale->value_pos == GTK_POS_RIGHT))
        {
          requisition->width += value_width + SCALE_CLASS (scale)->value_spacing;
          if (requisition->height < (value_height))
            requisition->height = value_height;
        }
      else if ((scale->value_pos == GTK_POS_TOP) ||
               (scale->value_pos == GTK_POS_BOTTOM))
        {
          if (requisition->width < value_width)
            requisition->width = value_width;
          requisition->height += value_height;
        }
    }
}

static void
gtk_vscale_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
  GtkRange *range;
  GtkScale *scale;
  gint width, height;
  gint x, y;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VSCALE (widget));
  g_return_if_fail (allocation != NULL);
  
  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      range = GTK_RANGE (widget);
      scale = GTK_SCALE (widget);
      
      gtk_vscale_pos_trough (GTK_VSCALE (widget), &x, &y, &width, &height);
      
      gdk_window_move_resize (range->trough, x, y, width, height);
      _gtk_range_slider_update (GTK_RANGE (widget));
    }
}

static void
gtk_vscale_pos_trough (GtkVScale *vscale,
                       gint      *x,
                       gint      *y,
                       gint      *w,
                       gint      *h)
{
  GtkWidget *widget = GTK_WIDGET (vscale);
  GtkScale *scale = GTK_SCALE (vscale);
  gint value_width, value_height;
  gint slider_width, trough_border;
  
  _gtk_range_get_props (GTK_RANGE (scale),
 			&slider_width, &trough_border, NULL, NULL);
    
  *w = (slider_width + trough_border * 2);
  *h = widget->allocation.height;
  
  if (scale->draw_value)
    {
      *x = 0;
      *y = 0;
      
      gtk_scale_get_value_size (scale, &value_width, &value_height);

      switch (scale->value_pos)
        {
        case GTK_POS_LEFT:
          *x = (value_width + SCALE_CLASS (scale)->value_spacing +
                (widget->allocation.width - widget->requisition.width) / 2);
          break;
        case GTK_POS_RIGHT:
          *x = (widget->allocation.width - widget->requisition.width) / 2;
          break;
        case GTK_POS_TOP:
          *x = (widget->allocation.width - *w) / 2;
          *y = value_height;
          *h -= *y;
          break;
        case GTK_POS_BOTTOM:
          *x = (widget->allocation.width - *w) / 2;
          *h -= value_height;
          break;
        }
    }
  else
    {
      *x = (widget->allocation.width - *w) / 2;
      *y = 0;
    }
  *y += 1;
  *h -= 2;
  
  *x += widget->allocation.x;
  *y += widget->allocation.y;
}

static void
gtk_vscale_pos_background (GtkVScale *vscale,
                           gint      *x,
                           gint      *y,
                           gint      *w,
                           gint      *h)
{
  GtkWidget *widget;
  GtkScale *scale;
  gint slider_width, trough_border;
  
  gint tx, ty, twidth, theight;
  
  g_return_if_fail (vscale != NULL);
  g_return_if_fail (GTK_IS_VSCALE (vscale));
  g_return_if_fail ((x != NULL) && (y != NULL) && (w != NULL) && (h != NULL));
  
  gtk_vscale_pos_trough (vscale, &tx, &ty, &twidth, &theight);
  
  widget = GTK_WIDGET (vscale);
  scale = GTK_SCALE (vscale);
  
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
gtk_vscale_draw_slider (GtkRange *range)
{
  GtkStateType state_type;
  
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_VSCALE (range));
  
  if (range->slider)
    {
      if ((range->in_child == RANGE_CLASS (range)->slider) ||
          (range->click_child == RANGE_CLASS (range)->slider))
        state_type = GTK_STATE_PRELIGHT;
      else
        state_type = GTK_STATE_NORMAL;
      
      gtk_paint_slider (GTK_WIDGET (range)->style, range->slider, state_type, 
			GTK_SHADOW_OUT, 
			NULL, GTK_WIDGET (range), "vscale",
			0, 0, -1, -1, 
			GTK_ORIENTATION_VERTICAL); 
    }
}

static void
gtk_vscale_draw_value (GtkScale *scale)
{
  GtkStateType state_type;
  GtkWidget *widget;
  gchar buffer[32];
  gint width, height;
  gint x, y;
  
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_VSCALE (scale));
  
  widget = GTK_WIDGET (scale);
  
  if (scale->draw_value)
    {
      PangoLayout *layout;
      PangoRectangle logical_rect;
      
      sprintf (buffer, "%0.*f", GTK_RANGE (scale)->digits, GTK_RANGE (scale)->adjustment->value);

      layout = gtk_widget_create_pango_layout (widget, buffer);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      switch (scale->value_pos)
        {
        case GTK_POS_LEFT:
          gdk_window_get_position (GTK_RANGE (scale)->trough, &x, NULL);
          gdk_window_get_position (GTK_RANGE (scale)->slider, NULL, &y);
          gdk_window_get_size (GTK_RANGE (scale)->trough, &width, NULL);
          gdk_window_get_size (GTK_RANGE (scale)->slider, NULL, &height);
          
          x -= SCALE_CLASS (scale)->value_spacing + logical_rect.width;
          y += widget->allocation.y + (height - logical_rect.height) / 2 +
                                       PANGO_ASCENT (logical_rect);
          break;
        case GTK_POS_RIGHT:
          gdk_window_get_position (GTK_RANGE (scale)->trough, &x, NULL);
          gdk_window_get_position (GTK_RANGE (scale)->slider, NULL, &y);
          gdk_window_get_size (GTK_RANGE (scale)->trough, &width, NULL);
          gdk_window_get_size (GTK_RANGE (scale)->slider, NULL, &height);
          
          x += width + SCALE_CLASS (scale)->value_spacing;
          y += widget->allocation.y + (height - logical_rect.height) / 2 +
                                       PANGO_ASCENT (logical_rect);
          break;
        case GTK_POS_TOP:
          gdk_window_get_position (GTK_RANGE (scale)->trough, &x, &y);
          gdk_window_get_size (GTK_RANGE (scale)->slider, &width, NULL);
          gdk_window_get_size (GTK_RANGE (scale)->trough, NULL, &height);
          
          x += (width - logical_rect.width) / 2;
          y -= PANGO_DESCENT (logical_rect);
          break;
        case GTK_POS_BOTTOM:
          gdk_window_get_position (GTK_RANGE (scale)->trough, &x, &y);
          gdk_window_get_size (GTK_RANGE (scale)->slider, &width, NULL);
          gdk_window_get_size (GTK_RANGE (scale)->trough, NULL, &height);
          
          x += (width - logical_rect.width) / 2;
          y += height + PANGO_ASCENT (logical_rect);
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
}
