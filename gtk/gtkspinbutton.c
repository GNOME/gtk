/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkSpinButton widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <locale.h>
#include "gdk/gdkkeysyms.h"
#include "gtkspinbutton.h"
#include "gtkmain.h"
#include "gtksignal.h"


#define MIN_SPIN_BUTTON_WIDTH              30
#define ARROW_SIZE                         11
#define SPIN_BUTTON_INITIAL_TIMER_DELAY    200
#define SPIN_BUTTON_TIMER_DELAY            20
#define MAX_TEXT_LENGTH                    256
#define MAX_TIMER_CALLS                    5
#define EPSILON                            1e-5

enum {
  ARG_0,
  ARG_ADJUSTMENT,
  ARG_CLIMB_RATE,
  ARG_DIGITS,
  ARG_SNAP_TO_TICKS,
  ARG_NUMERIC,
  ARG_WRAP,
  ARG_UPDATE_POLICY,
  ARG_SHADOW_TYPE,
  ARG_VALUE
};


static void gtk_spin_button_class_init     (GtkSpinButtonClass *klass);
static void gtk_spin_button_init           (GtkSpinButton      *spin_button);
static void gtk_spin_button_finalize       (GtkObject          *object);
static void gtk_spin_button_set_arg        (GtkObject          *object,
					    GtkArg             *arg,
					    guint               arg_id);
static void gtk_spin_button_get_arg        (GtkObject          *object,
					    GtkArg             *arg,
					    guint               arg_id);
static void gtk_spin_button_map            (GtkWidget          *widget);
static void gtk_spin_button_unmap          (GtkWidget          *widget);
static void gtk_spin_button_realize        (GtkWidget          *widget);
static void gtk_spin_button_unrealize      (GtkWidget          *widget);
static void gtk_spin_button_size_request   (GtkWidget          *widget,
					    GtkRequisition     *requisition);
static void gtk_spin_button_size_allocate  (GtkWidget          *widget,
					    GtkAllocation      *allocation);
static void gtk_spin_button_paint          (GtkWidget          *widget,
					    GdkRectangle       *area);
static void gtk_spin_button_draw           (GtkWidget          *widget,
					    GdkRectangle       *area);
static gint gtk_spin_button_expose         (GtkWidget          *widget,
					    GdkEventExpose     *event);
static gint gtk_spin_button_button_press   (GtkWidget          *widget,
					    GdkEventButton     *event);
static gint gtk_spin_button_button_release (GtkWidget          *widget,
					    GdkEventButton     *event);
static gint gtk_spin_button_motion_notify  (GtkWidget          *widget,
					    GdkEventMotion     *event);
static gint gtk_spin_button_enter_notify   (GtkWidget          *widget,
					    GdkEventCrossing   *event);
static gint gtk_spin_button_leave_notify   (GtkWidget          *widget,
					    GdkEventCrossing   *event);
static gint gtk_spin_button_focus_out      (GtkWidget          *widget,
					    GdkEventFocus      *event);
static void gtk_spin_button_draw_arrow     (GtkSpinButton      *spin_button, 
					    guint               arrow);
static gint gtk_spin_button_timer          (GtkSpinButton      *spin_button);
static void gtk_spin_button_value_changed  (GtkAdjustment      *adjustment,
					    GtkSpinButton      *spin_button); 
static gint gtk_spin_button_key_press      (GtkWidget          *widget,
					    GdkEventKey        *event);
static gint gtk_spin_button_key_release    (GtkWidget          *widget,
					    GdkEventKey        *event);
static void gtk_spin_button_activate       (GtkEditable        *editable);
static void gtk_spin_button_snap           (GtkSpinButton      *spin_button,
					    gfloat              val);
static void gtk_spin_button_insert_text    (GtkEditable        *editable,
					    const gchar        *new_text,
					    gint                new_text_length,
					    gint               *position);
static void gtk_spin_button_real_spin      (GtkSpinButton      *spin_button,
					    gfloat              step);


static GtkEntryClass *parent_class = NULL;


