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

#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtktogglebutton.h"


#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_ACTIVE,
  ARG_DRAW_INDICATOR
};


static void gtk_toggle_button_class_init (GtkToggleButtonClass *klass);
static void gtk_toggle_button_init       (GtkToggleButton      *toggle_button);
static void gtk_toggle_button_paint      (GtkWidget            *widget,
					  GdkRectangle         *area);
static void gtk_toggle_button_size_allocate (GtkWidget         *widget,
					     GtkAllocation     *allocation);
static void gtk_toggle_button_draw       (GtkWidget            *widget,
					  GdkRectangle         *area);
static gint gtk_toggle_button_expose     (GtkWidget            *widget,
					  GdkEventExpose       *event);
static void gtk_toggle_button_pressed    (GtkButton            *button);
static void gtk_toggle_button_released   (GtkButton            *button);
static void gtk_toggle_button_clicked    (GtkButton            *button);
static void gtk_toggle_button_enter      (GtkButton            *button);
static void gtk_toggle_button_leave      (GtkButton            *button);
static void gtk_toggle_button_set_arg	 (GtkObject	       *object,
					  GtkArg    	       *arg,
					  guint      		arg_id);
static void gtk_toggle_button_get_arg	 (GtkObject	       *object,
					  GtkArg    	       *arg,
					  guint      		arg_id);
static void gtk_toggle_button_leave      (GtkButton            *button);
static void gtk_toggle_button_realize    (GtkWidget            *widget);
static void gtk_toggle_button_unrealize  (GtkWidget            *widget);
static void gtk_toggle_button_map        (GtkWidget            *widget);
static void gtk_toggle_button_unmap      (GtkWidget            *widget);

static guint toggle_button_signals[LAST_SIGNAL] = { 0 };
static GtkContainerClass *parent_class = NULL;

GtkType
gtk_toggle_button_get_type (void)
{
  static GtkType toggle_button_type = 0;

  if (!toggle_button_type)
    {
      static const GtkTypeInfo toggle_button_info =
      {
	"GtkToggleButton",
	sizeof (GtkToggleButton),
	sizeof (GtkToggleButtonClass),
	(GtkClassInitFunc) gtk_toggle_button_class_init,
	(GtkObjectInitFunc) gtk_toggle_button_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      toggle_button_type = gtk_type_unique (GTK_TYPE_BUTTON, &toggle_button_info);
    }

  return toggle_button_type;
}

static void
gtk_toggle_button_class_init (GtkToggleButtonClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkButtonClass *button_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  button_class = (GtkButtonClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_BUTTON);

  gtk_object_add_arg_type ("GtkToggleButton::active", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ACTIVE);
  gtk_object_add_arg_type ("GtkToggleButton::draw_indicator", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_DRAW_INDICATOR);

  toggle_button_signals[TOGGLED] =
    gtk_signal_new ("toggled",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkToggleButtonClass, toggled),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, toggle_button_signals, LAST_SIGNAL);

  object_class->set_arg = gtk_toggle_button_set_arg;
  object_class->get_arg = gtk_toggle_button_get_arg;

  widget_class->size_allocate = gtk_toggle_button_size_allocate;
  widget_class->draw = gtk_toggle_button_draw;
  widget_class->expose_event = gtk_toggle_button_expose;
  widget_class->realize = gtk_toggle_button_realize;
  widget_class->unrealize = gtk_toggle_button_unrealize;
  widget_class->map = gtk_toggle_button_map;
  widget_class->unmap = gtk_toggle_button_unmap;

  button_class->pressed = gtk_toggle_button_pressed;
  button_class->released = gtk_toggle_button_released;
  button_class->clicked = gtk_toggle_button_clicked;
  button_class->enter = gtk_toggle_button_enter;
  button_class->leave = gtk_toggle_button_leave;

  class->toggled = NULL;
}

static void
gtk_toggle_button_init (GtkToggleButton *toggle_button)
{
  toggle_button->active = FALSE;
  toggle_button->draw_indicator = FALSE;
  GTK_WIDGET_UNSET_FLAGS (toggle_button, GTK_NO_WINDOW);
}


GtkWidget*
gtk_toggle_button_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_toggle_button_get_type ()));
}

GtkWidget*
gtk_toggle_button_new_with_label (const gchar *label)
{
  GtkWidget *toggle_button;
  GtkWidget *label_widget;

  toggle_button = gtk_toggle_button_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.5, 0.5);

  gtk_container_add (GTK_CONTAINER (toggle_button), label_widget);
  gtk_widget_show (label_widget);

  return toggle_button;
}

