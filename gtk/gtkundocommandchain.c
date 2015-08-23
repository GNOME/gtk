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

#include "gtkundocommandchainprivate.h"

#include <glib/gi18n-lib.h>
#include <string.h>

typedef struct _GtkUndoCommandChainPrivate GtkUndoCommandChainPrivate;

struct _GtkUndoCommandChainPrivate {
  GtkUndoCommand **commands;
  gsize            n_commands;
};

G_DEFINE_TYPE_WITH_CODE (GtkUndoCommandChain, gtk_undo_command_chain, GTK_TYPE_UNDO_COMMAND,
                         G_ADD_PRIVATE (GtkUndoCommandChain))

static gboolean
gtk_undo_command_chain_undo (GtkUndoCommand *command)
{
  GtkUndoCommandChainPrivate *priv = gtk_undo_command_chain_get_instance_private (GTK_UNDO_COMMAND_CHAIN (command));
  gboolean result;
  gsize i;

  result = FALSE;

  for (i = priv->n_commands; i --> 0; )
    {
      result |= gtk_undo_command_undo (priv->commands[i]);
    }

  return result;
}

gboolean
gtk_undo_command_chain_redo (GtkUndoCommand *command)
{
  GtkUndoCommandChainPrivate *priv = gtk_undo_command_chain_get_instance_private (GTK_UNDO_COMMAND_CHAIN (command));
  gboolean result;
  gsize i;

  result = FALSE;

  for (i = 0; i < priv->n_commands; i++)
    {
      result |= gtk_undo_command_redo (priv->commands[i]);
    }

  return result;
}

GtkUndoCommand *
gtk_undo_command_chain_merge (GtkUndoCommand *command,
                              GtkUndoCommand *followup)
{
  return gtk_undo_command_chain_new_merge (command, followup);
}

static void
gtk_undo_command_chain_dispose (GObject *object)
{
  GtkUndoCommandChainPrivate *priv = gtk_undo_command_chain_get_instance_private (GTK_UNDO_COMMAND_CHAIN (object));
  gsize i;

  for (i = 0; i < priv->n_commands; i++)
    {
      g_object_unref (priv->commands[i]);
    }
  g_free (priv->commands);
  priv->n_commands = 0;

  G_OBJECT_CLASS (gtk_undo_command_chain_parent_class)->dispose (object);
}

static void
gtk_undo_command_chain_class_init (GtkUndoCommandChainClass *klass)
{
  GtkUndoCommandClass *undo_class = GTK_UNDO_COMMAND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_undo_command_chain_dispose;

  undo_class->undo = gtk_undo_command_chain_undo;
  undo_class->redo = gtk_undo_command_chain_redo;
  undo_class->merge = gtk_undo_command_chain_merge;
}

static void
gtk_undo_command_chain_init (GtkUndoCommandChain *command)
{
}

/**
 * gtk_undo_command_chain_new:
 * @commands: (array length=n_commands): commands to chain
 * @n_commands: number of commands
 *
 * Creates a new command that undoes or redoes all given @commands
 * in order. The first command in the @commands array is the oldest
 * command. So it is the first command to be executed during redo
 * and the last command executed during undo.
 *
 * Returns: a new #GtkUndoCommand
 */
GtkUndoCommand *
gtk_undo_command_chain_new (GtkUndoCommand **commands,
                            gsize            n_commands)
{
  GtkUndoCommandChainPrivate *priv;
  GtkUndoCommand *result;
  char *title;
  gsize i;

  g_return_val_if_fail (commands != NULL, NULL);
  g_return_val_if_fail (n_commands > 0, NULL);

  title = g_strdup_printf (_("Execute %zu commands"), n_commands);
  result = g_object_new (GTK_TYPE_UNDO_COMMAND_CHAIN,
                         "timestamp", gtk_undo_command_get_timestamp (commands[n_commands - 1]),
                         "title", title,
                         NULL);
  g_free (title);
  priv = gtk_undo_command_chain_get_instance_private (GTK_UNDO_COMMAND_CHAIN (result));

  priv->commands = g_new (GtkUndoCommand *, n_commands);
  priv->n_commands = n_commands;
  for (i = 0; i < n_commands; i++)
    {
      priv->commands[i] = g_object_ref (commands[i]);
    }

  return result;
}

/**
 * gtk_undo_command_chain_new_merge:
 * @command: first command to merge
 * @followup: followup command to merge
 *
 * Merges the two given command. This is a convenience function for use
 * in GtkUndCommandClass::merge implementations as a fallback.
 *
 * Returns: A new command
 */
GtkUndoCommand *
gtk_undo_command_chain_new_merge (GtkUndoCommand *command,
                                  GtkUndoCommand *followup)
{
  GtkUndoCommand **commands, **first;
  GtkUndoCommand *result;
  gsize n_commands, n_first;

  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (command), NULL);
  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (followup), NULL);

  if (GTK_IS_UNDO_COMMAND_CHAIN (command))
    {
      GtkUndoCommandChainPrivate *priv = gtk_undo_command_chain_get_instance_private (GTK_UNDO_COMMAND_CHAIN (command));

      n_commands = n_first = priv->n_commands;
      first = priv->commands;
    }
  else
    {
      n_commands = n_first = 1;
      first = &command;
    }

  if (GTK_IS_UNDO_COMMAND_CHAIN (followup))
    {
      GtkUndoCommandChainPrivate *priv = gtk_undo_command_chain_get_instance_private (GTK_UNDO_COMMAND_CHAIN (followup));

      n_commands += priv->n_commands;
      commands = g_new (GtkUndoCommand *, n_commands);
      memcpy (commands + n_first, priv->commands, sizeof (GtkUndoCommand *) * priv->n_commands);
    }
  else
    {
      n_commands++;
      commands = g_new (GtkUndoCommand *, n_commands);
      commands[n_first] = followup;
    }
  memcpy (commands, first, sizeof (GtkUndoCommand *) * n_first);

  result = gtk_undo_command_chain_new (commands, n_commands);

  g_free (commands);

  return result;
}
