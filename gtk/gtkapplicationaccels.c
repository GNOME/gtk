/*
 * Copyright © 2013 Canonical Limited
 * Copyright © 2016 Sébastien Wilmet
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Sébastien Wilmet <swilmet@gnome.org>
 */

#include "config.h"
#include "gtkapplicationaccelsprivate.h"
#include <string.h>
#include "gtkactionmuxer.h"

typedef struct
{
  guint           key;
  GdkModifierType modifier;
} AccelKey;

struct _GtkApplicationAccels
{
  GObject parent;

  GHashTable *action_to_accels;
  GHashTable *accel_to_actions;
};

G_DEFINE_TYPE (GtkApplicationAccels, gtk_application_accels, G_TYPE_OBJECT)

static AccelKey *
accel_key_copy (const AccelKey *source)
{
  AccelKey *dest;

  dest = g_slice_new (AccelKey);
  dest->key = source->key;
  dest->modifier = source->modifier;

  return dest;
}

static void
accel_key_free (gpointer data)
{
  AccelKey *key = data;

  g_slice_free (AccelKey, key);
}

static guint
accel_key_hash (gconstpointer data)
{
  const AccelKey *key = data;

  return key->key + (key->modifier << 16);
}

static gboolean
accel_key_equal (gconstpointer a,
                 gconstpointer b)
{
  const AccelKey *ak = a;
  const AccelKey *bk = b;

  return ak->key == bk->key && ak->modifier == bk->modifier;
}

static void
add_entry (GtkApplicationAccels *accels,
           AccelKey             *key,
           const gchar          *action_and_target)
{
  const gchar **old;
  const gchar **new;
  gint n;

  old = g_hash_table_lookup (accels->accel_to_actions, key);
  if (old != NULL)
    for (n = 0; old[n]; n++)  /* find the length */
      ;
  else
    n = 0;

  new = g_new (const gchar *, n + 1 + 1);
  memcpy (new, old, n * sizeof (const gchar *));
  new[n] = action_and_target;
  new[n + 1] = NULL;

  g_hash_table_insert (accels->accel_to_actions, accel_key_copy (key), new);
}

static void
remove_entry (GtkApplicationAccels *accels,
              AccelKey             *key,
              const gchar          *action_and_target)
{
  const gchar **old;
  const gchar **new;
  gint n, i;

  /* if we can't find the entry then something has gone very wrong... */
  old = g_hash_table_lookup (accels->accel_to_actions, key);
  g_assert (old != NULL);

  for (n = 0; old[n]; n++)  /* find the length */
    ;
  g_assert_cmpint (n, >, 0);

  if (n == 1)
    {
      /* The simple case of removing the last action for an accel. */
      g_assert_cmpstr (old[0], ==, action_and_target);
      g_hash_table_remove (accels->accel_to_actions, key);
      return;
    }

  for (i = 0; i < n; i++)
    if (g_str_equal (old[i], action_and_target))
      break;

  /* We must have found it... */
  g_assert_cmpint (i, <, n);

  new = g_new (const gchar *, n - 1 + 1);
  memcpy (new, old, i * sizeof (const gchar *));
  memcpy (new + i, old + i + 1, (n - (i + 1)) * sizeof (const gchar *));
  new[n - 1] = NULL;

  g_hash_table_insert (accels->accel_to_actions, accel_key_copy (key), new);
}

static void
gtk_application_accels_finalize (GObject *object)
{
  GtkApplicationAccels *accels = GTK_APPLICATION_ACCELS (object);

  g_hash_table_unref (accels->accel_to_actions);
  g_hash_table_unref (accels->action_to_accels);

  G_OBJECT_CLASS (gtk_application_accels_parent_class)->finalize (object);
}

static void
gtk_application_accels_class_init (GtkApplicationAccelsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_application_accels_finalize;
}

