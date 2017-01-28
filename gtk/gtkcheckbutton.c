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

#include "gtkcheckbuttonprivate.h"

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
 * A GtkCheckButton with indicator (see gtk_check_button_set_draw_indicator()) has a
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


static void gtk_check_button_size_allocate       (GtkWidget           *widget,
						  GtkAllocation       *allocation);
static void gtk_check_button_snapshot            (GtkWidget           *widget,
						  GtkSnapshot         *snapshot);

typedef struct {
  GtkCssGadget *gadget;
  GtkCssGadget *indicator_gadget;

  guint draw_indicator : 1;
  guint inconsistent   : 1;
} GtkCheckButtonPrivate;

enum {
  PROP_0,
  PROP_DRAW_INDICATOR,
  PROP_INCONSISTENT,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

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
gtk_check_button_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkCssGadget *gadget;

  if (priv->draw_indicator)
    gadget = priv->gadget;
  else
    gadget = GTK_BUTTON (widget)->priv->gadget;

  gtk_css_gadget_get_preferred_size (gadget,
                                     orientation,
                                     for_size,
                                     minimum, natural,
                                     minimum_baseline, natural_baseline);
}

static void
gtk_check_button_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
      case PROP_DRAW_INDICATOR:
        gtk_check_button_set_draw_indicator (GTK_CHECK_BUTTON (object),
                                             g_value_get_boolean (value));

      break;
      case PROP_INCONSISTENT:
        gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (object),
                                           g_value_get_boolean (value));
      break;
      default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_check_button_get_property (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
      case PROP_DRAW_INDICATOR:
        g_value_set_boolean (value, gtk_check_button_get_draw_indicator (GTK_CHECK_BUTTON (object)));
      break;
      case PROP_INCONSISTENT:
        g_value_set_boolean (value, gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (object)));
      break;
      default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_check_button_class_init (GtkCheckButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->finalize = gtk_check_button_finalize;
  object_class->set_property = gtk_check_button_set_property;
  object_class->get_property = gtk_check_button_get_property;

  widget_class->measure = gtk_check_button_measure;
  widget_class->size_allocate = gtk_check_button_size_allocate;
  widget_class->snapshot = gtk_check_button_snapshot;
  widget_class->state_flags_changed = gtk_check_button_state_flags_changed;
  widget_class->direction_changed = gtk_check_button_direction_changed;

  container_class->add = gtk_check_button_add;
  container_class->remove = gtk_check_button_remove;

  props[PROP_DRAW_INDICATOR] =
      g_param_spec_boolean ("draw-indicator",
                            P_("Draw Indicator"),
                            P_("If the indicator part of the button is displayed"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_INCONSISTENT] =
      g_param_spec_boolean ("inconsistent",
                            P_("Inconsistent"),
                            P_("If the check button is in an “in between” state"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_CHECK_BOX);
  gtk_widget_class_set_css_name (widget_class, "checkbutton");
}

static void
draw_indicator_changed (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);
  GtkCssNode *widget_node;
  GtkCssNode *indicator_node;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (check_button));
  indicator_node = gtk_css_gadget_get_node (priv->indicator_gadget);

  if (priv->draw_indicator)
    {
      gtk_css_node_set_visible (indicator_node, TRUE);
      if (GTK_IS_RADIO_BUTTON (check_button))
        {
          gtk_css_node_remove_class (widget_node, g_quark_from_static_string ("radio"));
          gtk_css_node_set_name (widget_node, I_("radiobutton"));
        }
      else if (GTK_IS_CHECK_BUTTON (check_button))
        {
          gtk_css_node_remove_class (widget_node, g_quark_from_static_string ("check"));
          gtk_css_node_set_name (widget_node, I_("checkbutton"));
        }
    }
  else
    {
      gtk_css_node_set_visible (indicator_node, FALSE);
      if (GTK_IS_RADIO_BUTTON (check_button))
        {
          gtk_css_node_add_class (widget_node, g_quark_from_static_string ("radio"));
          gtk_css_node_set_name (widget_node, I_("button"));
        }
      else if (GTK_IS_CHECK_BUTTON (check_button))
        {
          gtk_css_node_add_class (widget_node, g_quark_from_static_string ("check"));
          gtk_css_node_set_name (widget_node, I_("button"));
        }
    }
}

