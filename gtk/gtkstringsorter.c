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

#include "gtkstringsorter.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

/**
 * SECTION:gtkstringsorter
 * @Title: GtkStringSorter
 * @Short_description: Sort by comparing strings
 * @See_also: #GtkExpression
 *
 * GtkStringSorter is a #GtkSorter that compares strings. It does the
 * comparison in a linguistically correct way using the current locale by
 * normalizing Unicode strings and possibly case-folding them before
 * performing the comparison.
 *
 * To obtain the strings to compare, this sorter evaluates a #GtkExpression.
 */

struct _GtkStringSorter
{
  GtkSorter parent_instance;

  gboolean ignore_case;

  GtkExpression *expression;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_IGNORE_CASE,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringSorter, gtk_string_sorter, GTK_TYPE_SORTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GtkOrdering
gtk_string_sorter_compare (GtkSorter *sorter,
                           gpointer   item1,
                           gpointer   item2)
{
  GtkStringSorter *self = GTK_STRING_SORTER (sorter);
  GValue value1 = G_VALUE_INIT;
  GValue value2 = G_VALUE_INIT;
  const char *s1, *s2;
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

  s1 = g_value_get_string (&value1);
  s2 = g_value_get_string (&value2);

  /* If strings are NULL, order them before "". */
  if (s1 == NULL)
    {
      result = s2 == NULL ? GTK_ORDERING_EQUAL : GTK_ORDERING_SMALLER;
      goto out;
    }
  else if (s2 == NULL)
    {
      result = GTK_ORDERING_LARGER;
      goto out;
    }

  if (self->ignore_case)
    {
      char *t1, *t2;

      t1 = g_utf8_casefold (s1, -1);
      t2 = g_utf8_casefold (s2, -1);

      result = gtk_ordering_from_cmpfunc (g_utf8_collate (t1, t2));

      g_free (t1);
      g_free (t2);
    }
  else
    result = gtk_ordering_from_cmpfunc (g_utf8_collate (s1, s2));

out:
  g_value_unset (&value1);
  g_value_unset (&value2);

  return result;
}

static GtkSorterOrder
gtk_string_sorter_get_order (GtkSorter *sorter)
{
  GtkStringSorter *self = GTK_STRING_SORTER (sorter);

  if (self->expression == NULL)
    return GTK_SORTER_ORDER_NONE;

  return GTK_SORTER_ORDER_PARTIAL;
}

static void
gtk_string_sorter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_string_sorter_set_expression (self, g_value_get_boxed (value));
      break;

    case PROP_IGNORE_CASE:
      gtk_string_sorter_set_ignore_case (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_string_sorter_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      g_value_set_boxed (value, self->expression);
      break;

    case PROP_IGNORE_CASE:
      g_value_set_boolean (value, self->ignore_case);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_string_sorter_dispose (GObject *object)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  g_clear_pointer (&self->expression, gtk_expression_unref);

  G_OBJECT_CLASS (gtk_string_sorter_parent_class)->dispose (object);
}

static void
gtk_string_sorter_class_init (GtkStringSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_string_sorter_compare;
  sorter_class->get_order = gtk_string_sorter_get_order;

  object_class->get_property = gtk_string_sorter_get_property;
  object_class->set_property = gtk_string_sorter_set_property;
  object_class->dispose = gtk_string_sorter_dispose;

  /**
   * GtkStringSorter:expression:
   *
   * The expression to evalute on item to get a string to compare with
   */
  properties[PROP_EXPRESSION] =
      g_param_spec_boxed ("expression",
                          P_("Expression"),
                          P_("Expression to compare with"),
                          GTK_TYPE_EXPRESSION,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringSorter:ignore-case:
   *
   * If matching is case sensitive
   */
  properties[PROP_IGNORE_CASE] =
      g_param_spec_boolean ("ignore-case",
                            P_("Ignore case"),
                            P_("If matching is case sensitive"),
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_string_sorter_init (GtkStringSorter *self)
{
  self->ignore_case = TRUE;
}

/**
 * gtk_string_sorter_new:
 * @expression: (transfer full) (optional): The expression to evaluate
 *
 * Creates a new string sorter that compares items using the given
 * @expression.
 *
 * Unless an expression is set on it, this sorter will always
 * compare items as invalid.
 *
 * Returns: a new #GtkSorter
 */
GtkSorter *
gtk_string_sorter_new (GtkExpression *expression)
{
  GtkSorter *result;

  result = g_object_new (GTK_TYPE_STRING_SORTER,
                         "expression", expression,
                         NULL);

  g_clear_pointer (&expression, gtk_expression_unref);

  return result;
}

/**
 * gtk_string_sorter_get_expression:
 * @self: a #GtkStringSorter
 *
 * Gets the expression that is evaluated to obtain strings from items.
 *
 * Returns: (nullable): a #GtkExpression, or %NULL
 */
GtkExpression *
gtk_string_sorter_get_expression (GtkStringSorter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_SORTER (self), NULL);

  return self->expression;
}

/**
 * gtk_string_sorter_set_expression:
 * @self: a #GtkStringSorter
 * @expression: (nullable) (transfer none): a #GtkExpression, or %NULL
 *
 * Sets the expression that is evaluated to obtain strings from items.
 *
 * The expression must have the type G_TYPE_STRING.
 */
void
gtk_string_sorter_set_expression (GtkStringSorter *self,
                                  GtkExpression   *expression)
{
  g_return_if_fail (GTK_IS_STRING_SORTER (self));
  g_return_if_fail (expression == NULL || gtk_expression_get_value_type (expression) == G_TYPE_STRING);

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  if (expression)
    self->expression = gtk_expression_ref (expression);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_string_sorter_get_ignore_case:
 * @self: a #GtkStringSorter
 *
 * Gets whether the sorter ignores case differences.
 *
 * Returns: %TRUE if @self is ignoring case differences
 */
gboolean
gtk_string_sorter_get_ignore_case (GtkStringSorter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_SORTER (self), TRUE);

  return self->ignore_case;
}

/**
 * gtk_string_sorter_set_ignore_case:
 * @self: a #GtkStringSorter
 *
 * Sets whether the sorter will ignore case differences.
 */
void
gtk_string_sorter_set_ignore_case (GtkStringSorter *self,
                                   gboolean         ignore_case)
{
  g_return_if_fail (GTK_IS_STRING_SORTER (self));

  if (self->ignore_case == ignore_case)
    return;

  self->ignore_case = ignore_case;

  gtk_sorter_changed (GTK_SORTER (self), ignore_case ? GTK_SORTER_CHANGE_LESS_STRICT : GTK_SORTER_CHANGE_MORE_STRICT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IGNORE_CASE]);
}
