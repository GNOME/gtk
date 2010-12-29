/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkcheckbutton.h"

#include "gtkbuttonprivate.h"
#include "gtklabel.h"

#include "gtkprivate.h"
#include "gtkintl.h"


#define INDICATOR_SIZE     13
#define INDICATOR_SPACING  2


static void gtk_check_button_get_preferred_width  (GtkWidget          *widget,
                                                   gint               *minimum,
                                                   gint               *natural);
static void gtk_check_button_get_preferred_height (GtkWidget          *widget,
                                                   gint               *minimum,
                                                   gint               *natural);
static void gtk_check_button_size_allocate       (GtkWidget           *widget,
						  GtkAllocation       *allocation);
static gboolean gtk_check_button_draw            (GtkWidget           *widget,
						  cairo_t             *cr);
static void gtk_check_button_paint               (GtkWidget           *widget,
						  cairo_t             *cr);
static void gtk_check_button_draw_indicator      (GtkCheckButton      *check_button,
						  cairo_t             *cr);
static void gtk_real_check_button_draw_indicator (GtkCheckButton      *check_button,
						  cairo_t             *cr);

G_DEFINE_TYPE (GtkCheckButton, gtk_check_button, GTK_TYPE_TOGGLE_BUTTON)

static void
gtk_check_button_class_init (GtkCheckButtonClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->get_preferred_width = gtk_check_button_get_preferred_width;
  widget_class->get_preferred_height = gtk_check_button_get_preferred_height;
  widget_class->size_allocate = gtk_check_button_size_allocate;
  widget_class->draw = gtk_check_button_draw;

  class->draw_indicator = gtk_real_check_button_draw_indicator;

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("indicator-size",
							     P_("Indicator Size"),
							     P_("Size of check or radio indicator"),
							     0,
							     G_MAXINT,
							     INDICATOR_SIZE,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("indicator-spacing",
							     P_("Indicator Spacing"),
							     P_("Spacing around check or radio indicator"),
							     0,
							     G_MAXINT,
							     INDICATOR_SPACING,
							     GTK_PARAM_READABLE));
}

static void
gtk_check_button_init (GtkCheckButton *check_button)
{
  gtk_widget_set_has_window (GTK_WIDGET (check_button), FALSE);
  gtk_widget_set_receives_default (GTK_WIDGET (check_button), FALSE);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check_button), TRUE);
  gtk_button_set_alignment (GTK_BUTTON (check_button), 0.0, 0.5);
}

GtkWidget*
gtk_check_button_new (void)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, NULL);
}


GtkWidget*
gtk_check_button_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, "label", label, NULL);
}

/**
 * gtk_check_button_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkCheckButton
 *
 * Creates a new #GtkCheckButton containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the check button.
 **/
GtkWidget*
gtk_check_button_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, 
                       "label", label, 
                       "use-underline", TRUE, 
                       NULL);
}


/* This should only be called when toggle_button->draw_indicator
 * is true.
 */
static void
gtk_check_button_paint (GtkWidget    *widget,
			cairo_t      *cr)
{
  GtkCheckButton *check_button = GTK_CHECK_BUTTON (widget);
  gint border_width;
  gint interior_focus;
  gint focus_width;
  gint focus_pad;
      
  gtk_widget_style_get (widget,
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        NULL);

  gtk_check_button_draw_indicator (check_button, cr);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  if (gtk_widget_has_focus (widget))
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
      GtkStyleContext *context;
      GtkStateFlags state;
      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);

      gtk_style_context_set_state (context, state);

      if (interior_focus && child && gtk_widget_get_visible (child))
        {
          GtkAllocation child_allocation;

          gtk_widget_get_allocation (child, &child_allocation);
          gtk_render_focus (context, cr,
                            child_allocation.x - allocation.x - focus_width - focus_pad,
                            child_allocation.y - allocation.y - focus_width - focus_pad,
                            child_allocation.width + 2 * (focus_width + focus_pad),
                            child_allocation.height + 2 * (focus_width + focus_pad));
        }
      else
        gtk_render_focus (context, cr,
                          border_width, border_width,
                          allocation.width - 2 * border_width,
                          allocation.height - 2 * border_width);
    }
}