static void
gtk_check_button_init (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);
  GtkCssNode *widget_node;

  priv->draw_indicator = TRUE;

  gtk_widget_set_receives_default (GTK_WIDGET (check_button), FALSE);

  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (check_button)), "toggle");

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (check_button));
  priv->gadget = gtk_box_gadget_new_for_node (widget_node, GTK_WIDGET (check_button));
  gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_gadget_set_draw_focus (GTK_BOX_GADGET (priv->gadget), TRUE);
  priv->indicator_gadget = gtk_builtin_icon_new ("check",
                                                 GTK_WIDGET (check_button),
                                                 priv->gadget,
                                                 NULL);
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
gtk_check_button_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));
  GtkButton *button = GTK_BUTTON (widget);
  GtkCssGadget *gadget;
  GdkRectangle clip;
  PangoContext *pango_context;
  PangoFontMetrics *metrics;

  if (priv->draw_indicator)
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

static void
gtk_check_button_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));

  if (!priv->draw_indicator)
    GTK_WIDGET_CLASS (gtk_check_button_parent_class)->snapshot (widget, snapshot);
  else
    gtk_css_gadget_snapshot (priv->gadget, snapshot);
}

GtkCssNode *
gtk_check_button_get_indicator_node (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  return gtk_css_gadget_get_node (priv->indicator_gadget);
}

/**
 * gtk_check_button_set_draw_indicator:
 * @check_button: a #GtkCheckButton
 * @draw_indicator: Whether or not to draw the indicator part of the button
 *
 * Sets whether the indicator part of the button is drawn. This is important for
 * cases where the check button should have the functinality of a check button,
 * but the visuals of a regular button, like in a #GtkStackSwitcher.
 */
void
gtk_check_button_set_draw_indicator (GtkCheckButton *check_button,
                                     gboolean        draw_indicator)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  g_return_if_fail (GTK_IS_CHECK_BUTTON (check_button));

  draw_indicator = !!draw_indicator;

  if (draw_indicator != priv->draw_indicator)
    {
      priv->draw_indicator = draw_indicator;
      draw_indicator_changed (check_button);
      gtk_widget_queue_resize (GTK_WIDGET (check_button));
      g_object_notify_by_pspec (G_OBJECT (check_button), props[PROP_DRAW_INDICATOR]);
    }
}

/**
 * gtk_check_button_get_draw_indicator:
 * @check_button: a #GtkCheckButton
 *
 * Returns Whether or not the indicator part of the button gets drawn.
 *
 * Returns: The value of the GtkCheckButton:draw-indicator property.
 */
gboolean
gtk_check_button_get_draw_indicator (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (check_button), FALSE);

  return priv->draw_indicator;
}

/**
 * gtk_check_button_set_inconsistent:
 * @check_button: a #GtkCheckButton
 * @inconsistent: %TRUE if state is inconsistent
 *
 * If the user has selected a range of elements (such as some text or
 * spreadsheet cells) that are affected by a check button, and the
 * current values in that range are inconsistent, you may want to
 * display the toggle in an "in between" state. Normally you would
 * turn off the inconsistent state again if the user checks the
 * check button. This has to be done manually,
 * gtk_check_button_set_inconsistent only affects visual appearance,
 * not the semantics of the button.
 */
void
gtk_check_button_set_inconsistent (GtkCheckButton *check_button,
                                   gboolean        inconsistent)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  g_return_if_fail (GTK_IS_CHECK_BUTTON (check_button));

  inconsistent = !!inconsistent;
  if (inconsistent != priv->inconsistent)
    {
      if (inconsistent)
        gtk_widget_set_state_flags (GTK_WIDGET (check_button), GTK_STATE_FLAG_INCONSISTENT, FALSE);
      else
        gtk_widget_unset_state_flags (GTK_WIDGET (check_button), GTK_STATE_FLAG_INCONSISTENT);

      g_object_notify_by_pspec (G_OBJECT (check_button), props[PROP_INCONSISTENT]);
    }
}

/**
 * gtk_check_button_get_inconsistent:
 * @check_button: a #GtkCheckButton
 *
 * Returns: %TRUE if @check_button is currently in an 'in between' state,
 *   %FALSE otherwise.
 */
gboolean
gtk_check_button_get_inconsistent (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (check_button), FALSE);

  return priv->inconsistent;
}
