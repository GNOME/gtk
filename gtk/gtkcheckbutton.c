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
#include "gtkcssnodeprivate.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkradiobutton.h"


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
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * checkbutton
 * ╰── check
 * ]|
 *
 * A GtkCheckButton with indicator (see gtk_toggle_button_set_mode()) has a
 * main CSS node with name checkbutton and a subnode with name check.
 *
 * |[<!-- language="plain" -->
 * button.check
 * ╰── [check]
 * ]|
 *
 * A GtkCheckButton without indicator changes the name of its main node
 * to button and adds a .check style class to it. The subnode is invisible
 * in this case.
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
static void gtk_check_button_draw_indicator      (GtkCheckButton      *check_button,
						  cairo_t             *cr);
static void gtk_real_check_button_draw_indicator (GtkCheckButton      *check_button,
						  cairo_t             *cr);

static void     gtk_check_button_measure       (GtkCssGadget        *gadget,
                                                GtkOrientation       orientation,
                                                int                  for_size,
                                                int                 *minimum,
                                                int                 *natural,
                                                int                 *minimum_baseline,
                                                int                 *natural_baseline,
                                                gpointer             unused);
static void     gtk_check_button_allocate      (GtkCssGadget        *gadget,
                                                const GtkAllocation *allocation,
                                                int                  baseline,
                                                GtkAllocation       *out_clip,
                                                gpointer             unused);
static gboolean gtk_check_button_render        (GtkCssGadget        *gadget,
                                                cairo_t             *cr,
                                                int                  x,
                                                int                  y,
                                                int                  width,
                                                int                  height,
                                                gpointer             data);
static void     gtk_check_button_measure_check (GtkCssGadget        *gadget,
                                                GtkOrientation       orientation,
                                                int                  for_size,
                                                int                 *minimum,
                                                int                 *natural,
                                                int                 *minimum_baseline,
                                                int                 *natural_baseline,
                                                gpointer             unused);
static gboolean gtk_check_button_render_check  (GtkCssGadget        *gadget,
                                                cairo_t             *cr,
                                                int                  x,
                                                int                  y,
                                                int                  width,
                                                int                  height,
                                                gpointer             data);

typedef struct {
  GtkCssGadget *gadget;
  GtkCssGadget *indicator_gadget;
} GtkCheckButtonPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkCheckButton, gtk_check_button, GTK_TYPE_TOGGLE_BUTTON)

static void
gtk_check_button_state_flags_changed (GtkWidget     *widget,
				      GtkStateFlags  previous_state_flags)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));

  gtk_css_node_set_state (gtk_css_gadget_get_node (priv->indicator_gadget), gtk_widget_get_state_flags (widget));

  GTK_WIDGET_CLASS (gtk_check_button_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_check_button_direction_changed (GtkWidget        *widget,
                                    GtkTextDirection  previous_direction)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    {
      GtkCssNode *node, *parent, *sibling;

      node = gtk_css_gadget_get_node (priv->indicator_gadget);
      parent = gtk_css_node_get_parent (node);
      sibling = gtk_widget_get_css_node (child);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
        gtk_css_node_insert_before (parent, node, sibling);
      else
        gtk_css_node_insert_after (parent, node, sibling);
    }

  GTK_WIDGET_CLASS (gtk_check_button_parent_class)->direction_changed (widget, previous_direction);
}

static void
gtk_check_button_finalize (GObject *object)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (object));

  g_clear_object (&priv->gadget);
  g_clear_object (&priv->indicator_gadget);

  G_OBJECT_CLASS (gtk_check_button_parent_class)->finalize (object);
}

