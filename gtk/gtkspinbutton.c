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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gdk/gdkkeysyms.h"
#include "gtkspinbutton.h"
#include "gtkmain.h"
#include "gtksignal.h"


#define MIN_SPIN_BUTTON_WIDTH           30
#define ARROW_SIZE                      11
#define SPIN_BUTTON_INITIAL_TIMER_DELAY 200
#define SPIN_BUTTON_TIMER_DELAY         20
#define MAX_TEXT_LENGTH                 256
#define MAX_TIMER_CALLS                 5


static void gtk_spin_button_class_init     (GtkSpinButtonClass *klass);
static void gtk_spin_button_init           (GtkSpinButton      *spin_button);
static void gtk_spin_button_finalize       (GtkObject          *object);
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
static void gtk_spin_button_spin           (GtkSpinButton      *spin_button,
		                            guint               direction,
		                            gfloat              step);
static void gtk_spin_button_value_changed  (GtkWidget          *widget,
					    GtkSpinButton      *spin_button); 
static gint gtk_spin_button_key_press      (GtkWidget          *widget,
					    GdkEventKey        *event);
static void gtk_spin_button_update         (GtkSpinButton      *spin_button);
static void gtk_spin_button_changed        (GtkEditable        *editable);
static void gtk_spin_button_activate       (GtkEntry           *entry);


static GtkWidgetClass *parent_class = NULL;


guint
gtk_spin_button_get_type ()
{
  static guint spin_button_type = 0;

  if (!spin_button_type)
    {
      GtkTypeInfo spin_button_info =
      {
	"GtkSpinButton",
	sizeof (GtkSpinButton),
	sizeof (GtkSpinButtonClass),
	(GtkClassInitFunc) gtk_spin_button_class_init,
	(GtkObjectInitFunc) gtk_spin_button_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      spin_button_type = gtk_type_unique (gtk_entry_get_type (), 
					  &spin_button_info);
    }
  return spin_button_type;
}

static void
gtk_spin_button_class_init (GtkSpinButtonClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkEditableClass *editable_class;
  GtkEntryClass  *entry_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  entry_class  = (GtkEntryClass*)  class; 
  editable_class  = (GtkEditableClass*)  class; 

  parent_class = gtk_type_class (gtk_entry_get_type ());

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
  widget_class->enter_notify_event = gtk_spin_button_enter_notify;
  widget_class->leave_notify_event = gtk_spin_button_leave_notify;
  widget_class->focus_out_event = gtk_spin_button_focus_out;

  editable_class->changed = gtk_spin_button_changed;
  entry_class->activate = gtk_spin_button_activate;
}

static void
gtk_spin_button_init (GtkSpinButton *spin_button)
{
  spin_button->adjustment = NULL;
  spin_button->panel      = NULL;

  spin_button->timer = 0;
  spin_button->climb_rate = 0.0;
  spin_button->timer_step = 0.0;
  
  spin_button->update_policy = GTK_UPDATE_ALWAYS | GTK_UPDATE_SNAP_TO_TICKS;

  spin_button->snapped = 0;
  spin_button->in_child = 2;
  spin_button->click_child = 2;
  spin_button->button = 0;
  spin_button->need_timer = 0;
  spin_button->timer_calls = 0;
  spin_button->digits = 0;
}

GtkWidget*
gtk_spin_button_new (GtkAdjustment *adjustment,
		     gfloat         climb_rate,
		     gint           digits)
{
  GtkSpinButton *spin;
  char buf[MAX_TEXT_LENGTH];

  g_return_val_if_fail (digits >= 0 && digits < 128, NULL);

  spin = gtk_type_new (gtk_spin_button_get_type ());

  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0, 0, 0, 0, 0, 0);

  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (spin), adjustment);
  spin->digits = digits;
  sprintf (buf, "%0.*f", digits, adjustment->value);
  gtk_entry_set_text (GTK_ENTRY (spin), buf);
  spin->climb_rate = climb_rate;

  return GTK_WIDGET (spin);
}

static void
gtk_spin_button_finalize (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (object));

  gtk_object_unref (GTK_OBJECT (GTK_SPIN_BUTTON (object)->adjustment));
  
  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_spin_button_map (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  if (!GTK_WIDGET_MAPPED (widget))
    {
      gdk_window_show (GTK_SPIN_BUTTON (widget)->panel);
      GTK_WIDGET_CLASS (parent_class)->map (widget);
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

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  spin = GTK_SPIN_BUTTON (widget);

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK 
    | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK 
    | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  attributes.x = widget->allocation.x + widget->allocation.width
    - ARROW_SIZE - 2 * widget->style->klass->xthickness;
  attributes.y = widget->allocation.y; 

  attributes.width = ARROW_SIZE;
  attributes.height = widget->allocation.height;
  
  spin->panel = gdk_window_new (gtk_widget_get_parent_window (widget), 
				&attributes, attributes_mask);

  gtk_style_set_background (widget->style, spin->panel, GTK_STATE_NORMAL);

  gdk_window_set_user_data (spin->panel, widget);
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
  child_allocation.width = allocation->width - ARROW_SIZE  
    - 2 * widget->style->klass->xthickness;
  child_allocation.height = allocation->height;

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, &child_allocation);

  widget->allocation = *allocation;
  
  child_allocation.width = ARROW_SIZE + 2 * widget->style->klass->xthickness;
  child_allocation.height = widget->requisition.height;  
  child_allocation.x = allocation->x + allocation->width - ARROW_SIZE 
    - 2 * widget->style->klass->xthickness;
  child_allocation.y = allocation->y + 
    (allocation->height - widget->requisition.height) / 2;

  gdk_window_move_resize (GTK_SPIN_BUTTON (widget)->panel, 
			  child_allocation.x,
			  child_allocation.y,
			  child_allocation.width,
			  child_allocation.height); 
}

