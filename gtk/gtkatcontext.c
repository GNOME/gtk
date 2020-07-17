/* gtkatcontext.c: Assistive technology context
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
 * SECTION:gtkatcontext
 * @Title: GtkATContext
 * @Short_description: An object communicating to Assistive Technologies
 *
 * GtkATContext is an abstract class provided by GTK to communicate to
 * platform-specific assistive technologies API.
 *
 * Each platform supported by GTK implements a #GtkATContext subclass, and
 * is responsible for updating the accessible state in response to state
 * changes in #GtkAccessible.
 */

#include "config.h"

#include "gtkatcontextprivate.h"

#include "gtkaccessiblevalueprivate.h"
#include "gtktestatcontextprivate.h"
#include "gtktypebuiltins.h"

G_DEFINE_ABSTRACT_TYPE (GtkATContext, gtk_at_context, G_TYPE_OBJECT)

enum
{
  PROP_ACCESSIBLE_ROLE = 1,
  PROP_ACCESSIBLE,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

static void
gtk_at_context_finalize (GObject *gobject)
{
  GtkATContext *self = GTK_AT_CONTEXT (gobject);

  gtk_accessible_property_set_unref (self->properties);
  gtk_accessible_relation_set_unref (self->relations);
  gtk_accessible_state_set_unref (self->states);

  G_OBJECT_CLASS (gtk_at_context_parent_class)->finalize (gobject);
}

static void
gtk_at_context_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkATContext *self = GTK_AT_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_ACCESSIBLE_ROLE:
      self->accessible_role = g_value_get_enum (value);
      break;

    case PROP_ACCESSIBLE:
      self->accessible = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_context_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkATContext *self = GTK_AT_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_ACCESSIBLE_ROLE:
      g_value_set_enum (value, self->accessible_role);
      break;

    case PROP_ACCESSIBLE:
      g_value_set_object (value, self->accessible);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_context_real_state_change (GtkATContext                *self,
                                  GtkAccessibleStateChange     changed_states,
                                  GtkAccessiblePropertyChange  changed_properties,
                                  GtkAccessibleRelationChange  changed_relations,
                                  GtkAccessibleStateSet       *states,
                                  GtkAccessiblePropertySet    *properties,
                                  GtkAccessibleRelationSet    *relations)
{
}

static void
gtk_at_context_class_init (GtkATContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_at_context_set_property;
  gobject_class->get_property = gtk_at_context_get_property;
  gobject_class->finalize = gtk_at_context_finalize;

  klass->state_change = gtk_at_context_real_state_change;

  /**
   * GtkATContext:accessible-role:
   *
   * The accessible role used by the AT context.
   *
   * Depending on the given role, different states and properties can be
   * set or retrieved.
   */
  obj_props[PROP_ACCESSIBLE_ROLE] =
    g_param_spec_enum ("accessible-role",
                       "Accessible Role",
                       "The accessible role of the AT context",
                       GTK_TYPE_ACCESSIBLE_ROLE,
                       GTK_ACCESSIBLE_ROLE_WIDGET,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_STATIC_STRINGS);

  /**
   * GtkATContext:accessible:
   *
   * The #GtkAccessible that created the #GtkATContext instance.
   */
  obj_props[PROP_ACCESSIBLE] =
    g_param_spec_object ("accessible",
                         "Accessible",
                         "The accessible implementation",
                         GTK_TYPE_ACCESSIBLE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_at_context_init (GtkATContext *self)
{
  self->accessible_role = GTK_ACCESSIBLE_ROLE_WIDGET;

  self->properties = gtk_accessible_property_set_new ();
  self->relations = gtk_accessible_relation_set_new ();
  self->states = gtk_accessible_state_set_new ();
}

/**
 * gtk_at_context_get_accessible:
 * @self: a #GtkATContext
 *
 * Retrieves the #GtkAccessible using this context.
 *
 * Returns: (transfer none): a #GtkAccessible
 */
GtkAccessible *
gtk_at_context_get_accessible (GtkATContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  return self->accessible;
}

/**
 * gtk_at_context_get_accessible_role:
 * @self: a #GtkATContext
 *
 * Retrieves the accessible role of this context.
 *
 * Returns: a #GtkAccessibleRole
 */
GtkAccessibleRole
gtk_at_context_get_accessible_role (GtkATContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), GTK_ACCESSIBLE_ROLE_WIDGET);

  return self->accessible_role;
}

/*< private >
 * gtk_at_context_create:
 * @accessible_role: the accessible role used by the #GtkATContext
 * @accessible: the #GtkAccessible implementation using the #GtkATContext
 *
 * Creates a new #GtkATContext instance for the given accessible role and
 * accessible instance.
 *
 * The #GtkATContext implementation being instantiated will depend on the
 * platform.
 *
 * Returns: (nullable): the #GtkATContext
 */
GtkATContext *
gtk_at_context_create (GtkAccessibleRole  accessible_role,
                       GtkAccessible     *accessible)
{
  static const char *gtk_test_accessible;
  static const char *gtk_no_a11y;

  if (G_UNLIKELY (gtk_test_accessible == NULL))
    {
      const char *env = g_getenv ("GTK_TEST_ACCESSIBLE");

      if (env != NULL && *env !='\0')
        gtk_test_accessible = "1";
      else
        gtk_test_accessible = "0";
    }

  if (G_UNLIKELY (gtk_no_a11y == NULL))
    {
      const char *env = g_getenv ("GTK_NO_A11Y");

      if (env != NULL && *env != '\0')
        gtk_no_a11y = "1";
      else
        gtk_no_a11y = "0";
    }

  /* Shortcut everything if we're running with the test AT context */
  if (gtk_test_accessible[0] == '1')
    return gtk_test_at_context_new (accessible_role, accessible);

  if (gtk_no_a11y[0] == '1')
    return NULL;

  /* FIXME: Add GIOExtension for AT contexts */
  return gtk_test_at_context_new (accessible_role, accessible);
}

/*< private >
 * gtk_at_context_update:
 * @self: a #GtkATContext
 *
 * Notifies the AT connected to this #GtkATContext that the accessible
 * state and its properties have changed.
 */
void
gtk_at_context_update (GtkATContext *self)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  GtkAccessibleStateChange changed_states = 0;
  GtkAccessiblePropertyChange changed_properties = 0;
  GtkAccessibleRelationChange changed_relations = 0;

