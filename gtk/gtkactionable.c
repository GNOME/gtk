/*
 * Copyright © 2012 Canonical Limited
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkactionable.h"

#include "gtkwidget.h"
#include "gtkintl.h"

/**
 * SECTION:gtkactionable
 * @title: GtkActionable
 * @short_description: An interface for widgets that can be associated
 *     with actions
 *
 * This interface provides a convenient way of associating widgets with
 * actions on a #GtkApplicationWindow or #GtkApplication.
 *
 * It primarily consists of two properties: #GtkActionable:action-name
 * and #GtkActionable:action-target. There are also some convenience APIs
 * for setting these properties.
 *
 * This interface is presently only meaningful if used on a widget that
 * is (or will be) located inside of a #GtkApplicationWindow and can
 * only be used to associate the widget with actions on that window, or
 * its associated #GtkApplication.
 *
 * Since: 3.4
 **/

/**
 * GtkActionable:
 *
 * An opaque pointer type.
 **/

/**
 * GtkActionableInterface:
 * @get_action_name: virtual pointer for gtk_actionable_get_action_name()
 * @set_action_name: virtual pointer for gtk_actionable_set_action_name()
 * @get_action_target_value: virtual pointer for gtk_actionable_get_action_target_value()
 * @set_action_target_value: virtual pointer for gtk_actionable_set_action_target_value
 *
 * The interface vtable for #GtkActionable.
 **/

G_DEFINE_INTERFACE (GtkActionable, gtk_actionable, GTK_TYPE_WIDGET)

static void
gtk_actionable_default_init (GtkActionableInterface *iface)
{
  g_object_interface_install_property (iface,
    g_param_spec_string ("action-name", P_("action name"),
                         P_("The name of the associated action, like 'app.quit'"),
                         NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
    g_param_spec_variant ("action-target", P_("action target value"),
                          P_("The parameter for action invocations"),
                          G_VARIANT_TYPE_ANY, NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * gtk_actionable_get_action_name:
 * @actionable: a #GtkActionable widget
 *
 * Gets the action name for @actionable.
 *
 * See gtk_actionable_set_action_name() for more information.
 *
 * Returns: the action name, or %NULL if none is set
 *
 * Since: 3.4
 **/
const gchar *
gtk_actionable_get_action_name (GtkActionable *actionable)
{
  g_return_val_if_fail (GTK_IS_ACTIONABLE (actionable), NULL);

  return GTK_ACTIONABLE_GET_IFACE (actionable)
    ->get_action_name (actionable);
}

/**
 * gtk_actionable_set_action_name:
 * @actionable: a #GtkActionable widget
 * @action_name: an action name, or %NULL
 *
 * Specifies the name of the action with which this widget should be
 * associated.  If @action_name is %NULL then the widget will be
 * unassociated from any previous action.
 *
 * Usually this function is used when the widget is located (or will be
 * located) within the hierarchy of a #GtkApplicationWindow.
 *
 * Names are of the form "win.save" or "app.quit" for actions on the
 * containing #GtkApplicationWindow or its associated #GtkApplication,
 * respectively.  This is the same form used for actions in the #GMenu
 * associated with the window.
 *
 * Since: 3.4
 **/
void
gtk_actionable_set_action_name (GtkActionable *actionable,
                                const gchar   *action_name)
{
  g_return_if_fail (GTK_IS_ACTIONABLE (actionable));

  GTK_ACTIONABLE_GET_IFACE (actionable)
    ->set_action_name (actionable, action_name);
}

/**
 * gtk_actionable_get_action_target_value:
 * @actionable: a #GtkActionable widget
 *
 * Gets the current target value of @actionabe.
 *
 * See gtk_actionable_set_action_target_value() for more information.
 *
 * Returns: (transfer none): the current target value
 *
 * Since: 3.4
 **/
GVariant *
gtk_actionable_get_action_target_value (GtkActionable *actionable)
{
  g_return_val_if_fail (GTK_IS_ACTIONABLE (actionable), NULL);

  return GTK_ACTIONABLE_GET_IFACE (actionable)
    ->get_action_target_value (actionable);
}

/**
 * gtk_actionable_set_action_target_value:
 * @actionable: a #GtkActionable widget
 * @target_value: a #GVariant to set as the target value, or %NULL
 *
 * Sets the target value of an actionable widget.
 *
 * If @target_value is %NULL then the target value is unset.
 *
 * The target value has two purposes.  First, it is used as the
 * parameter to activation of the action associated with the
 * #GtkActionable widget. Second, it is used to determine if the widget
 * should be rendered as "active" - the widget is active if the state
 * is equal to the given target.
 *
 * Consider the example of associating a set of buttons with a #GAction
 * with string state in a typical "radio button" situation.  Each button
 * will be associated with the same action, but with a different target
 * value for that action.  Clicking on a particular button will activate
 * the action with the target of that button, which will typically cause
 * the action's state to change to that value.  Since the action's state
 * is now equal to the target value of the button, the button will now
 * be rendered as active (and the other buttons, with different targets,
 * rendered inactive).
 *
 * Since: 3.4
 **/
void
gtk_actionable_set_action_target_value (GtkActionable *actionable,
                                        GVariant      *target_value)
{
  g_return_if_fail (GTK_IS_ACTIONABLE (actionable));

  GTK_ACTIONABLE_GET_IFACE (actionable)
    ->set_action_target_value (actionable, target_value);
}

/**
 * gtk_actionable_set_action_target:
 * @actionable: a #GtkActionable widget
 * @format_string: a GVariant format string
 * @...: arguments appropriate for @format_string
 *
 * Sets the target of an actionable widget.
 *
 * This is a convenience function that calls g_variant_new() for
 * @format_string and uses the result to call
 * gtk_actionable_set_action_target_value().
 *
 * If you are setting a string-valued target and want to set the action
 * name at the same time, you can use
 * gtk_actionable_set_detailed_action_name ().
 *
 * Since: 3.4
 **/
void
gtk_actionable_set_action_target (GtkActionable *actionable,
                                  const gchar   *format_string,
                                  ...)
{
  va_list ap;

  va_start (ap, format_string);
  gtk_actionable_set_action_target_value (actionable, g_variant_new_va (format_string, NULL, &ap));
  va_end (ap);
}

/**
 * gtk_actionable_set_detailed_action_name:
 * @actionable: a #GtkActionable widget
 * @detailed_action_name: the detailed action name
 *
 * Sets the action-name and associated string target value of an
 * actionable widget.
 *
 * This allows for the effect of both gtk_actionable_set_action_name()
 * and gtk_actionable_set_action_target_value() in the common case that
 * the target is string-valued.
 *
 * @detailed_action_name is a string of the form
 * <literal>"action::target"</literal> where <literal>action</literal>
 * is the action name and <literal>target</literal> is the string to use
 * as the target.
 *
 * Since: 3.4
 **/
void
gtk_actionable_set_detailed_action_name (GtkActionable *actionable,
                                         const gchar   *detailed_action_name)
{
  gchar **parts;

  g_return_if_fail (GTK_IS_ACTIONABLE (actionable));

  if (detailed_action_name == NULL)
    {
      gtk_actionable_set_action_name (actionable, NULL);
      gtk_actionable_set_action_target_value (actionable, NULL);
      return;
    }

  parts = g_strsplit (detailed_action_name, "::", 2);
  gtk_actionable_set_action_name (actionable, parts[0]);
  if (parts[0] && parts[1])
    gtk_actionable_set_action_target (actionable, "s", parts[1]);
  else
    gtk_actionable_set_action_target_value (actionable, NULL);
  g_strfreev (parts);
}
