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
#include <string.h>
#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"


#define CHILD_SPACING     1
#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7


enum {
  PRESSED,
  RELEASED,
  CLICKED,
  ENTER,
  LEAVE,
  LAST_SIGNAL
};
enum {
  ARG_0,
  ARG_LABEL
};
  


static void gtk_button_class_init     (GtkButtonClass   *klass);
static void gtk_button_init           (GtkButton        *button);
static void gtk_button_set_arg        (GtkButton        *button,
				       GtkArg           *arg,
				       guint		 arg_id);
static void gtk_button_map            (GtkWidget        *widget);
static void gtk_button_unmap          (GtkWidget        *widget);
static void gtk_button_realize        (GtkWidget        *widget);
static void gtk_button_size_request   (GtkWidget        *widget,
				       GtkRequisition   *requisition);
static void gtk_button_size_allocate  (GtkWidget        *widget,
				       GtkAllocation    *allocation);
static void gtk_button_paint          (GtkWidget        *widget,
				       GdkRectangle     *area);
static void gtk_button_draw           (GtkWidget        *widget,
				       GdkRectangle     *area);
static void gtk_button_draw_focus     (GtkWidget        *widget);
static void gtk_button_draw_default   (GtkWidget        *widget);
static gint gtk_button_expose         (GtkWidget        *widget,
				       GdkEventExpose   *event);
static gint gtk_button_button_press   (GtkWidget        *widget,
				       GdkEventButton   *event);
static gint gtk_button_button_release (GtkWidget        *widget,
				       GdkEventButton   *event);
static gint gtk_button_enter_notify   (GtkWidget        *widget,
				       GdkEventCrossing *event);
static gint gtk_button_leave_notify   (GtkWidget        *widget,
				       GdkEventCrossing *event);
static gint gtk_button_focus_in       (GtkWidget        *widget,
				       GdkEventFocus    *event);
static gint gtk_button_focus_out      (GtkWidget        *widget,
				       GdkEventFocus    *event);
static void gtk_button_add            (GtkContainer     *container,
				       GtkWidget        *widget);
static void gtk_button_remove         (GtkContainer     *container,
				       GtkWidget        *widget);
static void gtk_button_foreach        (GtkContainer     *container,
				       GtkCallback       callback,
				       gpointer          callback_data);
static void gtk_real_button_pressed   (GtkButton        *button);
static void gtk_real_button_released  (GtkButton        *button);
static void gtk_real_button_enter     (GtkButton        *button);
static void gtk_real_button_leave     (GtkButton        *button);


static GtkContainerClass *parent_class;
static guint button_signals[LAST_SIGNAL] = { 0 };


guint
gtk_button_get_type ()
{
  static guint button_type = 0;

  if (!button_type)
    {
      GtkTypeInfo button_info =
      {
	"GtkButton",
	sizeof (GtkButton),
	sizeof (GtkButtonClass),
	(GtkClassInitFunc) gtk_button_class_init,
	(GtkObjectInitFunc) gtk_button_init,
	(GtkArgSetFunc) gtk_button_set_arg,
	(GtkArgGetFunc) NULL,
      };

      button_type = gtk_type_unique (gtk_container_get_type (), &button_info);
    }

  return button_type;
}

