/* gtkactionsnapshotprivate.h
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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum _GtkActionChange
{
  GTK_ACTION_CHANGE_NONE      = 0,
  GTK_ACTION_CHANGE_PRESENT   = 1 << 0,
  GTK_ACTION_CHANGE_PROVIDER  = 1 << 1,
  GTK_ACTION_CHANGE_SIGNATURE = 1 << 2,
  GTK_ACTION_CHANGE_ENABLED   = 1 << 3,
  GTK_ACTION_CHANGE_STATE     = 1 << 4,
  GTK_ACTION_CHANGE_ACTIVE    = 1 << 5,
  GTK_ACTION_CHANGE_ROLE      = 1 << 6,
  GTK_ACTION_CHANGE_ACCEL     = 1 << 7,
} GtkActionChange;

typedef enum _GtkActionInterest
{
  GTK_ACTION_INTEREST_NONE      = 0,
  GTK_ACTION_INTEREST_PRESENT   = 1 << 0,
  GTK_ACTION_INTEREST_ENABLED   = 1 << 1,
  GTK_ACTION_INTEREST_ACTIVE    = 1 << 2,
  GTK_ACTION_INTEREST_ROLE      = 1 << 3,
  GTK_ACTION_INTEREST_ACCEL     = 1 << 4,
  GTK_ACTION_INTEREST_RAW_STATE = 1 << 5,
} GtkActionInterest;

typedef struct _GtkActionSnapshot
{
  GVariantType *parameter_type;
  GVariantType *state_type;
  GVariant     *state_hint;
  GVariant     *state;
  char         *primary_accel;
  gpointer      provider;
  guint64       revision;
  int           ref_count;
  guint         present : 1;
  guint         enabled : 1;
} GtkActionSnapshot;

#define GTK_ACTION_SNAPSHOT_INIT { .ref_count = 1 }

void               gtk_action_snapshot_init       (GtkActionSnapshot       *self);
GtkActionSnapshot *gtk_action_snapshot_new        (void);
GtkActionSnapshot *gtk_action_snapshot_ref        (GtkActionSnapshot       *self);
void               gtk_action_snapshot_unref      (GtkActionSnapshot       *self);
void               gtk_action_snapshot_clear      (GtkActionSnapshot       *self);
void               gtk_action_snapshot_copy       (GtkActionSnapshot       *self,
                                                   const GtkActionSnapshot *other);
gboolean           gtk_action_snapshot_equal      (const GtkActionSnapshot *a,
                                                   const GtkActionSnapshot *b);
GtkActionChange    gtk_action_snapshot_difference (const GtkActionSnapshot *old_snapshot,
                                                   const GtkActionSnapshot *new_snapshot);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkActionSnapshot, gtk_action_snapshot_unref)

G_END_DECLS