GtkType
gtk_spin_button_get_type (void)
{
  static guint spin_button_type = 0;

  if (!spin_button_type)
    {
      static const GtkTypeInfo spin_button_info =
      {
	"GtkSpinButton",
	sizeof (GtkSpinButton),
	sizeof (GtkSpinButtonClass),
	(GtkClassInitFunc) gtk_spin_button_class_init,
	(GtkObjectInitFunc) gtk_spin_button_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      spin_button_type = gtk_type_unique (GTK_TYPE_ENTRY, &spin_button_info);
    }
  return spin_button_type;
}

static void
gtk_spin_button_class_init (GtkSpinButtonClass *class)
{
  GtkObjectClass   *object_class;
  GtkWidgetClass   *widget_class;
  GtkEditableClass *editable_class;

  object_class   = (GtkObjectClass*)   class;
  widget_class   = (GtkWidgetClass*)   class;
  editable_class = (GtkEditableClass*) class; 

  parent_class = gtk_type_class (GTK_TYPE_ENTRY);

  gtk_object_add_arg_type ("GtkSpinButton::adjustment",
			   GTK_TYPE_ADJUSTMENT,
			   GTK_ARG_READWRITE,
			   ARG_ADJUSTMENT);
  gtk_object_add_arg_type ("GtkSpinButton::climb_rate",
			   GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE,
			   ARG_CLIMB_RATE);
  gtk_object_add_arg_type ("GtkSpinButton::digits",
			   GTK_TYPE_UINT,
			   GTK_ARG_READWRITE,
			   ARG_DIGITS);
  gtk_object_add_arg_type ("GtkSpinButton::snap_to_ticks",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_SNAP_TO_TICKS);
  gtk_object_add_arg_type ("GtkSpinButton::numeric",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_NUMERIC);
  gtk_object_add_arg_type ("GtkSpinButton::wrap",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_WRAP);
  gtk_object_add_arg_type ("GtkSpinButton::update_policy",
			   GTK_TYPE_SPIN_BUTTON_UPDATE_POLICY,
			   GTK_ARG_READWRITE,
			   ARG_UPDATE_POLICY);
  gtk_object_add_arg_type ("GtkSpinButton::shadow_type",
			   GTK_TYPE_SHADOW_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_SHADOW_TYPE);
  gtk_object_add_arg_type ("GtkSpinButton::value",
			   GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE,
			   ARG_VALUE);
  

  object_class->set_arg = gtk_spin_button_set_arg;
  object_class->get_arg = gtk_spin_button_get_arg;
  object_class->finalize = gtk_spin_button_finalize;

  widget_class->map = gtk_spin_button_map;
  widget_class->unmap = gtk_spin_button_unmap;
  widget_class->realize = gtk_spin_button_realize;
  widget_class->unrealize = gtk_spin_button_unrealize;
  widget_class->size_request = gtk_spin_button_size_request;
  widget_class->size_allocate = gtk_spin_button_size_allocate;
  widget_class->draw = gtk_spin_button_draw;
  widget_class->expose_event = gtk_spin_button_expose;
  widget_class->button_press_event = gtk_spin_button_button_press;
  widget_class->button_release_event = gtk_spin_button_button_release;
  widget_class->motion_notify_event = gtk_spin_button_motion_notify;
  widget_class->key_press_event = gtk_spin_button_key_press;
  widget_class->key_release_event = gtk_spin_button_key_release;
  widget_class->enter_notify_event = gtk_spin_button_enter_notify;
  widget_class->leave_notify_event = gtk_spin_button_leave_notify;
  widget_class->focus_out_event = gtk_spin_button_focus_out;

  editable_class->insert_text = gtk_spin_button_insert_text;
  editable_class->activate = gtk_spin_button_activate;
}

static void
gtk_spin_button_set_arg (GtkObject        *object,
			 GtkArg           *arg,
			 guint             arg_id)
{
  GtkSpinButton *spin_button;

  spin_button = GTK_SPIN_BUTTON (object);
  
  switch (arg_id)
    {
      GtkAdjustment *adjustment;

    case ARG_ADJUSTMENT:
      adjustment = GTK_VALUE_POINTER (*arg);
      if (!adjustment)
	adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      gtk_spin_button_set_adjustment (spin_button, adjustment);
      break;
    case ARG_CLIMB_RATE:
      gtk_spin_button_configure (spin_button,
				 spin_button->adjustment,
				 GTK_VALUE_FLOAT (*arg),
				 spin_button->digits);
      break;
    case ARG_DIGITS:
      gtk_spin_button_configure (spin_button,
				 spin_button->adjustment,
				 spin_button->climb_rate,
				 GTK_VALUE_UINT (*arg));
      break;
    case ARG_SNAP_TO_TICKS:
      gtk_spin_button_set_snap_to_ticks (spin_button, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_NUMERIC:
      gtk_spin_button_set_numeric (spin_button, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_WRAP:
      gtk_spin_button_set_wrap (spin_button, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_UPDATE_POLICY:
      gtk_spin_button_set_update_policy (spin_button, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_SHADOW_TYPE:
      gtk_spin_button_set_shadow_type (spin_button, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_VALUE:
      gtk_spin_button_set_value (spin_button, GTK_VALUE_FLOAT (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_spin_button_get_arg (GtkObject        *object,
			 GtkArg           *arg,
			 guint             arg_id)
{
  GtkSpinButton *spin_button;

  spin_button = GTK_SPIN_BUTTON (object);
  
  switch (arg_id)
    {
    case ARG_ADJUSTMENT:
      GTK_VALUE_POINTER (*arg) = spin_button->adjustment;
      break;
    case ARG_CLIMB_RATE:
      GTK_VALUE_FLOAT (*arg) = spin_button->climb_rate;
      break;
    case ARG_DIGITS:
      GTK_VALUE_UINT (*arg) = spin_button->digits;
      break;
    case ARG_SNAP_TO_TICKS:
      GTK_VALUE_BOOL (*arg) = spin_button->snap_to_ticks;
      break;
    case ARG_NUMERIC:
      GTK_VALUE_BOOL (*arg) = spin_button->numeric;
      break;
    case ARG_WRAP:
      GTK_VALUE_BOOL (*arg) = spin_button->wrap;
      break;
    case ARG_UPDATE_POLICY:
      GTK_VALUE_ENUM (*arg) = spin_button->update_policy;
      break;
    case ARG_SHADOW_TYPE:
      GTK_VALUE_ENUM (*arg) = spin_button->shadow_type;
      break;
    case ARG_VALUE:
      GTK_VALUE_FLOAT (*arg) = spin_button->adjustment->value;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_spin_button_init (GtkSpinButton *spin_button)
{
  spin_button->adjustment = NULL;
  spin_button->panel = NULL;
  spin_button->shadow_type = GTK_SHADOW_NONE;
  spin_button->timer = 0;
  spin_button->ev_time = 0;
  spin_button->climb_rate = 0.0;
  spin_button->timer_step = 0.0;
  spin_button->update_policy = GTK_UPDATE_ALWAYS;
  spin_button->in_child = 2;
  spin_button->click_child = 2;
  spin_button->button = 0;
  spin_button->need_timer = FALSE;
  spin_button->timer_calls = 0;
  spin_button->digits = 0;
  spin_button->numeric = FALSE;
  spin_button->wrap = FALSE;
  spin_button->snap_to_ticks = FALSE;

  gtk_spin_button_set_adjustment (spin_button,
				  (GtkAdjustment*) gtk_adjustment_new (0, 0, 0, 0, 0, 0));
}

static void
gtk_spin_button_finalize (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (object));

  gtk_object_unref (GTK_OBJECT (GTK_SPIN_BUTTON (object)->adjustment));
  
  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_spin_button_map (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_CLASS (parent_class)->map (widget);
      gdk_window_show (GTK_SPIN_BUTTON (widget)->panel);
    }
}

static void
gtk_spin_button_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  if (GTK_WIDGET_MAPPED (widget))
    {
      gdk_window_hide (GTK_SPIN_BUTTON (widget)->panel);
      GTK_WIDGET_CLASS (parent_class)->unmap (widget);
    }
}

static void
gtk_spin_button_realize (GtkWidget *widget)
{
  GtkSpinButton *spin;
  GdkWindowAttr attributes;
  gint attributes_mask;
  guint real_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));
  
  spin = GTK_SPIN_BUTTON (widget);

  real_width = widget->allocation.width;
  widget->allocation.width -= ARROW_SIZE + 2 * widget->style->klass->xthickness;
  gtk_widget_set_events (widget, gtk_widget_get_events (widget) |
			 GDK_KEY_RELEASE_MASK);
  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  widget->allocation.width = real_width;
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK 
    | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK 
    | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  attributes.x = (widget->allocation.x + widget->allocation.width - ARROW_SIZE -
		  2 * widget->style->klass->xthickness);
  attributes.y = widget->allocation.y + (widget->allocation.height -
					 widget->requisition.height) / 2;
  attributes.width = ARROW_SIZE + 2 * widget->style->klass->xthickness;
  attributes.height = widget->requisition.height;
  
  spin->panel = gdk_window_new (gtk_widget_get_parent_window (widget), 
				&attributes, attributes_mask);
  gdk_window_set_user_data (spin->panel, widget);

  gtk_style_set_background (widget->style, spin->panel, GTK_STATE_NORMAL);
}

static void
gtk_spin_button_unrealize (GtkWidget *widget)
{
  GtkSpinButton *spin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  spin = GTK_SPIN_BUTTON (widget);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

  if (spin->panel)
    {
      gdk_window_set_user_data (spin->panel, NULL);
      gdk_window_destroy (spin->panel);
      spin->panel = NULL;
    }
}

static void
gtk_spin_button_size_request (GtkWidget      *widget,
			      GtkRequisition *requisition)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (requisition != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);
  
  requisition->width = MIN_SPIN_BUTTON_WIDTH + ARROW_SIZE 
    + 2 * widget->style->klass->xthickness;
}

static void
gtk_spin_button_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));
  g_return_if_fail (allocation != NULL);

  child_allocation = *allocation;
  if (child_allocation.width > ARROW_SIZE + 2 * widget->style->klass->xthickness)
    child_allocation.width -= ARROW_SIZE + 2 * widget->style->klass->xthickness;

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, &child_allocation);

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      child_allocation.width = ARROW_SIZE + 2 * widget->style->klass->xthickness;
      child_allocation.height = widget->requisition.height;  
      child_allocation.x = (allocation->x + allocation->width - ARROW_SIZE - 
			    2 * widget->style->klass->xthickness);
      child_allocation.y = allocation->y + (allocation->height - widget->requisition.height) / 2;

      gdk_window_move_resize (GTK_SPIN_BUTTON (widget)->panel, 
			      child_allocation.x,
			      child_allocation.y,
			      child_allocation.width,
			      child_allocation.height); 
    }
}

static GtkShadowType
gtk_spin_button_get_shadow_type (GtkSpinButton *spin_button)
{
  GtkWidget *widget = GTK_WIDGET (spin_button);
  
  GtkShadowType shadow_type =
    gtk_style_get_prop_experimental (widget->style,
				     "GtkSpinButton::shadow_type", -1);

  if (shadow_type != (GtkShadowType)-1)
    return shadow_type;
  else
    return spin_button->shadow_type;
}

static void
gtk_spin_button_paint (GtkWidget    *widget,
		       GdkRectangle *area)
{
  GtkSpinButton *spin;
  GtkShadowType shadow_type;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  spin = GTK_SPIN_BUTTON (widget);
  shadow_type = gtk_spin_button_get_shadow_type (spin);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (shadow_type != GTK_SHADOW_NONE)
	gtk_paint_box (widget->style, spin->panel,
		       GTK_STATE_NORMAL, shadow_type,
		       area, widget, "spinbutton",
		       0, 0, 
		       ARROW_SIZE + 2 * widget->style->klass->xthickness,
		       widget->requisition.height); 
      else
	{
	  gdk_window_set_back_pixmap (spin->panel, NULL, TRUE);
	  gdk_window_clear_area (spin->panel, area->x, area->y, area->width, area->height);
	}
      gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
      gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
      
      GTK_WIDGET_CLASS (parent_class)->draw (widget, area);
    }
}

static void
gtk_spin_button_draw (GtkWidget    *widget,
		      GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_spin_button_paint (widget, area);
}

static gint
gtk_spin_button_expose (GtkWidget      *widget,
			GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_spin_button_paint (widget, &event->area);

  return FALSE;
}

static void
gtk_spin_button_draw_arrow (GtkSpinButton *spin_button, 
			    guint          arrow)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GtkShadowType spin_shadow_type;
  GtkWidget *widget;
  gint x;
  gint y;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  
  widget = GTK_WIDGET (spin_button);

  spin_shadow_type = gtk_spin_button_get_shadow_type (spin_button);

  if (GTK_WIDGET_DRAWABLE (spin_button))
    {
      if (!spin_button->wrap &&
	  (((arrow == GTK_ARROW_UP &&
	  (spin_button->adjustment->upper - spin_button->adjustment->value
	   <= EPSILON))) ||
	  ((arrow == GTK_ARROW_DOWN &&
	  (spin_button->adjustment->value - spin_button->adjustment->lower
	   <= EPSILON)))))
	{
	  shadow_type = GTK_SHADOW_ETCHED_IN;
	  state_type = GTK_STATE_NORMAL;
	}
      else
	{
	  if (spin_button->in_child == arrow)
	    {
	      if (spin_button->click_child == arrow)
		state_type = GTK_STATE_ACTIVE;
	      else
		state_type = GTK_STATE_PRELIGHT;
	    }
	  else
	    state_type = GTK_STATE_NORMAL;
	  
	  if (spin_button->click_child == arrow)
	    shadow_type = GTK_SHADOW_IN;
	  else
	    shadow_type = GTK_SHADOW_OUT;
	}
      if (arrow == GTK_ARROW_UP)
	{
	  if (spin_shadow_type != GTK_SHADOW_NONE)
	    {
	      x = widget->style->klass->xthickness;
	      y = widget->style->klass->ythickness;
	    }
	  else
	    {
	      x = widget->style->klass->xthickness - 1;
	      y = widget->style->klass->ythickness - 1;
	    }
	  gtk_paint_arrow (widget->style, spin_button->panel,
			   state_type, shadow_type, 
			   NULL, widget, "spinbutton",
			   arrow, TRUE, 
			   x, y, ARROW_SIZE, widget->requisition.height / 2 
			   - widget->style->klass->ythickness);
	}
      else
	{
	  if (spin_shadow_type != GTK_SHADOW_NONE)
	    {
	      x = widget->style->klass->xthickness;
	      y = widget->requisition.height / 2;
	    }
	  else
	    {
	      x = widget->style->klass->xthickness - 1;
	      y = widget->requisition.height / 2 + 1;
	    }
	  gtk_paint_arrow (widget->style, spin_button->panel,
			   state_type, shadow_type, 
			   NULL, widget, "spinbutton",
			   arrow, TRUE, 
			   x, y, ARROW_SIZE, widget->requisition.height / 2 
			   - widget->style->klass->ythickness);
	}
    }
}