static void
gtk_button_class_init (GtkButtonClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  parent_class = gtk_type_class (gtk_container_get_type ());

  gtk_object_add_arg_type ("GtkButton::label", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_LABEL);

  button_signals[PRESSED] =
    gtk_signal_new ("pressed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, pressed),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  button_signals[RELEASED] =
    gtk_signal_new ("released",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, released),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  button_signals[CLICKED] =
    gtk_signal_new ("clicked",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, clicked),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  button_signals[ENTER] =
    gtk_signal_new ("enter",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, enter),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  button_signals[LEAVE] =
    gtk_signal_new ("leave",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, leave),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, button_signals, LAST_SIGNAL);

  widget_class->activate_signal = button_signals[CLICKED];
  widget_class->map = gtk_button_map;
  widget_class->unmap = gtk_button_unmap;
  widget_class->realize = gtk_button_realize;
  widget_class->draw = gtk_button_draw;
  widget_class->draw_focus = gtk_button_draw_focus;
  widget_class->draw_default = gtk_button_draw_default;
  widget_class->size_request = gtk_button_size_request;
  widget_class->size_allocate = gtk_button_size_allocate;
  widget_class->expose_event = gtk_button_expose;
  widget_class->button_press_event = gtk_button_button_press;
  widget_class->button_release_event = gtk_button_button_release;
  widget_class->enter_notify_event = gtk_button_enter_notify;
  widget_class->leave_notify_event = gtk_button_leave_notify;
  widget_class->focus_in_event = gtk_button_focus_in;
  widget_class->focus_out_event = gtk_button_focus_out;

  container_class->add = gtk_button_add;
  container_class->remove = gtk_button_remove;
  container_class->foreach = gtk_button_foreach;

  klass->pressed = gtk_real_button_pressed;
  klass->released = gtk_real_button_released;
  klass->clicked = NULL;
  klass->enter = gtk_real_button_enter;
  klass->leave = gtk_real_button_leave;
}

static void
gtk_button_init (GtkButton *button)
{
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_FOCUS);

  button->child = NULL;
  button->in_button = FALSE;
  button->button_down = FALSE;
}

static void
gtk_button_set_arg (GtkButton *button,
		    GtkArg    *arg,
		    guint      arg_id)
{
  GtkWidget *label;

  switch (arg_id)
    {
    case ARG_LABEL:
      gtk_container_disable_resize (GTK_CONTAINER (button));
      
      if (button->child)
	{
	  gtk_widget_unparent (button->child);
	  button->child = NULL;
	}
      
      label = gtk_label_new (GTK_VALUE_STRING(*arg));
      gtk_widget_show (label);
      
      gtk_container_add (GTK_CONTAINER (button), label);
      gtk_container_enable_resize (GTK_CONTAINER (button));
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

GtkWidget*
gtk_button_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_button_get_type ()));
}

GtkWidget*
gtk_button_new_with_label (const gchar *label)
{
  GtkWidget *button;
  GtkWidget *label_widget;

  button = gtk_button_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.5, 0.5);

  gtk_container_add (GTK_CONTAINER (button), label_widget);
  gtk_widget_show (label_widget);

  return button;
}

void
gtk_button_pressed (GtkButton *button)
{
  gtk_signal_emit (GTK_OBJECT (button), button_signals[PRESSED]);
}

void
gtk_button_released (GtkButton *button)
{
  gtk_signal_emit (GTK_OBJECT (button), button_signals[RELEASED]);
}

void
gtk_button_clicked (GtkButton *button)
{
  gtk_signal_emit (GTK_OBJECT (button), button_signals[CLICKED]);
}

void
gtk_button_enter (GtkButton *button)
{
  gtk_signal_emit (GTK_OBJECT (button), button_signals[ENTER]);
}

void
gtk_button_leave (GtkButton *button)
{
  gtk_signal_emit (GTK_OBJECT (button), button_signals[LEAVE]);
}

static void
gtk_button_map (GtkWidget *widget)
{
  GtkButton *button;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  gdk_window_show (widget->window);

  button = GTK_BUTTON (widget);

  if (button->child &&
      GTK_WIDGET_VISIBLE (button->child) &&
      !GTK_WIDGET_MAPPED (button->child))
    gtk_widget_map (button->child);
}

static void
gtk_button_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  gdk_window_hide (widget->window);
}

