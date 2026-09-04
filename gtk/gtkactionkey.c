/* gtkactionkey.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkactionkeyprivate.h"

#include <string.h>

struct _GtkActionKey
{
  GRefString *full_name;
  const char *local_name;
  gsize       prefix_len;
  guint       hash;
  guint       ref_count;
};

struct _GtkActionInvocationKey
{
  GtkActionKey *action_key;
  GVariant     *target;
  guint         hash;
  int           ref_count;
};

G_LOCK_DEFINE_STATIC (action_keys);
static GHashTable *action_keys;

static guint
variant_hash (GVariant *value)
{
  GBytes *bytes = NULL;
  guint hash;

  g_assert (value != NULL);

  bytes = g_variant_get_data_as_bytes (value);
  hash = g_bytes_hash (bytes);

  g_bytes_unref (bytes);

  return (hash * 33) ^ g_str_hash (g_variant_get_type_string (value));
}

GtkActionKey *
gtk_action_key_new (const char *full_name)
{
  GtkActionKey *self;
  const char *separator;

  g_return_val_if_fail (full_name != NULL, NULL);

  separator = strchr (full_name, '.');
  if (!g_action_name_is_valid (full_name) ||
      separator == NULL ||
      separator == full_name ||
      separator[1] == '\0')
    return NULL;

  G_LOCK (action_keys);

  if (action_keys == NULL)
    action_keys = g_hash_table_new (g_str_hash, g_str_equal);

  self = g_hash_table_lookup (action_keys, full_name);

  if (self != NULL)
    self->ref_count++;
  else
    {
      self = g_new0 (GtkActionKey, 1);
      self->full_name = g_ref_string_new_intern (full_name);
      self->local_name = self->full_name + (separator - full_name) + 1;
      self->prefix_len = separator - full_name;
      self->hash = g_str_hash (self->full_name);
      self->ref_count = 1;

      g_hash_table_insert (action_keys, self->full_name, self);
    }

  G_UNLOCK (action_keys);

  return self;
}

GtkActionKey *
gtk_action_key_ref (GtkActionKey *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  G_LOCK (action_keys);
  self->ref_count++;
  G_UNLOCK (action_keys);

  return self;
}

void
gtk_action_key_unref (GtkActionKey *self)
{
  g_return_if_fail (self != NULL);

  G_LOCK (action_keys);

  self->ref_count--;

  if (self->ref_count == 0)
    {
      g_hash_table_remove (action_keys, self->full_name);
      g_ref_string_release (self->full_name);
      g_free (self);

      if (g_hash_table_size (action_keys) == 0)
        g_clear_pointer (&action_keys, g_hash_table_unref);
    }

  G_UNLOCK (action_keys);
}

const char *
gtk_action_key_get_full_name (GtkActionKey *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->full_name;
}

const char *
gtk_action_key_get_local_name (GtkActionKey *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->local_name;
}

gsize
gtk_action_key_get_prefix_length (GtkActionKey *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->prefix_len;
}

guint
gtk_action_key_hash (gconstpointer data)
{
  const GtkActionKey *self = data;

  g_return_val_if_fail (self != NULL, 0);

  return self->hash;
}

gboolean
gtk_action_key_equal (gconstpointer a,
                      gconstpointer b)
{
  return a == b;
}

guint
gtk_action_key_pool_get_size (void)
{
  guint size;

  G_LOCK (action_keys);
  size = action_keys != NULL ? g_hash_table_size (action_keys) : 0;
  G_UNLOCK (action_keys);

  return size;
}

GtkActionInvocationKey *
gtk_action_invocation_key_new (GtkActionKey *action_key,
                               GVariant     *target)
{
  GtkActionInvocationKey *self;

  g_return_val_if_fail (action_key != NULL, NULL);

  self = g_new0 (GtkActionInvocationKey, 1);
  self->action_key = gtk_action_key_ref (action_key);
  self->target = target != NULL ? g_variant_ref_sink (target) : NULL;
  self->hash = gtk_action_key_hash (action_key);
  self->ref_count = 1;

  if (self->target != NULL)
    self->hash = (self->hash * 33) ^ variant_hash (self->target);

  return self;
}

GtkActionInvocationKey *
gtk_action_invocation_key_ref (GtkActionInvocationKey *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
gtk_action_invocation_key_unref (GtkActionInvocationKey *self)
{
  g_return_if_fail (self != NULL);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      gtk_action_key_unref (self->action_key);
      g_clear_pointer (&self->target, g_variant_unref);
      g_free (self);
    }
}

GtkActionKey *
gtk_action_invocation_key_get_action (GtkActionInvocationKey *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->action_key;
}

GVariant *
gtk_action_invocation_key_get_target (GtkActionInvocationKey *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->target;
}

guint
gtk_action_invocation_key_hash (gconstpointer data)
{
  const GtkActionInvocationKey *self = data;

  g_return_val_if_fail (self != NULL, 0);

  return self->hash;
}

gboolean
gtk_action_invocation_key_equal (gconstpointer a,
                                 gconstpointer b)
{
  const GtkActionInvocationKey *key_a = a;
  const GtkActionInvocationKey *key_b = b;

  if (key_a == key_b)
    return TRUE;

  if (key_a == NULL || key_b == NULL || key_a->action_key != key_b->action_key)
    return FALSE;

  return (key_a->target == key_b->target ||
          (key_a->target != NULL &&
           key_b->target != NULL &&
           g_variant_equal (key_a->target, key_b->target)));
}