static gint
gtk_spin_button_enter_notify (GtkWidget        *widget,
			      GdkEventCrossing *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (event->window == spin->panel)
    {
      gint x;
      gint y;

      gdk_window_get_pointer (spin->panel, &x, &y, NULL);

      if (y <= widget->requisition.height / 2)
	{
	  spin->in_child = GTK_ARROW_UP;
	  if (spin->click_child == 2) 
	    gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	}
      else
	{
	  spin->in_child = GTK_ARROW_DOWN;
	  if (spin->click_child == 2) 
	    gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	}
    }
  return FALSE;
}

static gint
gtk_spin_button_leave_notify (GtkWidget        *widget,
			      GdkEventCrossing *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (event->window == spin->panel && spin->click_child == 2)
    {
      if (spin->in_child == GTK_ARROW_UP) 
	{
	  spin->in_child = 2;
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	}
      else
	{
	  spin->in_child = 2;
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	}
    }
  return FALSE;
}

static gint
gtk_spin_button_focus_out (GtkWidget     *widget,
			   GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_EDITABLE (widget)->editable)
    gtk_spin_button_update (GTK_SPIN_BUTTON (widget));

  return GTK_WIDGET_CLASS (parent_class)->focus_out_event (widget, event);
}

static gint
gtk_spin_button_button_press (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (!spin->button)
    {
      if (event->window == spin->panel)
	{
	  if (!GTK_WIDGET_HAS_FOCUS (widget))
	    gtk_widget_grab_focus (widget);
	  gtk_grab_add (widget);
	  spin->button = event->button;
	  
	  if (GTK_EDITABLE (widget)->editable)
	    gtk_spin_button_update (spin);
	  
	  if (event->y <= widget->requisition.height / 2)
	    {
	      spin->click_child = GTK_ARROW_UP;
	      if (event->button == 1)
		{
		 gtk_spin_button_real_spin (spin, 
					    spin->adjustment->step_increment);
		  if (!spin->timer)
		    {
		      spin->timer_step = spin->adjustment->step_increment;
		      spin->need_timer = TRUE;
		      spin->timer = gtk_timeout_add 
			(SPIN_BUTTON_INITIAL_TIMER_DELAY, 
			 (GtkFunction) gtk_spin_button_timer, (gpointer) spin);
		    }
		}
	      else if (event->button == 2)
		{
		 gtk_spin_button_real_spin (spin, 
					    spin->adjustment->page_increment);
		  if (!spin->timer) 
		    {
		      spin->timer_step = spin->adjustment->page_increment;
		      spin->need_timer = TRUE;
		      spin->timer = gtk_timeout_add 
			(SPIN_BUTTON_INITIAL_TIMER_DELAY, 
			 (GtkFunction) gtk_spin_button_timer, (gpointer) spin);
		    }
		}
	      gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	    }
	  else 
	    {
	      spin->click_child = GTK_ARROW_DOWN;
	      if (event->button == 1)
		{
		  gtk_spin_button_real_spin (spin,
					     -spin->adjustment->step_increment);
		  if (!spin->timer)
		    {
		      spin->timer_step = spin->adjustment->step_increment;
		      spin->need_timer = TRUE;
		      spin->timer = gtk_timeout_add 
			(SPIN_BUTTON_INITIAL_TIMER_DELAY, 
			 (GtkFunction) gtk_spin_button_timer, (gpointer) spin);
		    }
		}      
	      else if (event->button == 2)
		{
		  gtk_spin_button_real_spin (spin,
					     -spin->adjustment->page_increment);
		  if (!spin->timer) 
		    {
		      spin->timer_step = spin->adjustment->page_increment;
		      spin->need_timer = TRUE;
		      spin->timer = gtk_timeout_add 
			(SPIN_BUTTON_INITIAL_TIMER_DELAY, 
			 (GtkFunction) gtk_spin_button_timer, (gpointer) spin);
		    }
		}
	      gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	    }
	}
      else
	GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
    }
  return FALSE;
}