  for (int i = 0; i < GTK_ACCESSIBLE_STATE_SELECTED; i++)
    {
      if (gtk_accessible_state_set_contains (self->states, i))
        changed_states |= (1 << i);
    }

  for (int i = 0; i < GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT; i++)
    {
      if (gtk_accessible_property_set_contains (self->properties, i))
        changed_properties |= (1 << i);
    }

  for (int i = 0; i < GTK_ACCESSIBLE_RELATION_SET_SIZE; i++)
    {
      if (gtk_accessible_relation_set_contains (self->relations, i))
        changed_relations |= (1 << i);
    }

  GTK_AT_CONTEXT_GET_CLASS (self)->state_change (self,
                                                 changed_states,
                                                 changed_properties,
                                                 changed_relations,
                                                 self->states,
                                                 self->properties,
                                                 self->relations);
}

/*< private >
 * gtk_at_context_set_state:
 * @self: a #GtkATContext
 * @state: a #GtkAccessibleState
 * @value: (nullable): #GtkAccessibleValue
 *
 * Sets the @value for the given @state of a #GtkATContext.
 *
 * If @value is %NULL, the state is unset.
 *
 * This function will accumulate state changes until gtk_at_context_update_state()
 * is called.
 */
void
gtk_at_context_set_accessible_state (GtkATContext       *self,
                                     GtkAccessibleState  state,
                                     GtkAccessibleValue *value)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  if (value != NULL)
    gtk_accessible_state_set_add (self->states, state, value);
  else
    gtk_accessible_state_set_remove (self->states, state);
}

/*< private >
 * gtk_at_context_set_accessible_property:
 * @self: a #GtkATContext
 * @property: a #GtkAccessibleProperty
 * @value: (nullable): #GtkAccessibleValue
 *
 * Sets the @value for the given @property of a #GtkATContext.
 *
 * If @value is %NULL, the property is unset.
 *
 * This function will accumulate property changes until gtk_at_context_update_state()
 * is called.
 */
void
gtk_at_context_set_accessible_property (GtkATContext          *self,
                                        GtkAccessibleProperty  property,
                                        GtkAccessibleValue    *value)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  if (value != NULL)
    gtk_accessible_property_set_add (self->properties, property, value);
  else
    gtk_accessible_property_set_remove (self->properties, property);
}

/*< private >
 * gtk_at_context_set_accessible_relation:
 * @self: a #GtkATContext
 * @relation: a #GtkAccessibleRelation
 * @value: (nullable): #GtkAccessibleValue
 *
 * Sets the @value for the given @relation of a #GtkATContext.
 *
 * If @value is %NULL, the relation is unset.
 *
 * This function will accumulate relation changes until gtk_at_context_update_state()
 * is called.
 */
void
gtk_at_context_set_accessible_relation (GtkATContext          *self,
                                        GtkAccessibleRelation  relation,
                                        GtkAccessibleValue    *value)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  if (value != NULL)
    gtk_accessible_relation_set_add (self->relations, relation, value);
  else
    gtk_accessible_relation_set_remove (self->relations, relation);
}
