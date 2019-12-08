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

  gboolean sort_increasing;

  GtkExpression *expression;
  GType target_type;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_SORT_INCREASING,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkNumericSorter, gtk_numeric_sorter, GTK_TYPE_SORTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static int
gtk_numeric_sorter_compare (GtkSorter *sorter,
                           gpointer   item1,
                           gpointer   item2)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (sorter);
  GValue value1 = G_VALUE_INIT;
  GValue value2 = G_VALUE_INIT;
  GValue dst = G_VALUE_INIT;
  int result = 0;

  if (self->expression == NULL ||
      !gtk_expression_evaluate (self->expression, item1, &value1) ||
      !gtk_expression_evaluate (self->expression, item2, &value2))
    goto out;

  switch (self->target_type)
    {
    case G_TYPE_INT64:
      {
        gint64 v1, v2;

        g_value_init (&dst, G_TYPE_INT64);
        g_value_transform (&value1, &dst);
        v1 = g_value_get_int64 (&dst);
        g_value_transform (&value2, &dst);
        v2 = g_value_get_int64 (&dst);

        if (v1 > v2)
          result = 1;
        else if (v1 < v2)
          result = -1;
        else
          result = 0;
      }
      break;

    case G_TYPE_UINT64:
      {
        guint64 v1, v2;

        g_value_init (&dst, G_TYPE_UINT64);
        g_value_transform (&value1, &dst);
        v1 = g_value_get_uint64 (&dst);
        g_value_transform (&value2, &dst);
        v2 = g_value_get_uint64 (&dst);

        if (v1 > v2)
          result = 1;
        else if (v1 < v2)
          result = -1;
        else
          result = 0;
      }
      break;

    case G_TYPE_DOUBLE:
      {
        double v1, v2;

        g_value_init (&dst, G_TYPE_DOUBLE);
        g_value_transform (&value1, &dst);
        v1 = g_value_get_double (&dst);
        g_value_transform (&value2, &dst);
        v2 = g_value_get_double (&dst);

        if (v1 > v2)
          result = 1;
        else if (v1 < v2)
          result = -1;
        else
          result = 0;
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

out:
  g_value_unset (&value1);
  g_value_unset (&value2);

  if (!self->sort_increasing)
    result = - result;

  return result;
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

    case PROP_SORT_INCREASING:
      gtk_numeric_sorter_set_sort_increasing (self, g_value_get_boolean (value));
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

    case PROP_SORT_INCREASING:
      g_value_set_boolean (value, self->sort_increasing);
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

  object_class->get_property = gtk_numeric_sorter_get_property;
  object_class->set_property = gtk_numeric_sorter_set_property;
  object_class->dispose = gtk_numeric_sorter_dispose;

  /**
   * GtkNumericSorter:expression:
   *
   * The expression to evalute on item to get a numeric to compare with
   */
  properties[PROP_EXPRESSION] =
      g_param_spec_boxed ("expression",
                          P_("Expression"),
                          P_("Expression to compare with"),
                          GTK_TYPE_EXPRESSION,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkNumericSorter:sort-increasing:
   *
   * Whether the sorter will sort smaller numbers first
   */
  properties[PROP_SORT_INCREASING] =
      g_param_spec_boolean ("sort-increasing",
                            P_("Sort increasing"),
                            P_("Whether to sort smaller numbers first"),
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_numeric_sorter_init (GtkNumericSorter *self)
{
  self->sort_increasing = TRUE;
}

/**
 * gtk_numeric_sorter_new:
 *
 * Creates a new numeric sorter.
 *
 * Until an expression is set on it, this sorter will always
 * compare items as equal.
 *
 * Returns: a new #GtkSorter
 */
GtkSorter *
gtk_numeric_sorter_new (void)
{
  return g_object_new (GTK_TYPE_NUMERIC_SORTER, NULL);
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
 * The expression must have a type that can be transformed to G_TYPE_INT64,
 * G_TYPE_UINT64 or G_TYPE_DOUBLE.
 */
void
gtk_numeric_sorter_set_expression (GtkNumericSorter *self,
                                  GtkExpression   *expression)
{
  g_return_if_fail (GTK_IS_NUMERIC_SORTER (self));
  g_return_if_fail (expression == NULL ||
                    g_value_type_transformable (gtk_expression_get_value_type (expression), G_TYPE_INT64) ||
                    g_value_type_transformable (gtk_expression_get_value_type (expression), G_TYPE_UINT64) ||
                    g_value_type_transformable (gtk_expression_get_value_type (expression), G_TYPE_DOUBLE));

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  self->expression = gtk_expression_ref (expression);

  if (g_value_type_transformable (gtk_expression_get_value_type (expression), G_TYPE_INT64))
    self->target_type = G_TYPE_INT64;
  else if (g_value_type_transformable (gtk_expression_get_value_type (expression), G_TYPE_UINT64))
    self->target_type = G_TYPE_UINT64;
  else 
    self->target_type = G_TYPE_DOUBLE;

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_numeric_sorter_set_sort_increasing:
 * @self: a #GtkNumericSorter
 * @sort_increasing: whether to sort smaller numbers first
 *
 * Sets whether to sort smaller numbers before larger ones.
 */
void
gtk_numeric_sorter_set_sort_increasing (GtkNumericSorter *self,
                                        gboolean          sort_increasing)
{
  g_return_if_fail (GTK_IS_NUMERIC_SORTER (self));

  if (self->sort_increasing == sort_increasing)
    return;

  self->sort_increasing = sort_increasing;

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_INVERTED);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORT_INCREASING]);
}

/**
 * gtk_numeric_sorter_get_sort_increasing:
 * @self: a #GtkNumericSorter
 *
 * Gets whether this sorter will sort smaller numbers first.
 *
 * Returns: %TRUE if smaller numbers are sorted first
 */
gboolean
gtk_numeric_sorter_get_sort_increasing (GtkNumericSorter *self)
{
  g_return_val_if_fail (GTK_IS_NUMERIC_SORTER (self), TRUE);

  return self->sort_increasing;
}
