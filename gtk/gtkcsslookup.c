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


static GHashTable *lookups;

static gboolean
gtk_css_lookup_equal (const GtkCssLookup *l1,
                      const GtkCssLookup *l2)
{
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
gtk_css_lookup_hash (const GtkCssLookup *l)
{
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

  g_print ("lookup counts: ");
  g_hash_table_iter_init (&iter, lookups);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      g_print ("%d ", GPOINTER_TO_INT (value));
    }
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

void
gtk_css_lookup_merge (GtkCssLookup      *lookup,
                      GtkCssLookupValue *values,
                      guint              n_values)
{
  int i, j;

  gtk_internal_return_if_fail (lookup != NULL);
  gtk_internal_return_if_fail (values != NULL);

  if (!lookup->values)
    lookup->values = g_ptr_array_sized_new (MAX (16, n_values));

  i = j = 0;
  while (j < n_values)
    {
      GtkCssLookupValue *v;

      if (i == lookup->values->len)
        break;

      v =  g_ptr_array_index (lookup->values, i);

      if (v->id < values[j].id)
        {
          i++;
        }
      else if (v->id > values[j].id)
        {
          /* insert value[j] here */
          g_ptr_array_insert (lookup->values, i, &values[j]);
          lookup->set_values = _gtk_bitmask_set (lookup->set_values, values[j].id, TRUE);
          i++;
          j++;
        }
      else
        {
          /* value[j] is already set, skip */
          i++;
          j++;
        }
    }

  /* append remaining values */
  for (; j < n_values; j++)
    {
      g_ptr_array_add (lookup->values, &values[j]);
      lookup->set_values = _gtk_bitmask_set (lookup->set_values, values[j].id, TRUE);
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
