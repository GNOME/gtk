/* gtkaccessible.c: Accessible interface
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkAccessible:
 *
 * `GtkAccessible` is an interface for describing UI elements for
 * Assistive Technologies.
 *
 * Every accessible implementation has:
 *
 *  - a “role”, represented by a value of the [enum@Gtk.AccessibleRole] enumeration
 *  - an “attribute”, represented by a set of [enum@Gtk.AccessibleState],
 *    [enum@Gtk.AccessibleProperty] and [enum@Gtk.AccessibleRelation] values
 *
 * The role cannot be changed after instantiating a `GtkAccessible`
 * implementation.
 *
 * The attributes are updated every time a UI element's state changes in
 * a way that should be reflected by assistive technologies. For instance,
 * if a `GtkWidget` visibility changes, the %GTK_ACCESSIBLE_STATE_HIDDEN
 * state will also change to reflect the [property@Gtk.Widget:visible] property.
 */

#include "config.h"

#include "gtkaccessibleprivate.h"

#include "gtkatcontextprivate.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"
#include "gtkwidget.h"

#include <glib/gi18n-lib.h>

#include <stdarg.h>

G_DEFINE_INTERFACE (GtkAccessible, gtk_accessible, G_TYPE_OBJECT)

static void
gtk_accessible_default_init (GtkAccessibleInterface *iface)
{
  /**
   * GtkAccessible:accessible-role: (attributes org.gtk.Property.get=gtk_accessible_get_accessible_role)
   *
   * The accessible role of the given `GtkAccessible` implementation.
   *
   * The accessible role cannot be changed once set.
   */
  GParamSpec *pspec =
    g_param_spec_enum ("accessible-role", NULL, NULL,
                       GTK_TYPE_ACCESSIBLE_ROLE,
                       GTK_ACCESSIBLE_ROLE_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_interface_install_property (iface, pspec);
}

/*< private >
 * gtk_accessible_get_at_context:
 * @self: a `GtkAccessible`
 *
 * Retrieves the `GtkATContext` for the given `GtkAccessible`.
 *
 * Returns: (transfer none): the `GtkATContext`
 */
GtkATContext *
gtk_accessible_get_at_context (GtkAccessible *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), NULL);

  return GTK_ACCESSIBLE_GET_IFACE (self)->get_at_context (self);
}

/**
 * gtk_accessible_get_accessible_role: (attributes org.gtk.Method.get_property=accessible-role)
 * @self: a `GtkAccessible`
 *
 * Retrieves the `GtkAccessibleRole` for the given `GtkAccessible`.
 *
 * Returns: a `GtkAccessibleRole`
 */
GtkAccessibleRole
gtk_accessible_get_accessible_role (GtkAccessible *self)
{
  GtkAccessibleRole role;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), GTK_ACCESSIBLE_ROLE_NONE);

  GtkATContext *context = gtk_accessible_get_at_context (self);
  if (context != NULL && gtk_at_context_is_realized (context))
    return gtk_at_context_get_accessible_role (context);

  g_object_get (G_OBJECT (self), "accessible-role", &role, NULL);

  return role;
}

/**
 * gtk_accessible_update_state:
 * @self: a `GtkAccessible`
 * @first_state: the first `GtkAccessibleState`
 * @...: a list of state and value pairs, terminated by -1
 *
 * Updates a list of accessible states. See the [enum@Gtk.AccessibleState]
 * documentation for the value types of accessible states.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * state change must be communicated to assistive technologies.
 *
 * Example:
 * ```c
 * value = GTK_ACCESSIBLE_TRISTATE_MIXED;
 * gtk_accessible_update_state (GTK_ACCESSIBLE (check_button),
 *                              GTK_ACCESSIBLE_STATE_CHECKED, value,
 *                              -1);
 * ```
 */
