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
#include "gtkcheckbutton.h"
#include "gtklabel.h"


#define INDICATOR_SIZE     10
#define INDICATOR_SPACING  2

#define CHECK_BUTTON_CLASS(w)  GTK_CHECK_BUTTON_CLASS (GTK_OBJECT (w)->klass)


static void gtk_check_button_class_init          (GtkCheckButtonClass *klass);
static void gtk_check_button_init                (GtkCheckButton      *check_button);
static void gtk_check_button_draw                (GtkWidget           *widget,
						  GdkRectangle        *area);
static void gtk_check_button_draw_focus          (GtkWidget           *widget);
static void gtk_check_button_size_request        (GtkWidget           *widget,
						  GtkRequisition      *requisition);
static void gtk_check_button_size_allocate       (GtkWidget           *widget,
						  GtkAllocation       *allocation);
static gint gtk_check_button_expose              (GtkWidget           *widget,
						  GdkEventExpose      *event);
static void gtk_check_button_draw_indicator      (GtkCheckButton      *check_button,
						  GdkRectangle        *area);
static void gtk_real_check_button_draw_indicator (GtkCheckButton      *check_button,
						  GdkRectangle        *area);


static GtkToggleButtonClass *parent_class = NULL;


guint
gtk_check_button_get_type ()
{
  static guint check_button_type = 0;

  if (!check_button_type)
    {
      GtkTypeInfo check_button_info =
      {
	"GtkCheckButton",
	sizeof (GtkCheckButton),
	sizeof (GtkCheckButtonClass),
	(GtkClassInitFunc) gtk_check_button_class_init,
	(GtkObjectInitFunc) gtk_check_button_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      check_button_type = gtk_type_unique (gtk_toggle_button_get_type (), &check_button_info);
    }

  return check_button_type;
}

static void
gtk_check_button_class_init (GtkCheckButtonClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (gtk_toggle_button_get_type ());

  widget_class->draw = gtk_check_button_draw;
  widget_class->draw_focus = gtk_check_button_draw_focus;
  widget_class->size_request = gtk_check_button_size_request;
  widget_class->size_allocate = gtk_check_button_size_allocate;
  widget_class->expose_event = gtk_check_button_expose;

  class->indicator_size = INDICATOR_SIZE;
  class->indicator_spacing = INDICATOR_SPACING;
  class->draw_indicator = gtk_real_check_button_draw_indicator;
}

static void
gtk_check_button_init (GtkCheckButton *check_button)
{
  check_button->toggle_button.draw_indicator = TRUE;
}

GtkWidget*
gtk_check_button_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_check_button_get_type ()));
}


GtkWidget*
gtk_check_button_new_with_label (const gchar *label)
{
  GtkWidget *check_button;
  GtkWidget *label_widget;

  check_button = gtk_check_button_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (check_button), label_widget);
  gtk_widget_show (label_widget);

  return check_button;
}

static void
gtk_check_button_draw (GtkWidget    *widget,
		       GdkRectangle *area)
{
  GtkButton *button;
  GtkCheckButton *check_button;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CHECK_BUTTON (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      check_button = GTK_CHECK_BUTTON (widget);

      if (check_button->toggle_button.draw_indicator)
	{
	  button = GTK_BUTTON (widget);

	  gtk_check_button_draw_indicator (check_button, area);

	  if (button->child && GTK_WIDGET_NO_WINDOW (button->child) &&
	      gtk_widget_intersect (button->child, area, &child_area))
	    gtk_widget_draw (button->child, &child_area);

	  gtk_widget_draw_focus (widget);
	}
      else
	{
	  if (GTK_WIDGET_CLASS (parent_class)->draw)
	    (* GTK_WIDGET_CLASS (parent_class)->draw) (widget, area);
	}
    }
}

static void
gtk_check_button_draw_focus (GtkWidget *widget)
{
  GtkCheckButton *check_button;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CHECK_BUTTON (widget));
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      check_button = GTK_CHECK_BUTTON (widget);
      if (check_button->toggle_button.draw_indicator)
	{
	  gint border_width;
	  
	  border_width = GTK_CONTAINER (widget)->border_width;
	  if (GTK_WIDGET_HAS_FOCUS (widget))
	    gdk_draw_rectangle (widget->window,
				widget->style->black_gc, FALSE,
				border_width, border_width,
				widget->allocation.width - 2 * border_width - 1,
				widget->allocation.height - 2 * border_width - 1);
	  else
	    gdk_draw_rectangle (widget->window,
				widget->style->bg_gc[GTK_STATE_NORMAL], FALSE,
				border_width, border_width,
				widget->allocation.width - 2 * border_width - 1,
				widget->allocation.height - 2 * border_width - 1);
	}
      else
	{
	  if (GTK_WIDGET_CLASS (parent_class)->draw_focus)
	    (* GTK_WIDGET_CLASS (parent_class)->draw_focus) (widget);
	}
    }
}

