/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileCopyrightText: 2019 Benjamin Otte <otte@gnome.org>
 * SPDX-FileCopyrightText: 2026 Jamie Gravendeel <me@jamie.garden>
 */

#include "config.h"

#include "gtkenumfilter.h"

#include "gtktypebuiltins.h"

/**
 * GtkEnumFilter:
 *
 * Determines whether to include items by comparing enum values.
 *
 * The values are obtained from the items by evaluating an expression
 * set with [method@Gtk.EnumFilter.set_expression], and they are
 * compared against a value set with [method@Gtk.EnumFilter.set_value].
 *
 * Since: 4.24
 */

struct _GtkEnumFilter
{
  GtkFilter parent_instance;

  long value;
  GType enum_type;

  GtkExpression *expression;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_VALUE,
  PROP_ENUM_TYPE,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkEnumFilter, gtk_enum_filter, GTK_TYPE_FILTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static gboolean
gtk_enum_filter_match (GtkFilter *filter,
                       gpointer   item)
{
  GtkEnumFilter *self = GTK_ENUM_FILTER (filter);
  GValue value = G_VALUE_INIT;
  int enum_value;

  if (self->value == G_MAXLONG)
    return TRUE;

  if (self->expression == NULL || !gtk_expression_evaluate (self->expression, item, &value))
    return FALSE;

  enum_value = g_value_get_enum (&value);
  g_value_unset (&value);

  return enum_value == self->value;
}

static GtkFilterMatch
gtk_enum_filter_get_strictness (GtkFilter *filter)
{
  GtkEnumFilter *self = GTK_ENUM_FILTER (filter);

  if (self->value == G_MAXLONG)
    return GTK_FILTER_MATCH_ALL;

  if (self->expression == NULL)
    return GTK_FILTER_MATCH_NONE;

  return GTK_FILTER_MATCH_SOME;
}

static void
gtk_enum_filter_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkEnumFilter *self = GTK_ENUM_FILTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_enum_filter_set_expression (self, gtk_value_get_expression (value));
      break;

    case PROP_VALUE:
      gtk_enum_filter_set_value (self, g_value_get_long (value));
      break;

    case PROP_ENUM_TYPE:
      g_assert (g_type_is_a (g_value_get_gtype (value), G_TYPE_ENUM));
      self->enum_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_enum_filter_get_property (GObject     *object,
                              guint        prop_id,
                              GValue      *value,
                              GParamSpec  *pspec)
{
  GtkEnumFilter *self = GTK_ENUM_FILTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_value_set_expression (value, self->expression);
      break;

    case PROP_VALUE:
      g_value_set_long (value, self->value);
      break;

    case PROP_ENUM_TYPE:
      g_value_set_gtype (value, self->enum_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_enum_filter_dispose (GObject *object)
{
  GtkEnumFilter *self = GTK_ENUM_FILTER (object);

  g_clear_pointer (&self->expression, gtk_expression_unref);

  G_OBJECT_CLASS (gtk_enum_filter_parent_class)->dispose (object);
}

static void
gtk_enum_filter_class_init (GtkEnumFilterClass *class)
{
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  filter_class->match = gtk_enum_filter_match;
  filter_class->get_strictness = gtk_enum_filter_get_strictness;

  object_class->get_property = gtk_enum_filter_get_property;
  object_class->set_property = gtk_enum_filter_set_property;
  object_class->dispose = gtk_enum_filter_dispose;

  /**
   * GtkEnumFilter:expression: (type GtkExpression)
   *
   * The expression to evaluate on each item to get a
   * value to compare with.
   *
   * Since: 4.24
   */
  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression", NULL, NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEnumFilter:value:
   *
   * The value to compare with, or %G_MAXLONG to match everything.
   *
   * Since: 4.24
   */
   properties[PROP_VALUE] =
     g_param_spec_long ("value", NULL, NULL,
                        -G_MAXLONG, G_MAXLONG,
                        G_MAXLONG,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEnumFilter:enum-type:
   *
   * The enum type [property@Gtk.EnumFilter:expression] evaluates to.
   *
   * Since: 4.24
   */
  properties[PROP_ENUM_TYPE] =
    g_param_spec_gtype ("enum-type", NULL, NULL,
                        G_TYPE_ENUM,
                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_enum_filter_init (GtkEnumFilter *self)
{
  self->value = G_MAXLONG;
}

/**
 * gtk_enum_filter_new:
 * @enum_type: the enum type `expression` evaluates to
 * @expression: (transfer full) (nullable): the expression to evaluate
 *
 * Creates a new enum filter.
 *
 * You will want to set up the filter by providing a value to compare with
 * and by providing a property to look up on the item.
 *
 * Returns: a new `GtkEnumFilter`
 *
 * Since: 4.24
 */
GtkEnumFilter *
gtk_enum_filter_new (GType          enum_type,
                     GtkExpression *expression)
{
  GtkEnumFilter *result;

  result = g_object_new (GTK_TYPE_ENUM_FILTER,
                         "enum-type", enum_type,
                         "expression", expression,
                         NULL);

  g_clear_pointer (&expression, gtk_expression_unref);

  return result;
}

/**
 * gtk_enum_filter_get_expression:
 * @self: an enum filter
 *
 * Gets the expression that the enum filter uses to
 * obtain values from items.
 *
 * Returns: (transfer none) (nullable): the expression
 *
 * Since: 4.24
 */
GtkExpression *
gtk_enum_filter_get_expression (GtkEnumFilter *self)
{
  g_return_val_if_fail (GTK_IS_ENUM_FILTER (self), NULL);

  return self->expression;
}

/**
 * gtk_enum_filter_set_expression:
 * @self: an enum filter
 * @expression: (nullable): the expression
 *
 * Sets the expression that the enum filter uses to
 * obtain values from items.
 *
 * The expression must have the value type %G_TYPE_ENUM.
 *
 * Since: 4.24
 */
void
gtk_enum_filter_set_expression (GtkEnumFilter *self,
                                GtkExpression *expression)
{
  g_return_if_fail (GTK_IS_ENUM_FILTER (self));
  g_return_if_fail (expression == NULL ||
                    g_type_is_a (gtk_expression_get_value_type (expression), G_TYPE_ENUM));

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  self->expression = gtk_expression_ref (expression);

  gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_DIFFERENT_REWATCH);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_enum_filter_get_value:
 * @self: an enum filter
 *
 * Gets the value to compare with.
 *
 * Returns: the value to compare with
 *
 * Since: 4.24
 */
long
gtk_enum_filter_get_value (GtkEnumFilter *self)
{
  g_return_val_if_fail (GTK_IS_ENUM_FILTER (self), G_MAXLONG);

  return self->value;
}

/**
 * gtk_enum_filter_set_value:
 * @self: an enum filter
 * @value: the value to compare with, or %G_MAXLONG
 *
 * Sets the value to compare with, or %G_MAXLONG to match all.
 *
 * Since: 4.24
 */
void
gtk_enum_filter_set_value (GtkEnumFilter *self,
                           long           value)
{
  GtkFilterChange change;

  g_return_if_fail (GTK_IS_ENUM_FILTER (self));

  if (value == self->value)
    return;

  if (value == G_MAXLONG)
    change = GTK_FILTER_CHANGE_LESS_STRICT;
  else if (self->value == G_MAXLONG)
    change = GTK_FILTER_CHANGE_MORE_STRICT;
  else
    change = GTK_FILTER_CHANGE_DIFFERENT;

  self->value = value;

  gtk_filter_changed (GTK_FILTER (self), change);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
}