void
_gtk_check_button_get_props (GtkCheckButton *check_button,
			     gint           *indicator_size,
			     gint           *indicator_spacing)
{
  GtkWidget *widget =  GTK_WIDGET (check_button);

  if (indicator_size)
    gtk_widget_style_get (widget, "indicator-size", indicator_size, NULL);

  if (indicator_spacing)
    gtk_widget_style_get (widget, "indicator-spacing", indicator_spacing, NULL);
}

static void
gtk_check_button_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (widget);
  
  if (gtk_toggle_button_get_mode (toggle_button))
    {
      GtkWidget *child;
      gint indicator_size;
      gint indicator_spacing;
      gint focus_width;
      gint focus_pad;
      guint border_width;

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

      gtk_widget_style_get (GTK_WIDGET (widget),
                            "focus-line-width", &focus_width,
                            "focus-padding", &focus_pad,
                            NULL);
      *minimum = 2 * border_width;
      *natural = 2 * border_width;

      _gtk_check_button_get_props (GTK_CHECK_BUTTON (widget),
                                   &indicator_size, &indicator_spacing);

      child = gtk_bin_get_child (GTK_BIN (widget));
      if (child && gtk_widget_get_visible (child))
        {
          gint child_min, child_nat;

          gtk_widget_get_preferred_width (child, &child_min, &child_nat);

          *minimum += child_min + indicator_spacing;
          *natural += child_nat + indicator_spacing;
        }

      *minimum += (indicator_size + indicator_spacing * 2 + 2 * (focus_width + focus_pad));
      *natural += (indicator_size + indicator_spacing * 2 + 2 * (focus_width + focus_pad));
    }
  else
    GTK_WIDGET_CLASS (gtk_check_button_parent_class)->get_preferred_width (widget, minimum, natural);
}

static void
gtk_check_button_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (gtk_toggle_button_get_mode (toggle_button))
    {
      GtkWidget *child;
      gint temp;
      gint indicator_size;
      gint indicator_spacing;
      gint focus_width;
      gint focus_pad;
      guint border_width;

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

      gtk_widget_style_get (GTK_WIDGET (widget),
                            "focus-line-width", &focus_width,
                            "focus-padding", &focus_pad,
                            NULL);

      *minimum = border_width * 2;
      *natural = border_width * 2;

      _gtk_check_button_get_props (GTK_CHECK_BUTTON (widget),
                                   &indicator_size, &indicator_spacing);

      child = gtk_bin_get_child (GTK_BIN (widget));
      if (child && gtk_widget_get_visible (child))
        {
          gint child_min, child_nat;

          gtk_widget_get_preferred_height (child, &child_min, &child_nat);

          *minimum += child_min;
          *natural += child_nat;
        }

      temp = indicator_size + indicator_spacing * 2;
      *minimum = MAX (*minimum, temp) + 2 * (focus_width + focus_pad);
      *natural = MAX (*natural, temp) + 2 * (focus_width + focus_pad);
    }
  else
    GTK_WIDGET_CLASS (gtk_check_button_parent_class)->get_preferred_height (widget, minimum, natural);
}

static void
gtk_check_button_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  GtkCheckButton *check_button;
  GtkToggleButton *toggle_button;
  GtkButton *button;
  GtkAllocation child_allocation;

  button = GTK_BUTTON (widget);
  check_button = GTK_CHECK_BUTTON (widget);
  toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (gtk_toggle_button_get_mode (toggle_button))
    {
      GtkWidget *child;
      gint indicator_size;
      gint indicator_spacing;
      gint focus_width;
      gint focus_pad;
      
      _gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);
      gtk_widget_style_get (widget,
			    "focus-line-width", &focus_width,
			    "focus-padding", &focus_pad,
			    NULL);

      gtk_widget_set_allocation (widget, allocation);

      if (gtk_widget_get_realized (widget))
	gdk_window_move_resize (gtk_button_get_event_window (button),
				allocation->x, allocation->y,
				allocation->width, allocation->height);

      child = gtk_bin_get_child (GTK_BIN (button));
      if (child && gtk_widget_get_visible (child))
	{
          guint border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

	  child_allocation.width = allocation->width -
	    ((border_width + focus_width + focus_pad) * 2 + indicator_size + indicator_spacing * 3);
	  child_allocation.width = MAX (child_allocation.width, 1);

	  child_allocation.height = allocation->height - (border_width + focus_width + focus_pad) * 2;
	  child_allocation.height = MAX (child_allocation.height, 1);
	  
	  child_allocation.x = (border_width + indicator_size + indicator_spacing * 3 +
                                allocation->x + focus_width + focus_pad);
          child_allocation.y = allocation->y + (allocation->height - child_allocation.height) / 2;

	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	    child_allocation.x = allocation->x + allocation->width
	      - (child_allocation.x - allocation->x + child_allocation.width);
	  
	  gtk_widget_size_allocate (child, &child_allocation);
	}
    }
  else
    GTK_WIDGET_CLASS (gtk_check_button_parent_class)->size_allocate (widget, allocation);
}

