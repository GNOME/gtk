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

#include "gtkundocommandprivate.h"

#include <glib/gi18n-lib.h>

typedef struct _GtkUndoCommandPrivate GtkUndoCommandPrivate;
struct _GtkUndoCommandPrivate {
  gint64 timestamp;
};

G_DEFINE_TYPE_WITH_CODE (GtkUndoCommand, gtk_undo_command, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkUndoCommand))

static gboolean
gtk_undo_command_real_undo (GtkUndoCommand *command)
{
  g_warning ("%s class failed to implement undo", G_OBJECT_TYPE_NAME (command));

  return FALSE;
}

gboolean
gtk_undo_command_real_redo (GtkUndoCommand *command)
{
  g_warning ("%s class failed to implement redo", G_OBJECT_TYPE_NAME (command));

  return FALSE;
}

GtkUndoCommand *
gtk_undo_command_real_merge (GtkUndoCommand *command,
                             GtkUndoCommand *followup)
{
  return NULL;
}

char *
gtk_undo_command_real_describe (GtkUndoCommand *command)
{
  g_warning ("%s class failed to implement undo", G_OBJECT_TYPE_NAME (command));

  /* translators: This string is the fallback message that gets used
   * when undo commands are improperly implemented and don't have a
   * proper description. */
  return g_strdup (_("unknown undo command"));
}

static void
gtk_undo_command_class_init (GtkUndoCommandClass *klass)
{
  klass->undo = gtk_undo_command_real_undo;
  klass->redo = gtk_undo_command_real_redo;
  klass->merge = gtk_undo_command_real_merge;
  klass->describe = gtk_undo_command_real_describe;
}

static void
gtk_undo_command_init (GtkUndoCommand *command)
{
  GtkUndoCommandPrivate *priv = gtk_undo_command_get_instance_private (command);

  priv->timestamp = g_get_real_time ();
}

gboolean
gtk_undo_command_undo (GtkUndoCommand *command)
{
  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (command), FALSE);

  return GTK_UNDO_COMMAND_GET_CLASS (command)->undo (command);
}

gboolean
gtk_undo_command_redo (GtkUndoCommand *command)
{
  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (command), FALSE);

  return GTK_UNDO_COMMAND_GET_CLASS (command)->redo (command);
}

GtkUndoCommand *
gtk_undo_command_merge (GtkUndoCommand *command,
                        GtkUndoCommand *followup_command)
{
  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (command), NULL);
  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (followup_command), NULL);

  return GTK_UNDO_COMMAND_GET_CLASS (command)->merge (command, followup_command);
}

char *
gtk_undo_command_describe (GtkUndoCommand *command)
{
  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (command), NULL);

  return GTK_UNDO_COMMAND_GET_CLASS (command)->describe (command);
}