static void
gtk_check_button_class_init (GtkCheckButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_check_button_finalize;

  widget_class->get_preferred_width = gtk_check_button_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_check_button_get_preferred_width_for_height;
  widget_class->get_preferred_height = gtk_check_button_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_check_button_get_preferred_height_for_width;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_check_button_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = gtk_check_button_size_allocate;
  widget_class->draw = gtk_check_button_draw;
  widget_class->state_flags_changed = gtk_check_button_state_flags_changed;
  widget_class->direction_changed = gtk_check_button_direction_changed;

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

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_CHECK_BOX);
  gtk_widget_class_set_css_name (widget_class, "checkbutton");
}

static void
draw_indicator_changed (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (button));
  GtkCssNode *widget_node;
  GtkCssNode *indicator_node;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (button));
  indicator_node = gtk_css_gadget_get_node (priv->indicator_gadget);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (button)))
    {
      gtk_button_set_alignment (button, 0.0, 0.5);
      gtk_css_node_set_visible (indicator_node, TRUE);
      if (GTK_IS_RADIO_BUTTON (button))
        {
          gtk_css_node_remove_class (widget_node, g_quark_from_static_string ("radio"));
          gtk_css_node_set_name (widget_node, I_("radiobutton"));
        }
      else if (GTK_IS_CHECK_BUTTON (button))
        {
          gtk_css_node_remove_class (widget_node, g_quark_from_static_string ("check"));
          gtk_css_node_set_name (widget_node, I_("checkbutton"));
        }
    }
  else
    {
      gtk_button_set_alignment (button, 0.5, 0.5);
      gtk_css_node_set_visible (indicator_node, FALSE);
      if (GTK_IS_RADIO_BUTTON (button))
        {
          gtk_css_node_add_class (widget_node, g_quark_from_static_string ("radio"));
          gtk_css_node_set_name (widget_node, I_("button"));
        }
      else if (GTK_IS_CHECK_BUTTON (button))
        {
          gtk_css_node_add_class (widget_node, g_quark_from_static_string ("check"));
          gtk_css_node_set_name (widget_node, I_("button"));
        }
    }
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_check_button_init (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);
  GtkCssNode *widget_node;

  gtk_widget_set_receives_default (GTK_WIDGET (check_button), FALSE);
  g_signal_connect (check_button, "notify::draw-indicator", G_CALLBACK (draw_indicator_changed), NULL);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check_button), TRUE);

  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (check_button)), "toggle");

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (check_button));
  priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                     GTK_WIDGET (check_button),
                                                     gtk_check_button_measure,
                                                     gtk_check_button_allocate,
                                                     gtk_check_button_render,
                                                     NULL,
                                                     NULL);

  priv->indicator_gadget = gtk_css_custom_gadget_new ("check",
                                                      GTK_WIDGET (check_button),
                                                      priv->gadget,
                                                      NULL,
                                                      gtk_check_button_measure_check,
                                                      NULL,
                                                      gtk_check_button_render_check,
                                                      NULL,
                                                      NULL);
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

