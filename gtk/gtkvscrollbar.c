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

#include "gtkvscrollbar.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"


#define EPSILON 0.01

#define RANGE_CLASS(w)  GTK_RANGE_CLASS (GTK_OBJECT (w)->klass)

enum {
  ARG_0,
  ARG_ADJUSTMENT
};

static void gtk_vscrollbar_class_init       (GtkVScrollbarClass *klass);
static void gtk_vscrollbar_init             (GtkVScrollbar      *vscrollbar);
static void gtk_vscrollbar_set_arg          (GtkObject          *object,
                                             GtkArg             *arg,
                                             guint               arg_id);
static void gtk_vscrollbar_get_arg          (GtkObject          *object,
                                             GtkArg             *arg,
                                             guint               arg_id);
static void gtk_vscrollbar_realize          (GtkWidget          *widget);
static void gtk_vscrollbar_size_request     (GtkWidget          *widget,
                                             GtkRequisition     *requisition);
static void gtk_vscrollbar_size_allocate    (GtkWidget          *widget,
                                             GtkAllocation      *allocation);
static void gtk_vscrollbar_draw_step_forw   (GtkRange           *range);
static void gtk_vscrollbar_draw_step_back   (GtkRange           *range);
static void gtk_vscrollbar_slider_update    (GtkRange           *range);
static void gtk_vscrollbar_calc_slider_size (GtkVScrollbar      *vscrollbar);
static gint gtk_vscrollbar_trough_keys      (GtkRange *range,
                                             GdkEventKey *key,
                                             GtkScrollType *scroll,
                                             GtkTroughType *pos);