static gint
gtk_spin_button_button_release (GtkWidget      *widget,
				GdkEventButton *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (event->button == spin->button)
    {
      guint click_child;

      if (spin->timer)
	{
	  gtk_timeout_remove (spin->timer);
	  spin->timer = 0;
	  spin->timer_calls = 0;
	  spin->need_timer = FALSE;
	}

      if (event->button == 3)
	{
	  if (event->y >= 0 && event->x >= 0 && 
	      event->y <= widget->requisition.height &&
	      event->x <= ARROW_SIZE + 2 * widget->style->klass->xthickness)
	    {
	      if (spin->click_child == GTK_ARROW_UP &&
		  event->y <= widget->requisition.height / 2)
		{
		  gfloat diff;

		  diff = spin->adjustment->upper - spin->adjustment->value;
		  if (diff > EPSILON)
		    gtk_spin_button_real_spin (spin, diff);
		}
	      else if (spin->click_child == GTK_ARROW_DOWN &&
		       event->y > widget->requisition.height / 2)
		{
		  gfloat diff;

		  diff = spin->adjustment->value - spin->adjustment->lower;
		  if (diff > EPSILON)
		    gtk_spin_button_real_spin (spin, -diff);
		}
	    }
	}		  
      gtk_grab_remove (widget);
      click_child = spin->click_child;
      spin->click_child = 2;
      spin->button = 0;
      gtk_spin_button_draw_arrow (spin, click_child);
    }
  else
    GTK_WIDGET_CLASS (parent_class)->button_release_event (widget, event);

  return FALSE;
}

