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
  gsize char_offset;            /* offset in characters into entry's text */
  char *text_deleted;           /* text that will be deleted if we *redo* this command */
  char *text_entered;           /* text that will be entered if we *redo* this command */
};

G_DEFINE_TYPE_WITH_CODE (GtkEntryUndoCommand, gtk_entry_undo_command, GTK_TYPE_UNDO_COMMAND,
                         G_ADD_PRIVATE (GtkEntryUndoCommand))

static gboolean
gtk_entry_undo_command_run (GtkEntry   *entry,
                            int         position,
                            int         chars_to_delete,
                            const char *text_to_insert)
{
  GtkEntryUndoMode old_mode;

  if (entry == NULL)
    return FALSE;

  old_mode = gtk_entry_set_undo_mode (entry, GTK_ENTRY_UNDO_REPLAY);

  if (chars_to_delete)
    gtk_editable_delete_text (GTK_EDITABLE (entry),
                              position,
                              position + chars_to_delete);

  if (text_to_insert)
    gtk_editable_insert_text (GTK_EDITABLE (entry),
                              text_to_insert,
                              -1,
                              &position);

  gtk_editable_set_position (GTK_EDITABLE (entry), position);

  gtk_entry_set_undo_mode (entry, old_mode);

  return TRUE;
}

static gboolean
gtk_entry_undo_command_undo (GtkUndoCommand *command)
{
  GtkEntryUndoCommandPrivate *priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (command));

  return gtk_entry_undo_command_run (priv->entry,
                                     priv->char_offset,
                                     priv->text_entered ? g_utf8_strlen (priv->text_entered, -1) : 0,
                                     priv->text_deleted);
}

gboolean
gtk_entry_undo_command_redo (GtkUndoCommand *command)
{
  GtkEntryUndoCommandPrivate *priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (command));

  return gtk_entry_undo_command_run (priv->entry,
                                     priv->char_offset,
                                     priv->text_deleted ? g_utf8_strlen (priv->text_deleted, -1) : 0,
                                     priv->text_entered);
}

GtkUndoCommand *
gtk_entry_undo_command_merge (GtkUndoCommand *command,
                              GtkUndoCommand *followup)
{
  return NULL;
}

char *
gtk_entry_undo_command_describe (GtkUndoCommand *command)
{
  GtkEntryUndoCommandPrivate *priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (command));

  if (priv->text_entered)
    return g_strdup_printf (_("Entered `%s'"), priv->text_entered);
  else if (priv->text_deleted)
    return g_strdup_printf (_("Deleted `%s'"), priv->text_deleted);
  else
    return g_strdup (_("Did nothing"));
}

static void
gtk_entry_undo_command_finalize (GObject *object)
{
  GtkEntryUndoCommandPrivate *priv = gtk_entry_undo_command_get_instance_private (GTK_ENTRY_UNDO_COMMAND (object));

  g_free (priv->text_deleted);
  g_free (priv->text_entered);

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
gtk_entry_undo_command_new (GtkEntry   *entry,
                            int         position,
                            const char *text_deleted,
                            const char *text_entered)
{
  GtkEntryUndoCommand *result;
  GtkEntryUndoCommandPrivate *priv;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  result = g_object_new (GTK_TYPE_ENTRY_UNDO_COMMAND, NULL);

  priv = gtk_entry_undo_command_get_instance_private (result);
  priv->entry = entry;
  priv->char_offset = position;
  priv->text_deleted = g_strdup (text_deleted);
  priv->text_entered = g_strdup (text_entered);

  return GTK_UNDO_COMMAND (result);
}