void
gtk_accessible_update_state (GtkAccessible      *self,
                             GtkAccessibleState  first_state,
                             ...)
{
  GtkAccessibleState state;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_state);

  state = first_state;

  while (state != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_state (state, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect value for state “%s”: %s",
                      gtk_accessible_state_get_attribute_name (state),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_state (context, state, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      state = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);
}

/**
 * gtk_accessible_update_state_value: (rename-to gtk_accessible_update_state)
 * @self: a `GtkAccessible`
 * @n_states: the number of accessible states to set
 * @states: (array length=n_states): an array of `GtkAccessibleState`
 * @values: (array length=n_states): an array of `GValues`, one for each state
 *
 * Updates an array of accessible states.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * state change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_state_value (GtkAccessible      *self,
                                   int                 n_states,
                                   GtkAccessibleState  states[],
                                   const GValue        values[])
{
  g_return_if_fail (GTK_IS_ACCESSIBLE (self));
  g_return_if_fail (n_states > 0);

  GtkATContext *context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  for (int i = 0; i < n_states; i++)
    {
      GtkAccessibleState state = states[i];
      const GValue *value = &(values[i]);
      GError *error = NULL;
      GtkAccessibleValue *real_value =
        gtk_accessible_value_collect_for_state_value (state, value, &error);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for state “%s”: %s",
                      gtk_accessible_state_get_attribute_name (state),
                      error->message);
          g_error_free (error);
          break;
        }

      gtk_at_context_set_accessible_state (context, state, real_value);

      if (real_value != NULL)
        gtk_accessible_value_unref (real_value);
    }

  gtk_at_context_update (context);
}

/**
 * gtk_accessible_reset_state:
 * @self: a `GtkAccessible`
 * @state: a `GtkAccessibleState`
 *
 * Resets the accessible @state to its default value.
 */
void
gtk_accessible_reset_state (GtkAccessible      *self,
                            GtkAccessibleState  state)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_state (context, state, NULL);
  gtk_at_context_update (context);
}

/**
 * gtk_accessible_update_property:
 * @self: a `GtkAccessible`
 * @first_property: the first `GtkAccessibleProperty`
 * @...: a list of property and value pairs, terminated by -1
 *
 * Updates a list of accessible properties.
 *
 * See the [enum@Gtk.AccessibleProperty] documentation for the
 * value types of accessible properties.
 *
 * This function should be called by `GtkWidget` types whenever
 * an accessible property change must be communicated to assistive
 * technologies.
 *
 * Example:
 * ```c
 * value = gtk_adjustment_get_value (adjustment);
 * gtk_accessible_update_property (GTK_ACCESSIBLE (spin_button),
                                   GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, value,
                                   -1);
 * ```
 */
void
gtk_accessible_update_property (GtkAccessible         *self,
                                GtkAccessibleProperty  first_property,
                                ...)
{
  GtkAccessibleProperty property;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_property);

  property = first_property;

  while (property != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_property (property, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for property “%s”: %s",
                      gtk_accessible_property_get_attribute_name (property),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_property (context, property, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      property = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);
}

/**
 * gtk_accessible_update_property_value: (rename-to gtk_accessible_update_property)
 * @self: a `GtkAccessible`
 * @n_properties: the number of accessible properties to set
 * @properties: (array length=n_properties): an array of `GtkAccessibleProperty`
 * @values: (array length=n_properties): an array of `GValues`, one for each property
 *
 * Updates an array of accessible properties.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * property change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_property_value (GtkAccessible         *self,
                                      int                    n_properties,
                                      GtkAccessibleProperty  properties[],
                                      const GValue           values[])
{
  g_return_if_fail (GTK_IS_ACCESSIBLE (self));
  g_return_if_fail (n_properties > 0);

  GtkATContext *context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  for (int i = 0; i < n_properties; i++)
    {
      GtkAccessibleProperty property = properties[i];
      const GValue *value = &(values[i]);
      GError *error = NULL;
      GtkAccessibleValue *real_value =
        gtk_accessible_value_collect_for_property_value (property, value, &error);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for property “%s”: %s",
                      gtk_accessible_property_get_attribute_name (property),
                      error->message);
          g_error_free (error);
          break;
        }

      gtk_at_context_set_accessible_property (context, property, real_value);

      if (real_value != NULL)
        gtk_accessible_value_unref (real_value);
    }

  gtk_at_context_update (context);
}

/**
 * gtk_accessible_reset_property:
 * @self: a `GtkAccessible`
 * @property: a `GtkAccessibleProperty`
 *
 * Resets the accessible @property to its default value.
 */
void
gtk_accessible_reset_property (GtkAccessible         *self,
                               GtkAccessibleProperty  property)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_property (context, property, NULL);
  gtk_at_context_update (context);
}