static void
gtk_application_accels_init (GtkApplicationAccels *accels)
{
  accels->accel_to_actions = g_hash_table_new_full (accel_key_hash, accel_key_equal,
                                                    accel_key_free, g_free);
  accels->action_to_accels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

GtkApplicationAccels *
gtk_application_accels_new (void)
{
  return g_object_new (GTK_TYPE_APPLICATION_ACCELS, NULL);
}

void
gtk_application_accels_set_accels_for_action (GtkApplicationAccels *accels,
                                              const gchar          *detailed_action_name,
                                              const gchar * const  *accelerators)
{
  gchar *action_and_target;
  AccelKey *keys, *old_keys;
  gint i, n;

  action_and_target = gtk_normalise_detailed_action_name (detailed_action_name);

  n = accelerators ? g_strv_length ((gchar **) accelerators) : 0;

  if (n > 0)
    {
      keys = g_new0 (AccelKey, n + 1);

      for (i = 0; i < n; i++)
        {
          gtk_accelerator_parse (accelerators[i], &keys[i].key, &keys[i].modifier);

          if (keys[i].key == 0)
            {
              g_warning ("Unable to parse accelerator '%s': ignored request to install %d accelerators",
                         accelerators[i], n);
              g_free (action_and_target);
              g_free (keys);
              return;
            }
        }
    }
  else
    keys = NULL;

  old_keys = g_hash_table_lookup (accels->action_to_accels, action_and_target);
  if (old_keys)
    {
      /* We need to remove accel entries from existing keys */
      for (i = 0; old_keys[i].key; i++)
        remove_entry (accels, &old_keys[i], action_and_target);
    }

  if (keys)
    {
      g_hash_table_replace (accels->action_to_accels, action_and_target, keys);

      for (i = 0; i < n; i++)
        add_entry (accels, &keys[i], action_and_target);
    }
  else
    {
      g_hash_table_remove (accels->action_to_accels, action_and_target);
      g_free (action_and_target);
    }
}

gchar **
gtk_application_accels_get_accels_for_action (GtkApplicationAccels *accels,
                                              const gchar          *detailed_action_name)
{
  gchar *action_and_target;
  AccelKey *keys;
  gchar **result;
  gint n, i = 0;

  action_and_target = gtk_normalise_detailed_action_name (detailed_action_name);

  keys = g_hash_table_lookup (accels->action_to_accels, action_and_target);
  if (!keys)
    {
      g_free (action_and_target);
      return g_new0 (gchar *, 0 + 1);
    }

  for (n = 0; keys[n].key; n++)
    ;

  result = g_new0 (gchar *, n + 1);

  for (i = 0; i < n; i++)
    result[i] = gtk_accelerator_name (keys[i].key, keys[i].modifier);

  g_free (action_and_target);
  return result;
}

gchar **
gtk_application_accels_get_actions_for_accel (GtkApplicationAccels *accels,
                                              const gchar          *accel)
{
  const gchar * const *actions_and_targets;
  gchar **detailed_actions;
  AccelKey accel_key;
  guint i, n;

  gtk_accelerator_parse (accel, &accel_key.key, &accel_key.modifier);

  if (accel_key.key == 0)
    {
      g_critical ("invalid accelerator string '%s'", accel);
      g_return_val_if_fail (accel_key.key != 0, NULL);
    }

  actions_and_targets = g_hash_table_lookup (accels->accel_to_actions, &accel_key);
  n = actions_and_targets ? g_strv_length ((gchar **) actions_and_targets) : 0;

  detailed_actions = g_new0 (gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      const gchar *action_and_target = actions_and_targets[i];
      const gchar *sep;
      GVariant *target;

      sep = strrchr (action_and_target, '|');
      target = g_variant_parse (NULL, action_and_target, sep, NULL, NULL);
      detailed_actions[i] = g_action_print_detailed_name (sep + 1, target);
      if (target)
        g_variant_unref (target);
    }

  detailed_actions[n] = NULL;

  return detailed_actions;
}

gchar **
gtk_application_accels_list_action_descriptions (GtkApplicationAccels *accels)
{
  GHashTableIter iter;
  gchar **result;
  gint n, i = 0;
  gpointer key;

  n = g_hash_table_size (accels->action_to_accels);
  result = g_new (gchar *, n + 1);

  g_hash_table_iter_init (&iter, accels->action_to_accels);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      const gchar *action_and_target = key;
      const gchar *sep;
      GVariant *target;

      sep = strrchr (action_and_target, '|');
      target = g_variant_parse (NULL, action_and_target, sep, NULL, NULL);
      result[i++] = g_action_print_detailed_name (sep + 1, target);
      if (target)
        g_variant_unref (target);
    }
  g_assert_cmpint (i, ==, n);
  result[i] = NULL;

  return result;
}

