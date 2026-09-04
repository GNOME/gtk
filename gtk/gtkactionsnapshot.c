/* gtkactionsnapshot.c
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

#include "gtkactionsnapshotprivate.h"

static gboolean
variant_equal0 (GVariant *a,
                GVariant *b)
{
  return a == b || (a && b && g_variant_equal (a, b));
}

static gboolean
variant_type_equal0 (const GVariantType *a,
                     const GVariantType *b)
{
  return a == b || (a && b && g_variant_type_equal (a, b));
}

void
gtk_action_snapshot_init (GtkActionSnapshot *self)
{
  g_return_if_fail (self != NULL);

  *self = (GtkActionSnapshot) GTK_ACTION_SNAPSHOT_INIT;
}

GtkActionSnapshot *
gtk_action_snapshot_new (void)
{
  GtkActionSnapshot *self;

  self = g_new0 (GtkActionSnapshot, 1);
  gtk_action_snapshot_init (self);

  return self;
}

GtkActionSnapshot *
gtk_action_snapshot_ref (GtkActionSnapshot *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
gtk_action_snapshot_unref (GtkActionSnapshot *self)
{
  g_return_if_fail (self != NULL);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      gtk_action_snapshot_clear (self);
      g_free (self);
    }
}

void
gtk_action_snapshot_clear (GtkActionSnapshot *self)
{
  g_return_if_fail (self != NULL);

  g_clear_pointer (&self->parameter_type, g_variant_type_free);
  g_clear_pointer (&self->state_type, g_variant_type_free);
  g_clear_pointer (&self->state_hint, g_variant_unref);
  g_clear_pointer (&self->state, g_variant_unref);
  g_clear_pointer (&self->primary_accel, g_free);

  self->provider = NULL;
  self->revision = 0;
  self->ref_count = 1;
  self->present = FALSE;
  self->enabled = FALSE;
}

void
gtk_action_snapshot_copy (GtkActionSnapshot       *self,
                          const GtkActionSnapshot *other)
{
  GtkActionSnapshot copy = GTK_ACTION_SNAPSHOT_INIT;

  g_return_if_fail (self != NULL);
  g_return_if_fail (other != NULL);

  copy.parameter_type = other->parameter_type != NULL ? g_variant_type_copy (other->parameter_type) : NULL;
  copy.state_type = other->state_type != NULL ? g_variant_type_copy (other->state_type) : NULL;
  copy.state_hint = other->state_hint != NULL ? g_variant_ref (other->state_hint) : NULL;
  copy.state = other->state != NULL ? g_variant_ref (other->state) : NULL;
  copy.primary_accel = g_strdup (other->primary_accel);
  copy.provider = other->provider;
  copy.revision = other->revision;
  copy.present = other->present;
  copy.enabled = other->enabled;

  gtk_action_snapshot_clear (self);

  *self = copy;
}

GtkActionChange
gtk_action_snapshot_difference (const GtkActionSnapshot *old_snapshot,
                                const GtkActionSnapshot *new_snapshot)
{
  GtkActionChange changed = GTK_ACTION_CHANGE_NONE;

  g_return_val_if_fail (old_snapshot != NULL, GTK_ACTION_CHANGE_NONE);
  g_return_val_if_fail (new_snapshot != NULL, GTK_ACTION_CHANGE_NONE);

  if (old_snapshot->present != new_snapshot->present)
    changed |= GTK_ACTION_CHANGE_PRESENT;

  if (old_snapshot->provider != new_snapshot->provider)
    changed |= GTK_ACTION_CHANGE_PROVIDER;

  if (!variant_type_equal0 (old_snapshot->parameter_type, new_snapshot->parameter_type) ||
      !variant_type_equal0 (old_snapshot->state_type, new_snapshot->state_type) ||
      !variant_equal0 (old_snapshot->state_hint, new_snapshot->state_hint))
    changed |= GTK_ACTION_CHANGE_SIGNATURE;

  if (old_snapshot->enabled != new_snapshot->enabled)
    changed |= GTK_ACTION_CHANGE_ENABLED;

  if (!variant_equal0 (old_snapshot->state, new_snapshot->state))
    changed |= GTK_ACTION_CHANGE_STATE;

  if (g_strcmp0 (old_snapshot->primary_accel, new_snapshot->primary_accel) != 0)
    changed |= GTK_ACTION_CHANGE_ACCEL;

  return changed;
}

gboolean
gtk_action_snapshot_equal (const GtkActionSnapshot *a,
                           const GtkActionSnapshot *b)
{
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);

  return gtk_action_snapshot_difference (a, b) == GTK_ACTION_CHANGE_NONE &&
         a->revision == b->revision;
}
