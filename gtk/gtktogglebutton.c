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
#include "gtkthemes.h"
#include "gtkprivate.h"
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


static void gtk_toggle_button_class_init (GtkToggleButtonClass *klass);
static void gtk_toggle_button_init       (GtkToggleButton      *toggle_button);
static void gtk_toggle_button_draw_focus (GtkWidget            *widget);
static void gtk_toggle_button_pressed    (GtkButton            *button);
static void gtk_toggle_button_released   (GtkButton            *button);
static void gtk_toggle_button_clicked    (GtkButton            *button);
static void gtk_toggle_button_enter      (GtkButton            *button);
static void gtk_toggle_button_leave      (GtkButton            *button);


static guint toggle_button_signals[LAST_SIGNAL] = { 0 };


guint
gtk_toggle_button_get_type ()
{
  static guint toggle_button_type = 0;

  if (!toggle_button_type)
    {
      GtkTypeInfo toggle_button_info =
      {
	"GtkToggleButton",
	sizeof (GtkToggleButton),
	sizeof (GtkToggleButtonClass),
	(GtkClassInitFunc) gtk_toggle_button_class_init,
	(GtkObjectInitFunc) gtk_toggle_button_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
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
gtk_toggle_button_new ()
{
   GtkToggleButton *toggle_button;
   GtkButton *button;
   
   toggle_button=GTK_TOGGLE_BUTTON(gtk_type_new(gtk_toggle_button_get_type()));
   if (toggle_button)
     {
	button=GTK_BUTTON(toggle_button);
	button->init=th_dat.functions.button.init;
	button->border=th_dat.functions.button.border;
	button->draw=th_dat.functions.button.draw;
	button->exit=th_dat.functions.button.exit;
     }
   return GTK_WIDGET(toggle_button);
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
   gtk_widget_queue_draw(widget);
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
