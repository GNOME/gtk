/*
 * Copyright © 2012 Red Hat Inc.
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcsscomputedvaluesprivate.h"

#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"

G_DEFINE_TYPE (GtkCssComputedValues, _gtk_css_computed_values, G_TYPE_OBJECT)

static void
gtk_css_computed_values_dispose (GObject *object)
{
  GtkCssComputedValues *values = GTK_CSS_COMPUTED_VALUES (object);

  if (values->values)
    {
      g_ptr_array_unref (values->values);
      values->values = NULL;
    }
  if (values->sections)
    {
      g_ptr_array_unref (values->sections);
      values->sections = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_computed_values_parent_class)->dispose (object);
}

static void
_gtk_css_computed_values_class_init (GtkCssComputedValuesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_css_computed_values_dispose;
}

static void
_gtk_css_computed_values_init (GtkCssComputedValues *computed_values)
{
  
}

GtkCssComputedValues *
_gtk_css_computed_values_new (void)
{
  return g_object_new (GTK_TYPE_CSS_COMPUTED_VALUES, NULL);
}

static void
maybe_unref_section (gpointer section)
{
  if (section)
    gtk_css_section_unref (section);
}

static void
gtk_css_computed_values_ensure_array (GtkCssComputedValues *values,
                                      guint                 at_least_size)
{
  if (values->values == NULL)
    values->values = g_ptr_array_new_with_free_func ((GDestroyNotify)_gtk_css_value_unref);
  if (at_least_size > values->values->len)
   g_ptr_array_set_size (values->values, at_least_size);
}

void
_gtk_css_computed_values_compute_value (GtkCssComputedValues *values,
                                        GtkStyleContext      *context,
                                        guint                 id,
                                        GtkCssValue          *specified,
                                        GtkCssSection        *section)
{
  GtkCssStyleProperty *prop;
  GtkStyleContext *parent;

  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  prop = _gtk_css_style_property_lookup_by_id (id);
  parent = gtk_style_context_get_parent (context);

  gtk_css_computed_values_ensure_array (values, id + 1);

  /* http://www.w3.org/TR/css3-cascade/#cascade
   * Then, for every element, the value for each property can be found
   * by following this pseudo-algorithm:
   * 1) Identify all declarations that apply to the element
   */
  if (specified != NULL)
    {
      if (_gtk_css_value_is_inherit (specified))
        {
          /* 3) if the value of the winning declaration is ‘inherit’,
           * the inherited value (see below) becomes the specified value.
           */
          specified = NULL;
        }
      else if (_gtk_css_value_is_initial (specified))
        {
          /* if the value of the winning declaration is ‘initial’,
           * the initial value (see below) becomes the specified value.
           */
          specified = _gtk_css_style_property_get_initial_value (prop);
        }

      /* 2) If the cascading process (described below) yields a winning
       * declaration and the value of the winning declaration is not
       * ‘initial’ or ‘inherit’, the value of the winning declaration
       * becomes the specified value.
       */
    }
  else
    {
      if (_gtk_css_style_property_is_inherit (prop))
        {
          /* 4) if the property is inherited, the inherited value becomes
           * the specified value.
           */
          specified = NULL;
        }
      else
        {
          /* 5) Otherwise, the initial value becomes the specified value.
           */
          specified = _gtk_css_style_property_get_initial_value (prop);
        }
    }

  if (specified == NULL && parent == NULL)
    {
      /* If the ‘inherit’ value is set on the root element, the property is
       * assigned its initial value. */
      specified = _gtk_css_style_property_get_initial_value (prop);
    }

  if (specified)
    {
      g_ptr_array_index (values->values, id) =
	_gtk_css_style_property_compute_value (prop,
					       context,
					       specified);
    }
  else
    {
      GtkCssValue *parent_value;
      /* Set NULL here and do the inheritance upon lookup? */
      parent_value = _gtk_style_context_peek_property (parent, id);

      g_ptr_array_index (values->values, id) = _gtk_css_value_ref (parent_value);
    }

  if (section)
    {
      if (values->sections == NULL)
        values->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (values->sections->len <= id)
        g_ptr_array_set_size (values->sections, id + 1);

      g_ptr_array_index (values->sections, id) = gtk_css_section_ref (section);
    }
}
                                    
void
_gtk_css_computed_values_set_value (GtkCssComputedValues *values,
                                    guint                 id,
                                    GtkCssValue          *value,
                                    GtkCssSection        *section)
{
  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));

  gtk_css_computed_values_ensure_array (values, id + 1);

  if (g_ptr_array_index (values->values, id))
    _gtk_css_value_unref (g_ptr_array_index (values->values, id));
  g_ptr_array_index (values->values, id) = _gtk_css_value_ref (value);

  if (section)
    {
      if (values->sections == NULL)
        values->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (values->sections->len <= id)
        g_ptr_array_set_size (values->sections, id + 1);

      g_ptr_array_index (values->sections, id) = gtk_css_section_ref (section);
    }
}

GtkCssValue *
_gtk_css_computed_values_get_value (GtkCssComputedValues *values,
                                    guint                 id)
{
  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  if (values->values == NULL ||
      id >= values->values->len)
    return NULL;

  return g_ptr_array_index (values->values, id);
}

GtkCssSection *
_gtk_css_computed_values_get_section (GtkCssComputedValues *values,
                                      guint                 id)
{
  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  if (values->sections == NULL ||
      id >= values->sections->len)
    return NULL;

  return g_ptr_array_index (values->sections, id);
}

GtkBitmask *
_gtk_css_computed_values_get_difference (GtkCssComputedValues *values,
                                         GtkCssComputedValues *other)
{
  GtkBitmask *result;
  guint i, len;

  len = MIN (values->values->len, other->values->len);
  result = _gtk_bitmask_new ();
  if (values->values->len != other->values->len)
    result = _gtk_bitmask_invert_range (result, len, MAX (values->values->len, other->values->len));
  
  for (i = 0; i < len; i++)
    {
      if (!_gtk_css_value_equal (g_ptr_array_index (values->values, i),
                                 g_ptr_array_index (other->values, i)))
        result = _gtk_bitmask_set (result, i, TRUE);
    }

  return result;
}