/**
 * gtk_accessible_update_relation:
 * @self: a `GtkAccessible`
 * @first_relation: the first `GtkAccessibleRelation`
 * @...: a list of relation and value pairs, terminated by -1
 *
 * Updates a list of accessible relations.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * relation change must be communicated to assistive technologies.
 *
 * If the [enum@Gtk.AccessibleRelation] requires a list of references,
 * you should pass each reference individually, followed by %NULL, e.g.
 *
 * ```c
 * gtk_accessible_update_relation (accessible,
 *                                 GTK_ACCESSIBLE_RELATION_CONTROLS,
 *                                   ref1, NULL,
 *                                 GTK_ACCESSIBLE_RELATION_LABELLED_BY,
 *                                   ref1, ref2, ref3, NULL,
 *                                 -1);
 * ```
 */
void
gtk_accessible_update_relation (GtkAccessible         *self,
                                GtkAccessibleRelation  first_relation,
                                ...)
{
  GtkAccessibleRelation relation;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_relation);

  relation = first_relation;

  while (relation != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_relation (relation, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for relation “%s”: %s",
                      gtk_accessible_relation_get_attribute_name (relation),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_relation (context, relation, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      relation = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);
}

/**
 * gtk_accessible_update_relation_value: (rename-to gtk_accessible_update_relation)
 * @self: a `GtkAccessible`
 * @n_relations: the number of accessible relations to set
 * @relations: (array length=n_relations): an array of `GtkAccessibleRelation`
 * @values: (array length=n_relations): an array of `GValues`, one for each relation
 *
 * Updates an array of accessible relations.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * relation change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_relation_value (GtkAccessible         *self,
                                      int                    n_relations,
                                      GtkAccessibleRelation  relations[],
                                      const GValue           values[])
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));
  g_return_if_fail (n_relations > 0);

  context = gtk_accessible_get_at_context (self);

  for (int i = 0; i < n_relations; i++)
    {
      GtkAccessibleRelation relation = relations[i];
      const GValue *value = &(values[i]);
      GError *error = NULL;
      GtkAccessibleValue *real_value =
        gtk_accessible_value_collect_for_relation_value (relation, value, &error);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for relation “%s”: %s",
                      gtk_accessible_relation_get_attribute_name (relation),
                      error->message);
          g_error_free (error);
          break;
        }

      if (context)
        gtk_at_context_set_accessible_relation (context, relation, real_value);

      if (real_value != NULL)
        gtk_accessible_value_unref (real_value);
    }

  if (context)
    gtk_at_context_update (context);
}

/**
 * gtk_accessible_reset_relation:
 * @self: a `GtkAccessible`
 * @relation: a `GtkAccessibleRelation`
 *
 * Resets the accessible @relation to its default value.
 */
void
gtk_accessible_reset_relation (GtkAccessible         *self,
                               GtkAccessibleRelation  relation)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_relation (context, relation, NULL);
  gtk_at_context_update (context);
}

