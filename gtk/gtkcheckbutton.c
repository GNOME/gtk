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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkrender.h"
#include "gtkwidgetprivate.h"


/**
 * SECTION:gtkcheckbutton
 * @Short_description: Create widgets with a discrete toggle button
 * @Title: GtkCheckButton
 * @See_also: #GtkCheckMenuItem, #GtkButton, #GtkToggleButton, #GtkRadioButton
 *
 * A #GtkCheckButton places a discrete #GtkToggleButton next to a widget,
 * (usually a #GtkLabel). See the section on #GtkToggleButton widgets for
 * more information about toggle/check buttons.
 *
 * The important signal ( #GtkToggleButton::toggled ) is also inherited from
 * #GtkToggleButton.
 */


#define INDICATOR_SIZE     16
#define INDICATOR_SPACING  2


static void gtk_check_button_get_preferred_width                         (GtkWidget          *widget,
                                                                          gint               *minimum,
                                                                          gint               *natural);
static void gtk_check_button_get_preferred_width_for_height              (GtkWidget          *widget,
                                                                          gint                height,
                                                                          gint               *minimum,
                                                                          gint               *natural);
static void gtk_check_button_get_preferred_height                        (GtkWidget          *widget,
                                                                          gint               *minimum,
                                                                          gint               *natural);
static void gtk_check_button_get_preferred_height_for_width              (GtkWidget          *widget,
                                                                          gint                width,
                                                                          gint               *minimum,
                                                                          gint               *natural);
static void gtk_check_button_get_preferred_height_and_baseline_for_width (GtkWidget          *widget,
									  gint                width,
									  gint               *minimum,
									  gint               *natural,
									  gint               *minimum_baseline,
									  gint               *natural_baseline);
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
gtk_check_button_state_flags_changed (GtkWidget     *widget,
				      GtkStateFlags  previous_state_flags)
{
  /* FIXME
   * This is a hack to get around the optimizations done by the CSS engine.
   *
   * The CSS engine will notice that no CSS properties changed on the
   * widget itself when going from one state to another and not queue
   * a redraw.
   * And the reason for no properties changing will be that only the
   * checkmark itself changes, but that is hidden behind a
   * gtk_style_context_save()/_restore() pair, so it won't be caught.
   */
  gtk_widget_queue_draw (widget);

  GTK_WIDGET_CLASS (gtk_check_button_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_check_button_class_init (GtkCheckButtonClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->get_preferred_width = gtk_check_button_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_check_button_get_preferred_width_for_height;
  widget_class->get_preferred_height = gtk_check_button_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_check_button_get_preferred_height_for_width;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_check_button_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = gtk_check_button_size_allocate;
  widget_class->draw = gtk_check_button_draw;
  widget_class->state_flags_changed = gtk_check_button_state_flags_changed;

  class->draw_indicator = gtk_real_check_button_draw_indicator;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_CHECK_BOX);

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
draw_indicator_changed (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  GtkButton *button = GTK_BUTTON (object);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (button)))
    gtk_button_set_alignment (button, 0.0, 0.5);
  else
    gtk_button_set_alignment (button, 0.5, 0.5);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_check_button_init (GtkCheckButton *check_button)
{
  gtk_widget_set_receives_default (GTK_WIDGET (check_button), FALSE);
  g_signal_connect (check_button, "notify::draw-indicator", G_CALLBACK (draw_indicator_changed), NULL);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check_button), TRUE);
}

/**
 * gtk_check_button_new:
 *
 * Creates a new #GtkCheckButton.
 *
 * Returns: a #GtkWidget.
 */
GtkWidget*
gtk_check_button_new (void)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, NULL);
}


/**
 * gtk_check_button_new_with_label:
 * @label: the text for the check button.
 *
 * Creates a new #GtkCheckButton with a #GtkLabel to the right of it.
 *
 * Returns: a #GtkWidget.
 */
GtkWidget*
gtk_check_button_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, "label", label, NULL);
}