static void
gtk_check_button_size_request (GtkWidget      *widget,
			       GtkRequisition *requisition)
{
  GtkCheckButton *check_button;
  GtkButton *button;
  gint temp;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CHECK_BUTTON (widget));
  g_return_if_fail (requisition != NULL);

  check_button = GTK_CHECK_BUTTON (widget);

  if (GTK_WIDGET_CLASS (parent_class)->size_request)
    (* GTK_WIDGET_CLASS (parent_class)->size_request) (widget, requisition);

  if (check_button->toggle_button.draw_indicator)
    {
      button = GTK_BUTTON (widget);

      requisition->width += (CHECK_BUTTON_CLASS (widget)->indicator_size +
			     CHECK_BUTTON_CLASS (widget)->indicator_spacing * 3 + 2);

      temp = (CHECK_BUTTON_CLASS (widget)->indicator_size +
	      CHECK_BUTTON_CLASS (widget)->indicator_spacing * 2);
      requisition->height = MAX (requisition->height, temp) + 2;
    }
}

static void
gtk_check_button_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  GtkCheckButton *check_button;
  GtkButton *button;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CHECK_BUTTON (widget));
  g_return_if_fail (allocation != NULL);

  check_button = GTK_CHECK_BUTTON (widget);
  if (check_button->toggle_button.draw_indicator)
    {
      widget->allocation = *allocation;
      if (GTK_WIDGET_REALIZED (widget))
	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);

      button = GTK_BUTTON (widget);

      if (button->child && GTK_WIDGET_VISIBLE (button->child))
	{
	  child_allocation.x = (GTK_CONTAINER (widget)->border_width +
				CHECK_BUTTON_CLASS (widget)->indicator_size +
				CHECK_BUTTON_CLASS (widget)->indicator_spacing * 3 + 1);
	  child_allocation.y = GTK_CONTAINER (widget)->border_width + 1;
	  child_allocation.width = MAX (1, allocation->width - child_allocation.x  -
				    GTK_CONTAINER (widget)->border_width - 1);
	  child_allocation.height = MAX (1, allocation->height - child_allocation.y * 2);

	  gtk_widget_size_allocate (button->child, &child_allocation);
	}
    }
  else
    {
      if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
	(* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);
    }
}

static gint
gtk_check_button_expose (GtkWidget      *widget,
			 GdkEventExpose *event)
{
  GtkButton *button;
  GtkCheckButton *check_button;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      check_button = GTK_CHECK_BUTTON (widget);

      if (check_button->toggle_button.draw_indicator)
	{
	  button = GTK_BUTTON (widget);

	  gtk_check_button_draw_indicator (check_button, &event->area);

	  child_event = *event;
	  if (button->child && GTK_WIDGET_NO_WINDOW (button->child) &&
	      gtk_widget_intersect (button->child, &event->area, &child_event.area))
	    gtk_widget_event (button->child, (GdkEvent*) &child_event);

	  gtk_widget_draw_focus (widget);
	}
      else
	{
	  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
	    (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
	}
    }

  return FALSE;
}


static void
gtk_check_button_draw_indicator (GtkCheckButton *check_button,
				 GdkRectangle   *area)
{
  GtkCheckButtonClass *class;

  g_return_if_fail (check_button != NULL);
  g_return_if_fail (GTK_IS_CHECK_BUTTON (check_button));
  g_return_if_fail (CHECK_BUTTON_CLASS (check_button) != NULL);

  class = CHECK_BUTTON_CLASS (check_button);

  if (class->draw_indicator)
    (* class->draw_indicator) (check_button, area);
}

static void
gtk_real_check_button_draw_indicator (GtkCheckButton *check_button,
				      GdkRectangle    *area)
{
  GtkWidget *widget;
  GtkToggleButton *toggle_button;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GdkRectangle restrict_area;
  GdkRectangle new_area;
  gint width, height;
  gint x, y;

  g_return_if_fail (check_button != NULL);
  g_return_if_fail (GTK_IS_CHECK_BUTTON (check_button));

  if (GTK_WIDGET_DRAWABLE (check_button))
    {
      widget = GTK_WIDGET (check_button);
      toggle_button = GTK_TOGGLE_BUTTON (check_button);

      state_type = GTK_WIDGET_STATE (widget);
      if ((state_type != GTK_STATE_NORMAL) &&
	  (state_type != GTK_STATE_PRELIGHT))
	state_type = GTK_STATE_NORMAL;

      restrict_area.x = GTK_CONTAINER (widget)->border_width;
      restrict_area.y = restrict_area.x;
      restrict_area.width = widget->allocation.width - restrict_area.x * 2;
      restrict_area.height = widget->allocation.height - restrict_area.x * 2;

      if (gdk_rectangle_intersect (area, &restrict_area, &new_area))
	{
	  gtk_style_set_background (widget->style, widget->window, state_type);
	  gdk_window_clear_area (widget->window, new_area.x, new_area.y,
				 new_area.width, new_area.height);
	}
      
      x = CHECK_BUTTON_CLASS (widget)->indicator_spacing + GTK_CONTAINER (widget)->border_width;
      y = (widget->allocation.height - CHECK_BUTTON_CLASS (widget)->indicator_size) / 2;
      width = CHECK_BUTTON_CLASS (widget)->indicator_size;
      height = CHECK_BUTTON_CLASS (widget)->indicator_size;

      if (GTK_TOGGLE_BUTTON (widget)->active)
	shadow_type = GTK_SHADOW_IN;
      else
	shadow_type = GTK_SHADOW_OUT;

      gdk_draw_rectangle (widget->window,
			  widget->style->bg_gc[GTK_WIDGET_STATE (widget)],
			  TRUE, x + 1, y + 1, width, height);
      gtk_draw_shadow (widget->style, widget->window,
		       GTK_WIDGET_STATE (widget), shadow_type,
		       x + 1, y + 1, width, height);
    }
}
