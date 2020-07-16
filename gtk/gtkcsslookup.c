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

#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkprivate.h"

GtkCssLookupValue *
gtk_css_lookup_value_new (guint          id,
                          GtkCssValue   *value,
                          GtkCssSection *section)
{
  GtkCssLookupValue *v;

  v = g_new0 (GtkCssLookupValue, 1);
  v->ref_count = 1;

  v->id = id;
  v->value = _gtk_css_value_ref (value);
  if (section)
    v->section = gtk_css_section_ref (section);

  return v;
}

static void
gtk_css_lookup_value_free (GtkCssLookupValue *value)
{
  _gtk_css_value_unref (value->value);
  if (value->section)
    gtk_css_section_unref (value->section);
  g_free (value);
}

GtkCssLookupValue *
gtk_css_lookup_value_ref (GtkCssLookupValue *value)
{
  value->ref_count++;
  return value;
}

void
gtk_css_lookup_value_unref (GtkCssLookupValue *value)
{
  value->ref_count--;
  if (value->ref_count == 0)
    gtk_css_lookup_value_free (value);
}

static GHashTable *lookups;

static gboolean
gtk_css_lookup_equal (gconstpointer p1,
                      gconstpointer p2)
{
  const GtkCssLookup *l1 = p1;
  const GtkCssLookup *l2 = p2;
  int i;

  if (!_gtk_bitmask_equals (l1->set_values, l2->set_values))
    return FALSE;

  if (l1->values == NULL && l2->values == NULL)
    return TRUE;

  if (l1->values == NULL || l2->values == NULL)
    return FALSE;

  if (l1->values->len != l2->values->len)
    return FALSE;

  for (i = 0; i < l1->values->len; i++)
    {
      GtkCssLookupValue *v1 = g_ptr_array_index (l1->values, i);
      GtkCssLookupValue *v2 = g_ptr_array_index (l2->values, i);

      if (!_gtk_css_value_equal (v1->value, v2->value))
        return FALSE;
    }

  return TRUE;
}

static guint
gtk_css_lookup_hash (gconstpointer data)
{
  const GtkCssLookup *l = data;
  int i;
  guint h;

  if (l->values == NULL)
    return 0;

  h = 0;
  for (i = 0; i < l->values->len; i++)
    {
      GtkCssLookupValue *v = g_ptr_array_index (l->values, i);
      char *s = _gtk_css_value_to_string (v->value);
      h += g_str_hash (s);
      g_free (s);
    }

  return h;
}

static gboolean
dump_lookups (gpointer data)
{
  GHashTableIter iter;
  GtkCssLookup *key;
  gpointer value;
  int total = 0;
  int length = 0;

  g_hash_table_iter_init (&iter, lookups);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      total += GPOINTER_TO_INT (value);
      length += GPOINTER_TO_INT (value) * key->values->len;
    }

  g_print ("lookup stats:\n");
  g_print ("\t%d different lookups, %d total, %g avg length\n",
           g_hash_table_size (lookups),
           total,
           length / (1.0 * total));

  g_hash_table_iter_init (&iter, lookups);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    g_print ("\t%d\t%d\n", GPOINTER_TO_INT (value), key->values->len);
  g_print ("\n");

  return G_SOURCE_CONTINUE;
}

GtkCssLookup *
gtk_css_lookup_new (void)
{
  GtkCssLookup *lookup = g_new0 (GtkCssLookup, 1);

  lookup->ref_count = 1;
  lookup->set_values = _gtk_bitmask_new ();

  if (!lookups)
    {
      lookups = g_hash_table_new (gtk_css_lookup_hash, gtk_css_lookup_equal);
      g_timeout_add (1000, dump_lookups, NULL);
    }

  return lookup;
}

void
gtk_css_lookup_register (GtkCssLookup *lookup)
{
  gint count;

  count = GPOINTER_TO_INT (g_hash_table_lookup (lookups, lookup));
  count++;

  if (count == 1)
    gtk_css_lookup_ref (lookup);

  g_hash_table_insert (lookups, lookup, GINT_TO_POINTER (count));
}

static void
gtk_css_lookup_unregister (GtkCssLookup *lookup)
{
  gint count;

  count = GPOINTER_TO_INT (g_hash_table_lookup (lookups, lookup));
  g_assert (count > 0);
  if (count == 1)
    g_hash_table_remove (lookups, lookup);
  else
    {
      count--;
      g_hash_table_insert (lookups, lookup, GINT_TO_POINTER (count));
    }
}

static void
gtk_css_lookup_free (GtkCssLookup *lookup)
{
  gtk_css_lookup_unregister (lookup);

  _gtk_bitmask_free (lookup->set_values);
  if (lookup->values)
    g_ptr_array_unref (lookup->values);
  g_free (lookup);
}

GtkCssLookup *
gtk_css_lookup_ref (GtkCssLookup *lookup)
{
  gtk_internal_return_val_if_fail (lookup != NULL, NULL);
  gtk_internal_return_val_if_fail (lookup->ref_count > 0, NULL);

  lookup->ref_count++;

  return lookup;
}

void
gtk_css_lookup_unref (GtkCssLookup *lookup)
{
  gtk_internal_return_if_fail (lookup != NULL);
  gtk_internal_return_if_fail (lookup->ref_count > 0);

  lookup->ref_count--;

  if (lookup->ref_count == 1 &&
      GPOINTER_TO_INT (g_hash_table_lookup (lookups, lookup)) == 1)
    lookup->ref_count--;

  if (lookup->ref_count == 0)
    gtk_css_lookup_free (lookup);
}

/*
 * gtk_css_lookup_fill:
 * @lookup: the #GtkCssLookup to populate
 * @values: array of values to place into unpopulated slots in @lookup
 * @n_values: the length of @values
 *
 * Add the @values to @lookup for properties for which
 * @lookup does not have a value yet.
 */
void
gtk_css_lookup_fill (GtkCssLookup       *lookup,
                     GtkCssLookupValue **values,
                     guint               n_values)
{
  int i, j;

  gtk_internal_return_if_fail (lookup != NULL);
  gtk_internal_return_if_fail (values != NULL);

  if (!lookup->values)
    lookup->values = g_ptr_array_new_full (MAX (16, n_values),
                                           (GDestroyNotify)gtk_css_lookup_value_unref);

  for (i = 0, j = 0; j < n_values; j++, i++)
    {
      GtkCssLookupValue *v = NULL;

      for (; i < lookup->values->len; i++)
        {
          v =  g_ptr_array_index (lookup->values, i);
          if (v->id >= values[j]->id)
            break;
        }

      if (i == lookup->values->len || v->id > values[j]->id)
        {
          g_ptr_array_insert (lookup->values, i, gtk_css_lookup_value_ref (values[j]));
          lookup->set_values = _gtk_bitmask_set (lookup->set_values, values[j]->id, TRUE);
        }
    }
}

GtkCssSection *
gtk_css_lookup_get_section (GtkCssLookup *lookup,
                            guint         id)
{
  if (_gtk_bitmask_get (lookup->set_values, id))
    {
      int i;

      for (i = 0; i < lookup->values->len; i++)
        {
          GtkCssLookupValue *value = g_ptr_array_index (lookup->values, i);
          if (value->id == id)
            return value->section;
        }
    }

  return NULL;
}
