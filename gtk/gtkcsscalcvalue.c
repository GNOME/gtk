/* GTK - The GIMP Toolkit
 * Copyright © 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtkcsscalcvalueprivate.h"

#include <string.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  gsize                 n_terms;
  GtkCssValue *         terms[1];
};

static gsize
gtk_css_value_calc_get_size (gsize n_terms)
{
  g_assert (n_terms > 0);

  return sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (n_terms - 1);
}
static void
gtk_css_value_calc_free (GtkCssValue *value)
{
  gsize i;

  for (i = 0; i < value->n_terms; i++)
    {
      _gtk_css_value_unref (value->terms[i]);
    }

  g_slice_free1 (gtk_css_value_calc_get_size (value->n_terms), value);
}

static GtkCssValue *gtk_css_calc_value_new (gsize n_terms);

static GtkCssValue *
gtk_css_value_new_from_array (GPtrArray *array)
{
  GtkCssValue *result;
  
  if (array->len > 1)
    {
      result = gtk_css_calc_value_new (array->len);
      memcpy (result->terms, array->pdata, array->len * sizeof (GtkCssValue *));
    }
  else
    {
      result = g_ptr_array_index (array, 0);
    }

  g_ptr_array_free (array, TRUE);

  return result;
}

static void
gtk_css_calc_array_add (GPtrArray *array, GtkCssValue *value)
{
  gsize i;
  gint calc_term_order;

  calc_term_order = gtk_css_number_value_get_calc_term_order (value);

  for (i = 0; i < array->len; i++)
    {
      GtkCssValue *sum = gtk_css_number_value_try_add (g_ptr_array_index (array, i), value);

      if (sum)
        {
          g_ptr_array_index (array, i) = sum;
          _gtk_css_value_unref (value);
          return;
        }
      else if (gtk_css_number_value_get_calc_term_order (g_ptr_array_index (array, i)) > calc_term_order)
        {
          g_ptr_array_insert (array, i, value);
          return;
        }
    }

  g_ptr_array_add (array, value);
}

static GtkCssValue *
gtk_css_value_calc_compute (GtkCssValue             *value,
                            guint                    property_id,
                            GtkStyleProviderPrivate *provider,
                            GtkCssStyle             *style,
                            GtkCssStyle             *parent_style)
{
  GtkCssValue *result;
  GPtrArray *array;
  gboolean changed = FALSE;
  gsize i;

  array = g_ptr_array_new ();
  for (i = 0; i < value->n_terms; i++)
    {
      GtkCssValue *computed = _gtk_css_value_compute (value->terms[i], property_id, provider, style, parent_style);
      changed |= computed != value->terms[i];
      gtk_css_calc_array_add (array, computed);
    }

  if (changed)
    {
      result = gtk_css_value_new_from_array (array);
    }
  else
    {
      g_ptr_array_set_free_func (array, (GDestroyNotify) _gtk_css_value_unref);
      g_ptr_array_free (array, TRUE);
      result = _gtk_css_value_ref (value);
    }

  return result;
}


static gboolean
gtk_css_value_calc_equal (const GtkCssValue *value1,
                          const GtkCssValue *value2)
{
  gsize i;

  if (value1->n_terms != value2->n_terms)
    return FALSE;

  for (i = 0; i < value1->n_terms; i++)
    {
      if (!_gtk_css_value_equal (value1->terms[i], value2->terms[i]))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_css_value_calc_print (const GtkCssValue *value,
                          GString           *string)
{
  gsize i;

  g_string_append (string, "calc(");
  _gtk_css_value_print (value->terms[0], string);

  for (i = 1; i < value->n_terms; i++)
    {
      g_string_append (string, " + ");
      _gtk_css_value_print (value->terms[i], string);
    }
  g_string_append (string, ")");
}

static double
gtk_css_value_calc_get (const GtkCssValue *value,
                        double             one_hundred_percent)
{
  double result = 0.0;
  gsize i;

  for (i = 0; i < value->n_terms; i++)
    {
      result += _gtk_css_number_value_get (value->terms[i], one_hundred_percent);
    }

  return result;
}

static GtkCssDimension
gtk_css_value_calc_get_dimension (const GtkCssValue *value)
{
  GtkCssDimension dimension = GTK_CSS_DIMENSION_PERCENTAGE;
  gsize i;

  for (i = 0; i < value->n_terms && dimension == GTK_CSS_DIMENSION_PERCENTAGE; i++)
    {
      dimension = gtk_css_number_value_get_dimension (value->terms[i]);
    }

  return dimension;
}

static gboolean
gtk_css_value_calc_has_percent (const GtkCssValue *value)
{
  gsize i;

  for (i = 0; i < value->n_terms; i++)
    {
      if (gtk_css_number_value_has_percent (value->terms[i]))
        return TRUE;
    }

  return FALSE;
}

static GtkCssValue *
gtk_css_value_calc_multiply (const GtkCssValue *value,
                             double             factor)
{
  GtkCssValue *result;
  gsize i;

  result = gtk_css_calc_value_new (value->n_terms);

  for (i = 0; i < value->n_terms; i++)
    {
      result->terms[i] = gtk_css_number_value_multiply (value->terms[i], factor);
    }

  return result;
}

static GtkCssValue *
gtk_css_value_calc_try_add (const GtkCssValue *value1,
                            const GtkCssValue *value2)
{
  return NULL;
}

static gint
gtk_css_value_calc_get_calc_term_order (const GtkCssValue *value)
{
  /* This should never be needed because calc() can't contain calc(),
   * but eh...
   */
  return 0;
}