static void
gtk_button_realize (GtkWidget *widget)
{
  GtkButton *button;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));

  button = GTK_BUTTON (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
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
  gdk_window_set_user_data (widget->window, button);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_button_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkButton *button;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));
  g_return_if_fail (requisition != NULL);

  button = GTK_BUTTON (widget);

  requisition->width = (GTK_CONTAINER (widget)->border_width + CHILD_SPACING +
			GTK_WIDGET (widget)->style->klass->xthickness) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width + CHILD_SPACING +
			 GTK_WIDGET (widget)->style->klass->ythickness) * 2;

  if (GTK_WIDGET_CAN_DEFAULT (widget))
    {
      requisition->width += (GTK_WIDGET (widget)->style->klass->xthickness * 2 +
			     DEFAULT_SPACING);
      requisition->height += (GTK_WIDGET (widget)->style->klass->ythickness * 2 +
			      DEFAULT_SPACING);
    }

  if (button->child && GTK_WIDGET_VISIBLE (button->child))
    {
      gtk_widget_size_request (button->child, &button->child->requisition);

      requisition->width += button->child->requisition.width;
      requisition->height += button->child->requisition.height;
    }
}

static void
gtk_button_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkButton *button;
  GtkAllocation child_allocation;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  border_width = GTK_CONTAINER (widget)->border_width;

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    widget->allocation.x + border_width,
			    widget->allocation.y + border_width,
			    widget->allocation.width - border_width * 2,
			    widget->allocation.height - border_width * 2);

  button = GTK_BUTTON (widget);

  if (button->child && GTK_WIDGET_VISIBLE (button->child))
    {
      child_allocation.x = (CHILD_SPACING + GTK_WIDGET (widget)->style->klass->xthickness);
      child_allocation.y = (CHILD_SPACING + GTK_WIDGET (widget)->style->klass->ythickness);

      child_allocation.width = widget->allocation.width - child_allocation.x * 2 -
	                         border_width * 2;
      child_allocation.height = widget->allocation.height - child_allocation.y * 2 -
	                          border_width * 2;

      if (GTK_WIDGET_CAN_DEFAULT (button))
	{
	  child_allocation.x += (GTK_WIDGET (widget)->style->klass->xthickness +
				 DEFAULT_LEFT_POS);
	  child_allocation.y += (GTK_WIDGET (widget)->style->klass->ythickness +
				 DEFAULT_TOP_POS);
	  child_allocation.width -= (GTK_WIDGET (widget)->style->klass->xthickness * 2 +
				     DEFAULT_SPACING);
	  child_allocation.height -= (GTK_WIDGET (widget)->style->klass->xthickness * 2 +
				      DEFAULT_SPACING);
	}

      gtk_widget_size_allocate (button->child, &child_allocation);
    }
}

static void
gtk_button_paint (GtkWidget    *widget,
		  GdkRectangle *area)
{
  GdkRectangle restrict_area;
  GdkRectangle new_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      restrict_area.x = GTK_WIDGET (widget)->style->klass->xthickness;
      restrict_area.y = GTK_WIDGET (widget)->style->klass->ythickness;
      restrict_area.width = (GTK_WIDGET (widget)->allocation.width - restrict_area.x * 2 -
                             GTK_CONTAINER (widget)->border_width * 2);
      restrict_area.height = (GTK_WIDGET (widget)->allocation.height - restrict_area.y * 2 -
                              GTK_CONTAINER (widget)->border_width * 2);

      if (GTK_WIDGET_CAN_DEFAULT (widget))
	{
	  restrict_area.x += DEFAULT_LEFT_POS;
	  restrict_area.y += DEFAULT_TOP_POS;
	  restrict_area.width -= DEFAULT_SPACING;
	  restrict_area.height -= DEFAULT_SPACING;
	}

      if (gdk_rectangle_intersect (area, &restrict_area, &new_area))
	{
	  gtk_style_set_background (widget->style, widget->window, GTK_WIDGET_STATE (widget));
	  gdk_window_clear_area (widget->window,
				 new_area.x, new_area.y,
				 new_area.width, new_area.height);
	}
    }
}