/**
 * gtk_check_button_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *   mnemonic character
 *
 * Creates a new #GtkCheckButton containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the check button.
 *
 * Returns: a new #GtkCheckButton
 */
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

  gtk_check_button_draw_indicator (check_button, cr);

  if (gtk_widget_has_visible_focus (widget))
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
      GtkStyleContext *context;
      GtkAllocation allocation;
      gint border_width;

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      gtk_widget_get_allocation (widget, &allocation);
      context = gtk_widget_get_style_context (widget);

      if (child && gtk_widget_get_visible (child))
        {
          GtkAllocation child_allocation;

          gtk_widget_get_allocation (child, &child_allocation);
          gtk_render_focus (context, cr,
                            child_allocation.x - allocation.x,
                            child_allocation.y - allocation.y,
                            child_allocation.width,
                            child_allocation.height);
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
get_padding_and_border (GtkWidget *widget,
                        GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
  border->left += tmp.left;
}

static void
gtk_check_button_get_full_border (GtkCheckButton *check_button,
                                  GtkBorder      *border,
                                  gint           *indicator)
{
  int indicator_size, indicator_spacing, indicator_extra, border_width;
  GtkWidget *child;

  get_padding_and_border (GTK_WIDGET (check_button), border);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (check_button));
  gtk_widget_style_get (GTK_WIDGET (check_button),
                        "indicator-size", &indicator_size,
                        "indicator-spacing", &indicator_spacing,
                        NULL);
  child = gtk_bin_get_child (GTK_BIN (check_button));

  border->left += border_width;
  border->right += border_width;
  border->top += border_width;
  border->bottom += border_width;

  indicator_extra = indicator_size + 2 * indicator_spacing;
  if (child && gtk_widget_get_visible (child))
    indicator_extra += indicator_spacing;
  if (gtk_widget_get_direction (GTK_WIDGET (check_button)) == GTK_TEXT_DIR_RTL)
    border->right += indicator_extra;
  else
    border->left += indicator_extra;

  if (indicator)
    *indicator = indicator_size + 2 * indicator_spacing;
}

static void
gtk_check_button_get_preferred_width_for_height (GtkWidget *widget,
                                                 gint       height,
                                                 gint      *minimum,
                                                 gint      *natural)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (widget);
  
  if (gtk_toggle_button_get_mode (toggle_button))
    {
      GtkWidget *child;
      GtkBorder border;

      gtk_check_button_get_full_border (GTK_CHECK_BUTTON (widget), &border, NULL);


      child = gtk_bin_get_child (GTK_BIN (widget));
      if (child && gtk_widget_get_visible (child))
        {
          if (height > -1)
            height -= border.top + border.bottom;

          _gtk_widget_get_preferred_size_for_size (child,
                                                   GTK_ORIENTATION_HORIZONTAL,
                                                   height,
                                                   minimum, natural,
                                                   NULL, NULL);
        }
      else
        {
          *minimum = 0;
          *natural = 0;
        }

      *minimum += border.left + border.right;
      *natural += border.left + border.right;
    }
  else
    GTK_WIDGET_CLASS (gtk_check_button_parent_class)->get_preferred_width (widget, minimum, natural);
}

static void
gtk_check_button_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  gtk_check_button_get_preferred_width_for_height (widget, -1, minimum, natural);
}

static void
gtk_check_button_get_preferred_height_and_baseline_for_width (GtkWidget          *widget,
							      gint                width,
							      gint               *minimum,
							      gint               *natural,
							      gint               *minimum_baseline,
							      gint               *natural_baseline)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (gtk_toggle_button_get_mode (toggle_button))
    {
      GtkWidget *child;
      GtkBorder border;
      gint indicator;

      gtk_check_button_get_full_border (GTK_CHECK_BUTTON (widget), &border, &indicator);

      child = gtk_bin_get_child (GTK_BIN (widget));
      if (child && gtk_widget_get_visible (child))
        {
          gint child_min, child_nat;
	  gint child_min_baseline = -1, child_nat_baseline = -1;

          if (width > -1)
            width -= border.left + border.right;

          gtk_widget_get_preferred_height_and_baseline_for_width (child, width,
								  &child_min, &child_nat,
								  &child_min_baseline, &child_nat_baseline);

          *minimum = MAX (indicator, child_min);
          *natural = MAX (indicator, child_nat);

	  if (minimum_baseline && child_min_baseline >= 0)
	    *minimum_baseline = child_min_baseline + border.top + (*minimum - child_min) / 2;
	  if (natural_baseline && child_nat_baseline >= 0)
	    *natural_baseline = child_nat_baseline + border.top + (*natural - child_nat) / 2;
        }
      else
        {
          *minimum = indicator;
          *natural = indicator;
        }

      *minimum += border.top + border.bottom;
      *natural += border.top + border.bottom;
    }
  else
    GTK_WIDGET_CLASS (gtk_check_button_parent_class)->get_preferred_height_and_baseline_for_width (widget, width,
												   minimum, natural,
												   minimum_baseline, natural_baseline);
}

