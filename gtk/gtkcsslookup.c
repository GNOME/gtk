/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkcsslookupprivate.h"

#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkstylepropertiesprivate.h"

GtkCssLookup *
_gtk_css_lookup_new (const GtkBitmask *relevant)
{
  GtkCssLookup *lookup;
  guint n = _gtk_css_style_property_get_n_properties ();

  lookup = g_malloc0 (sizeof (GtkCssLookup) + sizeof (GtkCssLookupValue) * n);

  if (relevant)
    {
      lookup->missing = _gtk_bitmask_copy (relevant);
    }
  else
    {
      lookup->missing = _gtk_bitmask_new ();
      lookup->missing = _gtk_bitmask_invert_range (lookup->missing, 0, n);
    }

  return lookup;
}

void
_gtk_css_lookup_free (GtkCssLookup *lookup)
{
  g_return_if_fail (lookup != NULL);

  _gtk_bitmask_free (lookup->missing);
  g_free (lookup);
}

gboolean
_gtk_css_lookup_is_missing (const GtkCssLookup *lookup,
                            guint               id)
{
  g_return_val_if_fail (lookup != NULL, FALSE);

  return _gtk_bitmask_get (lookup->missing, id);
}

/**
 * _gtk_css_lookup_set:
 * @lookup: the lookup
 * @id: id of the property to set, see _gtk_style_property_get_id()
 * @section: (allow-none): The @section the value was defined in or %NULL
 * @value: the "cascading value" to use
 *
 * Sets the @value for a given @id. No value may have been set for @id
 * before. See _gtk_css_lookup_is_missing(). This function is used to
 * set the "winning declaration" of a lookup. Note that for performance
 * reasons @value and @section are not copied. It is your responsibility
 * to ensure they are kept alive until _gtk_css_lookup_free() is called.
 **/
void
_gtk_css_lookup_set (GtkCssLookup  *lookup,
                     guint          id,
                     GtkCssSection *section,
                     GtkCssValue   *value)
{
  g_return_if_fail (lookup != NULL);
  g_return_if_fail (_gtk_bitmask_get (lookup->missing, id));
  g_return_if_fail (value != NULL);

  lookup->missing = _gtk_bitmask_set (lookup->missing, id, FALSE);
  lookup->values[id].value = value;
  lookup->values[id].section = section;
}

/**
 * _gtk_css_lookup_set_computed:
 * @lookup: the lookup
 * @id: id of the property to set, see _gtk_style_property_get_id()
 * @section: (allow-none): The @section the value was defined in or %NULL
 * @value: the "computed value" to use
 *
 * Sets the @value for a given @id. No value may have been set for @id
 * before. See _gtk_css_lookup_is_missing(). This function is used to
 * set the "winning declaration" of a lookup. Note that for performance
 * reasons @value and @section are not copied. It is your responsibility
 * to ensure they are kept alive until _gtk_css_lookup_free() is called.
 *
 * As opposed to _gtk_css_lookup_set(), this function forces a computed
 * value and will not cause computation to happen. In particular, with this
 * method relative lengths or symbolic colors can not be used. This is
 * usually only useful for doing overrides. It should not be used for proper
 * CSS.
 **/
void
_gtk_css_lookup_set_computed (GtkCssLookup  *lookup,
                              guint          id,
                              GtkCssSection *section,
                              GtkCssValue  *value)
{
  g_return_if_fail (lookup != NULL);
  g_return_if_fail (_gtk_bitmask_get (lookup->missing, id));
  g_return_if_fail (value != NULL);

  lookup->missing = _gtk_bitmask_set (lookup->missing, id, FALSE);
  lookup->values[id].computed = value;
  lookup->values[id].section = section;
}

/**
 * _gtk_css_lookup_resolve:
 * @lookup: the lookup
 * @context: the context the values are resolved for
 * @values: a new #GtkCssComputedValues to be filled with the new properties
 *
 * Resolves the current lookup into a styleproperties object. This is done
 * by converting from the "winning declaration" to the "computed value".
 *
 * XXX: This bypasses the notion of "specified value". If this ever becomes
 * an issue, go fix it.
 **/
void
_gtk_css_lookup_resolve (GtkCssLookup            *lookup,
                         GtkStyleProviderPrivate *provider,
                         GtkCssComputedValues    *values,
                         GtkCssComputedValues    *parent_values)
{
  guint i, n;

  g_return_if_fail (lookup != NULL);
  g_return_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider));
  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  g_return_if_fail (parent_values == NULL || GTK_IS_CSS_COMPUTED_VALUES (parent_values));

  n = _gtk_css_style_property_get_n_properties ();

  for (i = 0; i < n; i++)
    {
      if (lookup->values[i].computed)
        _gtk_css_computed_values_set_value (values,
                                            i,
                                            lookup->values[i].computed,
                                            0,
                                            lookup->values[i].section);
      else if (lookup->values[i].value ||
               _gtk_bitmask_get (lookup->missing, i))
        _gtk_css_computed_values_compute_value (values,
                                                provider,
                                                parent_values,
                                                i,
                                                lookup->values[i].value,
                                                lookup->values[i].section);
      /* else not a relevant property */
    }
}
