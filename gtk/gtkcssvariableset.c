/*
 * Copyright (C) 2023 GNOME Foundation Inc.
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
 * Authors: Alice Mikhaylenko <alicem@gnome.org>
 */

#include "gtkcssvariablesetprivate.h"

#include "gtkcsscustompropertypoolprivate.h"

static void
unref_custom_property_name (gpointer pointer)
{
  GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();

  gtk_css_custom_property_pool_unref (pool, GPOINTER_TO_INT (pointer));
}

static void
maybe_unref_variable (gpointer pointer)
{
  if (pointer != NULL)
    gtk_css_variable_value_unref (pointer);
}

GtkCssVariableSet *
gtk_css_variable_set_new (void)
{
  GtkCssVariableSet *self = g_new0 (GtkCssVariableSet, 1);

  self->ref_count = 1;
  self->variables = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           unref_custom_property_name,
                                           maybe_unref_variable);

  return self;
}

GtkCssVariableSet *
gtk_css_variable_set_ref (GtkCssVariableSet *self)
{
  self->ref_count++;

  return self;
}

void
gtk_css_variable_set_unref (GtkCssVariableSet *self)
{
  self->ref_count--;
  if (self->ref_count > 0)
    return;

  g_hash_table_unref (self->variables);
  g_clear_pointer (&self->parent, gtk_css_variable_set_unref);
  g_free (self);
}

GtkCssVariableSet *
gtk_css_variable_set_copy (GtkCssVariableSet *self)
{
  GtkCssVariableSet *ret = gtk_css_variable_set_new ();
  GHashTableIter iter;
  gpointer id;
  GtkCssVariableValue *value;

  g_hash_table_iter_init (&iter, self->variables);

  while (g_hash_table_iter_next (&iter, &id, (gpointer) &value))
    gtk_css_variable_set_add (ret, GPOINTER_TO_INT (id), value);

  gtk_css_variable_set_set_parent (ret, self->parent);

  return ret;
}

void
gtk_css_variable_set_set_parent (GtkCssVariableSet *self,
                                 GtkCssVariableSet *parent)
{
  if (self->parent)
    gtk_css_variable_set_unref (self->parent);

  self->parent = parent;

  if (self->parent)
    gtk_css_variable_set_ref (self->parent);
}

void
gtk_css_variable_set_add (GtkCssVariableSet   *self,
                          int                  id,
                          GtkCssVariableValue *value)
{
  GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();

  g_hash_table_insert (self->variables,
                       GINT_TO_POINTER (gtk_css_custom_property_pool_ref (pool, id)),
                       value ? gtk_css_variable_value_ref (value) : NULL);
}

static gboolean check_variable (GtkCssVariableSet   *self,
                                GHashTable          *unvisited_variables,
                                GArray              *stack,
                                int                  id,
                                GtkCssVariableValue *value);