static const char *role_names[] = {
  [GTK_ACCESSIBLE_ROLE_ALERT] = NC_("accessibility", "alert"),
  [GTK_ACCESSIBLE_ROLE_ALERT_DIALOG] = NC_("accessibility", "alert dialog"),
  [GTK_ACCESSIBLE_ROLE_BANNER] = NC_("accessibility", "banner"),
  [GTK_ACCESSIBLE_ROLE_BUTTON] = NC_("accessibility", "button"),
  [GTK_ACCESSIBLE_ROLE_CAPTION] = NC_("accessibility", "caption"),
  [GTK_ACCESSIBLE_ROLE_CELL] = NC_("accessibility", "cell"),
  [GTK_ACCESSIBLE_ROLE_CHECKBOX] = NC_("accessibility", "checkbox"),
  [GTK_ACCESSIBLE_ROLE_COLUMN_HEADER] = NC_("accessibility", "column header"),
  [GTK_ACCESSIBLE_ROLE_COMBO_BOX] = NC_("accessibility", "combo box"),
  [GTK_ACCESSIBLE_ROLE_COMMAND] = NC_("accessibility", "command"),
  [GTK_ACCESSIBLE_ROLE_COMPOSITE] = NC_("accessibility", "composite"),
  [GTK_ACCESSIBLE_ROLE_DIALOG] = NC_("accessibility", "dialog"),
  [GTK_ACCESSIBLE_ROLE_DOCUMENT] = NC_("accessibility", "document"),
  [GTK_ACCESSIBLE_ROLE_FEED] = NC_("accessibility", "feed"),
  [GTK_ACCESSIBLE_ROLE_FORM] = NC_("accessibility", "form"),
  [GTK_ACCESSIBLE_ROLE_GENERIC] = NC_("accessibility", "generic"),
  [GTK_ACCESSIBLE_ROLE_GRID] = NC_("accessibility", "grid"),
  [GTK_ACCESSIBLE_ROLE_GRID_CELL] = NC_("accessibility", "grid cell"),
  [GTK_ACCESSIBLE_ROLE_GROUP] = NC_("accessibility", "group"),
  [GTK_ACCESSIBLE_ROLE_HEADING] = NC_("accessibility", "heading"),
  [GTK_ACCESSIBLE_ROLE_IMG] = NC_("accessibility", "image"),
  [GTK_ACCESSIBLE_ROLE_INPUT] = NC_("accessibility", "input"),
  [GTK_ACCESSIBLE_ROLE_LABEL] = NC_("accessibility", "label"),
  [GTK_ACCESSIBLE_ROLE_LANDMARK] = NC_("accessibility", "landmark"),
  [GTK_ACCESSIBLE_ROLE_LEGEND] = NC_("accessibility", "legend"),
  [GTK_ACCESSIBLE_ROLE_LINK] = NC_("accessibility", "link"),
  [GTK_ACCESSIBLE_ROLE_LIST] = NC_("accessibility", "list"),
  [GTK_ACCESSIBLE_ROLE_LIST_BOX] = NC_("accessibility", "list box"),
  [GTK_ACCESSIBLE_ROLE_LIST_ITEM] = NC_("accessibility", "list item"),
  [GTK_ACCESSIBLE_ROLE_LOG] = NC_("accessibility", "log"),
  [GTK_ACCESSIBLE_ROLE_MAIN] = NC_("accessibility", "main"),
  [GTK_ACCESSIBLE_ROLE_MARQUEE] = NC_("accessibility", "marquee"),
  [GTK_ACCESSIBLE_ROLE_MATH] = NC_("accessibility", "math"),
  [GTK_ACCESSIBLE_ROLE_METER] = NC_("accessibility", "meter"),
  [GTK_ACCESSIBLE_ROLE_MENU] = NC_("accessibility", "menu"),
  [GTK_ACCESSIBLE_ROLE_MENU_BAR] = NC_("accessibility", "menu bar"),
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM] = NC_("accessibility", "menu item"),
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX] = NC_("accessibility", "menu item checkbox"),
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO] = NC_("accessibility", "menu item radio"),
  [GTK_ACCESSIBLE_ROLE_NAVIGATION] = NC_("accessibility", "navigation"),
  [GTK_ACCESSIBLE_ROLE_NONE] = NC_("accessibility", "none"),
  [GTK_ACCESSIBLE_ROLE_NOTE] = NC_("accessibility", "note"),
  [GTK_ACCESSIBLE_ROLE_OPTION] = NC_("accessibility", "option"),
  [GTK_ACCESSIBLE_ROLE_PRESENTATION] = NC_("accessibility", "presentation"),
  [GTK_ACCESSIBLE_ROLE_PROGRESS_BAR] = NC_("accessibility", "progress bar"),
  [GTK_ACCESSIBLE_ROLE_RADIO] = NC_("accessibility", "radio"),
  [GTK_ACCESSIBLE_ROLE_RADIO_GROUP] = NC_("accessibility", "radio group"),
  [GTK_ACCESSIBLE_ROLE_RANGE] = NC_("accessibility", "range"),
  [GTK_ACCESSIBLE_ROLE_REGION] = NC_("accessibility", "region"),
  [GTK_ACCESSIBLE_ROLE_ROW] = NC_("accessibility", "row"),
  [GTK_ACCESSIBLE_ROLE_ROW_GROUP] = NC_("accessibility", "row group"),
  [GTK_ACCESSIBLE_ROLE_ROW_HEADER] = NC_("accessibility", "row header"),
  [GTK_ACCESSIBLE_ROLE_SCROLLBAR] = NC_("accessibility", "scroll bar"),
  [GTK_ACCESSIBLE_ROLE_SEARCH] = NC_("accessibility", "search"),
  [GTK_ACCESSIBLE_ROLE_SEARCH_BOX] = NC_("accessibility", "search box"),
  [GTK_ACCESSIBLE_ROLE_SECTION] = NC_("accessibility", "section"),
  [GTK_ACCESSIBLE_ROLE_SECTION_HEAD] = NC_("accessibility", "section head"),
  [GTK_ACCESSIBLE_ROLE_SELECT] = NC_("accessibility", "select"),
  [GTK_ACCESSIBLE_ROLE_SEPARATOR] = NC_("accessibility", "separator"),
  [GTK_ACCESSIBLE_ROLE_SLIDER] = NC_("accessibility", "slider"),
  [GTK_ACCESSIBLE_ROLE_SPIN_BUTTON] = NC_("accessibility", "spin button"),
  [GTK_ACCESSIBLE_ROLE_STATUS] = NC_("accessibility", "status"),
  [GTK_ACCESSIBLE_ROLE_STRUCTURE] = NC_("accessibility", "structure"),
  [GTK_ACCESSIBLE_ROLE_SWITCH] = NC_("accessibility", "switch"),
  [GTK_ACCESSIBLE_ROLE_TAB] = NC_("accessibility", "tab"),
  [GTK_ACCESSIBLE_ROLE_TABLE] = NC_("accessibility", "table"),
  [GTK_ACCESSIBLE_ROLE_TAB_LIST] = NC_("accessibility", "tab list"),
  [GTK_ACCESSIBLE_ROLE_TAB_PANEL] = NC_("accessibility", "tab panel"),
  [GTK_ACCESSIBLE_ROLE_TEXT_BOX] = NC_("accessibility", "text box"),
  [GTK_ACCESSIBLE_ROLE_TIME] = NC_("accessibility", "time"),
  [GTK_ACCESSIBLE_ROLE_TIMER] = NC_("accessibility", "timer"),
  [GTK_ACCESSIBLE_ROLE_TOOLBAR] = NC_("accessibility", "tool bar"),
  [GTK_ACCESSIBLE_ROLE_TOOLTIP] = NC_("accessibility", "tool tip"),
  [GTK_ACCESSIBLE_ROLE_TREE] = NC_("accessibility", "tree"),
  [GTK_ACCESSIBLE_ROLE_TREE_GRID] = NC_("accessibility", "tree grid"),
  [GTK_ACCESSIBLE_ROLE_TREE_ITEM] = NC_("accessibility", "tree item"),
  [GTK_ACCESSIBLE_ROLE_WIDGET] = NC_("accessibility", "widget"),
  [GTK_ACCESSIBLE_ROLE_WINDOW] = NC_("accessibility", "window"),
};

