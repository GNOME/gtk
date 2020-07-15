/* gtkaccessiblevaluestatic.c: GtkAccessibleValue static implementations
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

#include "config.h"

#include "gtkaccessiblevalueprivate.h"

/* {{{ Undefined value */

static void
gtk_undefined_accessible_value_print (const GtkAccessibleValue *value,
                                      GString                  *buffer)
{
  g_string_append (buffer, "undefined");
}

static const GtkAccessibleValueClass GTK_UNDEFINED_ACCESSIBLE_VALUE = {
  .type_name = "GtkUndefinedAccessibleValue",
  .instance_size = sizeof (GtkAccessibleValue),
  .print = gtk_undefined_accessible_value_print,
};

static GtkAccessibleValue undefined_value = {
  &GTK_UNDEFINED_ACCESSIBLE_VALUE, 1
};

GtkAccessibleValue *
gtk_undefined_accessible_value_new (void)
{
  return gtk_accessible_value_ref (&undefined_value);
}

/* }}} */

/* {{{ Boolean values */ 

typedef struct
{
  GtkAccessibleValue parent;

  gboolean value;
} GtkBooleanAccessibleValue;

static gboolean 
gtk_boolean_accessible_value_equal (const GtkAccessibleValue *value_a,
                                    const GtkAccessibleValue *value_b)
{
  const GtkBooleanAccessibleValue *bool_a = (GtkBooleanAccessibleValue *) value_a;
  const GtkBooleanAccessibleValue *bool_b = (GtkBooleanAccessibleValue *) value_b;

  return bool_a->value == bool_b->value;
}

static void
gtk_boolean_accessible_value_print (const GtkAccessibleValue *value,
                                    GString                  *buffer)
{
  const GtkBooleanAccessibleValue *self = (GtkBooleanAccessibleValue *) value;

  g_string_append_printf (buffer, "%s", self->value ? "true" : "false");
}

static const GtkAccessibleValueClass GTK_BOOLEAN_ACCESSIBLE_VALUE = {
  .type_name = "GtkBooleanAccessibleValue",
  .instance_size = sizeof (GtkBooleanAccessibleValue),
  .equal = gtk_boolean_accessible_value_equal,
  .print = gtk_boolean_accessible_value_print,
};

static GtkBooleanAccessibleValue boolean_values[] = {
  { { &GTK_BOOLEAN_ACCESSIBLE_VALUE, 1 }, FALSE },
  { { &GTK_BOOLEAN_ACCESSIBLE_VALUE, 1 }, TRUE },
};

GtkAccessibleValue *
gtk_boolean_accessible_value_new (gboolean state)
{
  if (state)
    return gtk_accessible_value_ref ((GtkAccessibleValue *) &boolean_values[1]);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &boolean_values[0]);
}

gboolean
gtk_boolean_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkBooleanAccessibleValue *self = (GtkBooleanAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (value->value_class == &GTK_BOOLEAN_ACCESSIBLE_VALUE, FALSE);

  return self->value;
}

/* }}} */

/* {{{ Tri-state values */

typedef struct {
  GtkAccessibleValue parent;

  int value;
} GtkTristateAccessibleValue;

static gboolean
gtk_tristate_accessible_value_equal (const GtkAccessibleValue *value_a,
                                     const GtkAccessibleValue *value_b)
{
  const GtkTristateAccessibleValue *tri_a = (GtkTristateAccessibleValue *) value_a;
  const GtkTristateAccessibleValue *tri_b = (GtkTristateAccessibleValue *) value_b;

  return tri_a->value == tri_b->value;
}

