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

#include "gtkstylepropertyprivate.h"
#include "gtkstylepropertiesprivate.h"

struct _GtkCssLookup {
  GtkBitmask    *missing;
  const GValue  *values[1];
};

GtkCssLookup *
_gtk_css_lookup_new (void)
{
  GtkCssLookup *lookup;
  guint n = _gtk_style_property_get_count ();

  lookup = g_malloc0 (sizeof (GtkCssLookup) + sizeof (const GValue *) * n);
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

  return lookup->values[id] == NULL;
}

/**
 * _gtk_css_lookup_set:
 * @lookup: the lookup
 * @id: id of the property to set, see _gtk_style_property_get_id()
 * @value: the "cascading value" to use
 *
 * Sets the @value for a given @id. No value may have been set for @id
 * before. See _gtk_css_lookup_is_missing(). This function is used to
 * set the "winning declaration" of a lookup.
 **/
void
_gtk_css_lookup_set (GtkCssLookup *lookup,
                     guint         id,
                     const GValue *value)
{
  g_return_if_fail (lookup != NULL);
  g_return_if_fail (_gtk_bitmask_get (lookup->missing, id));
  g_return_if_fail (value != NULL);

  _gtk_bitmask_set (lookup->missing, id, FALSE);
  lookup->values[id] = value;
}

/**
 * _gtk_css_lookup_resolve:
 * @lookup: the lookup
 * @parent: the parent properties to look up inherited values from or %NULL
 *     if none
 *
 * Resolves the current lookup into a styleproperties object. This is done
 * by converting from the "winning declaration" to the "computed value".
 *
 * XXX: This bypasses the notion of "specified value". If this ever becomes
 * an issue, go fix it.
 *
 * Returns: a new #GtkStyleProperties
 **/
GtkStyleProperties *
_gtk_css_lookup_resolve (GtkCssLookup    *lookup,
                         GtkStyleContext *parent)
{
  GtkStyleProperties *props;
  guint i, n;

  g_return_val_if_fail (lookup != NULL, NULL);
  g_return_val_if_fail (parent == NULL || GTK_IS_STYLE_CONTEXT (parent), NULL);

  n = _gtk_style_property_get_count ();
  props = gtk_style_properties_new ();

  for (i = 0; i < n; i++)
    {
      if (lookup->values[i] == NULL)
        continue;

      _gtk_style_properties_set_property_by_property (props,
                                                      _gtk_style_property_get (i),
                                                      0,
                                                      lookup->values[i]);
    }

  return props;
}
