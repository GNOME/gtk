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
#include "gtklabel.h"
#include "gtkradiobutton.h"
#include "gtksignal.h"


#define CHECK_BUTTON_CLASS(w)  GTK_CHECK_BUTTON_CLASS (GTK_OBJECT (w)->klass)


static void gtk_radio_button_class_init     (GtkRadioButtonClass  *klass);
static void gtk_radio_button_init           (GtkRadioButton       *radio_button);
static void gtk_radio_button_destroy        (GtkObject            *object);
static void gtk_radio_button_clicked        (GtkButton            *button);
static void gtk_radio_button_draw_indicator (GtkCheckButton       *check_button,
					     GdkRectangle         *area);


static GtkCheckButtonClass *parent_class = NULL;


guint
gtk_radio_button_get_type ()
{
  static guint radio_button_type = 0;

  if (!radio_button_type)
    {
      GtkTypeInfo radio_button_info =
      {
	"GtkRadioButton",
	sizeof (GtkRadioButton),
	sizeof (GtkRadioButtonClass),
	(GtkClassInitFunc) gtk_radio_button_class_init,
	(GtkObjectInitFunc) gtk_radio_button_init,
	(GtkArgFunc) NULL,
      };

      radio_button_type = gtk_type_unique (gtk_check_button_get_type (), &radio_button_info);
    }

  return radio_button_type;
}

static void
gtk_radio_button_class_init (GtkRadioButtonClass *class)
{
  GtkObjectClass *object_class;
  GtkButtonClass *button_class;
  GtkCheckButtonClass *check_button_class;

  object_class = (GtkObjectClass*) class;
  button_class = (GtkButtonClass*) class;
  check_button_class = (GtkCheckButtonClass*) class;

  parent_class = gtk_type_class (gtk_check_button_get_type ());

  object_class->destroy = gtk_radio_button_destroy;

  button_class->clicked = gtk_radio_button_clicked;

  check_button_class->draw_indicator = gtk_radio_button_draw_indicator;
}

static void
gtk_radio_button_init (GtkRadioButton *radio_button)
{
  radio_button->group = NULL;
}

GtkWidget*
gtk_radio_button_new (GSList *group)
{
  GtkRadioButton *radio_button;
  GtkRadioButton *tmp_button;
  GSList *tmp_list;

  radio_button = gtk_type_new (gtk_radio_button_get_type ());

  tmp_list = group;
  radio_button->group = g_slist_prepend (group, radio_button);

  if (tmp_list)
    {
      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

	  tmp_button->group = radio_button->group;
	}
    }
  else
    {
      GTK_TOGGLE_BUTTON (radio_button)->active = TRUE;
      gtk_widget_set_state (GTK_WIDGET (radio_button), GTK_STATE_ACTIVE);
    }

  return GTK_WIDGET (radio_button);
}

GtkWidget*
gtk_radio_button_new_with_label (GSList      *group,
				 const gchar *label)
{
  GtkWidget *radio_button;
  GtkWidget *label_widget;

  radio_button = gtk_radio_button_new (group);
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (radio_button), label_widget);
  gtk_widget_show (label_widget);

  return radio_button;
}

GtkWidget*
gtk_radio_button_new_from_widget (GtkRadioButton *group)
{
  GSList *l = NULL;
  if (group)
    l = gtk_radio_button_group (group);
  return gtk_radio_button_new (l);
}


GtkWidget*
gtk_radio_button_new_with_label_from_widget (GtkRadioButton *group,
					     const gchar    *label)
{
  GSList *l = NULL;
  if (group)
    l = gtk_radio_button_group (group);
  return gtk_radio_button_new_with_label (l, label);
}

GSList*
gtk_radio_button_group (GtkRadioButton *radio_button)
{
  g_return_val_if_fail (radio_button != NULL, NULL);
  g_return_val_if_fail (GTK_IS_RADIO_BUTTON (radio_button), NULL);

  return radio_button->group;
}