static gboolean
gtk_check_button_render (GtkCssGadget *gadget,
                         cairo_t      *cr,
                         int           x,
                         int           y,
                         int           width,
                         int           height,
                         gpointer      data)
{
  GtkWidget *widget;
  GtkWidget *child;

  widget = gtk_css_gadget_get_owner (gadget);

  gtk_check_button_draw_indicator (GTK_CHECK_BUTTON (widget), cr);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);

  if (gtk_widget_has_visible_focus (widget))
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
      GtkStyleContext *context;
      GtkAllocation allocation;

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
        gtk_render_focus (context, cr, x, y, width, height);
    }

  return FALSE;
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
gtk_check_button_measure (GtkCssGadget   *gadget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline,
                          gpointer        unused)
{
  GtkWidget *widget;
  GtkCheckButtonPrivate *priv;

  widget = gtk_css_gadget_get_owner (gadget);
  priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      GtkWidget *child;
      gint check_min, check_nat;

      gtk_css_gadget_get_preferred_size (priv->indicator_gadget,
                                         GTK_ORIENTATION_HORIZONTAL,
                                         for_size,
                                         &check_min, &check_nat,
                                         NULL, NULL);

      child = gtk_bin_get_child (GTK_BIN (widget));
      if (child && gtk_widget_get_visible (child))
        {
          gint child_min, child_nat;
          gint spacing;

          gtk_widget_style_get (widget,
                                "indicator-spacing", &spacing,
                                NULL);

          _gtk_widget_get_preferred_size_for_size (child,
                                                   GTK_ORIENTATION_HORIZONTAL,
                                                   for_size,
                                                   &child_min, &child_nat,
                                                   NULL, NULL);
          *minimum = check_min + 2 * spacing + child_min;
          *natural = check_nat + 2 * spacing + child_nat;
        }
      else
        {
          *minimum = check_min;
          *natural = check_nat;
        }
    }
  else
    {
      GtkWidget *child;
      gint check_min, check_nat;
      gint check_min_width, check_nat_width;

      gtk_css_gadget_get_preferred_size (priv->indicator_gadget,
                                         GTK_ORIENTATION_HORIZONTAL,
                                         -1,
                                         &check_min_width, &check_nat_width,
                                         NULL, NULL);
      gtk_css_gadget_get_preferred_size (priv->indicator_gadget,
                                         GTK_ORIENTATION_VERTICAL,
                                         -1,
                                         &check_min, &check_nat,
                                         NULL, NULL);

      child = gtk_bin_get_child (GTK_BIN (widget));
      if (child && gtk_widget_get_visible (child))
        {
          gint child_min, child_nat;
	  gint child_min_baseline = -1, child_nat_baseline = -1;

          if (for_size > -1)
            for_size -= check_nat_width;

          gtk_widget_get_preferred_height_and_baseline_for_width (child, for_size,
                                                                  &child_min, &child_nat,
                                                                  &child_min_baseline, &child_nat_baseline);

          *minimum = MAX (check_min, child_min);
          *natural = MAX (check_nat, child_nat);

          if (minimum_baseline && child_min_baseline >= 0)
            *minimum_baseline = child_min_baseline + (*minimum - child_min) / 2;
          if (natural_baseline && child_nat_baseline >= 0)
            *natural_baseline = child_nat_baseline + (*natural - child_nat) / 2;
        }
      else
        {
          *minimum = check_min;
          *natural = check_nat;
        }
    }
}

static void
gtk_check_button_measure_check (GtkCssGadget   *gadget,
                                GtkOrientation  orientation,
                                int             for_size,
                                int            *minimum,
                                int            *natural,
                                int            *minimum_baseline,
                                int            *natural_baseline,
                                gpointer        unused)
{
  GtkWidget *widget;
  gint indicator_size;

  widget = gtk_css_gadget_get_owner (gadget);
  gtk_widget_style_get (widget,
                        "indicator-size", &indicator_size,
                        NULL);

  *minimum = *natural = indicator_size;
}

static void
gtk_check_button_get_preferred_width_for_height (GtkWidget *widget,
                                                 gint       height,
                                                 gint      *minimum,
                                                 gint      *natural)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssGadget *gadget;

  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    gadget = priv->gadget;
  else
    gadget = GTK_BUTTON (widget)->priv->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     height,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_check_button_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssGadget *gadget;

  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    gadget = priv->gadget;
  else
    gadget = GTK_BUTTON (widget)->priv->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_check_button_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
							      gint       width,
							      gint      *minimum,
							      gint      *natural,
							      gint      *minimum_baseline,
							      gint      *natural_baseline)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssGadget *gadget;

  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    gadget = priv->gadget;
  else
    gadget = GTK_BUTTON (widget)->priv->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum, natural,
                                     minimum_baseline, natural_baseline);
}

