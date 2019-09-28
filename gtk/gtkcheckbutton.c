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
#include "gtkboxlayout.h"
#include "gtkbinlayout.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkradiobutton.h"
#include "gtkiconprivate.h"


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

typedef struct {
  GtkWidget *indicator_widget;

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

  if (!priv->indicator_widget)
    return;

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

  gtk_icon_set_image (GTK_ICON (priv->indicator_widget), image_type);
  gtk_widget_set_state_flags (priv->indicator_widget, state, TRUE);
}


static void
gtk_check_button_state_flags_changed (GtkWidget     *widget,
				      GtkStateFlags  previous_state_flags)
{
  gtk_check_button_update_node_state (widget);

  GTK_WIDGET_CLASS (gtk_check_button_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_check_button_finalize (GObject *object)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (object));

  g_clear_pointer (&priv->indicator_widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_check_button_parent_class)->finalize (object);
}

static void
gtk_check_button_add (GtkContainer *container,
                      GtkWidget    *widget)
{
  _gtk_bin_set_child (GTK_BIN (container), widget);

  if (gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL)
    gtk_widget_insert_after (widget, GTK_WIDGET (container), NULL);
  else
    gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
gtk_check_button_remove (GtkContainer *container,
                         GtkWidget    *widget)
{
  _gtk_bin_set_child (GTK_BIN (container), NULL);
  gtk_widget_unparent (widget);
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
gtk_check_button_direction_changed (GtkWidget        *widget,
                                    GtkTextDirection  previous_direction)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (widget));

  if (!priv->indicator_widget)
    return;

  if (previous_direction == GTK_TEXT_DIR_LTR)
    {
      /* Now RTL -> Move the indicator to the right */
      gtk_widget_insert_before (priv->indicator_widget, widget, NULL);
    }
  else
    {
      /* Now LTR -> Move the indicator to the left */
      gtk_widget_insert_after (priv->indicator_widget, widget, NULL);
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
  gtk_widget_class_set_css_name (widget_class, I_("checkbutton"));
}

static void
gtk_check_button_init (GtkCheckButton *check_button)
{
  gtk_widget_set_receives_default (GTK_WIDGET (check_button), FALSE);

  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (check_button)), "toggle");

  gtk_check_button_set_draw_indicator (check_button, TRUE);
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

GtkCssNode *
gtk_check_button_get_indicator_node (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  return gtk_widget_get_css_node (priv->indicator_widget);
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
  GtkCssNode *widget_node;

  g_return_if_fail (GTK_IS_CHECK_BUTTON (check_button));

  draw_indicator = !!draw_indicator;
  widget_node = gtk_widget_get_css_node (GTK_WIDGET (check_button));

  if (draw_indicator && !priv->indicator_widget)
    {
      const gboolean is_ltr = gtk_widget_get_direction (GTK_WIDGET (check_button)) == GTK_TEXT_DIR_LTR;

      priv->indicator_widget = gtk_icon_new ("check");
      gtk_widget_set_halign (priv->indicator_widget, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (priv->indicator_widget, GTK_ALIGN_CENTER);

      if (is_ltr)
        gtk_widget_insert_after (priv->indicator_widget, GTK_WIDGET (check_button), NULL);
      else
        gtk_widget_insert_before (priv->indicator_widget, GTK_WIDGET (check_button), NULL);

      gtk_widget_set_layout_manager (GTK_WIDGET (check_button), gtk_box_layout_new (GTK_ORIENTATION_HORIZONTAL));

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

      g_object_notify_by_pspec (G_OBJECT (check_button), props[PROP_DRAW_INDICATOR]);
    }
  else if (!draw_indicator && priv->indicator_widget)
    {
      g_clear_pointer (&priv->indicator_widget, gtk_widget_unparent);

      gtk_widget_set_layout_manager (GTK_WIDGET (check_button), gtk_bin_layout_new ());

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

  return priv->indicator_widget != NULL;
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
  if (priv->inconsistent != inconsistent)
    {
      priv->inconsistent = inconsistent;

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
 * Returns whether the check button is in an inconsistent state.
 * 
 * Returns: %TRUE if @check_button is currently in an 'in between' state, %FALSE otherwise.
 */
gboolean
gtk_check_button_get_inconsistent (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (check_button), FALSE);

  return priv->inconsistent;
}