static gboolean
check_references (GtkCssVariableSet   *self,
                  GHashTable          *unvisited_variables,
                  GArray              *stack,
                  GtkCssVariableValue *value)
{
  GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();
  int i;

  for (i = 0; i < value->n_references; i++)
    {
      GtkCssVariableValueReference *ref = &value->references[i];
      int ref_id = gtk_css_custom_property_pool_lookup (pool, ref->name);

      if (g_hash_table_contains (unvisited_variables, GINT_TO_POINTER (ref_id)))
        {
          GtkCssVariableValue *ref_value;

          ref_value = g_hash_table_lookup (self->variables, GINT_TO_POINTER (ref_id));

          /* This variable was already removed, no point in checking further */
          if (!ref_value)
            return FALSE;

          if (check_variable (self, unvisited_variables, stack, ref_id, ref_value))
            return TRUE;
        }

      if (ref->fallback)
        {
          if (check_references (self, unvisited_variables, stack, ref->fallback))
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
check_variable (GtkCssVariableSet   *self,
                GHashTable          *unvisited_variables,
                GArray              *stack,
                int                  id,
                GtkCssVariableValue *value)
{
  int i;

  g_array_append_val (stack, id);

  for (i = stack->len - 2; i >= 0; i--)
    {
      /* Found a cycle, stop checking */
      if (id == g_array_index (stack, int, i))
        return TRUE;
    }

  if (check_references (self, unvisited_variables, stack, value))
    return TRUE;

  g_array_remove_index_fast (stack, stack->len - 1);
  g_hash_table_remove (unvisited_variables, GINT_TO_POINTER (id));

  return FALSE;
}

void
gtk_css_variable_set_resolve_cycles (GtkCssVariableSet *self)
{
  GHashTable *variables = g_hash_table_new (g_direct_hash, g_direct_equal);
  GHashTable *unvisited_variables = g_hash_table_new (g_direct_hash, g_direct_equal);
  GArray *stack;
  GHashTableIter iter;
  gpointer id;
  GtkCssVariableValue *value;

  stack = g_array_new (FALSE, FALSE, sizeof (int));

  g_hash_table_iter_init (&iter, self->variables);

  while (g_hash_table_iter_next (&iter, &id, (gpointer) &value))
    {
      /* Have 2 copies of self->variables: "variables" can be iterated safely as
       * we remove values, and "unvisited_variables" as a visited node marker
       * for DFS */
      g_hash_table_insert (variables, id, value);
      g_hash_table_add (unvisited_variables, id);
    }

  g_hash_table_iter_init (&iter, variables);

  while (g_hash_table_iter_next (&iter, &id, (gpointer) &value))
    {
      if (!g_hash_table_contains (unvisited_variables, id))
        continue;

      if (!g_hash_table_contains (self->variables, id))
        continue;

      if (check_variable (self, unvisited_variables, stack, GPOINTER_TO_INT (id), value))
        {
          int i;

          /* Found a cycle, remove the offending variables */
          for (i = stack->len - 2; i >= 0; i--)
            {
              int id_to_remove = g_array_index (stack, int, i);

              g_hash_table_remove (self->variables, GINT_TO_POINTER (id_to_remove));

              if (g_array_index (stack, int, i) == g_array_index (stack, int, stack->len - 1))
                break;
            }

          g_array_remove_range (stack, 0, stack->len);
        }
    }

  g_array_unref (stack);
  g_hash_table_unref (variables);
  g_hash_table_unref (unvisited_variables);
}

GtkCssVariableValue *
gtk_css_variable_set_lookup (GtkCssVariableSet  *self,
                             int                 id,
                             GtkCssVariableSet **source)
{
  GtkCssVariableValue *ret = g_hash_table_lookup (self->variables, GINT_TO_POINTER (id));

  if (ret)
    {
      if (source)
        *source = self;
      return ret;
    }

  if (self->parent)
    return gtk_css_variable_set_lookup (self->parent, id, source);

  if (source)
    *source = NULL;

  return NULL;
}

static void
list_ids_recursive (GtkCssVariableSet *self,
                    GHashTable        *table)
{
  GHashTableIter iter;
  gpointer id;

  if (self->parent)
    list_ids_recursive (self->parent, table);

  g_hash_table_iter_init (&iter, self->variables);

  while (g_hash_table_iter_next (&iter, &id, NULL))
    g_hash_table_add (table, id);
}

static int
compare_ids (gconstpointer a, gconstpointer b, gpointer user_data)
{
  GtkCssCustomPropertyPool *pool = user_data;
  int id1 = GPOINTER_TO_INT (*((const int *) a));
  int id2 = GPOINTER_TO_INT (*((const int *) b));
  const char *name1, *name2;

  name1 = gtk_css_custom_property_pool_get_name (pool, id1);
  name2 = gtk_css_custom_property_pool_get_name (pool, id2);

  return strcmp (name1, name2);
}

GArray *
gtk_css_variable_set_list_ids (GtkCssVariableSet  *self)
{
  GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();
  GArray *ret = g_array_new (FALSE, FALSE, sizeof (int));
  GHashTable *all_ids;
  GHashTableIter iter;
  gpointer id;

  all_ids = g_hash_table_new (g_direct_hash, g_direct_equal);
  list_ids_recursive (self, all_ids);

  g_hash_table_iter_init (&iter, all_ids);

  while (g_hash_table_iter_next (&iter, &id, NULL))
    {
      int value = GPOINTER_TO_INT (id);
      g_array_append_val (ret, value);
    }

  g_hash_table_unref (all_ids);

  g_array_sort_with_data (ret, compare_ids, pool);

  return ret;
}

gboolean
gtk_css_variable_set_equal (GtkCssVariableSet *set1,
                            GtkCssVariableSet *set2)
{
  GHashTableIter iter;
  gpointer id;
  GtkCssVariableValue *value1;

  if (set1 == set2)
    return TRUE;

  if (set1 == NULL || set2 == NULL)
    return FALSE;

  if (g_hash_table_size (set1->variables) != g_hash_table_size (set2->variables))
    return FALSE;

  if (set1->parent != set2->parent)
    return FALSE;

  g_hash_table_iter_init (&iter, set2->variables);

  while (g_hash_table_iter_next (&iter, &id, NULL))
    {
      if (!g_hash_table_contains (set1->variables, id))
        return FALSE;
    }

  g_hash_table_iter_init (&iter, set1->variables);

  while (g_hash_table_iter_next (&iter, &id, (gpointer) &value1))
    {
      GtkCssVariableValue *value2 = g_hash_table_lookup (set2->variables, id);

      if (value2 == NULL)
         return FALSE;

      if (!gtk_css_variable_value_equal (value1, value2))
         return FALSE;
    }

  return TRUE;
}