static const GtkCssNumberValueClass GTK_CSS_VALUE_CALC = {
  {
    gtk_css_value_calc_free,
    gtk_css_value_calc_compute,
    gtk_css_value_calc_equal,
    gtk_css_number_value_transition,
    gtk_css_value_calc_print
  },
  gtk_css_value_calc_get,
  gtk_css_value_calc_get_dimension,
  gtk_css_value_calc_has_percent,
  gtk_css_value_calc_multiply,
  gtk_css_value_calc_try_add,
  gtk_css_value_calc_get_calc_term_order
};

static GtkCssValue *
gtk_css_calc_value_new (gsize n_terms)
{
  GtkCssValue *result;

  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_CALC.value_class,
                                 gtk_css_value_calc_get_size (n_terms));
  result->n_terms = n_terms;

  return result;
}

GtkCssValue *
gtk_css_calc_value_new_sum (GtkCssValue *value1,
                            GtkCssValue *value2)
{
  GPtrArray *array;
  gsize i;

  array = g_ptr_array_new ();

  if (value1->class == &GTK_CSS_VALUE_CALC.value_class)
    {
      for (i = 0; i < value1->n_terms; i++)
        {
          gtk_css_calc_array_add (array, _gtk_css_value_ref (value1->terms[i]));
        }
    }
  else
    {
      gtk_css_calc_array_add (array, _gtk_css_value_ref (value1));
    }

  if (value2->class == &GTK_CSS_VALUE_CALC.value_class)
    {
      for (i = 0; i < value2->n_terms; i++)
        {
          gtk_css_calc_array_add (array, _gtk_css_value_ref (value2->terms[i]));
        }
    }
  else
    {
      gtk_css_calc_array_add (array, _gtk_css_value_ref (value2));
    }

  return gtk_css_value_new_from_array (array);
}

GtkCssValue *   gtk_css_calc_value_parse_sum (GtkCssParser           *parser,
                                              GtkCssNumberParseFlags  flags);

GtkCssValue *
gtk_css_calc_value_parse_value (GtkCssParser           *parser,
                                GtkCssNumberParseFlags  flags)
{
  if (_gtk_css_parser_has_prefix (parser, "calc"))
    {
      _gtk_css_parser_error (parser, "Nested calc() expressions are not allowed.");
      return NULL;
    }

  if (_gtk_css_parser_try (parser, "(", TRUE))
    {
      GtkCssValue *result = gtk_css_calc_value_parse_sum (parser, flags);
      if (result == NULL)
        return NULL;

      if (!_gtk_css_parser_try (parser, ")", TRUE))
        {
          _gtk_css_parser_error (parser, "Missing closing ')' in calc() subterm");
          _gtk_css_value_unref (result);
          return NULL;
        }

      return result;
    }

  return _gtk_css_number_value_parse (parser, flags);
}

