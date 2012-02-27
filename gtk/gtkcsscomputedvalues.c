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

#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"

G_DEFINE_TYPE (GtkCssComputedValues, _gtk_css_computed_values, G_TYPE_OBJECT)

static void
gtk_css_computed_values_dispose (GObject *object)
{
  GtkCssComputedValues *values = GTK_CSS_COMPUTED_VALUES (object);

  if (values->values)
    {
      g_array_free (values->values, TRUE);
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

void
_gtk_css_computed_values_compute_value (GtkCssComputedValues *values,
                                        GtkStyleContext      *context,
                                        guint                 id,
                                        const GValue         *specified,
                                        GtkCssSection        *section)
{
  GtkCssStyleProperty *prop;
  GtkStyleContext *parent;

  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (specified == NULL || G_IS_VALUE (specified));

  prop = _gtk_css_style_property_lookup_by_id (id);
  parent = gtk_style_context_get_parent (context);

  if (values->values == NULL)
    {
      values->values = g_array_new (FALSE, TRUE, sizeof (GValue));
      g_array_set_clear_func (values->values, (GDestroyNotify) g_value_unset);
    }
  if (id <= values->values->len)
   g_array_set_size (values->values, id + 1);

  /* http://www.w3.org/TR/css3-cascade/#cascade
   * Then, for every element, the value for each property can be found
   * by following this pseudo-algorithm:
   * 1) Identify all declarations that apply to the element
   */
  if (specified != NULL)
    {
      if (G_VALUE_HOLDS (specified, GTK_TYPE_CSS_SPECIAL_VALUE))
        {
          switch (g_value_get_enum (specified))
            {
            case GTK_CSS_INHERIT:
              /* 3) if the value of the winning declaration is ‘inherit’,
               * the inherited value (see below) becomes the specified value.
               */
              specified = NULL;
              break;
            case GTK_CSS_INITIAL:
              /* if the value of the winning declaration is ‘initial’,
               * the initial value (see below) becomes the specified value.
               */
              specified = _gtk_css_style_property_get_initial_value (prop);
              break;
            default:
              /* This is part of (2) above */
              break;
            }
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
      _gtk_css_style_property_compute_value (prop,
                                             &g_array_index (values->values, GValue, id),
                                             context,
                                             specified);
    }
  else
    {
      const GValue *parent_value;
      GValue *value = &g_array_index (values->values, GValue, id);
      /* Set NULL here and do the inheritance upon lookup? */
      parent_value = _gtk_style_context_peek_property (parent,
                                                       _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop)));
      g_value_init (value, G_VALUE_TYPE (parent_value));
      g_value_copy (parent_value, value);
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
                                    const GValue         *value,
                                    GtkCssSection        *section)
{
  GValue *set;

  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  g_return_if_fail (value == NULL || G_IS_VALUE (value));

  if (values->values == NULL)
    {
      values->values = g_array_new (FALSE, TRUE, sizeof (GValue));
      g_array_set_clear_func (values->values, (GDestroyNotify) g_value_unset);
    }
  if (id <= values->values->len)
   g_array_set_size (values->values, id + 1);


  set = &g_array_index (values->values, GValue, id);
  g_value_init (set, G_VALUE_TYPE (value));
  g_value_copy (value, set);

  if (section)
    {
      if (values->sections == NULL)
        values->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (values->sections->len <= id)
        g_ptr_array_set_size (values->sections, id + 1);

      g_ptr_array_index (values->sections, id) = gtk_css_section_ref (section);
    }
}

const GValue *
_gtk_css_computed_values_get_value (GtkCssComputedValues *values,
                                    guint                 id)
{
  const GValue *v;

  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  if (values->values == NULL ||
      id >= values->values->len)
    return NULL;

  v = &g_array_index (values->values, GValue, id);
  if (!G_IS_VALUE (v))
    return NULL;

  return v;
}

const GValue *
_gtk_css_computed_values_get_value_by_name (GtkCssComputedValues *values,
                                            const char           *name)
{
  GtkStyleProperty *prop;

  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  prop = _gtk_style_property_lookup (name);
  g_assert (GTK_IS_CSS_STYLE_PROPERTY (prop));
  
  return _gtk_css_computed_values_get_value (values, _gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (prop)));
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