static void
gtk_spin_button_paint (GtkWidget    *widget,
		       GdkRectangle *area)
{
  GtkSpinButton *spin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  spin = GTK_SPIN_BUTTON (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_draw_shadow (widget->style, spin->panel,
		       GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		       0, 0, ARROW_SIZE + 2 * widget->style->klass->xthickness,
		       widget->requisition.height); 

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
  GtkWidget *widget;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  
  widget = GTK_WIDGET (spin_button);

  if (GTK_WIDGET_DRAWABLE (spin_button))
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

      if (arrow == GTK_ARROW_UP)
	{
	  gtk_draw_arrow (widget->style, spin_button->panel,
			  state_type, shadow_type, arrow, TRUE, 
			  widget->style->klass->xthickness, 
			  widget->style->klass->ythickness, 
			  ARROW_SIZE, 
			  widget->requisition.height / 2 
			  - widget->style->klass->ythickness);
	}
      else
	{
	  gtk_draw_arrow (widget->style, spin_button->panel,
			  state_type, shadow_type, arrow, TRUE, 
			  widget->style->klass->xthickness, 
			  widget->requisition.height / 2,
			  ARROW_SIZE, 
			  widget->requisition.height / 2 
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
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);
  if ((spin->update_policy & GTK_UPDATE_SNAP_TO_TICKS) && !spin->snapped)
    gtk_spin_button_update (spin);

  GTK_WIDGET_CLASS (parent_class)->focus_out_event (widget, event);
  
  return FALSE;
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

	  if (event->y <= widget->requisition.height / 2)
	    {
	      spin->click_child = GTK_ARROW_UP;
	      if (event->button == 1)
		{
		  gtk_spin_button_spin (spin, spin->click_child,
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
		  gtk_spin_button_spin (spin, spin->click_child,
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
		  gtk_spin_button_spin (spin, spin->click_child,
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
		  gtk_spin_button_spin (spin, spin->click_child,
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

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

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
		  spin->adjustment->value < spin->adjustment->upper &&
		  event->y <= widget->requisition.height / 2)
		{
		  spin->adjustment->value = spin->adjustment->upper;
		  gtk_signal_emit_by_name (GTK_OBJECT (spin->adjustment), 
					   "value_changed");
		}
	      else if (spin->click_child == GTK_ARROW_DOWN &&
		       spin->adjustment->value > spin->adjustment->lower &&
		       event->y > widget->requisition.height / 2)
		{
		  spin->adjustment->value = spin->adjustment->lower;
		  gtk_signal_emit_by_name (GTK_OBJECT (spin->adjustment), 
					   "value_changed");
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
	  
  GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);

  return FALSE;
}

static gint
gtk_spin_button_timer (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (spin_button != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  if (spin_button->timer)
    {
      gtk_spin_button_spin (spin_button, spin_button->click_child, 
			    spin_button->timer_step);

      if (spin_button->need_timer) {
	spin_button->need_timer = FALSE;
	spin_button->timer = gtk_timeout_add 
	  (SPIN_BUTTON_TIMER_DELAY, (GtkFunction) gtk_spin_button_timer, 
	   (gpointer) spin_button);
	return FALSE;
      }
      else if (spin_button->climb_rate > 0.0 && 
	   spin_button->timer_step < spin_button->adjustment->page_increment)
	{
	  if (spin_button->timer_calls < MAX_TIMER_CALLS)
	    spin_button->timer_calls++;
	  else 
	    {
	      spin_button->timer_calls = 0;
	      spin_button->timer_step += spin_button->climb_rate;
	    }
      }
      return TRUE;
    }
  return FALSE;
}

static void
gtk_spin_button_spin (GtkSpinButton *spin_button,
		      guint          direction,
		      gfloat         step)
{
  gfloat new_value = 0.0;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (direction == GTK_ARROW_UP)
    {
      new_value = MIN (spin_button->adjustment->value + step,
		       spin_button->adjustment->upper);
    }
  else if (direction == GTK_ARROW_DOWN) 
    {
      new_value = MAX (spin_button->adjustment->value - step,
		       spin_button->adjustment->lower);
    }

  if (new_value != spin_button->adjustment->value)
    {  
      spin_button->adjustment->value = new_value;
      gtk_signal_emit_by_name (GTK_OBJECT (spin_button->adjustment),
			       "value_changed");
    }
}

static void
gtk_spin_button_value_changed (GtkWidget     *widget,
			       GtkSpinButton *spin_button)
{
  char buf[MAX_TEXT_LENGTH];

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (widget));

  sprintf (buf, "%0.*f", spin_button->digits, spin_button->adjustment->value);
  gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
}
  
static gint
gtk_spin_button_key_press (GtkWidget     *widget,
			   GdkEventKey   *event)
{
  GtkSpinButton *spin;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  spin = GTK_SPIN_BUTTON (widget);

  switch (event->keyval)
    {
    case GDK_Up:
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), 
					"key_press_event");
	  gtk_spin_button_spin (spin, GTK_ARROW_UP,
				spin->adjustment->step_increment);
	  return TRUE;
	}
      return FALSE;
    case GDK_Down:
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), 
					"key_press_event");
	  gtk_spin_button_spin (spin, GTK_ARROW_DOWN,
				spin->adjustment->step_increment);
	  return TRUE;
	}
      return FALSE;
    case GDK_Page_Up:
      if (event->state & GDK_CONTROL_MASK)
	{
	  spin->adjustment->value = spin->adjustment->upper;
	  gtk_signal_emit_by_name (GTK_OBJECT (spin->adjustment),
				   "value_changed");
	}
      else
	gtk_spin_button_spin (spin, GTK_ARROW_UP,
			      spin->adjustment->page_increment);
      return TRUE;
    case GDK_Page_Down:
      if (event->state & GDK_CONTROL_MASK)
	{
	  spin->adjustment->value = spin->adjustment->lower;
	  gtk_signal_emit_by_name (GTK_OBJECT (spin->adjustment),
				   "value_changed");
	}
      else
	gtk_spin_button_spin (spin, GTK_ARROW_DOWN,
			      spin->adjustment->page_increment);
      return TRUE;
    default:
      break;
    }
  return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
}

static void 
gtk_spin_button_update (GtkSpinButton *spin_button)
{
  gfloat tmp;
  gfloat val;
  gchar *error = NULL;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  val = strtod (GTK_ENTRY (spin_button)->text, &error);
  
  if (spin_button->update_policy & GTK_UPDATE_ALWAYS)
    {
      if (val < spin_button->adjustment->lower)
	val = spin_button->adjustment->lower;
      else if (val > spin_button->adjustment->upper)
	val = spin_button->adjustment->upper;
    }
  else if ((spin_button->update_policy & GTK_UPDATE_IF_VALID) && 
	   (*error ||
	   val < spin_button->adjustment->lower ||
	   val > spin_button->adjustment->upper))
    {
      gtk_signal_emit_by_name (GTK_OBJECT (spin_button->adjustment),
			       "value_changed"); 
      return;
    }

  if (spin_button->update_policy & GTK_UPDATE_SNAP_TO_TICKS)
    {
      gfloat inc;

      inc = spin_button->adjustment->step_increment;
      tmp = (val - spin_button->adjustment->lower) / inc;
      if (tmp - floor (tmp) < ceil (tmp) - tmp)
	val = spin_button->adjustment->lower + floor (tmp) * inc;
      else
	val = spin_button->adjustment->lower + ceil (tmp) * inc;
      spin_button->snapped = 1;
    }
  spin_button->adjustment->value = val;
  gtk_signal_emit_by_name (GTK_OBJECT (spin_button->adjustment),
			   "value_changed"); 
}

static void
gtk_spin_button_changed (GtkEditable *editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (editable));

  GTK_EDITABLE_CLASS (parent_class)->changed (editable);
  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (editable)))
    {
      GtkSpinButton *spin;
      gfloat val;
      gchar *error = NULL;

      spin = GTK_SPIN_BUTTON (editable);
      spin->snapped = 0;

      val = strtod (GTK_ENTRY (editable)->text, &error);
      if (val < spin->adjustment->lower)
	val = spin->adjustment->lower;
      else if (val > spin->adjustment->upper)
	val = spin->adjustment->upper;
      spin->adjustment->value = val;
    }
}

static void
gtk_spin_button_activate (GtkEntry *entry)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (entry));

  if (GTK_EDITABLE(entry)->editable)
    gtk_spin_button_update (GTK_SPIN_BUTTON (entry));
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
          gtk_signal_connect 
	    (GTK_OBJECT (adjustment), "value_changed",
	     (GtkSignalFunc) gtk_spin_button_value_changed,
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
			    gint           digits)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  g_return_if_fail (digits >= 0 || digits < 128);

  if (spin_button->digits != digits)
    {
      spin_button->digits = digits;
      if (GTK_WIDGET_VISIBLE (spin_button) && GTK_WIDGET_MAPPED (spin_button))
        gtk_signal_emit_by_name (GTK_OBJECT (spin_button->adjustment),
				 "value_changed"); 
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

  if (spin_button->adjustment->value != value)
    {
      spin_button->adjustment->value = value;
      if (GTK_WIDGET_VISIBLE (spin_button) && GTK_WIDGET_MAPPED (spin_button))
        gtk_signal_emit_by_name (GTK_OBJECT (spin_button->adjustment),
				 "value_changed"); 
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