static gint
gtk_spin_button_motion_notify (GtkWidget      *widget,
			       GdkEventMotion *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);
  
  if (spin->button)
    return FALSE;

  if (event->window == spin->panel)
    {
      gint y;

      y = event->y;
      if (event->is_hint)
	gdk_window_get_pointer (spin->panel, NULL, &y, NULL);

      if (y <= widget->requisition.height / 2 && 
	  spin->in_child == GTK_ARROW_DOWN)
	{
	  spin->in_child = GTK_ARROW_UP;
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	}
      else if (y > widget->requisition.height / 2 && 
	  spin->in_child == GTK_ARROW_UP)
	{
	  spin->in_child = GTK_ARROW_DOWN;
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	}
      return FALSE;
    }
	  
  return GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);
}

static gint
gtk_spin_button_timer (GtkSpinButton *spin_button)
{
  gboolean retval = FALSE;
  
  GDK_THREADS_ENTER ();

  if (spin_button->timer)
    {
      if (spin_button->click_child == GTK_ARROW_UP)
	gtk_spin_button_real_spin (spin_button,	spin_button->timer_step);
      else
	gtk_spin_button_real_spin (spin_button,	-spin_button->timer_step);

      if (spin_button->need_timer)
	{
	  spin_button->need_timer = FALSE;
	  spin_button->timer = gtk_timeout_add 
	    (SPIN_BUTTON_TIMER_DELAY, (GtkFunction) gtk_spin_button_timer, 
	     (gpointer) spin_button);
	}
      else 
	{
	  if (spin_button->climb_rate > 0.0 && spin_button->timer_step 
	      < spin_button->adjustment->page_increment)
	    {
	      if (spin_button->timer_calls < MAX_TIMER_CALLS)
		spin_button->timer_calls++;
	      else 
		{
		  spin_button->timer_calls = 0;
		  spin_button->timer_step += spin_button->climb_rate;
		}
	    }
	  retval = TRUE;
	}
    }

  GDK_THREADS_LEAVE ();

  return retval;
}