static void
gtk_tristate_accessible_value_print (const GtkAccessibleValue *value,
                                     GString                  *buffer)
{
  const GtkTristateAccessibleValue *self = (GtkTristateAccessibleValue *) value;

  switch (self->value)
    {
    case 0:
      g_string_append (buffer, "false");
      break;

    case 1:
      g_string_append (buffer, "true");
      break;

    case -1:
      g_string_append (buffer, "undefined");
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static const GtkAccessibleValueClass GTK_EXPANDED_ACCESSIBLE_VALUE = {
  .type_name = "GtkExpandedAccessibleValue",
  .instance_size = sizeof (GtkTristateAccessibleValue),
  .equal = gtk_tristate_accessible_value_equal,
  .print = gtk_tristate_accessible_value_print,
};

static GtkTristateAccessibleValue expanded_values[] = {
  { { &GTK_EXPANDED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_VALUE_UNDEFINED },
  { { &GTK_EXPANDED_ACCESSIBLE_VALUE, 1 }, 0 },
  { { &GTK_EXPANDED_ACCESSIBLE_VALUE, 1 }, 1 },
};

GtkAccessibleValue *
gtk_expanded_accessible_value_new (int state)
{
  int index_;

  if (state < 0)
    index_ = 0;
  else if (state == 0)
    index_ = 1;
  else
    index_ = 2;

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &expanded_values[index_]);
}

int
gtk_expanded_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkTristateAccessibleValue *self = (GtkTristateAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_VALUE_UNDEFINED);
  g_return_val_if_fail (value->value_class == &GTK_EXPANDED_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_VALUE_UNDEFINED);

  return self->value;
}

static const GtkAccessibleValueClass GTK_GRABBED_ACCESSIBLE_VALUE = {
  .type_name = "GtkGrabbedAccessibleValue",
  .instance_size = sizeof (GtkTristateAccessibleValue),
  .equal = gtk_tristate_accessible_value_equal,
  .print = gtk_tristate_accessible_value_print,
};

static GtkTristateAccessibleValue grabbed_values[] = {
  { { &GTK_GRABBED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_VALUE_UNDEFINED },
  { { &GTK_GRABBED_ACCESSIBLE_VALUE, 1 }, 0 },
  { { &GTK_GRABBED_ACCESSIBLE_VALUE, 1 }, 1 },
};

GtkAccessibleValue *
gtk_grabbed_accessible_value_new (int state)
{
  int index_;

  if (state < 0)
    index_ = 0;
  else if (state == 0)
    index_ = 1;
  else
    index_ = 2;

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &grabbed_values[index_]);
}

int
gtk_grabbed_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkTristateAccessibleValue *self = (GtkTristateAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_VALUE_UNDEFINED);
  g_return_val_if_fail (value->value_class == &GTK_GRABBED_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_VALUE_UNDEFINED);

  return self->value;
}

static const GtkAccessibleValueClass GTK_SELECTED_ACCESSIBLE_VALUE = {
  .type_name = "GtkSelectedAccessibleValue",
  .instance_size = sizeof (GtkTristateAccessibleValue),
  .equal = gtk_tristate_accessible_value_equal,
  .print = gtk_tristate_accessible_value_print,
};

static GtkTristateAccessibleValue selected_values[] = {
  { { &GTK_SELECTED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_VALUE_UNDEFINED },
  { { &GTK_SELECTED_ACCESSIBLE_VALUE, 1 }, 0 },
  { { &GTK_SELECTED_ACCESSIBLE_VALUE, 1 }, 1 },
};

GtkAccessibleValue *
gtk_selected_accessible_value_new (int state)
{
  int index_;

  if (state < 0)
    index_ = 0;
  else if (state == 0)
    index_ = 1;
  else
    index_ = 2;

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &selected_values[index_]);
}

int
gtk_selected_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkTristateAccessibleValue *self = (GtkTristateAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_VALUE_UNDEFINED);
  g_return_val_if_fail (value->value_class == &GTK_SELECTED_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_VALUE_UNDEFINED);

  return self->value;
}

/* }}} */

/* {{{ Enumeration values */

typedef struct {
  GtkAccessibleValue parent;

  int value;
  const char *token;
} GtkEnumAccessibleValue;

static gboolean
gtk_enum_accessible_value_equal (const GtkAccessibleValue *value_a,
                                 const GtkAccessibleValue *value_b)
{
  const GtkEnumAccessibleValue *enum_a = (GtkEnumAccessibleValue *) value_a;
  const GtkEnumAccessibleValue *enum_b = (GtkEnumAccessibleValue *) value_b;

  return enum_a->value == enum_b->value;
}

static void
gtk_enum_accessible_value_print (const GtkAccessibleValue *value,
                                 GString                  *buffer)
{
  const GtkEnumAccessibleValue *self = (GtkEnumAccessibleValue *) value;

  g_string_append (buffer, self->token);
}

static const GtkAccessibleValueClass GTK_CHECKED_ACCESSIBLE_VALUE = {
  .type_name = "GtkCheckedAccessibleValue",
  .instance_size = sizeof (GtkEnumAccessibleValue),
  .equal = gtk_enum_accessible_value_equal,
  .print = gtk_enum_accessible_value_print,
};