/*< private >
 * gtk_accessible_role_to_name:
 * @role: a `GtkAccessibleRole`
 * @domain: (nullable): the translation domain
 *
 * Converts a `GtkAccessibleRole` value to the equivalent role name.
 *
 * If @domain is not %NULL, the returned string will be localized.
 *
 * Returns: (transfer none): the name of the role
 */
const char *
gtk_accessible_role_to_name (GtkAccessibleRole  role,
                             const char        *domain)
{
  if (domain != NULL)
    return g_dpgettext2 (domain, "accessibility", role_names[role]);

  return role_names[role];
}

/*<private>
 * gtk_accessible_platform_changed:
 * @self: a `GtkAccessible`
 * @change: the platform state change to report
 *
 * Notify accessible technologies that a platform value has changed.
 *
 * ARIA discriminates between author-controlled states and 'platform'
 * states, which are not. This function can be used by widgets to
 * inform ATs that a platform state, such as focus, has changed.
 *
 * Note that the state itself is not included in this API.
 * AT backends should use [method@Gtk.Accessible.get_platform_state]
 * to obtain the actual state.
 */
void
gtk_accessible_platform_changed (GtkAccessible               *self,
                                 GtkAccessiblePlatformChange  change)
{
  GtkATContext *context;

  if (GTK_IS_WIDGET (self) &&
      gtk_widget_get_root (GTK_WIDGET (self)) == NULL)
    return;

  context = gtk_accessible_get_at_context (self);

  /* propagate changes up from ignored widgets */
  if (gtk_accessible_get_accessible_role (self) == GTK_ACCESSIBLE_ROLE_NONE)
    context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (gtk_widget_get_parent (GTK_WIDGET (self))));

  if (context == NULL)
    return;

  gtk_at_context_platform_changed (context, change);
  gtk_at_context_update (context);
}

