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

#include <string.h>
#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"


#define CHILD_SPACING     1
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
  ARG_LABEL,
  ARG_RELIEF
};



static void gtk_button_class_init     (GtkButtonClass   *klass);
static void gtk_button_init           (GtkButton        *button);
static void gtk_button_set_arg        (GtkObject        *object,
				       GtkArg           *arg,
				       guint		 arg_id);
static void gtk_button_get_arg        (GtkObject        *object,
				       GtkArg           *arg,
				       guint		 arg_id);
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
static void gtk_real_button_pressed   (GtkButton        *button);
static void gtk_real_button_released  (GtkButton        *button);
static void gtk_real_button_enter     (GtkButton        *button);
static void gtk_real_button_leave     (GtkButton        *button);
static GtkType gtk_button_child_type  (GtkContainer     *container);


static GtkBinClass *parent_class = NULL;
static guint button_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_button_get_type (void)
{
  static GtkType button_type = 0;

  if (!button_type)
    {
      static const GtkTypeInfo button_info =
      {
	"GtkButton",
	sizeof (GtkButton),
	sizeof (GtkButtonClass),
	(GtkClassInitFunc) gtk_button_class_init,
	(GtkObjectInitFunc) gtk_button_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      button_type = gtk_type_unique (GTK_TYPE_BIN, &button_info);
      gtk_type_set_chunk_alloc (button_type, 16);
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

  parent_class = gtk_type_class (GTK_TYPE_BIN);

  gtk_object_add_arg_type ("GtkButton::label", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL);
  gtk_object_add_arg_type ("GtkButton::relief", GTK_TYPE_RELIEF_STYLE, GTK_ARG_READWRITE, ARG_RELIEF);

  button_signals[PRESSED] =
    gtk_signal_new ("pressed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, pressed),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  button_signals[RELEASED] =
    gtk_signal_new ("released",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, released),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  button_signals[CLICKED] =
    gtk_signal_new ("clicked",
                    GTK_RUN_FIRST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, clicked),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  button_signals[ENTER] =
    gtk_signal_new ("enter",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, enter),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  button_signals[LEAVE] =
    gtk_signal_new ("leave",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkButtonClass, leave),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, button_signals, LAST_SIGNAL);

  object_class->set_arg = gtk_button_set_arg;
  object_class->get_arg = gtk_button_get_arg;

  widget_class->activate_signal = button_signals[CLICKED];
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
  container_class->child_type = gtk_button_child_type;

  klass->pressed = gtk_real_button_pressed;
  klass->released = gtk_real_button_released;
  klass->clicked = NULL;
  klass->enter = gtk_real_button_enter;
  klass->leave = gtk_real_button_leave;
}

static void
gtk_button_init (GtkButton *button)
{
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_FOCUS | GTK_RECEIVES_DEFAULT);
  GTK_WIDGET_UNSET_FLAGS (button, GTK_NO_WINDOW);

  button->child = NULL;
  button->in_button = FALSE;
  button->button_down = FALSE;
  button->relief = GTK_RELIEF_NORMAL;
}

static GtkType
gtk_button_child_type  (GtkContainer     *container)
{
  if (!GTK_BIN (container)->child)
    return GTK_TYPE_WIDGET;
  else
    return GTK_TYPE_NONE;
}

static void
gtk_button_set_arg (GtkObject *object,
		    GtkArg    *arg,
		    guint      arg_id)
{
  GtkButton *button;

  button = GTK_BUTTON (object);

  switch (arg_id)
    {
      GtkWidget *child;

    case ARG_LABEL:
      child = GTK_BIN (button)->child;
      if (!child)
	child = gtk_widget_new (GTK_TYPE_LABEL,
				"visible", TRUE,
				"parent", button,
				NULL);
      if (GTK_IS_LABEL (child))
	gtk_label_set_text (GTK_LABEL (child),
		       GTK_VALUE_STRING (*arg) ? GTK_VALUE_STRING (*arg) : "");
      break;
    case ARG_RELIEF:
      gtk_button_set_relief (button, GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_button_get_arg (GtkObject *object,
		    GtkArg    *arg,
		    guint      arg_id)
{
  GtkButton *button;

  button = GTK_BUTTON (object);

  switch (arg_id)
    {
    case ARG_LABEL:
      if (GTK_BIN (button)->child && GTK_IS_LABEL (GTK_BIN (button)->child))
	GTK_VALUE_STRING (*arg) = g_strdup (GTK_LABEL (GTK_BIN (button)->child)->label);
      else
	GTK_VALUE_STRING (*arg) = NULL;
      break;
    case ARG_RELIEF:
      GTK_VALUE_ENUM (*arg) = gtk_button_get_relief (button);
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

GtkWidget*
gtk_button_new (void)
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
  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[PRESSED]);
}

void
gtk_button_released (GtkButton *button)
{
  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[RELEASED]);
}

void
gtk_button_clicked (GtkButton *button)
{
  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[CLICKED]);
}

void
gtk_button_enter (GtkButton *button)
{
  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[ENTER]);
}

void
gtk_button_leave (GtkButton *button)
{
  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[LEAVE]);
}