static GtkEnumAccessibleValue checked_values[] = {
  { { &GTK_CHECKED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_CHECKED_FALSE,     "false" },
  { { &GTK_CHECKED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_CHECKED_TRUE,      "true" },
  { { &GTK_CHECKED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_PRESSED_UNDEFINED, "undefined" },
  { { &GTK_CHECKED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_CHECKED_MIXED,     "mixed" },
};

GtkAccessibleValue *
gtk_checked_accessible_value_new (GtkAccessibleCheckedState state)
{
  int index_;

  switch (state)
    {
    case GTK_ACCESSIBLE_CHECKED_FALSE:
      index_ = 0;
      break;

    case GTK_ACCESSIBLE_CHECKED_TRUE:
      index_ = 1;
      break;

    case GTK_ACCESSIBLE_CHECKED_UNDEFINED:
      index_ = 2;
      break;

    case GTK_ACCESSIBLE_CHECKED_MIXED:
      index_ = 3;
      break;

    default:
      g_assert_not_reached ();
      return NULL;
    }

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &checked_values[index_]);
}

GtkAccessibleCheckedState
gtk_checked_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkEnumAccessibleValue *self = (GtkEnumAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_CHECKED_UNDEFINED);
  g_return_val_if_fail (value->value_class == &GTK_CHECKED_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_CHECKED_UNDEFINED);

  return self->value;
}

static const GtkAccessibleValueClass GTK_PRESSED_ACCESSIBLE_VALUE = {
  .type_name = "GtkPressedAccessibleValue",
  .instance_size = sizeof (GtkEnumAccessibleValue),
  .equal = gtk_enum_accessible_value_equal,
  .print = gtk_enum_accessible_value_print,
};

static GtkEnumAccessibleValue pressed_values[] = {
  { { &GTK_PRESSED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_PRESSED_FALSE,     "false" },
  { { &GTK_PRESSED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_PRESSED_TRUE,      "true" },
  { { &GTK_PRESSED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_PRESSED_UNDEFINED, "undefined" },
  { { &GTK_PRESSED_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_PRESSED_MIXED,     "mixed" },
};

GtkAccessibleValue *
gtk_pressed_accessible_value_new (GtkAccessiblePressedState state)
{
  int index_;

  switch (state)
    {
    case GTK_ACCESSIBLE_PRESSED_FALSE:
      index_ = 0;
      break;

    case GTK_ACCESSIBLE_PRESSED_TRUE:
      index_ = 1;
      break;

    case GTK_ACCESSIBLE_PRESSED_UNDEFINED:
      index_ = 2;
      break;

    case GTK_ACCESSIBLE_PRESSED_MIXED:
      index_ = 3;
      break;

    default:
      g_assert_not_reached ();
      return NULL;
    }

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &pressed_values[index_]);
}

GtkAccessiblePressedState
gtk_pressed_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkEnumAccessibleValue *self = (GtkEnumAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_PRESSED_UNDEFINED);
  g_return_val_if_fail (value->value_class == &GTK_PRESSED_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_PRESSED_UNDEFINED);

  return self->value;
}

static const GtkAccessibleValueClass GTK_INVALID_ACCESSIBLE_VALUE = {
  .type_name = "GtkInvalidAccessibleValue",
  .instance_size = sizeof (GtkEnumAccessibleValue),
  .equal = gtk_enum_accessible_value_equal,
  .print = gtk_enum_accessible_value_print,
};

static GtkEnumAccessibleValue invalid_values[] = {
  { { &GTK_INVALID_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_INVALID_FALSE,    "false" },
  { { &GTK_INVALID_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_INVALID_TRUE,     "true" },
  { { &GTK_INVALID_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_INVALID_GRAMMAR,  "grammar" },
  { { &GTK_INVALID_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_INVALID_SPELLING, "spelling" },
};

GtkAccessibleValue *
gtk_invalid_accessible_value_new (GtkAccessibleInvalidState state)
{
  g_return_val_if_fail (state >= GTK_ACCESSIBLE_INVALID_FALSE &&
                        state <= GTK_ACCESSIBLE_INVALID_SPELLING,
                        NULL);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &invalid_values[state]);
}

GtkAccessibleInvalidState
gtk_invalid_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkEnumAccessibleValue *self = (GtkEnumAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_INVALID_FALSE);
  g_return_val_if_fail (value->value_class == &GTK_INVALID_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_INVALID_FALSE);

  return self->value;
}

static const GtkAccessibleValueClass GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE = {
  .type_name = "GtkAutocompleteAccessibleValue",
  .instance_size = sizeof (GtkEnumAccessibleValue),
  .equal = gtk_enum_accessible_value_equal,
  .print = gtk_enum_accessible_value_print,
};

static GtkEnumAccessibleValue autocomplete_values[] = {
  { { &GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_AUTOCOMPLETE_NONE,   "none" },
  { { &GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_AUTOCOMPLETE_INLINE, "inline" },
  { { &GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_AUTOCOMPLETE_LIST,   "list" },
  { { &GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_AUTOCOMPLETE_BOTH,   "both" },
};

GtkAccessibleValue *
gtk_autocomplete_accessible_value_new (GtkAccessibleAutocomplete value)
{
  g_return_val_if_fail (value >= GTK_ACCESSIBLE_AUTOCOMPLETE_NONE &&
                        value <= GTK_ACCESSIBLE_AUTOCOMPLETE_BOTH,
                        NULL);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &autocomplete_values[value]);
}

GtkAccessibleAutocomplete
gtk_autocomplete_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkEnumAccessibleValue *self = (GtkEnumAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_AUTOCOMPLETE_NONE);
  g_return_val_if_fail (value->value_class == &GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_AUTOCOMPLETE_NONE);

  return self->value;
}

static const GtkAccessibleValueClass GTK_ORIENTATION_ACCESSIBLE_VALUE = {
  .type_name = "GtkOrientationAccessibleValue",
  .instance_size = sizeof (GtkEnumAccessibleValue),
  .equal = gtk_enum_accessible_value_equal,
  .print = gtk_enum_accessible_value_print,
};

static GtkEnumAccessibleValue orientation_values[] = {
  { { &GTK_ORIENTATION_ACCESSIBLE_VALUE, 1 }, GTK_ORIENTATION_HORIZONTAL,     "horizontal" },
  { { &GTK_ORIENTATION_ACCESSIBLE_VALUE, 1 }, GTK_ORIENTATION_VERTICAL,       "vertical"   },
  { { &GTK_ORIENTATION_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_VALUE_UNDEFINED, "undefined"  },
};

GtkAccessibleValue *
gtk_orientation_accessible_value_new (GtkOrientation value)
{
  switch (value)
    {
    case GTK_ORIENTATION_HORIZONTAL:
      return gtk_accessible_value_ref ((GtkAccessibleValue *) &orientation_values[0]);

    case GTK_ORIENTATION_VERTICAL:
      return gtk_accessible_value_ref ((GtkAccessibleValue *) &orientation_values[1]);

    default:
      g_assert_not_reached ();
      break;
    }

  return NULL;
}

GtkOrientation
gtk_orientation_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkEnumAccessibleValue *self = (GtkEnumAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ORIENTATION_HORIZONTAL);
  g_return_val_if_fail (value->value_class == &GTK_ORIENTATION_ACCESSIBLE_VALUE,
                        GTK_ORIENTATION_HORIZONTAL);

  return self->value;
}

static const GtkAccessibleValueClass GTK_SORT_ACCESSIBLE_VALUE = {
  .type_name = "GtkSortAccessibleValue",
  .instance_size = sizeof (GtkEnumAccessibleValue),
  .equal = gtk_enum_accessible_value_equal,
  .print = gtk_enum_accessible_value_print,
};

static GtkEnumAccessibleValue sort_values[] = {
  { { &GTK_SORT_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_SORT_NONE,       "none" },
  { { &GTK_SORT_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_SORT_ASCENDING,  "ascending" },
  { { &GTK_SORT_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_SORT_DESCENDING, "descending" },
  { { &GTK_SORT_ACCESSIBLE_VALUE, 1 }, GTK_ACCESSIBLE_SORT_OTHER,      "other" },
};

GtkAccessibleValue *
gtk_sort_accessible_value_new (GtkAccessibleSort value)
{
  g_return_val_if_fail (value >= GTK_ACCESSIBLE_SORT_NONE &&
                        value <= GTK_ACCESSIBLE_SORT_OTHER,
                        NULL);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &sort_values[value]);
}

GtkAccessibleSort
gtk_sort_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkEnumAccessibleValue *self = (GtkEnumAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_SORT_NONE);
  g_return_val_if_fail (value->value_class == &GTK_SORT_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_SORT_NONE);

  return self->value;
}

/* }}} */
