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
#include "gtkhscrollbar.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"


#define EPSILON 0.01

#define RANGE_CLASS(w)  GTK_RANGE_CLASS (GTK_OBJECT (w)->klass)


static void gtk_hscrollbar_class_init       (GtkHScrollbarClass *klass);
static void gtk_hscrollbar_init             (GtkHScrollbar      *hscrollbar);
static void gtk_hscrollbar_realize          (GtkWidget          *widget);
static void gtk_hscrollbar_size_allocate    (GtkWidget          *widget,
					     GtkAllocation      *allocation);
static void gtk_hscrollbar_draw_step_forw   (GtkRange           *range);
static void gtk_hscrollbar_draw_step_back   (GtkRange           *range);
static void gtk_hscrollbar_slider_update    (GtkRange           *range);
static void gtk_hscrollbar_calc_slider_size (GtkHScrollbar      *hscrollbar);
static gint gtk_hscrollbar_trough_keys      (GtkRange *range,
					     GdkEventKey *key,
					     GtkScrollType *scroll,
					     GtkTroughType *pos);


guint
gtk_hscrollbar_get_type (void)
{
  static guint hscrollbar_type = 0;

  if (!hscrollbar_type)
    {
      GtkTypeInfo hscrollbar_info =
      {
	"GtkHScrollbar",
	sizeof (GtkHScrollbar),
	sizeof (GtkHScrollbarClass),
	(GtkClassInitFunc) gtk_hscrollbar_class_init,
	(GtkObjectInitFunc) gtk_hscrollbar_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      hscrollbar_type = gtk_type_unique (gtk_scrollbar_get_type (), &hscrollbar_info);
    }

  return hscrollbar_type;
}

static void
gtk_hscrollbar_class_init (GtkHScrollbarClass *class)
{
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;

  widget_class = (GtkWidgetClass*) class;
  range_class = (GtkRangeClass*) class;

  widget_class->realize = gtk_hscrollbar_realize;
  widget_class->size_allocate = gtk_hscrollbar_size_allocate;

  range_class->draw_step_forw = gtk_hscrollbar_draw_step_forw;
  range_class->draw_step_back = gtk_hscrollbar_draw_step_back;
  range_class->slider_update = gtk_hscrollbar_slider_update;
  range_class->trough_click = gtk_range_default_htrough_click;
  range_class->trough_keys = gtk_hscrollbar_trough_keys;
  range_class->motion = gtk_range_default_hmotion;
}

static void
gtk_hscrollbar_init (GtkHScrollbar *hscrollbar)
{
  GtkWidget *widget;
  GtkRequisition *requisition;

  widget = GTK_WIDGET (hscrollbar);
  requisition = &widget->requisition;

  requisition->width = (RANGE_CLASS (widget)->min_slider_size +
			RANGE_CLASS (widget)->stepper_size +
			RANGE_CLASS (widget)->stepper_slider_spacing +
			widget->style->klass->xthickness) * 2;
  requisition->height = (RANGE_CLASS (widget)->slider_width +
			 widget->style->klass->ythickness * 2);
}

GtkWidget*
gtk_hscrollbar_new (GtkAdjustment *adjustment)
{
  GtkHScrollbar *hscrollbar;

  hscrollbar = gtk_type_new (gtk_hscrollbar_get_type ());

  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  gtk_range_set_adjustment (GTK_RANGE (hscrollbar), adjustment);

  return GTK_WIDGET (hscrollbar);
}


static void
gtk_hscrollbar_realize (GtkWidget *widget)
{
  GtkRange *range;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HSCROLLBAR (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  range = GTK_RANGE (widget);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y + (widget->allocation.height - widget->requisition.height) / 2;
  attributes.width = widget->allocation.width;
  attributes.height = widget->requisition.height;
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

  attributes.x = widget->style->klass->xthickness;
  attributes.y = widget->style->klass->ythickness;
  attributes.width = RANGE_CLASS (widget)->stepper_size;
  attributes.height = RANGE_CLASS (widget)->stepper_size;

  range->step_back = gdk_window_new (range->trough, &attributes, attributes_mask);

  attributes.x = (widget->allocation.width -
		  widget->style->klass->xthickness -
		  RANGE_CLASS (widget)->stepper_size);

  range->step_forw = gdk_window_new (range->trough, &attributes, attributes_mask);

  attributes.x = 0;
  attributes.y = widget->style->klass->ythickness;
  attributes.width = RANGE_CLASS (widget)->min_slider_size;
  attributes.height = RANGE_CLASS (widget)->slider_width;
  attributes.event_mask |= (GDK_BUTTON_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);

  range->slider = gdk_window_new (range->trough, &attributes, attributes_mask);

  gtk_hscrollbar_calc_slider_size (GTK_HSCROLLBAR (widget));
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
gtk_hscrollbar_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkRange *range;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HSCROLLBAR (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      range = GTK_RANGE (widget);

      gdk_window_move_resize (range->trough,
			      allocation->x,
			      allocation->y + (allocation->height - widget->requisition.height) / 2,
			      allocation->width, widget->requisition.height);
      gdk_window_move_resize (range->step_back,
			      widget->style->klass->xthickness,
			      widget->style->klass->ythickness,
			      RANGE_CLASS (widget)->stepper_size,
			      widget->requisition.height - widget->style->klass->ythickness * 2);
      gdk_window_move_resize (range->step_forw,
			      allocation->width - widget->style->klass->xthickness -
			      RANGE_CLASS (widget)->stepper_size,
			      widget->style->klass->ythickness,
			      RANGE_CLASS (widget)->stepper_size,
			      widget->requisition.height - widget->style->klass->ythickness * 2);
      gdk_window_resize (range->slider,
			 RANGE_CLASS (widget)->min_slider_size,
			 widget->requisition.height - widget->style->klass->ythickness * 2);

      gtk_range_slider_update (GTK_RANGE (widget));
    }
}

static void
gtk_hscrollbar_draw_step_forw (GtkRange *range)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;

  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_HSCROLLBAR (range));

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

      gtk_draw_arrow (GTK_WIDGET (range)->style, range->step_forw,
		      state_type, shadow_type, GTK_ARROW_RIGHT,
		      TRUE, 0, 0, -1, -1);
    }
}

