/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "config.h"

#include "gtkprivate.h"
#include "gtkcssvalueprivate.h"

#include "gtkcssstyleprivate.h"
#include "gtkstyleproviderprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
};

G_DEFINE_BOXED_TYPE (GtkCssValue, _gtk_css_value, _gtk_css_value_ref, _gtk_css_value_unref)

static GHashTable *counters;

static void
dump_value_counts (void)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  int sum1 = 0, sum2 = 0;
  g_hash_table_iter_init (&iter, counters);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
       const char *class = key;
       int *c = value;
       if (c[0] != 1)
         g_print ("%d %d %s\n", c[0], c[0] - c[1], class);

       sum1 += c[0];
       sum2 += c[0] - c[1];
    }

  g_print ("SUM: %d, %d\n", sum1, sum2);
}

static void
count_value (const char *class, int delta)
{
  int *c;

  if (!counters)
    {
      counters = g_hash_table_new (g_str_hash, g_str_equal);
      atexit (dump_value_counts);
    }
  c = g_hash_table_lookup (counters, class);
  if (!c)
    {
       c = g_new0 (int, 2);
       g_hash_table_insert (counters, (gpointer)class, c);
    }

  if (delta == 1)
    c[0]++;
  else
    c[1]++;
}

GtkCssValue *
_gtk_css_value_alloc (const GtkCssValueClass *klass,
                      gsize                   size)
{
  GtkCssValue *value;

  value = g_slice_alloc0 (size);

  value->class = klass;
  value->ref_count = 1;

  count_value (klass->type_name, 1);

  return value;
}

GtkCssValue *
gtk_css_value_ref (GtkCssValue *value)
{
  gtk_internal_return_val_if_fail (value != NULL, NULL);

  value->ref_count += 1;

  return value;
}

void
gtk_css_value_unref (GtkCssValue *value)
{
  if (value == NULL)
    return;

  value->ref_count -= 1;
  if (value->ref_count > 0)
    return;

  count_value (value->class->type_name, -1);

  value->class->free (value);
}

/**
 * _gtk_css_value_compute:
 * @value: the value to compute from
 * @property_id: the ID of the property to compute
 * @provider: Style provider for looking up extra information
 * @style: Style to compute for
 * @parent_style: parent style to use for inherited values
 *
 * Converts the specified @value into the computed value for the CSS
 * property given by @property_id using the information in @context.
 * This step is explained in detail in the
 * [CSS Documentation](http://www.w3.org/TR/css3-cascade/#computed).
 *
 * Returns: the computed value
 **/
GtkCssValue *
_gtk_css_value_compute (GtkCssValue      *value,
                        guint             property_id,
                        GtkStyleProvider *provider,
                        GtkCssStyle      *style,
                        GtkCssStyle      *parent_style)
{
  if (gtk_css_value_is_computed (value))
    return _gtk_css_value_ref (value);

  return value->class->compute (value, property_id, provider, style, parent_style);
}

gboolean
_gtk_css_value_equal (const GtkCssValue *value1,
                      const GtkCssValue *value2)
{
  gtk_internal_return_val_if_fail (value1 != NULL, FALSE);
  gtk_internal_return_val_if_fail (value2 != NULL, FALSE);

  if (value1 == value2)
    return TRUE;

  if (value1->class != value2->class)
    return FALSE;

  return value1->class->equal (value1, value2);
}

gboolean
_gtk_css_value_equal0 (const GtkCssValue *value1,
                       const GtkCssValue *value2)
{
  /* Inclues both values being NULL */
  if (value1 == value2)
    return TRUE;

  if (value1 == NULL || value2 == NULL)
    return FALSE;

  return _gtk_css_value_equal (value1, value2);
}

GtkCssValue *
_gtk_css_value_transition (GtkCssValue *start,
                           GtkCssValue *end,
                           guint        property_id,
                           double       progress)
{
  gtk_internal_return_val_if_fail (start != NULL, FALSE);
  gtk_internal_return_val_if_fail (end != NULL, FALSE);

  /* We compare functions here instead of classes so that number
   * values can all transition to each other */
  if (start->class->transition != end->class->transition)
    return NULL;

  return start->class->transition (start, end, property_id, progress);
}

char *
_gtk_css_value_to_string (const GtkCssValue *value)
{
  GString *string;

  gtk_internal_return_val_if_fail (value != NULL, NULL);

  string = g_string_new (NULL);
  _gtk_css_value_print (value, string);
  return g_string_free (string, FALSE);
}

/**
 * _gtk_css_value_print:
 * @value: the value to print
 * @string: the string to print to
 *
 * Prints @value to the given @string in CSS format. The @value must be a
 * valid specified value as parsed using the parse functions or as assigned
 * via _gtk_style_property_assign().
 **/
void
_gtk_css_value_print (const GtkCssValue *value,
                      GString           *string)
{
  gtk_internal_return_if_fail (value != NULL);
  gtk_internal_return_if_fail (string != NULL);

  value->class->print (value, string);
}

/**
 * gtk_css_value_is_dynamic:
 * @value: a #GtkCssValue
 *
 * A "dynamic" value has a different value at different times. This means that
 * the value needs to be animated when time is progressing.
 *
 * Examples of dynamic values are animated images, such as videos or dynamic shaders.
 *
 * Use gtk_css_value_get_dynamic_value() to get the value for a given timestamp.
 *
 * Returns %TRUE if the value is dynamic
 */
gboolean
gtk_css_value_is_dynamic (GtkCssValue *value)
{
  gtk_internal_return_val_if_fail (value != NULL, FALSE);

  if (!value->class->is_dynamic)
    return FALSE;

  return value->class->is_dynamic (value);
}

/**
 * gtk_css_value_get_dynamic_value:
 * @value: a #GtkCssValue
 * @monotonic_time: the timestamp for which to get the dynamic value
 *
 * Gets the dynamic value for a given timestamp. If @monotonic_time is 0,
 * the default value is returned.
 *
 * See gtk_css_value_is_dynamic() for details about dynamic values.
 *
 * Returns: (transfer full): The dynamic value for @value at the given
 *     timestamp
 **/
GtkCssValue *
gtk_css_value_get_dynamic_value (GtkCssValue *value,
                                 gint64       monotonic_time)
{
  gtk_internal_return_val_if_fail (value != NULL, NULL);

  if (!value->class->get_dynamic_value)
    return gtk_css_value_ref (value);

  return value->class->get_dynamic_value (value, monotonic_time);
}

gboolean
gtk_css_value_is_computed (const GtkCssValue *value)
{
  return value->is_computed;
}