void
gtk_button_set_relief (GtkButton *button,
		       GtkReliefStyle newrelief)
{
  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_BUTTON (button));

  button->relief = newrelief;
  gtk_widget_queue_draw (GTK_WIDGET (button));
}

GtkReliefStyle
gtk_button_get_relief (GtkButton *button)
{
  g_return_val_if_fail (button != NULL, GTK_RELIEF_NORMAL);
  g_return_val_if_fail (GTK_IS_BUTTON (button), GTK_RELIEF_NORMAL);

  return button->relief;
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
gtk_button_get_props (GtkButton *button,
		      gint      *default_spacing)
{
  GtkWidget *widget =  GTK_WIDGET (button);
  
  if (default_spacing)
    *default_spacing = gtk_style_get_prop_experimental (widget->style,
							"GtkButton::default_spacing",
							DEFAULT_SPACING);
}
	
static void
gtk_button_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkButton *button = GTK_BUTTON (widget);
  gint default_spacing;

  gtk_button_get_props (button, &default_spacing);
  
  requisition->width = (GTK_CONTAINER (widget)->border_width + CHILD_SPACING +
			GTK_WIDGET (widget)->style->klass->xthickness) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width + CHILD_SPACING +
			 GTK_WIDGET (widget)->style->klass->ythickness) * 2;

  if (GTK_WIDGET_CAN_DEFAULT (widget))
    {
      requisition->width += (GTK_WIDGET (widget)->style->klass->xthickness * 2 +
			     default_spacing);
      requisition->height += (GTK_WIDGET (widget)->style->klass->ythickness * 2 +
			      default_spacing);
    }

  if (GTK_BIN (button)->child && GTK_WIDGET_VISIBLE (GTK_BIN (button)->child))
    {
      GtkRequisition child_requisition;

      gtk_widget_size_request (GTK_BIN (button)->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
gtk_button_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkAllocation child_allocation;

  gint border_width = GTK_CONTAINER (widget)->border_width;
  gint xthickness = GTK_WIDGET (widget)->style->klass->xthickness;
  gint ythickness = GTK_WIDGET (widget)->style->klass->ythickness;
  gint default_spacing;

  gtk_button_get_props (button, &default_spacing);
  
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    widget->allocation.x + border_width,
			    widget->allocation.y + border_width,
			    widget->allocation.width - border_width * 2,
			    widget->allocation.height - border_width * 2);

  if (GTK_BIN (button)->child && GTK_WIDGET_VISIBLE (GTK_BIN (button)->child))
    {
      child_allocation.x = (CHILD_SPACING + xthickness);
      child_allocation.y = (CHILD_SPACING + ythickness);

      child_allocation.width = MAX (1, (gint)widget->allocation.width - child_allocation.x * 2 -
	                         border_width * 2);
      child_allocation.height = MAX (1, (gint)widget->allocation.height - child_allocation.y * 2 -
	                          border_width * 2);

      if (GTK_WIDGET_CAN_DEFAULT (button))
	{
	  child_allocation.x += (GTK_WIDGET (widget)->style->klass->xthickness +
				(1 + default_spacing) / 2);
	  child_allocation.y += (GTK_WIDGET (widget)->style->klass->ythickness +
				 (1 + default_spacing) / 2);
	  child_allocation.width =  MAX (1, (gint)child_allocation.width -
					 (gint)(GTK_WIDGET (widget)->style->klass->xthickness * 2 + default_spacing));
	  child_allocation.height = MAX (1, (gint)child_allocation.height -
					 (gint)(GTK_WIDGET (widget)->style->klass->xthickness * 2 + default_spacing));
	}

      gtk_widget_size_allocate (GTK_BIN (button)->child, &child_allocation);
    }
}