static void
gtk_toggle_button_set_arg (GtkObject *object,
			   GtkArg    *arg,
			   guint      arg_id)
{
  GtkToggleButton *tb;

  tb = GTK_TOGGLE_BUTTON (object);

  switch (arg_id)
    {
    case ARG_ACTIVE:
      gtk_toggle_button_set_active (tb, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_DRAW_INDICATOR:
      gtk_toggle_button_set_mode (tb, GTK_VALUE_BOOL (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_toggle_button_get_arg (GtkObject *object,
			   GtkArg    *arg,
			   guint      arg_id)
{
  GtkToggleButton *tb;

  tb = GTK_TOGGLE_BUTTON (object);

  switch (arg_id)
    {
    case ARG_ACTIVE:
      GTK_VALUE_BOOL (*arg) = tb->active;
      break;
    case ARG_DRAW_INDICATOR:
      GTK_VALUE_BOOL (*arg) = tb->draw_indicator;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

void
gtk_toggle_button_set_mode (GtkToggleButton *toggle_button,
			    gboolean         draw_indicator)
{
  GtkWidget *widget;

  g_return_if_fail (toggle_button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  widget = GTK_WIDGET (toggle_button);

  draw_indicator = draw_indicator ? TRUE : FALSE;

  if (toggle_button->draw_indicator != draw_indicator)
    {
      if (GTK_WIDGET_REALIZED (toggle_button))
	{
	  gboolean visible = GTK_WIDGET_VISIBLE (toggle_button);

	  if (visible)
	    gtk_widget_hide (widget);

	  gtk_widget_unrealize (widget);
	  toggle_button->draw_indicator = draw_indicator;

	  if (toggle_button->draw_indicator)
	    GTK_WIDGET_SET_FLAGS (toggle_button, GTK_NO_WINDOW);
	  else
	    GTK_WIDGET_UNSET_FLAGS (toggle_button, GTK_NO_WINDOW);
	  
	  gtk_widget_realize (widget);

	  if (visible)
	    gtk_widget_show (widget);
	}
      else
	{
	  toggle_button->draw_indicator = draw_indicator;

	  if (toggle_button->draw_indicator)
	    GTK_WIDGET_SET_FLAGS (toggle_button, GTK_NO_WINDOW);
	  else
	    GTK_WIDGET_UNSET_FLAGS (toggle_button, GTK_NO_WINDOW);
	}

      if (GTK_WIDGET_VISIBLE (toggle_button))
	gtk_widget_queue_resize (GTK_WIDGET (toggle_button));
    }
}


void
gtk_toggle_button_set_active (GtkToggleButton *toggle_button,
			      gboolean         is_active)
{
  g_return_if_fail (toggle_button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  is_active = is_active != 0;

  if (toggle_button->active != is_active)
    gtk_button_clicked (GTK_BUTTON (toggle_button));
}


gboolean
gtk_toggle_button_get_active (GtkToggleButton *toggle_button)
{
  g_return_val_if_fail (toggle_button != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return (toggle_button->active) ? TRUE : FALSE;
}


void
gtk_toggle_button_toggled (GtkToggleButton *toggle_button)
{
  g_return_if_fail (toggle_button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  gtk_signal_emit (GTK_OBJECT (toggle_button), toggle_button_signals[TOGGLED]);
}


static void
gtk_toggle_button_paint (GtkWidget    *widget,
			 GdkRectangle *area)
{
  GtkButton *button;
  GtkToggleButton *toggle_button;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;

  button = GTK_BUTTON (widget);
  toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
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
                         area, widget, "togglebuttondefault",
                         x, y, width, height);
        }

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

      if ((GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE) ||
	  toggle_button->active)
	shadow_type = GTK_SHADOW_IN;
      else
	shadow_type = GTK_SHADOW_OUT;
      
      if (button->relief != GTK_RELIEF_NONE ||
	  (GTK_WIDGET_STATE(widget) != GTK_STATE_NORMAL &&
	   GTK_WIDGET_STATE(widget) != GTK_STATE_INSENSITIVE))
	gtk_paint_box (widget->style, widget->window,
		       GTK_WIDGET_STATE (widget),
		       shadow_type, area, widget, "togglebutton",
		       x, y, width, height);
      
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x -= 1;
	  y -= 1;
	  width += 2;
	  height += 2;

	  gtk_paint_focus (widget->style, widget->window,
			   area, widget, "togglebutton",
			   x, y, width - 1, height - 1);
	}
    }
}

static void
gtk_toggle_button_size_allocate (GtkWidget     *widget,
				 GtkAllocation *allocation)
{
  if (!GTK_WIDGET_NO_WINDOW (widget) &&
      GTK_WIDGET_CLASS (parent_class)->size_allocate)
    GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
}

static gint
gtk_toggle_button_expose (GtkWidget      *widget,
			  GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      
      gtk_toggle_button_paint (widget, &event->area);
      
      child_event = *event;
      if (bin->child && GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return TRUE;
}

static void
gtk_toggle_button_draw (GtkWidget    *widget,
			GdkRectangle *area)
{
  GdkRectangle child_area;
  GdkRectangle tmp_area;
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));
  g_return_if_fail (area != NULL);

  bin = GTK_BIN (widget);

  if (GTK_WIDGET_DRAWABLE (widget) && !GTK_WIDGET_NO_WINDOW (widget))
    {
      tmp_area = *area;
      tmp_area.x -= GTK_CONTAINER (widget)->border_width;
      tmp_area.y -= GTK_CONTAINER (widget)->border_width;

      gtk_toggle_button_paint (widget, &tmp_area);

      if (bin->child && gtk_widget_intersect (bin->child, &tmp_area, &child_area))
	gtk_widget_draw (bin->child, &child_area);
    }
}

static void
gtk_toggle_button_pressed (GtkButton *button)
{
  GtkToggleButton *toggle_button;
  GtkStateType new_state;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  toggle_button = GTK_TOGGLE_BUTTON (button);

  button->button_down = TRUE;

  if (toggle_button->active)
    new_state = (button->in_button ? GTK_STATE_NORMAL : GTK_STATE_ACTIVE);
  else
    new_state = (button->in_button ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL);

  if (GTK_WIDGET_STATE (button) != new_state)
    gtk_widget_set_state (GTK_WIDGET (button), new_state);
}

static void
gtk_toggle_button_released (GtkButton *button)
{
  GtkToggleButton *toggle_button;
  GtkStateType new_state;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  if (button->button_down)
    {
      toggle_button = GTK_TOGGLE_BUTTON (button);

      button->button_down = FALSE;

      if (button->in_button)
	{
	  gtk_button_clicked (button);
	}
      else
	{
	  if (toggle_button->active)
	    new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
	  else
	    new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL);

	  if (GTK_WIDGET_STATE (button) != new_state)
	    gtk_widget_set_state (GTK_WIDGET (button), new_state);
	}
    }
}

static void
gtk_toggle_button_clicked (GtkButton *button)
{
  GtkToggleButton *toggle_button;
  GtkStateType new_state;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  toggle_button = GTK_TOGGLE_BUTTON (button);
  toggle_button->active = !toggle_button->active;

  gtk_toggle_button_toggled (toggle_button);

  if (toggle_button->active)
    new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
  else
    new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL);

  if (GTK_WIDGET_STATE (button) != new_state)
    gtk_widget_set_state (GTK_WIDGET (button), new_state);
  else
    gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
gtk_toggle_button_enter (GtkButton *button)
{
  GtkToggleButton *toggle_button;
  GtkStateType new_state;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  toggle_button = GTK_TOGGLE_BUTTON (button);

  if (toggle_button->active)
    new_state = (button->button_down ? GTK_STATE_NORMAL : GTK_STATE_PRELIGHT);
  else
    new_state = (button->button_down ? GTK_STATE_ACTIVE : GTK_STATE_PRELIGHT);

  if (GTK_WIDGET_STATE (button) != new_state)
    gtk_widget_set_state (GTK_WIDGET (button), new_state);
}

static void
gtk_toggle_button_leave (GtkButton *button)
{
  GtkToggleButton *toggle_button;
  GtkStateType new_state;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  toggle_button = GTK_TOGGLE_BUTTON (button);

  new_state = (toggle_button->active ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL);

  if (GTK_WIDGET_STATE (button) != new_state)
    gtk_widget_set_state (GTK_WIDGET (button), new_state);
}

static void
gtk_toggle_button_realize (GtkWidget *widget)
{
  GtkToggleButton *toggle_button;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));
  
  toggle_button = GTK_TOGGLE_BUTTON (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  
  border_width = GTK_CONTAINER (widget)->border_width;
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      attributes.wclass = GDK_INPUT_ONLY;
      attributes_mask = GDK_WA_X | GDK_WA_Y;

      widget->window = gtk_widget_get_parent_window (widget);
      gdk_window_ref (widget->window);
      
      toggle_button->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
						    &attributes, attributes_mask);
      gdk_window_set_user_data (toggle_button->event_window, toggle_button);
    }
  else
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				       &attributes, attributes_mask);
      gdk_window_set_user_data (widget->window, toggle_button);
    }

  widget->style = gtk_style_attach (widget->style, widget->window);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}
  
static void
gtk_toggle_button_unrealize (GtkWidget *widget)
{
  GtkToggleButton *toggle_button;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

  toggle_button = GTK_TOGGLE_BUTTON (widget);
  
  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      gdk_window_set_user_data (toggle_button->event_window, NULL);
      gdk_window_destroy (toggle_button->event_window);
      toggle_button->event_window = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_toggle_button_map (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

  if (GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_show (GTK_TOGGLE_BUTTON (widget)->event_window);

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
gtk_toggle_button_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

  if (GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_hide (GTK_TOGGLE_BUTTON (widget)->event_window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}
