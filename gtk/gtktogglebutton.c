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
static void gtk_toggle_button_draw_focus (GtkWidget            *widget);
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
     

static guint toggle_button_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_toggle_button_get_type (void)
{
  static GtkType toggle_button_type = 0;

  if (!toggle_button_type)
    {
      GtkTypeInfo toggle_button_info =
      {
	"GtkToggleButton",
	sizeof (GtkToggleButton),
	sizeof (GtkToggleButtonClass),
	(GtkClassInitFunc) gtk_toggle_button_class_init,
	(GtkObjectInitFunc) gtk_toggle_button_init,
	(GtkArgSetFunc) gtk_toggle_button_set_arg,
        (GtkArgGetFunc) gtk_toggle_button_get_arg,
      };

      toggle_button_type = gtk_type_unique (gtk_button_get_type (), &toggle_button_info);
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

  gtk_object_add_arg_type ("GtkToggleButton::active", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ACTIVE);
  gtk_object_add_arg_type ("GtkToggleButton::draw_indicator", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_DRAW_INDICATOR);

  toggle_button_signals[TOGGLED] =
    gtk_signal_new ("toggled",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkToggleButtonClass, toggled),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, toggle_button_signals, LAST_SIGNAL);

  widget_class->draw_focus = gtk_toggle_button_draw_focus;

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
      gtk_toggle_button_set_state (tb, GTK_VALUE_BOOL (*arg));
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
			    gint             draw_indicator)
{
  g_return_if_fail (toggle_button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  draw_indicator = draw_indicator ? TRUE : FALSE;

  if (toggle_button->draw_indicator != draw_indicator)
    {
      toggle_button->draw_indicator = draw_indicator;

      if (GTK_WIDGET_VISIBLE (toggle_button))
	gtk_widget_queue_resize (GTK_WIDGET (toggle_button));
    }
}

void
gtk_toggle_button_set_state (GtkToggleButton *toggle_button,
			     gint             state)
{
  g_return_if_fail (toggle_button != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  if (toggle_button->active != (state != FALSE))
    gtk_button_clicked (GTK_BUTTON (toggle_button));
}

void
gtk_toggle_button_toggled (GtkToggleButton *toggle_button)
{
  gtk_signal_emit (GTK_OBJECT (toggle_button), toggle_button_signals[TOGGLED]);
}


static void
gtk_toggle_button_draw_focus (GtkWidget *widget)
{
  GtkButton *button;
  GtkToggleButton *toggle_button;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      button = GTK_BUTTON (widget);
      toggle_button = GTK_TOGGLE_BUTTON (widget);

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
	  if (GTK_WIDGET_STATE (toggle_button) == GTK_STATE_ACTIVE)
	    gdk_draw_rectangle (widget->window,
				widget->style->bg_gc[GTK_WIDGET_STATE (widget)], FALSE,
				x + 1, y + 1, width - 4, height - 4);
	  else
	    gdk_draw_rectangle (widget->window,
				widget->style->bg_gc[GTK_WIDGET_STATE (widget)], FALSE,
				x + 2, y + 2, width - 5, height - 5);
	}

      if (toggle_button->active)
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
    {
      gtk_widget_set_state (GTK_WIDGET (button), new_state);
      gtk_widget_queue_draw (GTK_WIDGET (button));
    }
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
	    {
	      gtk_widget_set_state (GTK_WIDGET (button), new_state);
	      gtk_widget_queue_draw (GTK_WIDGET (button));
	    }
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
    {
      gtk_widget_set_state (GTK_WIDGET (button), new_state);
      gtk_widget_queue_draw (GTK_WIDGET (button));
    }
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
    {
      gtk_widget_set_state (GTK_WIDGET (button), new_state);
      gtk_widget_queue_draw (GTK_WIDGET (button));
    }
}
