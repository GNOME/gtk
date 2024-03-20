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

#include "gtkcsscustompropertypoolprivate.h"

struct _GtkCssCustomPropertyPool
{
  GObject parent_instance;

  GArray *names;
  GHashTable *name_mappings;
};

struct _GtkCssCustomPropertyName
{
  int ref_count;
  char *name;
};

typedef struct _GtkCssCustomPropertyName GtkCssCustomPropertyName;

static GtkCssCustomPropertyPool *instance = NULL;

G_DEFINE_FINAL_TYPE (GtkCssCustomPropertyPool, gtk_css_custom_property_pool, G_TYPE_OBJECT)

static void
clear_custom_property_name (GtkCssCustomPropertyName *name)
{
  g_clear_pointer (&name->name, g_free);
}

static void
gtk_css_custom_property_pool_finalize (GObject *object)
{
  GtkCssCustomPropertyPool *self = (GtkCssCustomPropertyPool *)object;

  g_hash_table_unref (self->name_mappings);
  g_array_unref (self->names);

  G_OBJECT_CLASS (gtk_css_custom_property_pool_parent_class)->finalize (object);
}

static void
gtk_css_custom_property_pool_class_init (GtkCssCustomPropertyPoolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_custom_property_pool_finalize;
}

static void
gtk_css_custom_property_pool_init (GtkCssCustomPropertyPool *self)
{
  self->name_mappings = g_hash_table_new (g_str_hash, g_str_equal);
  self->names = g_array_new (FALSE, FALSE, sizeof (GtkCssCustomPropertyName));

  g_array_set_clear_func (self->names, (GDestroyNotify) clear_custom_property_name);
}

GtkCssCustomPropertyPool *
gtk_css_custom_property_pool_get (void)
{
  if (instance == NULL)
    instance = g_object_new (GTK_TYPE_CSS_CUSTOM_PROPERTY_POOL, NULL);

  return instance;
}

int
gtk_css_custom_property_pool_add (GtkCssCustomPropertyPool *self,
                                  const char               *str)
{
  GtkCssCustomPropertyName name;
  int id;

  id = gtk_css_custom_property_pool_lookup (self, str);
  if (id > 0)
    return gtk_css_custom_property_pool_ref (self, id);

  name.ref_count = 1;
  name.name = g_strdup (str);

  // TODO reuse slots after they're gone
  g_array_append_val (self->names, name);

  id = self->names->len;

  g_hash_table_insert (self->name_mappings, (char *) name.name, GINT_TO_POINTER (id));

  return id;
}

int
gtk_css_custom_property_pool_lookup (GtkCssCustomPropertyPool *self,
                                     const char               *str)
{
  gpointer id;

  id = g_hash_table_lookup (self->name_mappings, str);

  return GPOINTER_TO_INT (id);
}

int
gtk_css_custom_property_pool_ref (GtkCssCustomPropertyPool *self,
                                  int                       id)
{
  GtkCssCustomPropertyName *name;

  name = &g_array_index (self->names, GtkCssCustomPropertyName, id - 1);

  name->ref_count++;

  return id;
}

void
gtk_css_custom_property_pool_unref (GtkCssCustomPropertyPool *self,
                                    int                       id)
{
  GtkCssCustomPropertyName *name;

  name = &g_array_index (self->names, GtkCssCustomPropertyName, id - 1);

  g_assert (name->ref_count > 0);

  name->ref_count--;

  if (name->ref_count == 0)
    {
      g_hash_table_remove (self->name_mappings, name->name);
      clear_custom_property_name (name);
    }
}

const char *
gtk_css_custom_property_pool_get_name (GtkCssCustomPropertyPool *self,
                                       int                       id)
{
  GtkCssCustomPropertyName *name;

  name = &g_array_index (self->names, GtkCssCustomPropertyName, id - 1);

  return name->name;
}