static void
gtk_spin_button_value_changed (GtkAdjustment *adjustment,
			       GtkSpinButton *spin_button)
{
  char buf[MAX_TEXT_LENGTH];

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  sprintf (buf, "%0.*f", spin_button->digits, adjustment->value);
  gtk_entry_set_text (GTK_ENTRY (spin_button), buf);

  gtk_spin_button_draw_arrow (spin_button, GTK_ARROW_UP);
  gtk_spin_button_draw_arrow (spin_button, GTK_ARROW_DOWN);
}

static gint
gtk_spin_button_key_press (GtkWidget     *widget,
			   GdkEventKey   *event)
{
  GtkSpinButton *spin;
  gint key;
  gboolean key_repeat = FALSE;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  spin = GTK_SPIN_BUTTON (widget);
  key = event->keyval;

  key_repeat = (event->time == spin->ev_time);

  if (GTK_EDITABLE (widget)->editable &&
      (key == GDK_Up || key == GDK_Down || 
       key == GDK_Page_Up || key == GDK_Page_Down))
    gtk_spin_button_update (spin);

  switch (key)
    {
    case GDK_Up:

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), 
					"key_press_event");
	  if (!key_repeat)
	    spin->timer_step = spin->adjustment->step_increment;

	 gtk_spin_button_real_spin (spin, spin->timer_step);

	  if (key_repeat)
	    {
	      if (spin->climb_rate > 0.0 && spin->timer_step
		  < spin->adjustment->page_increment)
		{
		  if (spin->timer_calls < MAX_TIMER_CALLS)
		    spin->timer_calls++;
		  else 
		    {
		      spin->timer_calls = 0;
		      spin->timer_step += spin->climb_rate;
		    }
		}
	    }
	  return TRUE;
	}
      return FALSE;

    case GDK_Down:

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), 
					"key_press_event");
	  if (!key_repeat)
	    spin->timer_step = spin->adjustment->step_increment;

	 gtk_spin_button_real_spin (spin, -spin->timer_step);

	  if (key_repeat)
	    {
	      if (spin->climb_rate > 0.0 && spin->timer_step
		  < spin->adjustment->page_increment)
		{
		  if (spin->timer_calls < MAX_TIMER_CALLS)
		    spin->timer_calls++;
		  else 
		    {
		      spin->timer_calls = 0;
		      spin->timer_step += spin->climb_rate;
		    }
		}
	    }
	  return TRUE;
	}
      return FALSE;

    case GDK_Page_Up:

      if (event->state & GDK_CONTROL_MASK)
	{
	  gfloat diff = spin->adjustment->upper - spin->adjustment->value;
	  if (diff > EPSILON)
	    gtk_spin_button_real_spin (spin, diff);
	}
      else
	gtk_spin_button_real_spin (spin, spin->adjustment->page_increment);
      return TRUE;

    case GDK_Page_Down:

      if (event->state & GDK_CONTROL_MASK)
	{
	  gfloat diff = spin->adjustment->value - spin->adjustment->lower;
	  if (diff > EPSILON)
	    gtk_spin_button_real_spin (spin, -diff);
	}
      else
	gtk_spin_button_real_spin (spin, -spin->adjustment->page_increment);
      return TRUE;

    default:
      break;
    }

  return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
}

static gint
gtk_spin_button_key_release (GtkWidget   *widget,
			     GdkEventKey *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  
  spin = GTK_SPIN_BUTTON (widget);
  
  spin->ev_time = event->time;
  return TRUE;
}

static void
gtk_spin_button_snap (GtkSpinButton *spin_button,
		      gfloat         val)
{
  gfloat inc;
  gfloat tmp;
  
  inc = spin_button->adjustment->step_increment;
  tmp = (val - spin_button->adjustment->lower) / inc;
  if (tmp - floor (tmp) < ceil (tmp) - tmp)
    val = spin_button->adjustment->lower + floor (tmp) * inc;
  else
    val = spin_button->adjustment->lower + ceil (tmp) * inc;

  if (fabs (val - spin_button->adjustment->value) > EPSILON)
    gtk_adjustment_set_value (spin_button->adjustment, val);
  else
    {
      char buf[MAX_TEXT_LENGTH];

      sprintf (buf, "%0.*f", spin_button->digits, 
	       spin_button->adjustment->value);
      if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
	gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
    }
}

void 
gtk_spin_button_update (GtkSpinButton *spin_button)
{
  gfloat val;
  gchar *error = NULL;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  val = strtod (gtk_entry_get_text (GTK_ENTRY (spin_button)), &error);

  if (spin_button->update_policy == GTK_UPDATE_ALWAYS)
    {
      if (val < spin_button->adjustment->lower)
	val = spin_button->adjustment->lower;
      else if (val > spin_button->adjustment->upper)
	val = spin_button->adjustment->upper;
    }
  else if ((spin_button->update_policy == GTK_UPDATE_IF_VALID) && 
	   (*error ||
	   val < spin_button->adjustment->lower ||
	   val > spin_button->adjustment->upper))
    {
      gtk_spin_button_value_changed (spin_button->adjustment, spin_button);
      return;
    }

  if (spin_button->snap_to_ticks)
    gtk_spin_button_snap (spin_button, val);
  else
    {
      if (fabs (val - spin_button->adjustment->value) > EPSILON)
	gtk_adjustment_set_value (spin_button->adjustment, val);
      else
	{
	  char buf[MAX_TEXT_LENGTH];
	  
	  sprintf (buf, "%0.*f", spin_button->digits, 
		   spin_button->adjustment->value);
	  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
	    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
	}
    }
}

