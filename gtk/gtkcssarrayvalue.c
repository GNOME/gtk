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

#include "gtkcssarrayvalueprivate.h"

#include <string.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint         n_values;
  GtkCssValue  *values[1];
};

static void
gtk_css_value_array_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->n_values; i++)
    {
      _gtk_css_value_unref (value->values[i]);
    }

  g_slice_free1 (sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (value->n_values - 1), value);
}

static gboolean
gtk_css_value_array_equal (const GtkCssValue *value1,
                           const GtkCssValue *value2)
{
  guint i;

  if (value1->n_values != value2->n_values)
    return FALSE;

  for (i = 0; i < value1->n_values; i++)
    {
      if (!_gtk_css_value_equal (value1->values[i],
                                 value2->values[i]))
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_array_transition (GtkCssValue *start,
                                GtkCssValue *end,
                                double       progress)
{
  return NULL;
}

static void
gtk_css_value_array_print (const GtkCssValue *value,
                           GString           *string)
{
  guint i;

  if (value->n_values == 0)
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->n_values; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      _gtk_css_value_print (value->values[i], string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_ARRAY = {
  gtk_css_value_array_free,
  gtk_css_value_array_equal,
  gtk_css_value_array_transition,
  gtk_css_value_array_print
};

static GtkCssValue none_singleton = { &GTK_CSS_VALUE_ARRAY, 1, 0, { NULL } };

GtkCssValue *
_gtk_css_array_value_new (GtkCssValue *content)
{
  if (content == NULL)
    return _gtk_css_value_ref (&none_singleton);

  return _gtk_css_array_value_new_from_array (&content, 1);
}

GtkCssValue *
_gtk_css_array_value_new_from_array (GtkCssValue **values,
                                     guint         n_values)
{
  GtkCssValue *result;
           
  g_return_val_if_fail (values != NULL, NULL);
  g_return_val_if_fail (n_values > 0, NULL);
         
  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_ARRAY, sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (n_values - 1));
  result->n_values = n_values;
  memcpy (&result->values[0], values, sizeof (GtkCssValue *) * n_values);
            
  return result;
}

GtkCssValue *
_gtk_css_array_value_parse (GtkCssParser *parser,
                            GtkCssValue  *(* parse_func) (GtkCssParser *parser),
                            gboolean      allow_none)
{
  GtkCssValue *value, *result;
  GPtrArray *values;

  if (allow_none &&
      _gtk_css_parser_try (parser, "none", TRUE))
    return _gtk_css_value_ref (&none_singleton);

  values = g_ptr_array_new ();

  do {
    value = parse_func (parser);

    if (value == NULL)
      {
        g_ptr_array_set_free_func (values, (GDestroyNotify) _gtk_css_value_unref);
        g_ptr_array_free (values, TRUE);
        return NULL;
      }

    g_ptr_array_add (values, value);
  } while (_gtk_css_parser_try (parser, ",", TRUE));

  result = _gtk_css_array_value_new_from_array ((GtkCssValue **) values->pdata, values->len);
  g_ptr_array_free (values, TRUE);
  return result;
}

GtkCssValue *
_gtk_css_array_value_compute (GtkCssValue     *value,
                              GtkCssValue *    (* compute_func) (GtkCssValue *, GtkStyleContext *),
                              GtkStyleContext *context)
{
  GtkCssValue *result;
  gboolean changed = FALSE;
  guint i;

  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_ARRAY, NULL);
  g_return_val_if_fail (compute_func != NULL, NULL);

  if (value->n_values == 0)
    return _gtk_css_value_ref (value);

  result = _gtk_css_array_value_new_from_array (value->values, value->n_values);
  for (i = 0; i < value->n_values; i++)
    {
      result->values[i] = (* compute_func) (value->values[i], context);
      changed |= (result->values[i] != value->values[i]);
    }

  if (!changed)
    {
      _gtk_css_value_unref (result);
      return _gtk_css_value_ref (value);
    }

  return result;
}

GtkCssValue *
_gtk_css_array_value_get_nth (const GtkCssValue *value,
                              guint              i)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_ARRAY, NULL);
  g_return_val_if_fail (value->n_values > 0, NULL);

  return value->values[i % value->n_values];
}

guint
_gtk_css_array_value_get_n_values (const GtkCssValue *value)
{
  g_return_val_if_fail (value != NULL, 0);
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_ARRAY, 0);

  return value->n_values;
}