static void
gtk_check_button_get_preferred_height_for_width (GtkWidget *widget,
                                                 gint       width,
                                                 gint      *minimum,
                                                 gint      *natural)
{
  gtk_check_button_get_preferred_height_and_baseline_for_width (widget, width,
								minimum, natural,
								NULL, NULL);
}

static void
gtk_check_button_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  gtk_check_button_get_preferred_height_and_baseline_for_width (widget, -1,
								minimum, natural,
								NULL, NULL);
}

static void
gtk_check_button_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  PangoContext *pango_context;
  PangoFontMetrics *metrics;
  GtkCheckButton *check_button;
  GtkToggleButton *toggle_button;
  GtkButton *button;
  GtkAllocation child_allocation;
  gint baseline;

  button = GTK_BUTTON (widget);
  check_button = GTK_CHECK_BUTTON (widget);
  toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (gtk_toggle_button_get_mode (toggle_button))
    {
      GtkWidget *child;

      gtk_widget_set_allocation (widget, allocation);

      if (gtk_widget_get_realized (widget))
	gdk_window_move_resize (gtk_button_get_event_window (button),
				allocation->x, allocation->y,
				allocation->width, allocation->height);

      child = gtk_bin_get_child (GTK_BIN (button));
      if (child && gtk_widget_get_visible (child))
	{
          GtkBorder border;
          
          gtk_check_button_get_full_border (check_button, &border, NULL);

          child_allocation.x = allocation->x + border.left;
          child_allocation.y = allocation->y + border.top;
	  child_allocation.width = allocation->width - border.left - border.right;
	  child_allocation.height = allocation->height - border.top - border.bottom;

	  baseline = gtk_widget_get_allocated_baseline (widget);
	  if (baseline != -1)
	    baseline -= border.top;
	  gtk_widget_size_allocate_with_baseline (child, &child_allocation, baseline);
	}

      pango_context = gtk_widget_get_pango_context (widget);
      metrics = pango_context_get_metrics (pango_context,
					   pango_context_get_font_description (pango_context),
					   pango_context_get_language (pango_context));
      button->priv->baseline_align =
	(double)pango_font_metrics_get_ascent (metrics) /
	(pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));
      pango_font_metrics_unref (metrics);

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
  GtkButton *button;
  gint x, y;
  gint indicator_size;
  gint indicator_spacing;
  gint baseline;
  guint border_width;
  GtkAllocation allocation;
  GtkStyleContext *context;

  widget = GTK_WIDGET (check_button);
  button = GTK_BUTTON (check_button);

  gtk_widget_get_allocation (widget, &allocation);
  baseline = gtk_widget_get_allocated_baseline (widget);
  context = gtk_widget_get_style_context (widget);

  _gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  x = indicator_spacing + border_width;
  if (baseline == -1)
    y = (allocation.height - indicator_size) / 2;
  else
    y = CLAMP (baseline - indicator_size * button->priv->baseline_align,
	       0, allocation.height - indicator_size);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x = allocation.width - (indicator_size + x);

  gtk_style_context_save (context);

  gtk_render_background (context, cr,
                         border_width, border_width,
                         allocation.width - (2 * border_width),
                         allocation.height - (2 * border_width));

  gtk_style_context_add_class (context, GTK_STYLE_CLASS_CHECK);

  gtk_render_check (context, cr,
		    x, y, indicator_size, indicator_size);

  gtk_style_context_restore (context);
}