static void
gtk_radio_button_destroy (GtkObject *object)
{
  GtkRadioButton *radio_button;
  GtkRadioButton *tmp_button;
  GSList *tmp_list;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_RADIO_BUTTON (object));

  radio_button = GTK_RADIO_BUTTON (object);

  radio_button->group = g_slist_remove (radio_button->group, radio_button);
  tmp_list = radio_button->group;

  while (tmp_list)
    {
      tmp_button = tmp_list->data;
      tmp_list = tmp_list->next;

      tmp_button->group = radio_button->group;
    }

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_radio_button_clicked (GtkButton *button)
{
  GtkToggleButton *toggle_button;
  GtkRadioButton *radio_button;
  GtkToggleButton *tmp_button;
  GtkStateType new_state;
  GSList *tmp_list;
  gint toggled;

  g_return_if_fail (button != NULL);
  g_return_if_fail (GTK_IS_RADIO_BUTTON (button));

  radio_button = GTK_RADIO_BUTTON (button);
  toggle_button = GTK_TOGGLE_BUTTON (button);
  toggled = FALSE;

  if (toggle_button->active)
    {
      tmp_button = NULL;
      tmp_list = radio_button->group;

      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_button->active && (tmp_button != toggle_button))
	    break;

	  tmp_button = NULL;
	}

      if (!tmp_button)
	{
	  new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
	}
      else
	{
	  toggled = TRUE;
	  toggle_button->active = !toggle_button->active;
	  new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL);
	}
    }
  else
    {
      toggled = TRUE;
      toggle_button->active = !toggle_button->active;

      tmp_list = radio_button->group;
      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_button->active && (tmp_button != toggle_button))
	    {
	      gtk_button_clicked (GTK_BUTTON (tmp_button));
	      break;
	    }
	}

      new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
    }

  if (GTK_WIDGET_STATE (button) != new_state)
    gtk_widget_set_state (GTK_WIDGET (button), new_state);
  if (toggled)
    gtk_toggle_button_toggled (toggle_button);
  gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
gtk_radio_button_draw_indicator (GtkCheckButton *check_button,
				 GdkRectangle   *area)
{
  GtkWidget *widget;
  GtkButton *button;
  GtkToggleButton *toggle_button;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GdkPoint pts[4];
  gint width, height;
  gint x, y;

  g_return_if_fail (check_button != NULL);
  g_return_if_fail (GTK_IS_RADIO_BUTTON (check_button));

  if (GTK_WIDGET_VISIBLE (check_button) && GTK_WIDGET_MAPPED (check_button))
    {
      widget = GTK_WIDGET (check_button);
      button = GTK_BUTTON (check_button);
      toggle_button = GTK_TOGGLE_BUTTON (check_button);

      state_type = GTK_WIDGET_STATE (widget);
      if ((state_type != GTK_STATE_NORMAL) &&
	  (state_type != GTK_STATE_PRELIGHT))
	state_type = GTK_STATE_NORMAL;

      gtk_style_set_background (widget->style, widget->window, state_type);
      gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      x = CHECK_BUTTON_CLASS (widget)->indicator_spacing + GTK_CONTAINER (widget)->border_width;
      y = (widget->allocation.height - CHECK_BUTTON_CLASS (widget)->indicator_size) / 2;
      width = CHECK_BUTTON_CLASS (widget)->indicator_size;
      height = CHECK_BUTTON_CLASS (widget)->indicator_size;

      if (GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE)
	shadow_type = GTK_SHADOW_IN;
      else if ((GTK_WIDGET_STATE (widget) == GTK_STATE_PRELIGHT) && toggle_button->active)
	shadow_type = GTK_SHADOW_IN;
      else
	shadow_type = GTK_SHADOW_OUT;

      pts[0].x = x + width / 2;
      pts[0].y = y;
      pts[1].x = x + width;
      pts[1].y = y + height / 2;
      pts[2].x = pts[0].x;
      pts[2].y = y + height;
      pts[3].x = x;
      pts[3].y = pts[1].y;

      gdk_draw_polygon (widget->window,
			widget->style->bg_gc[GTK_WIDGET_STATE (widget)],
			TRUE, pts, 4);
      gtk_draw_diamond (widget->style, widget->window,
			GTK_WIDGET_STATE (widget), shadow_type,
			x, y, width, height);
    }
}