/*
 * +------------------------------------------------+
 * |                   BORDER                       |
 * |  +------------------------------------------+  |
 * |  |\\\\\\\\\\\\\\\\DEFAULT\\\\\\\\\\\\\\\\\  |  |
 * |  |\\+------------------------------------+  |  |
 * |  |\\| |           SPACING       3      | |  |  |
 * |  |\\| +--------------------------------+ |  |  |
 * |  |\\| |########## FOCUS ###############| |  |  |
 * |  |\\| |#+----------------------------+#| |  |  |
 * |  |\\| |#|         RELIEF            \|#| |  |  |
 * |  |\\| |#|  +-----------------------+\|#| |  |  |
 * |  |\\|1|#|  +     THE TEXT          +\|#|2|  |  |
 * |  |\\| |#|  +-----------------------+\|#| |  |  |
 * |  |\\| |#| \\\\\ ythickness \\\\\\\\\\|#| |  |  |
 * |  |\\| |#+----------------------------+#| |  |  |
 * |  |\\| |########### 1 ##################| |  |  |
 * |  |\\| +--------------------------------+ |  |  |
 * |  |\\| |        default spacing   4     | |  |  |
 * |  |\\+------------------------------------+  |  |
 * |  |\            ythickness                   |  |
 * |  +------------------------------------------+  |
 * |                border_width                    |
 * +------------------------------------------------+
 */

static void
gtk_button_paint (GtkWidget    *widget,
		  GdkRectangle *area)
{
  GtkButton *button;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;
  gint default_spacing;
   
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      button = GTK_BUTTON (widget);

      gtk_button_get_props (button, &default_spacing);
	
      x = 0;
      y = 0;
      width = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
      height = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;

      gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
      gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      if (GTK_WIDGET_HAS_DEFAULT (widget) &&
	  GTK_BUTTON (widget)->relief == GTK_RELIEF_NORMAL)
	{
	  gtk_paint_box (widget->style, widget->window,
			 GTK_STATE_NORMAL, GTK_SHADOW_IN,
			 area, widget, "buttondefault",
			 x, y, width, height);
	}

      if (GTK_WIDGET_CAN_DEFAULT (widget))
	{
	  x += widget->style->klass->xthickness;
	  y += widget->style->klass->ythickness;
	  width -= 2 * x + default_spacing;
	  height -= 2 * y + default_spacing;
	  x += (1 + default_spacing) / 2;
	  y += (1 + default_spacing) / 2;
	}
       
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x += 1;
	  y += 1;
	  width -= 2;
	  height -= 2;
	}
	
      if (GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE)
	shadow_type = GTK_SHADOW_IN;
      else
	shadow_type = GTK_SHADOW_OUT;

      if ((button->relief != GTK_RELIEF_NONE) ||
	  ((GTK_WIDGET_STATE(widget) != GTK_STATE_NORMAL) &&
	   (GTK_WIDGET_STATE(widget) != GTK_STATE_INSENSITIVE)))
	gtk_paint_box (widget->style, widget->window,
		       GTK_WIDGET_STATE (widget),
		       shadow_type, area, widget, "button",
		       x, y, width, height);
       
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x -= 1;
	  y -= 1;
	  width += 2;
	  height += 2;
	     
	  gtk_paint_focus (widget->style, widget->window,
			   area, widget, "button",
			   x, y, width - 1, height - 1);
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

      if (GTK_BIN (button)->child && gtk_widget_intersect (GTK_BIN (button)->child, &tmp_area, &child_area))
	gtk_widget_draw (GTK_BIN (button)->child, &child_area);
    }
}

static void
gtk_button_draw_focus (GtkWidget *widget)
{
  gtk_widget_draw (widget, NULL);
}

static void
gtk_button_draw_default (GtkWidget *widget)
{
  gtk_widget_draw (widget, NULL);
}

static gint
gtk_button_expose (GtkWidget      *widget,
		   GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      
      gtk_button_paint (widget, &event->area);

      child_event = *event;
      if (bin->child && GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
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
  g_return_if_fail (container != NULL);
  g_return_if_fail (widget != NULL);

  if (GTK_CONTAINER_CLASS (parent_class)->add)
    GTK_CONTAINER_CLASS (parent_class)->add (container, widget);

  GTK_BUTTON (container)->child = GTK_BIN (container)->child;
}

static void
gtk_button_remove (GtkContainer *container,
		   GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (widget != NULL);

  if (GTK_CONTAINER_CLASS (parent_class)->remove)
    GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);

  GTK_BUTTON (container)->child = GTK_BIN (container)->child;
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
	  /* We _draw () instead of queue_draw so that if the operation
	   * blocks, the label doesn't vanish.
	   */
	  gtk_widget_draw (GTK_WIDGET (button), NULL);
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
