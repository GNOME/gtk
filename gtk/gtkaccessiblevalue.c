/* gtkaccessiblevalue.c: Accessible value
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

/*< private >
 * SECTION: gtkaccessiblevalue
 * @Title: GtkAccessibleValue
 * @Short_description: Container for accessible state and property values
 *
 * GtkAccessibleValue is a reference counted, generic container for values used
 * to represent the state and properties of a #GtkAccessible implementation.
 *
 * There are two kinds of accessible value types:
 *
 *  - hard coded, static values; GTK owns them, and their contents, and are
 *    guaranteed to exist for the duration of the application's life time
 *  - dynamic values; the accessible state owns the value and their contents,
 *    and they can be allocated and freed
 *
 * Typically, the former type of values is used for boolean, tristate, and
 * token value; the latter is used for numbers, strings, and token lists.
 *
 * For more information on the types of values, see the [WAI-ARIA](https://www.w3.org/WAI/PF/aria/states_and_properties#propcharacteristic_value)
 * reference.
 */

#include "config.h"

#include "gtkaccessiblevalueprivate.h"

#include "gtkenums.h"

G_DEFINE_QUARK (gtk-accessible-value-error-quark, gtk_accessible_value_error)

G_DEFINE_BOXED_TYPE (GtkAccessibleValue, gtk_accessible_value,
                     gtk_accessible_value_ref,
                     gtk_accessible_value_unref)

/*< private >
 * gtk_accessible_value_alloc:
 * @value_class: a #GtkAccessibleValueClass structure
 *
 * Allocates a new #GtkAccessibleValue subclass using @value_class as the
 * type definition.
 *
 * Returns: (transfer full): the newly allocated #GtkAccessibleValue
 */
GtkAccessibleValue *
gtk_accessible_value_alloc (const GtkAccessibleValueClass *value_class)
{
  g_return_val_if_fail (value_class != NULL, NULL);
  g_return_val_if_fail (value_class->instance_size >= sizeof (GtkAccessibleValue), NULL);

  GtkAccessibleValue *res = g_malloc0 (value_class->instance_size);

  /* We do not use grefcount, here, because we want to have statically
   * allocated GtkAccessibleValue subclasses, and those cannot be initialized
   * with g_ref_count_init()
   */
  res->ref_count = 1;
  res->value_class = value_class;

  if (res->value_class->init != NULL)
    res->value_class->init (res);

  return res;
}

/*< private >
 * gtk_accessible_value_ref:
 * @self: a #GtkAccessibleValue
 *
 * Acquires a reference on the given #GtkAccessibleValue.
 *
 * Returns: (transfer full): the value, with an additional reference
 */
GtkAccessibleValue *
gtk_accessible_value_ref (GtkAccessibleValue *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count += 1;

  return self;
}

/*< private >
 * gtk_accessible_value_unref:
 * @self: (transfer full): a #GtkAccessibleValue
 *
 * Releases a reference on the given #GtkAccessibleValue.
 */
void
gtk_accessible_value_unref (GtkAccessibleValue *self)
{
  g_return_if_fail (self != NULL);

  self->ref_count -= 1;
  if (self->ref_count == 0)
    {
      if (self->value_class->finalize != NULL)
        self->value_class->finalize (self);

      g_free (self);
    }
}

/*< private >
 * gtk_accessible_value_print:
 * @self: a #GtkAccessibleValue
 * @buffer: a #GString
 *
 * Prints the contents of a #GtkAccessibleValue into the given @buffer.
 */
void
gtk_accessible_value_print (const GtkAccessibleValue *self,
                            GString                  *buffer)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (buffer != NULL);

  if (self->value_class->print != NULL)
    self->value_class->print (self, buffer);
}

/*< private >
 * gtk_accessible_value_to_string:
 * @self: a #GtkAccessibleValue
 *
 * Fills a string with the contents of the given #GtkAccessibleValue.
 *
 * Returns: (transfer full): a string with the contents of the value
 */
char *
gtk_accessible_value_to_string (const GtkAccessibleValue *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  GString *buffer = g_string_new (NULL);

  gtk_accessible_value_print (self, buffer);

  return g_string_free (buffer, FALSE);
}

/*< private >
 * gtk_accessible_value_equal:
 * @value_a: (nullable): the first #GtkAccessibleValue
 * @value_b: (nullable): the second #GtkAccessibleValue
 *
 * Checks whether @value_a and @value_b are equal.
 *
 * This function is %NULL-safe.
 *
 * Returns: %TRUE if the given #GtkAccessibleValue instances are equal,
 *   and %FALSE otherwise
 */