static void
gtk_check_button_get_preferred_height_for_width (GtkWidget *widget,
                                                 gint       width,
                                                 gint      *minimum,
                                                 gint      *natural)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssGadget *gadget;

  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    gadget = priv->gadget;
  else
    gadget = GTK_BUTTON (widget)->priv->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_check_button_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssGadget *gadget;

  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    gadget = priv->gadget;
  else
    gadget = GTK_BUTTON (widget)->priv->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_check_button_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssGadget *gadget;
  GdkRectangle clip;

  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    gadget = priv->gadget;
  else
    gadget = GTK_BUTTON (widget)->priv->gadget;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (GTK_BUTTON (widget)->priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  gtk_css_gadget_allocate (gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_check_button_allocate (GtkCssGadget        *gadget,
                           const GtkAllocation *allocation,
                           int                  baseline,
                           GtkAllocation       *out_clip,
                           gpointer             unused)
{
  GtkCheckButtonPrivate *priv;
  GtkWidget *widget;
  PangoContext *pango_context;
  PangoFontMetrics *metrics;
  GtkCheckButton *check_button;
  GtkButton *button;
  GtkAllocation child_allocation;
  GtkWidget *child;
  gint check_min_width, check_nat_width;
  gint check_min_height, check_nat_height;
  gint spacing;
  GdkRectangle check_clip;

  widget = gtk_css_gadget_get_owner (gadget);
  button = GTK_BUTTON (widget);
  check_button = GTK_CHECK_BUTTON (widget);
  priv = gtk_check_button_get_instance_private (check_button);

  g_assert (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)));

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_button_get_event_window (button),
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  gtk_css_gadget_get_preferred_size (priv->indicator_gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     &check_min_width, &check_nat_width,
                                     NULL, NULL);
  gtk_css_gadget_get_preferred_size (priv->indicator_gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &check_min_height, &check_nat_height,
                                     NULL, NULL);
  gtk_widget_style_get (widget,
                        "indicator-spacing", &spacing,
                        NULL);

  child = gtk_bin_get_child (GTK_BIN (button));
  if (!child || !gtk_widget_get_visible (child))
    spacing = 0;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    child_allocation.x = allocation->x + spacing;
  else
    child_allocation.x = allocation->x + allocation->width - check_nat_width - spacing;
  child_allocation.y = allocation->y + (allocation->height - check_nat_height) / 2;
  child_allocation.width = check_nat_width;
  child_allocation.height = check_nat_height;

  gtk_css_gadget_allocate (priv->indicator_gadget,
                           &child_allocation,
                           baseline,
                           &check_clip);

  if (child && gtk_widget_get_visible (child))
    {
      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
        child_allocation.x = allocation->x + 2 * spacing + check_nat_width;
      else
        child_allocation.x = allocation->x;
      child_allocation.y = allocation->y;
      child_allocation.width = allocation->width - check_nat_width - 2 * spacing;
      child_allocation.height = allocation->height;

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

  gtk_container_get_children_clip (GTK_CONTAINER (widget), out_clip);
  gdk_rectangle_union (out_clip, &check_clip, out_clip);
}

static gint
gtk_check_button_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssGadget *gadget;

  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    gadget = priv->gadget;
  else
    gadget = GTK_BUTTON (widget)->priv->gadget;

  gtk_css_gadget_draw (gadget, cr);

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
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  gtk_css_gadget_draw (priv->indicator_gadget, cr);
}


static gboolean
gtk_check_button_render_check (GtkCssGadget *gadget,
                               cairo_t      *cr,
                               int           x,
                               int           y,
                               int           width,
                               int           height,
                               gpointer      data)
{
  GtkWidget *widget;
  GtkStyleContext *context;
  GtkCheckButtonPrivate *priv;

  widget = gtk_css_gadget_get_owner (gadget);
  priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save_to_node (context, gtk_css_gadget_get_node (priv->indicator_gadget));
  gtk_render_check (context, cr, x, y, width, height);
  gtk_style_context_restore (context);

  return FALSE;
}

GtkCssNode *
gtk_check_button_get_indicator_node (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  return gtk_css_gadget_get_node (priv->indicator_gadget);
}