static void
gtk_spin_button_activate (GtkEditable *editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (editable));

  if (editable->editable)
    gtk_spin_button_update (GTK_SPIN_BUTTON (editable));
}

static void
gtk_spin_button_insert_text (GtkEditable *editable,
			     const gchar *new_text,
			     gint         new_text_length,
			     gint        *position)
{
  GtkEntry *entry;
  GtkSpinButton *spin;
 
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (editable));

  entry = GTK_ENTRY (editable);
  spin  = GTK_SPIN_BUTTON (editable);

  if (spin->numeric)
    {
      struct lconv *lc;
      gboolean sign;
      gint dotpos = -1;
      gint i;
      GdkWChar pos_sign;
      GdkWChar neg_sign;
      gint entry_length;

      entry_length = entry->text_length;

      lc = localeconv ();

      if (*(lc->negative_sign))
	neg_sign = *(lc->negative_sign);
      else 
	neg_sign = '-';

      if (*(lc->positive_sign))
	pos_sign = *(lc->positive_sign);
      else 
	pos_sign = '+';

      for (sign=0, i=0; i<entry_length; i++)
	if ((entry->text[i] == neg_sign) ||
	    (entry->text[i] == pos_sign))
	  {
	    sign = 1;
	    break;
	  }

      if (sign && !(*position))
	return;

      for (dotpos=-1, i=0; i<entry_length; i++)
	if (entry->text[i] == *(lc->decimal_point))
	  {
	    dotpos = i;
	    break;
	  }

      if (dotpos > -1 && *position > dotpos &&
	  (gint)spin->digits - entry_length
	    + dotpos - new_text_length + 1 < 0)
	return;

      for (i = 0; i < new_text_length; i++)
	{
	  if (new_text[i] == neg_sign || new_text[i] == pos_sign)
	    {
	      if (sign || (*position) || i)
		return;
	      sign = TRUE;
	    }
	  else if (new_text[i] == *(lc->decimal_point))
	    {
	      if (!spin->digits || dotpos > -1 || 
 		  (new_text_length - 1 - i + entry_length
		    - *position > (gint)spin->digits)) 
		return;
	      dotpos = *position + i;
	    }
	  else if (new_text[i] < 0x30 || new_text[i] > 0x39)
	    return;
	}
    }

  GTK_EDITABLE_CLASS (parent_class)->insert_text (editable, new_text,
						  new_text_length, position);
}

static void
gtk_spin_button_real_spin (GtkSpinButton *spin_button,
			   gfloat         increment)
{
  GtkAdjustment *adj;
  gfloat new_value = 0.0;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  
  adj = spin_button->adjustment;

  new_value = adj->value + increment;

  if (increment > 0)
    {
      if (spin_button->wrap)
	{
	  if (fabs (adj->value - adj->upper) < EPSILON)
	    new_value = adj->lower;
	  else if (new_value > adj->upper)
	    new_value = adj->upper;
	}
      else
	new_value = MIN (new_value, adj->upper);
    }
  else if (increment < 0) 
    {
      if (spin_button->wrap)
	{
	  if (fabs (adj->value - adj->lower) < EPSILON)
	    new_value = adj->upper;
	  else if (new_value < adj->lower)
	    new_value = adj->lower;
	}
      else
	new_value = MAX (new_value, adj->lower);
    }

  if (fabs (new_value - adj->value) > EPSILON)
    gtk_adjustment_set_value (adj, new_value);
}


/***********************************************************
 ***********************************************************
 ***                  Public interface                   ***
 ***********************************************************
 ***********************************************************/


void
gtk_spin_button_configure (GtkSpinButton  *spin_button,
			   GtkAdjustment  *adjustment,
			   gfloat          climb_rate,
			   guint           digits)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  g_return_if_fail (digits < 6);

  if (adjustment)
    gtk_spin_button_set_adjustment (spin_button, adjustment);
  else
    adjustment = spin_button->adjustment;

  spin_button->digits = digits;
  spin_button->climb_rate = climb_rate;
  gtk_adjustment_value_changed (adjustment);
}

GtkWidget *
gtk_spin_button_new (GtkAdjustment *adjustment,
		     gfloat         climb_rate,
		     guint          digits)
{
  GtkSpinButton *spin;

  if (adjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), NULL);
  g_return_val_if_fail (digits < 6, NULL);

  spin = gtk_type_new (GTK_TYPE_SPIN_BUTTON);

  gtk_spin_button_configure (spin, adjustment, climb_rate, digits);

  return GTK_WIDGET (spin);
}

/* Callback used when the spin button's adjustment changes.  We need to redraw
 * the arrows when the adjustment's range changes.
 */
static void
adjustment_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
  GtkSpinButton *spin_button;

  spin_button = GTK_SPIN_BUTTON (data);

  gtk_spin_button_draw_arrow (spin_button, GTK_ARROW_UP);
  gtk_spin_button_draw_arrow (spin_button, GTK_ARROW_DOWN);
}