/*<private>
 * gtk_accessible_get_platform_state:
 * @self: a `GtkAccessible`
 * @state: platform state to query
 *
 * Query a platform state, such as focus.
 *
 * See gtk_accessible_platform_changed().
 *
 * This functionality can be overridden by `GtkAccessible`
 * implementations, e.g. to get platform state from an ignored
 * child widget, as is the case for `GtkText` wrappers.
 *
 * Returns: the value of @state for the accessible
 */
gboolean
gtk_accessible_get_platform_state (GtkAccessible              *self,
                                   GtkAccessiblePlatformState  state)
{
  return GTK_ACCESSIBLE_GET_IFACE (self)->get_platform_state (self, state);
}

/*<private>
 * gtk_accessible_bounds_changed:
 * @self: a `GtkAccessible`
 *
 * This function can be used to inform ATs that an
 * accessibles bounds (ie its screen extents) have
 * changed.
 *
 * Note that the bounds are not included in this API.
 * AT backends should use widget API to obtain them.
 */
void
gtk_accessible_bounds_changed (GtkAccessible *self)
{
  GtkATContext *context;

  if (GTK_IS_WIDGET (self) &&
      gtk_widget_get_root (GTK_WIDGET (self)) == NULL)
    return;

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_bounds_changed (context);
}

/*<private>
 * gtk_accessible_should_present:
 * @self: a `GtkAccessible`
 *
 * Returns whether this accessible should be represented to ATs.
 *
 * By default, hidden widgets are are among these, but there can
 * be other reasons to return %FALSE, e.g. for widgets that are
 * purely presentations, or for widgets whose functionality is
 * represented elsewhere, as is the case for `GtkText` widgets.
 *
 * Returns: %TRUE if the widget should be represented
 */
gboolean
gtk_accessible_should_present (GtkAccessible *self)
{
  GtkAccessibleRole role;
  GtkATContext *context;

  if (GTK_IS_WIDGET (self) &&
      !gtk_widget_get_visible (GTK_WIDGET (self)))
    return FALSE;

  role = gtk_accessible_get_accessible_role (self);
  if (role == GTK_ACCESSIBLE_ROLE_NONE ||
      role == GTK_ACCESSIBLE_ROLE_PRESENTATION)
    return FALSE;

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return FALSE;

  if (gtk_at_context_has_accessible_state (context, GTK_ACCESSIBLE_STATE_HIDDEN))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_state (context, GTK_ACCESSIBLE_STATE_HIDDEN);
      if (gtk_boolean_accessible_value_get (value))
        return FALSE;
    }

  return TRUE;
}

void
gtk_accessible_update_children (GtkAccessible           *self,
                                GtkAccessible           *child,
                                GtkAccessibleChildState  state)
{
  GtkATContext *context;

  if (GTK_IS_WIDGET (self) &&
      gtk_widget_get_root (GTK_WIDGET (self)) == NULL)
    return;

  context = gtk_accessible_get_at_context (self);

  /* propagate changes up from ignored widgets */
  if (gtk_accessible_get_accessible_role (self) == GTK_ACCESSIBLE_ROLE_NONE)
    context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (gtk_widget_get_parent (GTK_WIDGET (self))));

  if (context == NULL)
    return;

  gtk_at_context_child_changed (context, 1 << state, child);
  gtk_at_context_update (context);
}