static gboolean
is_number (GtkCssValue *value)
{
  return gtk_css_number_value_get_dimension (value) == GTK_CSS_DIMENSION_NUMBER
      && !gtk_css_number_value_has_percent (value);
}

GtkCssValue *
gtk_css_calc_value_parse_product (GtkCssParser           *parser,
                                  GtkCssNumberParseFlags  flags)
{
  GtkCssValue *result, *value, *temp;
  GtkCssNumberParseFlags actual_flags;

  actual_flags = flags | GTK_CSS_PARSE_NUMBER;

  result = gtk_css_calc_value_parse_value (parser, actual_flags);
  if (result == NULL)
    return NULL;

  while (_gtk_css_parser_begins_with (parser, '*') || _gtk_css_parser_begins_with (parser, '/'))
    {
      if (actual_flags != GTK_CSS_PARSE_NUMBER && !is_number (result))
        actual_flags = GTK_CSS_PARSE_NUMBER;

      if (_gtk_css_parser_try (parser, "*", TRUE))
        {
          value = gtk_css_calc_value_parse_product (parser, actual_flags);
          if (value == NULL)
            goto fail;
          if (is_number (value))
            temp = gtk_css_number_value_multiply (result, _gtk_css_number_value_get (value, 100));
          else
            temp = gtk_css_number_value_multiply (value, _gtk_css_number_value_get (result, 100));
          _gtk_css_value_unref (value);
          _gtk_css_value_unref (result);
          result = temp;
        }
      else if (_gtk_css_parser_try (parser, "/", TRUE))
        {
          value = gtk_css_calc_value_parse_product (parser, GTK_CSS_PARSE_NUMBER);
          if (value == NULL)
            goto fail;
          temp = gtk_css_number_value_multiply (result, 1.0 / _gtk_css_number_value_get (value, 100));
          _gtk_css_value_unref (value);
          _gtk_css_value_unref (result);
          result = temp;
        }
      else
        {
          g_assert_not_reached ();
          goto fail;
        }
    }

  if (is_number (result) && !(flags & GTK_CSS_PARSE_NUMBER))
    {
      _gtk_css_parser_error (parser, "calc() product term has no units");
      goto fail;
    }

  return result;

fail:
  _gtk_css_value_unref (result);
  return NULL;
}

GtkCssValue *
gtk_css_calc_value_parse_sum (GtkCssParser           *parser,
                              GtkCssNumberParseFlags  flags)
{
  GtkCssValue *result;

  result = gtk_css_calc_value_parse_product (parser, flags);
  if (result == NULL)
    return NULL;

  while (_gtk_css_parser_begins_with (parser, '+') || _gtk_css_parser_begins_with (parser, '-'))
    {
      GtkCssValue *next, *temp;

      if (_gtk_css_parser_try (parser, "+", TRUE))
        {
          next = gtk_css_calc_value_parse_product (parser, flags);
          if (next == NULL)
            goto fail;
        }
      else if (_gtk_css_parser_try (parser, "-", TRUE))
        {
          temp = gtk_css_calc_value_parse_product (parser, flags);
          if (temp == NULL)
            goto fail;
          next = gtk_css_number_value_multiply (temp, -1);
          _gtk_css_value_unref (temp);
        }
      else
        {
          g_assert_not_reached ();
          goto fail;
        }

      temp = gtk_css_number_value_add (result, next);
      _gtk_css_value_unref (result);
      _gtk_css_value_unref (next);
      result = temp;
    }

  return result;

fail:
  _gtk_css_value_unref (result);
  return NULL;
}

GtkCssValue *
gtk_css_calc_value_parse (GtkCssParser           *parser,
                          GtkCssNumberParseFlags  flags)
{
  GtkCssValue *value;

  /* This confuses '*' and '/' so we disallow backwards compat. */
  flags &= ~GTK_CSS_NUMBER_AS_PIXELS;
  /* This can only be handled at compute time, we allow '-' after all */
  flags &= ~GTK_CSS_POSITIVE_ONLY;

  if (!_gtk_css_parser_try (parser, "calc(", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected 'calc('");
      return NULL;
    }

  value = gtk_css_calc_value_parse_sum (parser, flags);
  if (value == NULL)
    return NULL;

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_value_unref (value);
      _gtk_css_parser_error (parser, "Expected ')' after calc() statement");
      return NULL;
    }

  return value;
}