void
gtk_spin_button_set_adjustment (GtkSpinButton *spin_button,
				GtkAdjustment *adjustment)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (spin_button->adjustment != adjustment)
    {
      if (spin_button->adjustment)
        {
          gtk_signal_disconnect_by_data (GTK_OBJECT (spin_button->adjustment),
                                         (gpointer) spin_button);
          gtk_object_unref (GTK_OBJECT (spin_button->adjustment));
        }
      spin_button->adjustment = adjustment;
      if (adjustment)
        {
          gtk_object_ref (GTK_OBJECT (adjustment));
	  gtk_object_sink (GTK_OBJECT (adjustment));
          gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			      (GtkSignalFunc) gtk_spin_button_value_changed,
			      (gpointer) spin_button);
	  gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
			      (GtkSignalFunc) adjustment_changed_cb,
			      (gpointer) spin_button);
        }
    }
}

GtkAdjustment *
gtk_spin_button_get_adjustment (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (spin_button != NULL, NULL);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), NULL);

  return spin_button->adjustment;
}

void
gtk_spin_button_set_digits (GtkSpinButton *spin_button,
			    guint          digits)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  g_return_if_fail (digits < 6);

  if (spin_button->digits != digits)
    {
      spin_button->digits = digits;
      gtk_spin_button_value_changed (spin_button->adjustment, spin_button);
    }
}

gfloat
gtk_spin_button_get_value_as_float (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (spin_button != NULL, 0.0);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0.0);

  return spin_button->adjustment->value;
}

gint
gtk_spin_button_get_value_as_int (GtkSpinButton *spin_button)
{
  gfloat val;

  g_return_val_if_fail (spin_button != NULL, 0);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

  val = spin_button->adjustment->value;
  if (val - floor (val) < ceil (val) - val)
    return floor (val);
  else
    return ceil (val);
}

void 
gtk_spin_button_set_value (GtkSpinButton *spin_button, 
			   gfloat         value)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (fabs (value - spin_button->adjustment->value) > EPSILON)
    gtk_adjustment_set_value (spin_button->adjustment, value);
  else
    {
      char buf[MAX_TEXT_LENGTH];

      sprintf (buf, "%0.*f", spin_button->digits, 
               spin_button->adjustment->value);
      if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
        gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
    }
}

void
gtk_spin_button_set_update_policy (GtkSpinButton             *spin_button,
				   GtkSpinButtonUpdatePolicy  policy)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->update_policy = policy;
}

void
gtk_spin_button_set_numeric (GtkSpinButton  *spin_button,
			     gboolean        numeric)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->numeric = (numeric != 0);
}

void
gtk_spin_button_set_wrap (GtkSpinButton  *spin_button,
			  gboolean        wrap)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->wrap = (wrap != 0);
}

void
gtk_spin_button_set_shadow_type (GtkSpinButton *spin_button,
				 GtkShadowType  shadow_type)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (shadow_type != spin_button->shadow_type)
    {
      spin_button->shadow_type = shadow_type;
      if (GTK_WIDGET_DRAWABLE (spin_button))
	gtk_widget_queue_draw (GTK_WIDGET (spin_button));
    }
}

void
gtk_spin_button_set_snap_to_ticks (GtkSpinButton *spin_button,
				   gboolean       snap_to_ticks)
{
  guint new_val;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  new_val = (snap_to_ticks != 0);

  if (new_val != spin_button->snap_to_ticks)
    {
      spin_button->snap_to_ticks = new_val;
      if (new_val)
	{
	  gchar *error = NULL;
	  gfloat val;

	  val = strtod (gtk_entry_get_text (GTK_ENTRY (spin_button)), &error);
	  gtk_spin_button_snap (spin_button, val);
	}
    }
}

void
gtk_spin_button_spin (GtkSpinButton *spin_button,
		      GtkSpinType    direction,
		      gfloat         increment)
{
  GtkAdjustment *adj;
  gfloat diff;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  
  adj = spin_button->adjustment;

  /* for compatibility with the 1.0.x version of this function */
  if (increment != 0 && increment != adj->step_increment &&
      (direction == GTK_SPIN_STEP_FORWARD ||
       direction == GTK_SPIN_STEP_BACKWARD))
    {
      if (direction == GTK_SPIN_STEP_BACKWARD && increment > 0)
	increment = -increment;
      direction = GTK_SPIN_USER_DEFINED;
    }

  switch (direction)
    {
    case GTK_SPIN_STEP_FORWARD:

      gtk_spin_button_real_spin (spin_button, adj->step_increment);
      break;

    case GTK_SPIN_STEP_BACKWARD:

      gtk_spin_button_real_spin (spin_button, -adj->step_increment);
      break;

    case GTK_SPIN_PAGE_FORWARD:

      gtk_spin_button_real_spin (spin_button, adj->page_increment);
      break;

    case GTK_SPIN_PAGE_BACKWARD:

      gtk_spin_button_real_spin (spin_button, -adj->page_increment);
      break;

    case GTK_SPIN_HOME:

      diff = adj->value - adj->lower;
      if (diff > EPSILON)
	gtk_spin_button_real_spin (spin_button, -diff);
      break;

    case GTK_SPIN_END:

      diff = adj->upper - adj->value;
      if (diff > EPSILON)
	gtk_spin_button_real_spin (spin_button, diff);
      break;

    case GTK_SPIN_USER_DEFINED:

      if (increment != 0)
	gtk_spin_button_real_spin (spin_button, increment);
      break;

    default:
      break;
    }
}
