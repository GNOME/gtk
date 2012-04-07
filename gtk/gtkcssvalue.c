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

#include "gtkcssvalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
};

G_DEFINE_BOXED_TYPE (GtkCssValue, _gtk_css_value, _gtk_css_value_ref, _gtk_css_value_unref)

GtkCssValue *
_gtk_css_value_alloc (const GtkCssValueClass *klass,
                      gsize                   size)
{
  GtkCssValue *value;

  value = g_slice_alloc0 (size);

  value->class = klass;
  value->ref_count = 1;

  return value;
}

GtkCssValue *
_gtk_css_value_ref (GtkCssValue *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  g_atomic_int_add (&value->ref_count, 1);

  return value;
}

void
_gtk_css_value_unref (GtkCssValue *value)
{
  if (value == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&value->ref_count))
    return;

  value->class->free (value);
}

gboolean
_gtk_css_value_equal (const GtkCssValue *value1,
                      const GtkCssValue *value2)
{
  g_return_val_if_fail (value1 != NULL, FALSE);
  g_return_val_if_fail (value2 != NULL, FALSE);

  if (value1->class != value2->class)
    return FALSE;

  return value1->class->equal (value1, value2);
}

gboolean
_gtk_css_value_equal0 (const GtkCssValue *value1,
                       const GtkCssValue *value2)
{
  if (value1 == NULL && value2 == NULL)
    return TRUE;

  if (value1 == NULL || value2 == NULL)
    return FALSE;

  return _gtk_css_value_equal (value1, value2);
}

GtkCssValue *
_gtk_css_value_transition (GtkCssValue *start,
                           GtkCssValue *end,
                           double       progress)
{
  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (end != NULL, FALSE);

  if (start->class != end->class)
    return NULL;

  return start->class->transition (start, end, progress);
}

char *
_gtk_css_value_to_string (const GtkCssValue *value)
{
  GString *string;

  g_return_val_if_fail (value != NULL, NULL);

  string = g_string_new (NULL);
  _gtk_css_value_print (value, string);
  return g_string_free (string, FALSE);
}

void
_gtk_css_value_print (const GtkCssValue *value,
                      GString           *string)
{
  g_return_if_fail (value != NULL);
  g_return_if_fail (string != NULL);

  value->class->print (value, string);
}

