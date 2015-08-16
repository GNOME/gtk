/*
 * Copyright © 2015 Red Hat Inc.
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkentryundocommandprivate.h"

#include <glib/gi18n-lib.h>

#include "gtkentry.h"
#include "gtkentryprivate.h"

typedef struct _GtkEntryUndoCommandPrivate GtkEntryUndoCommandPrivate;

struct _GtkEntryUndoCommandPrivate {
  GtkEntry *entry;              /* entry we're operating on or NULL if entry was deleted */
  GtkEntrySnapshot before;      /* what we undo to */
  GtkEntrySnapshot after;       /* what we redo to */
};

G_DEFINE_TYPE_WITH_CODE (GtkEntryUndoCommand, gtk_entry_undo_command, GTK_TYPE_UNDO_COMMAND,
                         G_ADD_PRIVATE (GtkEntryUndoCommand))

void
gtk_entry_snapshot_init_from_entry (GtkEntrySnapshot *snapshot,
                                    GtkEntry         *entry)
{
  gint start, end;

  snapshot->text = g_strdup (gtk_entry_get_text (entry));
  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end);
  snapshot->cursor = gtk_editable_get_position (GTK_EDITABLE (entry));
  if (start == snapshot->cursor)
    snapshot->selection_start = end;
  else
    snapshot->selection_start = start;
}

void
gtk_entry_snapshot_clear (GtkEntrySnapshot *snapshot)
{
  g_free (snapshot->text);
  snapshot->text = NULL;
}

static void
gtk_entry_snapshot_copy (GtkEntrySnapshot       *target,
                         const GtkEntrySnapshot *source)
{
  target->text = g_strdup (source->text);
  target->cursor = source->cursor;
  target->selection_start = source->selection_start;
}

static gboolean
gtk_entry_undo_command_run (GtkEntry               *entry,
                            const GtkEntrySnapshot *snapshot)
{
  GtkEntryUndoMode old_mode;

  if (entry == NULL)
    return FALSE;

  old_mode = gtk_entry_set_undo_mode (entry, GTK_ENTRY_UNDO_REPLAY);

  gtk_entry_set_text (entry, snapshot->text);
  gtk_editable_select_region (GTK_EDITABLE (entry), snapshot->cursor, snapshot->selection_start);

  gtk_entry_set_undo_mode (entry, old_mode);

  return TRUE;
}

static gboolean
gtk_entry_undo_command_undo (GtkUndoCommand *command)
{
  GtkEntryUndoCommandPrivate *priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (command));

  return gtk_entry_undo_command_run (priv->entry, &priv->before);
}

gboolean
gtk_entry_undo_command_redo (GtkUndoCommand *command)
{
  GtkEntryUndoCommandPrivate *priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (command));

  return gtk_entry_undo_command_run (priv->entry, &priv->after);
}

static GtkUndoCommand *
gtk_entry_undo_command_new_from_snapshots (GtkEntry               *entry,
                                           const GtkEntrySnapshot *before,
                                           const GtkEntrySnapshot *after)
{
  GtkEntryUndoCommand *command;
  GtkEntryUndoCommandPrivate *priv;

  command = g_object_new (GTK_TYPE_ENTRY_UNDO_COMMAND, NULL);
  priv = gtk_entry_undo_command_get_instance_private (command);

  priv->entry = entry;
  gtk_entry_snapshot_copy (&priv->before, before);
  gtk_entry_snapshot_copy (&priv->after, after);

  return GTK_UNDO_COMMAND (command);
}

GtkUndoCommand *
gtk_entry_undo_command_merge (GtkUndoCommand *command,
                              GtkUndoCommand *followup)
{
  GtkEntryUndoCommandPrivate *command_priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (command));
  GtkEntryUndoCommandPrivate *followup_priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (followup));

  if (!GTK_IS_ENTRY_UNDO_COMMAND (followup))
    return NULL;

  if (command_priv->entry != followup_priv->entry)
    return NULL;

  if (!g_str_equal (command_priv->after.text, followup_priv->before.text))
    return NULL;

  /* We don't insist on cursor positions being equal here, someone
   * might ie. move the cursor to correct a typo
   */
  return gtk_entry_undo_command_new_from_snapshots (command_priv->entry, &command_priv->before, &followup_priv->after);
}

static guint
get_prefix_len (const char *str1,
                const char *str2)
{
  guint i;

  for (i = 0; str1[i] == str2[i] && str1[i] != 0; i++)
    {
      /* nothing to do here */
    }

  return i;
}

static guint
get_suffix_len (const char *str1,
                guint       len1,
                const char *str2,
                guint       len2)
{
  const char *cur1, *cur2;
  guint i, max_len;

  cur1 = str1 + len1 - 1;
  cur2 = str2 + len2 - 1;
  max_len = MIN (len1, len2);

  for (i = 0; *cur1 == *cur2 && i < max_len; i++)
    {
      cur1--;
      cur2--;
    }

  return i;
}

char *
gtk_entry_undo_command_describe (GtkUndoCommand *command)
{
  GtkEntryUndoCommandPrivate *priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (command));
  guint before_len, after_len, prefix_len, suffix_len;

  before_len = strlen (priv->before.text);
  after_len = strlen (priv->after.text);
  prefix_len = get_prefix_len (priv->before.text, priv->after.text);
  suffix_len = get_suffix_len (priv->before.text, before_len,
                               priv->after.text, after_len);

  if (before_len == after_len && before_len == prefix_len)
    return g_strdup (_("No changes")); /* huh? */
  else if (prefix_len + suffix_len == before_len)
    return g_strdup_printf (_("Entered `%*s'"), after_len - prefix_len - suffix_len, priv->after.text + prefix_len);
  else if (prefix_len + suffix_len == after_len)
    return g_strdup_printf (_("Deleted `%*s'"), before_len - prefix_len - suffix_len, priv->before.text + prefix_len);
  else
    return g_strdup (_("Text changed"));
}

static void
gtk_entry_undo_command_finalize (GObject *object)
{
  GtkEntryUndoCommandPrivate *priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (object));

  gtk_entry_snapshot_clear (&priv->before);
  gtk_entry_snapshot_clear (&priv->after);

  G_OBJECT_CLASS (gtk_entry_undo_command_parent_class)->finalize (object);
}

static void
gtk_entry_undo_command_class_init (GtkEntryUndoCommandClass *klass)
{
  GtkUndoCommandClass *undo_class = GTK_UNDO_COMMAND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_entry_undo_command_finalize;

  undo_class->undo = gtk_entry_undo_command_undo;
  undo_class->redo = gtk_entry_undo_command_redo;
  undo_class->merge = gtk_entry_undo_command_merge;
  undo_class->describe = gtk_entry_undo_command_describe;
}

static void
gtk_entry_undo_command_init (GtkEntryUndoCommand *command)
{
}

GtkUndoCommand *
gtk_entry_undo_command_new (GtkEntry               *entry,
                            const GtkEntrySnapshot *before)
{
  GtkEntrySnapshot after;
  GtkUndoCommand *result;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (before != NULL, NULL);

  gtk_entry_snapshot_init_from_entry (&after, entry);

  result = gtk_entry_undo_command_new_from_snapshots (entry, before, &after);
  
  gtk_entry_snapshot_clear (&after);

  return result;
}