gboolean
gtk_accessible_value_equal (const GtkAccessibleValue *value_a,
                            const GtkAccessibleValue *value_b)
{
  if (value_a == value_b)
    return TRUE;

  if (value_a == NULL || value_b == NULL)
    return FALSE;

  return value_a->value_class->equal (value_a, value_b);
}

typedef enum {
  GTK_ACCESSIBLE_COLLECT_INVALID,
  GTK_ACCESSIBLE_COLLECT_BOOLEAN,
  GTK_ACCESSIBLE_COLLECT_INTEGER,
  GTK_ACCESSIBLE_COLLECT_TRISTATE,
  GTK_ACCESSIBLE_COLLECT_ENUM
} GtkAccessibleCollectType;

typedef GtkAccessibleValue *(* GtkAccessibleValueConstructor) (void);

typedef struct {
  GtkAccessibleState state;
  GtkAccessibleCollectType ctype;
  const char *state_name;
  GCallback ctor;
} GtkAccessibleCollectState;

static const GtkAccessibleCollectState collect_states[] = {
/* | State                         | Collected type                 | Name      | Constructor                      |
 * |-------------------------------|--------------------------------|-----------|----------------------------------|
 */
  { GTK_ACCESSIBLE_STATE_BUSY,     GTK_ACCESSIBLE_COLLECT_BOOLEAN,  "busy",     (GCallback) gtk_busy_accessible_value_new },
  { GTK_ACCESSIBLE_STATE_CHECKED,  GTK_ACCESSIBLE_COLLECT_ENUM,     "checked",  (GCallback) gtk_checked_accessible_value_new },
  { GTK_ACCESSIBLE_STATE_DISABLED, GTK_ACCESSIBLE_COLLECT_BOOLEAN,  "disabled", (GCallback) gtk_disabled_accessible_value_new },
  { GTK_ACCESSIBLE_STATE_EXPANDED, GTK_ACCESSIBLE_COLLECT_TRISTATE, "expanded", (GCallback) gtk_expanded_accessible_value_new },
  { GTK_ACCESSIBLE_STATE_GRABBED,  GTK_ACCESSIBLE_COLLECT_TRISTATE, "grabbed",  (GCallback) gtk_grabbed_accessible_value_new },
  { GTK_ACCESSIBLE_STATE_HIDDEN,   GTK_ACCESSIBLE_COLLECT_BOOLEAN,  "hidden",   (GCallback) gtk_hidden_accessible_value_new },
  { GTK_ACCESSIBLE_STATE_INVALID,  GTK_ACCESSIBLE_COLLECT_ENUM,     "invalid",  (GCallback) gtk_invalid_accessible_value_new },
  { GTK_ACCESSIBLE_STATE_PRESSED,  GTK_ACCESSIBLE_COLLECT_ENUM,     "pressed",  (GCallback) gtk_pressed_accessible_value_new },
  { GTK_ACCESSIBLE_STATE_SELECTED, GTK_ACCESSIBLE_COLLECT_TRISTATE, "selected", (GCallback) gtk_selected_accessible_value_new },
};

typedef GtkAccessibleValue * (* GtkAccessibleValueBooleanCtor)  (gboolean state);
typedef GtkAccessibleValue * (* GtkAccessibleValueIntCtor)      (int state);
typedef GtkAccessibleValue * (* GtkAccessibleValueTristateCtor) (int state);
typedef GtkAccessibleValue * (* GtkAccessibleValueEnumCtor)     (int state);

/*< private >
 * gtk_accessible_value_get_default_for_state:
 * @state: a #GtkAccessibleState
 *
 * Retrieves the #GtkAccessibleValue that contains the default for the
 * given @state.
 *
 * Returns: (transfer full): the #GtkAccessibleValue
 */
