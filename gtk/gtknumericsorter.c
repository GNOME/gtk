/*
 * Copyright © 2019 Matthias Clasen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtknumericsorter.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

#include <math.h>
#include "fallback-c89.c"

/**
 * SECTION:gtknumericsorter
 * @Title: GtkNumericSorter
 * @Short_description: Sort by comparing numbers 
 * @See_also: #GtkExpression
 *
 * GtkNumericSorter is a #GtkSorter that compares numbers.
 *
 * To obtain the numbers to compare, this sorter evaluates a #GtkExpression.
 */

struct _GtkNumericSorter
{
  GtkSorter parent_instance;

  GtkSortType sort_order;

  GtkExpression *expression;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_SORT_ORDER,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkNumericSorter, gtk_numeric_sorter, GTK_TYPE_SORTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

#define DO_COMPARISON(_result, _type, _getter, _order) G_STMT_START{ \
  _type num1 = _getter (&value1); \
  _type num2 = _getter (&value2); \
\
  if (num1 < num2) \
    _result = _order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER; \
  else if (num1 > num2) \
    _result = _order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER; \
  else \
    _result = GTK_ORDERING_EQUAL; \
}G_STMT_END

static GtkOrdering
gtk_numeric_sorter_compare (GtkSorter *sorter,
                           gpointer   item1,
                           gpointer   item2)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (sorter);
  GValue value1 = G_VALUE_INIT;
  GValue value2 = G_VALUE_INIT;
  gboolean res1, res2;
  GtkOrdering result;

  if (self->expression == NULL)
    return GTK_ORDERING_EQUAL;

  res1 = gtk_expression_evaluate (self->expression, item1, &value1);
  res2 = gtk_expression_evaluate (self->expression, item2, &value2);

  /* If items don't evaluate, order them at the end, so they aren't
   * in the way. */
  if (!res1)
    {
      result = res2 ? GTK_ORDERING_LARGER : GTK_ORDERING_EQUAL;
      goto out;
    }
  else if (!res2)
    {
      result = GTK_ORDERING_SMALLER;
      goto out;
    }

  switch (g_type_fundamental (G_VALUE_TYPE (&value1)))
    {
    case G_TYPE_BOOLEAN:
      DO_COMPARISON (result, gboolean, g_value_get_boolean, self->sort_order);
      break;

    case G_TYPE_CHAR:
      DO_COMPARISON (result, char, g_value_get_char, self->sort_order);
      break;

    case G_TYPE_UCHAR:
      DO_COMPARISON (result, guchar, g_value_get_uchar, self->sort_order);
      break;

    case G_TYPE_INT:
      DO_COMPARISON (result, int, g_value_get_int, self->sort_order);
      break;

    case G_TYPE_UINT:
      DO_COMPARISON (result, guint, g_value_get_uint, self->sort_order);
      break;

    case G_TYPE_FLOAT:
      {
        float num1 = g_value_get_float (&value1);
        float num2 = g_value_get_float (&value2);

        if (isnanf (num1) && isnanf (num2))
          result = GTK_ORDERING_EQUAL;
        else if (isnanf (num1))
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER;
        else if (isnanf (num2))
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
        else if (num1 < num2)
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
        else if (num1 > num2)
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER;
        else
          result = GTK_ORDERING_EQUAL;
        break;
      }

    case G_TYPE_DOUBLE:
      {
        double num1 = g_value_get_double (&value1);
        double num2 = g_value_get_double (&value2);

        if (isnan (num1) && isnan (num2))
          result = GTK_ORDERING_EQUAL;
        else if (isnan (num1))
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER;
        else if (isnan (num2))
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
        else if (num1 < num2)
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
        else if (num1 > num2)
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER;
        else
          result = GTK_ORDERING_EQUAL;
        break;
      }

    case G_TYPE_LONG:
      DO_COMPARISON (result, long, g_value_get_long, self->sort_order);
      break;

    case G_TYPE_ULONG:
      DO_COMPARISON (result, gulong, g_value_get_ulong, self->sort_order);
      break;

    case G_TYPE_INT64:
      DO_COMPARISON (result, gint64, g_value_get_int64, self->sort_order);
      break;

    case G_TYPE_UINT64:
      DO_COMPARISON (result, guint64, g_value_get_uint64, self->sort_order);
      break;

    default:
      g_critical ("Invalid value type %s for expression\n", g_type_name (gtk_expression_get_value_type (self->expression)));
      result = GTK_ORDERING_EQUAL;
      break;
    }

out:
  g_value_unset (&value1);
  g_value_unset (&value2);

  return result;
}

static GtkSorterOrder
gtk_numeric_sorter_get_order (GtkSorter *sorter)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (sorter);

  if (self->expression == NULL)
    return GTK_SORTER_ORDER_NONE;

  return GTK_SORTER_ORDER_PARTIAL;
}