static void
gtk_hscrollbar_draw_step_back (GtkRange *range)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;

  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_HSCROLLBAR (range));

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

      gtk_draw_arrow (GTK_WIDGET (range)->style, range->step_back,
		      state_type, shadow_type, GTK_ARROW_LEFT,
		      TRUE, 0, 0, -1, -1);
    }
}

static void
gtk_hscrollbar_slider_update (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_HSCROLLBAR (range));

  gtk_hscrollbar_calc_slider_size (GTK_HSCROLLBAR (range));
  gtk_range_default_hslider_update (range);
}

static void
gtk_hscrollbar_calc_slider_size (GtkHScrollbar *hscrollbar)
{
  GtkRange *range;
  gint step_back_x;
  gint step_back_width;
  gint step_forw_x;
  gint slider_width;
  gint slider_height;
  gint left, right;
  gint width;

  g_return_if_fail (hscrollbar != NULL);
  g_return_if_fail (GTK_IS_HSCROLLBAR (hscrollbar));

  if (GTK_WIDGET_REALIZED (hscrollbar))
    {
      range = GTK_RANGE (hscrollbar);

      gdk_window_get_size (range->step_back, &step_back_width, NULL);
      gdk_window_get_position (range->step_back, &step_back_x, NULL);
      gdk_window_get_position (range->step_forw, &step_forw_x, NULL);

      left = (step_back_x +
	      step_back_width +
	      RANGE_CLASS (hscrollbar)->stepper_slider_spacing);
      right = step_forw_x - RANGE_CLASS (hscrollbar)->stepper_slider_spacing;
      width = right - left;

      if ((range->adjustment->page_size > 0) &&
	  (range->adjustment->lower != range->adjustment->upper))
	{
	  if (range->adjustment->page_size >
	      (range->adjustment->upper - range->adjustment->lower))
	    range->adjustment->page_size = range->adjustment->upper - range->adjustment->lower;

	  width = (width * range->adjustment->page_size /
		   (range->adjustment->upper - range->adjustment->lower));

	  if (width < RANGE_CLASS (hscrollbar)->min_slider_size)
	    width = RANGE_CLASS (hscrollbar)->min_slider_size;
	}

      gdk_window_get_size (range->slider, &slider_width, &slider_height);

      if (slider_width != width)
	gdk_window_resize (range->slider, width, slider_height);
    }
}

static gint
gtk_hscrollbar_trough_keys(GtkRange *range,
			   GdkEventKey *key,
			   GtkScrollType *scroll,
			   GtkTroughType *pos)
{
  gint return_val = FALSE;
  switch (key->keyval)
    {
    case GDK_Left:
      return_val = TRUE;
      *scroll = GTK_SCROLL_STEP_BACKWARD;
      break;
    case GDK_Right:
      return_val = TRUE;
      *scroll = GTK_SCROLL_STEP_FORWARD;
      break;
    case GDK_Home:
      return_val = TRUE;
      if (key->state & GDK_CONTROL_MASK)
	*scroll = GTK_SCROLL_PAGE_BACKWARD;
      else
	*pos = GTK_TROUGH_START;
      break;
    case GDK_End:
      return_val = TRUE;
      if (key->state & GDK_CONTROL_MASK)
	*scroll = GTK_SCROLL_PAGE_FORWARD;
      else
	*pos = GTK_TROUGH_END;
      break;
    }
  return return_val;
}