GtkAccessibleValue *
gtk_accessible_value_get_default_for_state (GtkAccessibleState state)
{
  g_assert (state != GTK_ACCESSIBLE_STATE_NONE);

  const GtkAccessibleCollectState *cstate = &collect_states[state];

  switch (cstate->state)
    {
    case GTK_ACCESSIBLE_STATE_BUSY:
      return gtk_busy_accessible_value_new (FALSE);

    case GTK_ACCESSIBLE_STATE_CHECKED:
      return gtk_checked_accessible_value_new (GTK_ACCESSIBLE_CHECKED_UNDEFINED);

    case GTK_ACCESSIBLE_STATE_DISABLED:
      return gtk_disabled_accessible_value_new (FALSE);

    case GTK_ACCESSIBLE_STATE_EXPANDED:
      return gtk_expanded_accessible_value_new (GTK_ACCESSIBLE_STATE_UNDEFINED);

    case GTK_ACCESSIBLE_STATE_GRABBED:
      return gtk_grabbed_accessible_value_new (GTK_ACCESSIBLE_STATE_UNDEFINED);

    case GTK_ACCESSIBLE_STATE_HIDDEN:
      return gtk_hidden_accessible_value_new (FALSE);

    case GTK_ACCESSIBLE_STATE_INVALID:
      return gtk_invalid_accessible_value_new (GTK_ACCESSIBLE_INVALID_FALSE);

    case GTK_ACCESSIBLE_STATE_PRESSED:
      return gtk_pressed_accessible_value_new (GTK_ACCESSIBLE_PRESSED_UNDEFINED);

    case GTK_ACCESSIBLE_STATE_SELECTED:
      return gtk_selected_accessible_value_new (GTK_ACCESSIBLE_STATE_UNDEFINED);

    case GTK_ACCESSIBLE_STATE_NONE:
    default:
      g_critical ("Unknown value for accessible state “%s”", cstate->state_name);
      break;
    }

  return NULL;
}

/*< private >
 * gtk_accessible_value_collect_for_state:
 * @state: a #GtkAccessibleState
 * @args: a `va_list` reference
 *
 * Collects and consumes the next item in the @args variadic arguments list,
 * and returns a #GtkAccessibleValue for it.
 *
 * Returns: (transfer full): a #GtkAccessibleValue
 */
GtkAccessibleValue *
gtk_accessible_value_collect_for_state (GtkAccessibleState  state,
                                        va_list            *args)
{
  g_assert (state != GTK_ACCESSIBLE_STATE_NONE);

  const GtkAccessibleCollectState *cstate = &collect_states[state];

  GtkAccessibleValue *res = NULL;

  switch (cstate->ctype)
    {
    case GTK_ACCESSIBLE_COLLECT_BOOLEAN:
      {
        GtkAccessibleValueBooleanCtor ctor =
          (GtkAccessibleValueBooleanCtor) cstate->ctor;

        gboolean value = va_arg (*args, gboolean);

        res = (* ctor) (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_INTEGER:
      {
        GtkAccessibleValueIntCtor ctor =
          (GtkAccessibleValueIntCtor) cstate->ctor;

        int value = va_arg (*args, int);

        res = (* ctor) (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TRISTATE:
      {
        GtkAccessibleValueTristateCtor ctor =
          (GtkAccessibleValueIntCtor) cstate->ctor;

        int value = va_arg (*args, int);

        res = (* ctor) (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_ENUM:
      {
        GtkAccessibleValueEnumCtor ctor =
          (GtkAccessibleValueEnumCtor) cstate->ctor;

        int value = va_arg (*args, int);

        res = (* ctor) (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_INVALID:
    default:
      g_critical ("Unknown type for accessible state “%s”", cstate->state_name);
      break;
    }

  return res;
}

/*< private >
 * gtk_accessible_value_collect_for_state_value:
 * @state: a #GtkAccessibleState
 * @value: a #GValue
 *
 * Retrieves the value stored inside @value and returns a #GtkAccessibleValue
 * for the given @state.
 *
 * Returns: (transfer full): a #GtkAccessibleValue
 */
GtkAccessibleValue *
gtk_accessible_value_collect_for_state_value (GtkAccessibleState  state,
                                              const GValue       *value)
{
  g_assert (state != GTK_ACCESSIBLE_STATE_NONE);

  const GtkAccessibleCollectState *cstate = &collect_states[state];

  GtkAccessibleValue *res = NULL;

  switch (cstate->ctype)
    {
    case GTK_ACCESSIBLE_COLLECT_BOOLEAN:
      {
        GtkAccessibleValueBooleanCtor ctor =
          (GtkAccessibleValueBooleanCtor) cstate->ctor;

        gboolean value_ = g_value_get_boolean (value);

        res = (* ctor) (value_);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_INTEGER:
      {
        GtkAccessibleValueIntCtor ctor =
          (GtkAccessibleValueIntCtor) cstate->ctor;

        int value_ = g_value_get_int (value);

        res = (* ctor) (value_);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TRISTATE:
      {
        GtkAccessibleValueTristateCtor ctor =
          (GtkAccessibleValueIntCtor) cstate->ctor;

        int value_ = g_value_get_int (value);

        res = (* ctor) (value_);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_ENUM:
      {
        GtkAccessibleValueEnumCtor ctor =
          (GtkAccessibleValueEnumCtor) cstate->ctor;

        int value_ = g_value_get_enum (value);

        res = (* ctor) (value_);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_INVALID:
    default:
      g_critical ("Unknown value type for accessible state “%s”", cstate->state_name);
      break;
    }

  return res;
}