static void
gtk_button_draw (GtkWidget    *widget,
		 GdkRectangle *area)
{
  GtkButton *button;
  GdkRectangle child_area;
  GdkRectangle tmp_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      button = GTK_BUTTON (widget);

      tmp_area = *area;
      tmp_area.x -= GTK_CONTAINER (button)->border_width;
      tmp_area.y -= GTK_CONTAINER (button)->border_width;

      gtk_button_paint (widget, &tmp_area);

      if (button->child && gtk_widget_intersect (button->child, &tmp_area, &child_area))
	gtk_widget_draw (button->child, &child_area);

      gtk_widget_draw_default (widget);
      gtk_widget_draw_focus (widget);
    }
}

static void
gtk_button_draw_focus (GtkWidget *widget)
{
  GtkButton *button;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      button = GTK_BUTTON (widget);

      x = 0;
      y = 0;
      width = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
      height = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;

      if (GTK_WIDGET_CAN_DEFAULT (widget))
	{
	  x += widget->style->klass->xthickness;
	  y += widget->style->klass->ythickness;
	  width -= 2 * x + DEFAULT_SPACING;
	  height -= 2 * y + DEFAULT_SPACING;
	  x += DEFAULT_LEFT_POS;
	  y += DEFAULT_TOP_POS;
	}

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x += 1;
	  y += 1;
	  width -= 2;
	  height -= 2;
	}
      else
	{
	  if (GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE)
	    gdk_draw_rectangle (widget->window,
				widget->style->bg_gc[GTK_WIDGET_STATE (widget)], FALSE,
				x + 1, y + 1, width - 4, height - 4);
	  else
	    gdk_draw_rectangle (widget->window,
				widget->style->bg_gc[GTK_WIDGET_STATE (widget)], FALSE,
				x + 2, y + 2, width - 5, height - 5);
	}

      if (GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE)
	shadow_type = GTK_SHADOW_IN;
      else
	shadow_type = GTK_SHADOW_OUT;

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_WIDGET_STATE (widget), shadow_type,
		       x, y, width, height);

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x -= 1;
	  y -= 1;
	  width += 2;
	  height += 2;

	  gdk_draw_rectangle (widget->window,
			      widget->style->black_gc, FALSE,
			      x, y, width - 1, height - 1);
	}
    }
}

static void
gtk_button_draw_default (GtkWidget *widget)
{
  gint width, height;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      x = 0;
      y = 0;
      width = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
      height = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;

      if (GTK_WIDGET_HAS_DEFAULT (widget))
	{
	  gtk_draw_shadow (widget->style, widget->window,
			   GTK_STATE_NORMAL, GTK_SHADOW_IN,
			   x, y, width, height);
	}
      else
	{
	  gdk_draw_rectangle (widget->window, widget->style->bg_gc[GTK_STATE_NORMAL],
			      FALSE, x, y, width - 1, height - 1);
	  gdk_draw_rectangle (widget->window, widget->style->bg_gc[GTK_STATE_NORMAL],
			      FALSE, x + 1, y + 1, width - 3, height - 3);
	}
    }
}

static gint
gtk_button_expose (GtkWidget      *widget,
		   GdkEventExpose *event)
{
  GtkButton *button;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      button = GTK_BUTTON (widget);

      gtk_button_paint (widget, &event->area);

      child_event = *event;
      if (button->child && GTK_WIDGET_NO_WINDOW (button->child) &&
	  gtk_widget_intersect (button->child, &event->area, &child_event.area))
	gtk_widget_event (button->child, (GdkEvent*) &child_event);

      gtk_widget_draw_default (widget);
      gtk_widget_draw_focus (widget);
    }

  return FALSE;
}

static gint
gtk_button_button_press (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkButton *button;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type == GDK_BUTTON_PRESS)
    {
      button = GTK_BUTTON (widget);

      if (GTK_WIDGET_CAN_DEFAULT (widget) && (event->button == 1))
	gtk_widget_grab_default (widget);
      if (!GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);

      if (event->button == 1)
	{
	  gtk_grab_add (GTK_WIDGET (button));
	  gtk_button_pressed (button);
	}
    }

  return TRUE;
}