GtkType
gtk_vscrollbar_get_type (void)
{
  static GtkType vscrollbar_type = 0;
  
  if (!vscrollbar_type)
    {
      static const GtkTypeInfo vscrollbar_info =
      {
        "GtkVScrollbar",
        sizeof (GtkVScrollbar),
        sizeof (GtkVScrollbarClass),
        (GtkClassInitFunc) gtk_vscrollbar_class_init,
        (GtkObjectInitFunc) gtk_vscrollbar_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      vscrollbar_type = gtk_type_unique (GTK_TYPE_SCROLLBAR, &vscrollbar_info);
    }
  
  return vscrollbar_type;
}

static void
gtk_vscrollbar_class_init (GtkVScrollbarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  range_class = (GtkRangeClass*) class;
  
  gtk_object_add_arg_type ("GtkVScrollbar::adjustment",
                           GTK_TYPE_ADJUSTMENT,
                           GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
                           ARG_ADJUSTMENT);
  
  object_class->set_arg = gtk_vscrollbar_set_arg;
  object_class->get_arg = gtk_vscrollbar_get_arg;
  
  widget_class->realize = gtk_vscrollbar_realize;
  widget_class->size_request = gtk_vscrollbar_size_request;
  widget_class->size_allocate = gtk_vscrollbar_size_allocate;
  
  range_class->draw_step_forw = gtk_vscrollbar_draw_step_forw;
  range_class->draw_step_back = gtk_vscrollbar_draw_step_back;
  range_class->slider_update = gtk_vscrollbar_slider_update;
  range_class->trough_click = gtk_range_default_vtrough_click;
  range_class->trough_keys = gtk_vscrollbar_trough_keys;
  range_class->motion = gtk_range_default_vmotion;
}

static void
gtk_vscrollbar_set_arg (GtkObject          *object,
                        GtkArg             *arg,
                        guint               arg_id)
{
  GtkVScrollbar *vscrollbar;
  
  vscrollbar = GTK_VSCROLLBAR (object);
  
  switch (arg_id)
    {
    case ARG_ADJUSTMENT:
      gtk_range_set_adjustment (GTK_RANGE (vscrollbar), GTK_VALUE_POINTER (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_vscrollbar_get_arg (GtkObject          *object,
                        GtkArg             *arg,
                        guint               arg_id)
{
  GtkVScrollbar *vscrollbar;
  
  vscrollbar = GTK_VSCROLLBAR (object);
  
  switch (arg_id)
    {
    case ARG_ADJUSTMENT:
      GTK_VALUE_POINTER (*arg) = GTK_RANGE (vscrollbar);
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_vscrollbar_init (GtkVScrollbar *vscrollbar)
{
}

GtkWidget*
gtk_vscrollbar_new (GtkAdjustment *adjustment)
{
  GtkWidget *vscrollbar;
  
  vscrollbar = gtk_widget_new (GTK_TYPE_VSCROLLBAR,
			       "adjustment", adjustment,
			       NULL);
  
  return vscrollbar;
}


static void
gtk_vscrollbar_realize (GtkWidget *widget)
{
  GtkRange *range;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint slider_width;
  gint stepper_size;
  gint trough_border;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VSCROLLBAR (widget));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  range = GTK_RANGE (widget);
  
  _gtk_range_get_props (range, &slider_width, &trough_border,
			&stepper_size, NULL);

  attributes.x = widget->allocation.x + (widget->allocation.width - widget->requisition.width) / 2;
  attributes.y = widget->allocation.y;
  attributes.width = widget->requisition.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
                            GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK);
  
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  
  range->trough = widget->window;
  gdk_window_ref (range->trough);
  
  attributes.x = trough_border;
  attributes.y = trough_border;
  attributes.width = stepper_size;
  attributes.height = stepper_size;
  
  range->step_back = gdk_window_new (range->trough, &attributes, attributes_mask);
  
  attributes.y = (widget->allocation.height -
                  trough_border -
                  stepper_size);
  
  range->step_forw = gdk_window_new (range->trough, &attributes, attributes_mask);
  
  attributes.x = trough_border;
  attributes.y = 0;
  attributes.width = slider_width;
  attributes.height = RANGE_CLASS (widget)->min_slider_size;
  attributes.event_mask |= (GDK_BUTTON_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK);
  
  range->slider = gdk_window_new (range->trough, &attributes, attributes_mask);
  
  gtk_vscrollbar_calc_slider_size (GTK_VSCROLLBAR (widget));
  gtk_range_slider_update (GTK_RANGE (widget));
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  gdk_window_set_user_data (range->trough, widget);
  gdk_window_set_user_data (range->slider, widget);
  gdk_window_set_user_data (range->step_forw, widget);
  gdk_window_set_user_data (range->step_back, widget);
  
  gtk_style_set_background (widget->style, range->trough, GTK_STATE_ACTIVE);
  gtk_style_set_background (widget->style, range->slider, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, range->step_forw, GTK_STATE_ACTIVE);
  gtk_style_set_background (widget->style, range->step_back, GTK_STATE_ACTIVE);
  
  gdk_window_show (range->slider);
  gdk_window_show (range->step_forw);
  gdk_window_show (range->step_back);
}

static void
gtk_vscrollbar_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  gint slider_width;
  gint trough_border;
  gint stepper_size;
  gint stepper_spacing;
  
  GtkRange *range = GTK_RANGE (widget);

  _gtk_range_get_props (range, &slider_width, &trough_border,
			&stepper_size, &stepper_spacing);
  
  requisition->width = (slider_width +
                        trough_border * 2);
  requisition->height = (RANGE_CLASS (widget)->min_slider_size +
                         stepper_size +
                         stepper_spacing +
                         trough_border) * 2;
}

static void
gtk_vscrollbar_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkRange *range;
  gint trough_border;
  gint stepper_size;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VSCROLLBAR (widget));
  g_return_if_fail (allocation != NULL);
  
  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      range = GTK_RANGE (widget);

      _gtk_range_get_props (range, NULL, &trough_border, &stepper_size, NULL);
      
      gdk_window_move_resize (range->trough,
                              allocation->x + (allocation->width - widget->requisition.width) / 2,
                              allocation->y,
                              widget->requisition.width, allocation->height);
      gdk_window_move_resize (range->step_back,
                              trough_border,
                              trough_border,
                              widget->requisition.width - trough_border * 2,
                              stepper_size);
      gdk_window_move_resize (range->step_forw,
                              trough_border,
                              allocation->height - trough_border -
                              stepper_size,
                              widget->requisition.width - trough_border * 2,
                              stepper_size);
      gdk_window_resize (range->slider,
                         widget->requisition.width - trough_border * 2,
                         RANGE_CLASS (range)->min_slider_size);
      
      gtk_range_slider_update (GTK_RANGE (widget));
    }
}

static void
gtk_vscrollbar_draw_step_forw (GtkRange *range)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_VSCROLLBAR (range));
  
  if (GTK_WIDGET_DRAWABLE (range))
    {
      if (range->in_child == RANGE_CLASS (range)->step_forw)
        {
          if (range->click_child == RANGE_CLASS (range)->step_forw)
            state_type = GTK_STATE_ACTIVE;
          else
            state_type = GTK_STATE_PRELIGHT;
        }
      else
        state_type = GTK_STATE_NORMAL;
      
      if (range->click_child == RANGE_CLASS (range)->step_forw)
        shadow_type = GTK_SHADOW_IN;
      else
        shadow_type = GTK_SHADOW_OUT;
      
      gtk_paint_arrow (GTK_WIDGET (range)->style, range->step_forw,
                       state_type, shadow_type, 
                       NULL, GTK_WIDGET (range), "vscrollbar",
                       GTK_ARROW_DOWN,
                       TRUE, 0, 0, -1, -1);
    }
}

