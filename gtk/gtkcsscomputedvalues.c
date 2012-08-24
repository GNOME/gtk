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
gtk_css_computed_values_finalize (GObject *object)
{
  GtkCssComputedValues *values = GTK_CSS_COMPUTED_VALUES (object);

  _gtk_bitmask_free (values->depends_on_parent);
  _gtk_bitmask_free (values->equals_parent);
  _gtk_bitmask_free (values->depends_on_color);
  _gtk_bitmask_free (values->depends_on_font_size);

  G_OBJECT_CLASS (_gtk_css_computed_values_parent_class)->finalize (object);
}

static void
_gtk_css_computed_values_class_init (GtkCssComputedValuesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_css_computed_values_dispose;
  object_class->finalize = gtk_css_computed_values_finalize;
}

static void
_gtk_css_computed_values_init (GtkCssComputedValues *values)
{
  values->depends_on_parent = _gtk_bitmask_new ();
  values->equals_parent = _gtk_bitmask_new ();
  values->depends_on_color = _gtk_bitmask_new ();
  values->depends_on_font_size = _gtk_bitmask_new ();
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
  GtkCssDependencies dependencies;
  GtkCssValue *value;

  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  /* http://www.w3.org/TR/css3-cascade/#cascade
   * Then, for every element, the value for each property can be found
   * by following this pseudo-algorithm:
   * 1) Identify all declarations that apply to the element
   */
  if (specified == NULL)
    {
      GtkCssStyleProperty *prop = _gtk_css_style_property_lookup_by_id (id);

      if (_gtk_css_style_property_is_inherit (prop))
        specified = _gtk_css_inherit_value_new ();
      else
        specified = _gtk_css_initial_value_new ();
    }
  else
    _gtk_css_value_ref (specified);

  value = _gtk_css_value_compute (specified, id, context, &dependencies);

  _gtk_css_computed_values_set_value (values, id, value, dependencies, section);

  _gtk_css_value_unref (value);
  _gtk_css_value_unref (specified);
}
                                    
void
_gtk_css_computed_values_set_value (GtkCssComputedValues *values,
                                    guint                 id,
                                    GtkCssValue          *value,
                                    GtkCssDependencies    dependencies,
                                    GtkCssSection        *section)
{
  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));

  gtk_css_computed_values_ensure_array (values, id + 1);

  if (g_ptr_array_index (values->values, id))
    _gtk_css_value_unref (g_ptr_array_index (values->values, id));
  g_ptr_array_index (values->values, id) = _gtk_css_value_ref (value);

  if (dependencies & (GTK_CSS_DEPENDS_ON_PARENT | GTK_CSS_EQUALS_PARENT))
    values->depends_on_parent = _gtk_bitmask_set (values->depends_on_parent, id, TRUE);
  if (dependencies & (GTK_CSS_EQUALS_PARENT))
    values->equals_parent = _gtk_bitmask_set (values->equals_parent, id, TRUE);
  if (dependencies & (GTK_CSS_DEPENDS_ON_COLOR))
    values->depends_on_color = _gtk_bitmask_set (values->depends_on_color, id, TRUE);
  if (dependencies & (GTK_CSS_DEPENDS_ON_FONT_SIZE))
    values->depends_on_font_size = _gtk_bitmask_set (values->depends_on_font_size, id, TRUE);

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