static gint
gtk_check_button_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  GtkToggleButton *toggle_button;
  GtkBin *bin;
  GtkWidget *child;
  
  toggle_button = GTK_TOGGLE_BUTTON (widget);
  bin = GTK_BIN (widget);

  if (gtk_toggle_button_get_mode (toggle_button))
    {
      gtk_check_button_paint (widget, cr);

      child = gtk_bin_get_child (bin);
      if (child)
        gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                      child,
                                      cr);
    }
  else if (GTK_WIDGET_CLASS (gtk_check_button_parent_class)->draw)
    GTK_WIDGET_CLASS (gtk_check_button_parent_class)->draw (widget, cr);

  return FALSE;
}


static void
gtk_check_button_draw_indicator (GtkCheckButton *check_button,
				 cairo_t        *cr)
{
  GtkCheckButtonClass *class = GTK_CHECK_BUTTON_GET_CLASS (check_button);

  if (class->draw_indicator)
    class->draw_indicator (check_button, cr);
}

static void
gtk_real_check_button_draw_indicator (GtkCheckButton *check_button,
				      cairo_t        *cr)
{
  GtkWidget *widget;
  GtkWidget *child;
  GtkButton *button;
  GtkToggleButton *toggle_button;
  GtkStateFlags state = 0;
  gint x, y;
  gint indicator_size;
  gint indicator_spacing;
  gint focus_width;
  gint focus_pad;
  guint border_width;
  gboolean interior_focus;
  GtkAllocation allocation;
  GtkStyleContext *context;

  widget = GTK_WIDGET (check_button);
  button = GTK_BUTTON (check_button);
  toggle_button = GTK_TOGGLE_BUTTON (check_button);

  gtk_widget_get_allocation (widget, &allocation);
  context = gtk_widget_get_style_context (widget);

  gtk_widget_style_get (widget, 
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width, 
                        "focus-padding", &focus_pad, 
                        NULL);

  _gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  x = indicator_spacing + border_width;
  y = (allocation.height - indicator_size) / 2;

  child = gtk_bin_get_child (GTK_BIN (check_button));
  if (!interior_focus || !(child && gtk_widget_get_visible (child)))
    x += focus_width + focus_pad;      

  if (gtk_toggle_button_get_inconsistent (toggle_button))
    state |= GTK_STATE_FLAG_INCONSISTENT;
  else if (gtk_toggle_button_get_active (toggle_button) ||
           (button->priv->button_down && button->priv->in_button))
    state |= GTK_STATE_FLAG_ACTIVE;

  if (button->priv->activate_timeout || (button->priv->button_down && button->priv->in_button))
    state |= GTK_STATE_FLAG_SELECTED;

  if (button->priv->in_button)
    state |= GTK_STATE_FLAG_PRELIGHT;
  else if (!gtk_widget_is_sensitive (widget))
    state |= GTK_STATE_FLAG_INSENSITIVE;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x = allocation.width - (indicator_size + x);

  gtk_style_context_set_state (context, state);

  if (state & GTK_STATE_FLAG_PRELIGHT)
    gtk_render_background (context, cr,
                           border_width, border_width,
                           allocation.width - (2 * border_width),
                           allocation.height - (2 * border_width));

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_CHECK);

  gtk_render_check (context, cr,
		    x, y, indicator_size, indicator_size);

  gtk_style_context_restore (context);
}