static void
gtk_numeric_sorter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_numeric_sorter_set_expression (self, g_value_get_boxed (value));
      break;

    case PROP_SORT_ORDER:
      gtk_numeric_sorter_set_sort_order (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_numeric_sorter_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      g_value_set_boxed (value, self->expression);
      break;

    case PROP_SORT_ORDER:
      g_value_set_enum (value, self->sort_order);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_numeric_sorter_dispose (GObject *object)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (object);

  g_clear_pointer (&self->expression, gtk_expression_unref);

  G_OBJECT_CLASS (gtk_numeric_sorter_parent_class)->dispose (object);
}

static void
gtk_numeric_sorter_class_init (GtkNumericSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_numeric_sorter_compare;
  sorter_class->get_order = gtk_numeric_sorter_get_order;

  object_class->get_property = gtk_numeric_sorter_get_property;
  object_class->set_property = gtk_numeric_sorter_set_property;
  object_class->dispose = gtk_numeric_sorter_dispose;

  /**
   * GtkNumericSorter:expression:
   *
   * The expression to evalute on items to get a number to compare with
   */
  properties[PROP_EXPRESSION] =
      g_param_spec_boxed ("expression",
                          P_("Expression"),
                          P_("Expression to compare with"),
                          GTK_TYPE_EXPRESSION,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkNumericSorter:sort-order:
   *
   * Whether the sorter will sort smaller numbers first
   */
  properties[PROP_SORT_ORDER] =
      g_param_spec_enum ("sort-order",
                         P_("Sort order"),
                         P_("Whether to sort smaller numbers first"),
                         GTK_TYPE_SORT_TYPE,
                         GTK_SORT_ASCENDING,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_numeric_sorter_init (GtkNumericSorter *self)
{
  self->sort_order = GTK_SORT_ASCENDING;
}

/**
 * gtk_numeric_sorter_new:
 * @expression: (transfer full) (nullable): The expression to evaluate
 *
 * Creates a new numeric sorter using the given @expression.
 *
 * Smaller numbers will be sorted first. You can call
 * gtk_numeric_sorter_set_sort_order() to change this.
 *
 * Returns: a new #GtkSorter
 */
GtkSorter *
gtk_numeric_sorter_new (GtkExpression *expression)
{
  GtkSorter *result;

  result = g_object_new (GTK_TYPE_NUMERIC_SORTER,
                         "expression", expression,
                         NULL);

  g_clear_pointer (&expression, gtk_expression_unref);

  return result;
}

/**
 * gtk_numeric_sorter_get_expression:
 * @self: a #GtkNumericSorter
 *
 * Gets the expression that is evaluated to obtain numbers from items.
 *
 * Returns: (nullable): a #GtkExpression, or %NULL
 */
GtkExpression *
gtk_numeric_sorter_get_expression (GtkNumericSorter *self)
{
  g_return_val_if_fail (GTK_IS_NUMERIC_SORTER (self), NULL);

  return self->expression;
}

/**
 * gtk_numeric_sorter_set_expression:
 * @self: a #GtkNumericSorter
 * @expression: (nullable) (transfer none): a #GtkExpression, or %NULL
 *
 * Sets the expression that is evaluated to obtain numbers from items.
 *
 * Unless an expression is set on @self, the sorter will always
 * compare items as invalid.
 *
 * The expression must have a return type that can be compared
 * numerically, such as #G_TYPE_INT or #G_TYPE_DOUBLE.
 */
void
gtk_numeric_sorter_set_expression (GtkNumericSorter *self,
                                  GtkExpression   *expression)
{
  g_return_if_fail (GTK_IS_NUMERIC_SORTER (self));

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  if (expression)
    self->expression = gtk_expression_ref (expression);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_numeric_sorter_set_sort_order:
 * @self: a #GtkNumericSorter
 * @sort_order: whether to sort smaller numbers first
 *
 * Sets whether to sort smaller numbers before larger ones.
 */
void
gtk_numeric_sorter_set_sort_order (GtkNumericSorter *self,
                                   GtkSortType       sort_order)
{
  g_return_if_fail (GTK_IS_NUMERIC_SORTER (self));

  if (self->sort_order == sort_order)
    return;

  self->sort_order = sort_order;

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_INVERTED);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORT_ORDER]);
}

/**
 * gtk_numeric_sorter_get_sort_order:
 * @self: a #GtkNumericSorter
 *
 * Gets whether this sorter will sort smaller numbers first.
 *
 * Returns: the order of the numbers
 */
GtkSortType
gtk_numeric_sorter_get_sort_order (GtkNumericSorter *self)
{
  g_return_val_if_fail (GTK_IS_NUMERIC_SORTER (self), GTK_SORT_ASCENDING);

  return self->sort_order;
}