static void
gtk_vscrollbar_draw_step_back (GtkRange *range)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_VSCROLLBAR (range));
  
  if (GTK_WIDGET_DRAWABLE (range))
    {
      if (range->in_child == RANGE_CLASS (range)->step_back)
        {
          if (range->click_child == RANGE_CLASS (range)->step_back)
            state_type = GTK_STATE_ACTIVE;
          else
            state_type = GTK_STATE_PRELIGHT;
        }
      else
        state_type = GTK_STATE_NORMAL;
      
      if (range->click_child == RANGE_CLASS (range)->step_back)
        shadow_type = GTK_SHADOW_IN;
      else
        shadow_type = GTK_SHADOW_OUT;
      
      gtk_paint_arrow (GTK_WIDGET (range)->style, range->step_back,
                       state_type, shadow_type, 
                       NULL, GTK_WIDGET (range), "vscrollbar",
                       GTK_ARROW_UP,
                       TRUE, 0, 0, -1, -1);
    }
}

static void
gtk_vscrollbar_slider_update (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_VSCROLLBAR (range));
  
  gtk_vscrollbar_calc_slider_size (GTK_VSCROLLBAR (range));
  gtk_range_default_vslider_update (range);
}

static void
gtk_vscrollbar_calc_slider_size (GtkVScrollbar *vscrollbar)
{
  GtkRange *range;
  gint step_back_y;
  gint step_back_height;
  gint step_forw_y;
  gint stepper_spacing;
  gint slider_width;
  gint slider_height;
  gint top, bottom;
  gint height;
  
  g_return_if_fail (vscrollbar != NULL);
  g_return_if_fail (GTK_IS_VSCROLLBAR (vscrollbar));
  
  if (GTK_WIDGET_REALIZED (vscrollbar))
    {
      range = GTK_RANGE (vscrollbar);
      
      _gtk_range_get_props (range, NULL, NULL, NULL, &stepper_spacing);
      
      gdk_window_get_size (range->step_back, NULL, &step_back_height);
      gdk_window_get_position (range->step_back, NULL, &step_back_y);
      gdk_window_get_position (range->step_forw, NULL, &step_forw_y);
      
      top = (step_back_y +
             step_back_height +
             stepper_spacing);
      bottom = step_forw_y - stepper_spacing;
      height = bottom - top;
      
      if ((range->adjustment->page_size > 0) &&
          (range->adjustment->lower != range->adjustment->upper))
        {
          if (range->adjustment->page_size >
              (range->adjustment->upper - range->adjustment->lower))
            range->adjustment->page_size = range->adjustment->upper - range->adjustment->lower;
          
          height = (height * range->adjustment->page_size /
                    (range->adjustment->upper - range->adjustment->lower));
          
          if (height < RANGE_CLASS (vscrollbar)->min_slider_size)
            height = RANGE_CLASS (vscrollbar)->min_slider_size;
        }
      
      gdk_window_get_size (range->slider, &slider_width, &slider_height);
      
      if (slider_height != height)
        gdk_window_resize (range->slider, slider_width, height);
    }
}

static gint
gtk_vscrollbar_trough_keys(GtkRange *range,
                           GdkEventKey *key,
                           GtkScrollType *scroll,
                           GtkTroughType *pos)
{
  gint return_val = FALSE;
  switch (key->keyval)
    {
    case GDK_Up:
      return_val = TRUE;
      *scroll = GTK_SCROLL_STEP_BACKWARD;
      break;
    case GDK_Down:
      return_val = TRUE;
      *scroll = GTK_SCROLL_STEP_FORWARD;
      break;
    case GDK_Page_Up:
      return_val = TRUE;
      if (key->state & GDK_CONTROL_MASK)
        *pos = GTK_TROUGH_START;
      else
        *scroll = GTK_SCROLL_PAGE_BACKWARD;
      break;
    case GDK_Page_Down:
      return_val = TRUE;
      if (key->state & GDK_CONTROL_MASK)
        *pos = GTK_TROUGH_END;
      else
        *scroll = GTK_SCROLL_PAGE_FORWARD;
      break;
    }
  return return_val;
}
