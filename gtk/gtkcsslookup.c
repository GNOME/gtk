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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkcsslookupprivate.h"

#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkstylepropertiesprivate.h"

typedef struct {
  GtkCssSection     *section;
  const GValue      *value;
} GtkCssLookupValue;

struct _GtkCssLookup {
  GtkBitmask        *missing;
  GtkCssLookupValue  values[1];
};

GtkCssLookup *
_gtk_css_lookup_new (void)
{
  GtkCssLookup *lookup;
  guint n = _gtk_css_style_property_get_n_properties ();

  lookup = g_malloc0 (sizeof (GtkCssLookup) + sizeof (GtkCssLookupValue) * n);
  lookup->missing = _gtk_bitmask_new ();
  _gtk_bitmask_invert_range (lookup->missing, 0, n);

  return lookup;
}

void
_gtk_css_lookup_free (GtkCssLookup *lookup)
{
  g_return_if_fail (lookup != NULL);

  _gtk_bitmask_free (lookup->missing);
  g_free (lookup);
}

const GtkBitmask *
_gtk_css_lookup_get_missing (const GtkCssLookup *lookup)
{
  g_return_val_if_fail (lookup != NULL, NULL);

  return lookup->missing;
}

gboolean
_gtk_css_lookup_is_missing (const GtkCssLookup *lookup,
                            guint               id)
{
  g_return_val_if_fail (lookup != NULL, FALSE);

  return lookup->values[id].value == NULL;
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
                     const GValue  *value)
{
  g_return_if_fail (lookup != NULL);
  g_return_if_fail (_gtk_bitmask_get (lookup->missing, id));
  g_return_if_fail (value != NULL);

  _gtk_bitmask_set (lookup->missing, id, FALSE);
  lookup->values[id].value = value;
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
_gtk_css_lookup_resolve (GtkCssLookup         *lookup,
                         GtkStyleContext      *context,
                         GtkCssComputedValues *values)
{
  guint i, n;

  g_return_if_fail (lookup != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));

  n = _gtk_css_style_property_get_n_properties ();

  for (i = 0; i < n; i++)
    {
      _gtk_css_computed_values_compute_value (values,
                                              context,
                                              i,
                                              lookup->values[i].value,
                                              lookup->values[i].section);
    }
}