void
gtk_application_accels_foreach_key (GtkApplicationAccels     *accels,
                                    GtkWindow                *window,
                                    GtkWindowKeysForeachFunc  callback,
                                    gpointer                  user_data)
{
  GHashTableIter iter;
  gpointer key;

  g_hash_table_iter_init (&iter, accels->accel_to_actions);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      AccelKey *accel_key = key;

      (* callback) (window, accel_key->key, accel_key->modifier, FALSE, user_data);
    }
}

gboolean
gtk_application_accels_activate (GtkApplicationAccels *accels,
                                 GActionGroup         *action_group,
                                 guint                 key,
                                 GdkModifierType       modifier)
{
  AccelKey accel_key = { key, modifier };
  const gchar **actions;
  gint i;

  actions = g_hash_table_lookup (accels->accel_to_actions, &accel_key);

  if (actions == NULL)
    return FALSE;

  /* We may have more than one action on a given accel.  This could be
   * the case if we have different types of windows with different
   * actions in each.
   *
   * Find the first one that will successfully activate and use it.
   */
  for (i = 0; actions[i]; i++)
    {
      const GVariantType *parameter_type;
      const gchar *action_name;
      const gchar *sep;
      gboolean enabled;
      GVariant *target;

      sep = strrchr (actions[i], '|');
      action_name = sep + 1;

      if (!g_action_group_query_action (action_group, action_name, &enabled, &parameter_type, NULL, NULL, NULL))
        continue;

      if (!enabled)
        continue;

      /* We found an action with the correct name and it's enabled.
       * This is the action that we are going to try to invoke.
       *
       * There is still the possibility that the target value doesn't
       * match the expected parameter type.  In that case, we will print
       * a warning.
       *
       * Note: we want to hold a ref on the target while we're invoking
       * the action to prevent trouble if someone uninstalls the accel
       * from the handler.  That's not a problem since we're parsing it.
       */
      if (actions[i] != sep) /* if it has a target... */
        {
          GError *error = NULL;

          if (parameter_type == NULL)
            {
              gchar *accel_str = gtk_accelerator_name (key, modifier);
              g_warning ("Accelerator '%s' tries to invoke action '%s' with target, but action has no parameter",
                         accel_str, action_name);
              g_free (accel_str);
              return TRUE;
            }

          target = g_variant_parse (NULL, actions[i], sep, NULL, &error);
          g_assert_no_error (error);
          g_assert (target);

          if (!g_variant_is_of_type (target, parameter_type))
            {
              gchar *accel_str = gtk_accelerator_name (key, modifier);
              gchar *typestr = g_variant_type_dup_string (parameter_type);
              gchar *targetstr = g_variant_print (target, TRUE);
              g_warning ("Accelerator '%s' tries to invoke action '%s' with target '%s',"
                         " but action expects parameter with type '%s'", accel_str, action_name, targetstr, typestr);
              g_variant_unref (target);
              g_free (targetstr);
              g_free (accel_str);
              g_free (typestr);
              return TRUE;
            }
        }
      else
        {
          if (parameter_type != NULL)
            {
              gchar *accel_str = gtk_accelerator_name (key, modifier);
              gchar *typestr = g_variant_type_dup_string (parameter_type);
              g_warning ("Accelerator '%s' tries to invoke action '%s' without target,"
                         " but action expects parameter with type '%s'", accel_str, action_name, typestr);
              g_free (accel_str);
              g_free (typestr);
              return TRUE;
            }

          target = NULL;
        }

      g_action_group_activate_action (action_group, action_name, target);

      if (target)
        g_variant_unref (target);

      return TRUE;
    }

  return FALSE;
}
