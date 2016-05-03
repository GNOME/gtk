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
#include "gtkbuiltiniconprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkboxgadgetprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssnumbervalueprivate.h"
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
 * ├── check
 * ╰── <child>
 * ]|
 *
 * A GtkCheckButton with indicator (see gtk_toggle_button_set_mode()) has a
 * main CSS node with name checkbutton and a subnode with name check.
 *
 * |[<!-- language="plain" -->
 * button.check
 * ├── check
 * ╰── <child>
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

typedef struct {
  GtkCssGadget *gadget;
  GtkCssGadget *indicator_gadget;
} GtkCheckButtonPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkCheckButton, gtk_check_button, GTK_TYPE_TOGGLE_BUTTON)

static void
gtk_check_button_update_node_state (GtkWidget *widget)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssImageBuiltinType image_type;
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (widget);

  /* XXX: This is somewhat awkward here, but there's no better
   * way to update the icon
   */
  if (state & GTK_STATE_FLAG_CHECKED)
    image_type = GTK_IS_RADIO_BUTTON (widget) ? GTK_CSS_IMAGE_BUILTIN_OPTION : GTK_CSS_IMAGE_BUILTIN_CHECK;
  else if (state & GTK_STATE_FLAG_INCONSISTENT)
    image_type = GTK_IS_RADIO_BUTTON (widget) ? GTK_CSS_IMAGE_BUILTIN_OPTION_INCONSISTENT : GTK_CSS_IMAGE_BUILTIN_CHECK_INCONSISTENT;
  else
    image_type = GTK_CSS_IMAGE_BUILTIN_NONE;
  gtk_builtin_icon_set_image (GTK_BUILTIN_ICON (priv->indicator_gadget), image_type);

  gtk_css_gadget_set_state (priv->indicator_gadget, state);
}


static void
gtk_check_button_state_flags_changed (GtkWidget     *widget,
				      GtkStateFlags  previous_state_flags)
{
  gtk_check_button_update_node_state (widget);

  GTK_WIDGET_CLASS (gtk_check_button_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_check_button_direction_changed (GtkWidget        *widget,
                                    GtkTextDirection  previous_direction)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));

  gtk_box_gadget_reverse_children (GTK_BOX_GADGET (priv->gadget));
  gtk_box_gadget_set_allocate_reverse (GTK_BOX_GADGET (priv->gadget),
                                       gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  gtk_box_gadget_set_align_reverse (GTK_BOX_GADGET (priv->gadget),
                                    gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

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
gtk_check_button_add (GtkContainer *container,
                      GtkWidget    *widget)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (container));
  int pos;

  GTK_CONTAINER_CLASS (gtk_check_button_parent_class)->add (container, widget);

  pos = gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL ? 0 : 1;
  gtk_box_gadget_insert_widget (GTK_BOX_GADGET (priv->gadget), pos, widget);
  gtk_box_gadget_set_gadget_expand (GTK_BOX_GADGET (priv->gadget), G_OBJECT (widget), TRUE);
}

static void
gtk_check_button_remove (GtkContainer *container,
                         GtkWidget    *widget)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (container));

  gtk_box_gadget_remove_widget (GTK_BOX_GADGET (priv->gadget), widget);

  GTK_CONTAINER_CLASS (gtk_check_button_parent_class)->remove (container, widget);
}

static void
gtk_check_button_class_init (GtkCheckButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

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

  container_class->add = gtk_check_button_add;
  container_class->remove = gtk_check_button_remove;

  /**
   * GtkCheckButton:indicator-size:
   *
   * The size of the indicator.
   *
   * Deprecated: 3.20: Use CSS min-width and min-height on the indicator node.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("indicator-size",
							     P_("Indicator Size"),
							     P_("Size of check or radio indicator"),
							     0,
							     G_MAXINT,
							     INDICATOR_SIZE,
							     GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkCheckButton:indicator-spacing:
   *
   * The spacing around the indicator.
   *
   * Deprecated: 3.20: Use CSS margins of the indicator node,
   *    the value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("indicator-spacing",
							     P_("Indicator Spacing"),
							     P_("Spacing around check or radio indicator"),
							     0,
							     G_MAXINT,
							     INDICATOR_SPACING,
							     GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

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
  priv->gadget = gtk_box_gadget_new_for_node (widget_node, GTK_WIDGET (check_button));
  gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_gadget_set_draw_focus (GTK_BOX_GADGET (priv->gadget), TRUE);
  priv->indicator_gadget = gtk_builtin_icon_new ("check",
                                                 GTK_WIDGET (check_button),
                                                 priv->gadget,
                                                 NULL);
  gtk_builtin_icon_set_default_size_property (GTK_BUILTIN_ICON (priv->indicator_gadget), "indicator-size");
  gtk_box_gadget_insert_gadget (GTK_BOX_GADGET (priv->gadget), 0, priv->indicator_gadget, FALSE, GTK_ALIGN_BASELINE);

  gtk_check_button_update_node_state (GTK_WIDGET (check_button));
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
  GtkButton *button = GTK_BUTTON (widget);
  GtkCssGadget *gadget;
  GdkRectangle clip;
  PangoContext *pango_context;
  PangoFontMetrics *metrics;

  if (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    gadget = priv->gadget;
  else
    gadget = button->priv->gadget;

  gtk_widget_set_allocation (widget, allocation);
  gtk_css_gadget_allocate (gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);

  pango_context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (pango_context,
                                       pango_context_get_font_description (pango_context),
                                       pango_context_get_language (pango_context));
  button->priv->baseline_align =
      (double)pango_font_metrics_get_ascent (metrics) /
      (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));
  pango_font_metrics_unref (metrics);

  if (gtk_widget_get_realized (widget))
    {
      GtkAllocation border_allocation;
      gtk_css_gadget_get_border_allocation (gadget, &border_allocation, NULL);
      gdk_window_move_resize (GTK_BUTTON (widget)->priv->event_window,
                              border_allocation.x,
                              border_allocation.y,
                              border_allocation.width,
                              border_allocation.height);
    }
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

GtkCssNode *
gtk_check_button_get_indicator_node (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  return gtk_css_gadget_get_node (priv->indicator_gadget);
}