static gint
gtk_button_button_release (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GtkButton *button;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->button == 1)
    {
      button = GTK_BUTTON (widget);
      gtk_grab_remove (GTK_WIDGET (button));
      gtk_button_released (button);
    }

  return TRUE;
}

static gint
gtk_button_enter_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GtkButton *button;
  GtkWidget *event_widget;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  button = GTK_BUTTON (widget);
  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      button->in_button = TRUE;
      gtk_button_enter (button);
    }

  return FALSE;
}

static gint
gtk_button_leave_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GtkButton *button;
  GtkWidget *event_widget;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  button = GTK_BUTTON (widget);
  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      button->in_button = FALSE;
      gtk_button_leave (button);
    }

  return FALSE;
}

static gint
gtk_button_focus_in (GtkWidget     *widget,
		     GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return FALSE;
}

static gint
gtk_button_focus_out (GtkWidget     *widget,
		      GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return FALSE;
}

static void
gtk_button_add (GtkContainer *container,
		GtkWidget    *widget)
{
  GtkButton *button;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BUTTON (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (gtk_widget_basic (widget));

  button = GTK_BUTTON (container);

  if (!button->child)
    {
      gtk_widget_set_parent (widget, GTK_WIDGET (container));

      if (GTK_WIDGET_VISIBLE (widget->parent))
	{
	  if (GTK_WIDGET_REALIZED (widget->parent) &&
	      !GTK_WIDGET_REALIZED (widget))
	    gtk_widget_realize (widget);
	  
	  if (GTK_WIDGET_MAPPED (widget->parent) &&
	      !GTK_WIDGET_MAPPED (widget))
	    gtk_widget_map (widget);
	}
      
      button->child = widget;

      if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
	gtk_widget_queue_resize (widget);
    }
}

static void
gtk_button_remove (GtkContainer *container,
		   GtkWidget    *widget)
{
  GtkButton *button;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BUTTON (container));
  g_return_if_fail (widget != NULL);

  button = GTK_BUTTON (container);

  if (button->child == widget)
    {
      gtk_widget_unparent (widget);

      button->child = NULL;

      if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
	gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_button_foreach (GtkContainer *container,
		    GtkCallback   callback,
		    gpointer      callback_data)
{
  GtkButton *button;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BUTTON (container));
  g_return_if_fail (callback != NULL);

  button = GTK_BUTTON (container);

  if (button->child)
    (* callback) (button->child, callback_data);
}

static void
gtk_real_button_pressed (GtkButton *button)
{
  GtkStateType new_state;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  button->button_down = TRUE;

  new_state = (button->in_button ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL);

  if (GTK_WIDGET_STATE (button) != new_state)
    {
      gtk_widget_set_state (GTK_WIDGET (button), new_state);
      gtk_widget_queue_draw (GTK_WIDGET (button));
    }
}

static void
gtk_real_button_released (GtkButton *button)
{
  GtkStateType new_state;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  if (button->button_down)
    {
      button->button_down = FALSE;

      if (button->in_button)
	gtk_button_clicked (button);

      new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL);

      if (GTK_WIDGET_STATE (button) != new_state)
	{
	  gtk_widget_set_state (GTK_WIDGET (button), new_state);
	  gtk_widget_queue_draw (GTK_WIDGET (button));
	}
    }
}

static void
gtk_real_button_enter (GtkButton *button)
{
  GtkStateType new_state;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  new_state = (button->button_down ? GTK_STATE_ACTIVE : GTK_STATE_PRELIGHT);

  if (GTK_WIDGET_STATE (button) != new_state)
    {
      gtk_widget_set_state (GTK_WIDGET (button), new_state);
      gtk_widget_queue_draw (GTK_WIDGET (button));
    }
}

static void
gtk_real_button_leave (GtkButton *button)
{
  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  if (GTK_WIDGET_STATE (button) != GTK_STATE_NORMAL)
    {
      gtk_widget_set_state (GTK_WIDGET (button), GTK_STATE_NORMAL);
      gtk_widget_queue_draw (GTK_WIDGET (button));
    }
}
