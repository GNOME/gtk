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

#include "gtkundoundocommandprivate.h"

#include <glib/gi18n-lib.h>

typedef struct _GtkUndoUndoCommandPrivate GtkUndoUndoCommandPrivate;
struct _GtkUndoUndoCommandPrivate {
  GSequence *commands;          /* sequence of GtkUndoCommand, first to undo command first */
};

G_DEFINE_TYPE_WITH_CODE (GtkUndoUndoCommand, gtk_undo_undo_command, GTK_TYPE_UNDO_COMMAND,
                         G_ADD_PRIVATE (GtkUndoUndoCommand))

static gboolean
gtk_undo_undo_command_undo (GtkUndoCommand *command)
{
  GtkUndoUndoCommandPrivate *priv = gtk_undo_undo_command_get_instance_private (GTK_UNDO_UNDO_COMMAND (command));
  GSequenceIter *iter;
  gboolean result;

  result = FALSE;

  for (iter = g_sequence_get_begin_iter (priv->commands);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      result |= gtk_undo_command_redo (g_sequence_get (iter));
    }

  return result;
}

gboolean
gtk_undo_undo_command_redo (GtkUndoCommand *command)
{
  GtkUndoUndoCommandPrivate *priv = gtk_undo_undo_command_get_instance_private (GTK_UNDO_UNDO_COMMAND (command));
  GSequenceIter *iter;
  gboolean result;

  result = FALSE;

  iter = g_sequence_get_end_iter (priv->commands);
  while (!g_sequence_iter_is_begin (iter))
    {
      iter = g_sequence_iter_prev (iter);

      result |= gtk_undo_command_undo (g_sequence_get (iter));
    }

  return result;
}

gboolean
gtk_undo_undo_command_should_merge (GtkUndoCommand *command,
                                    GtkUndoCommand *followup)
{
  return FALSE;
}

static void
gtk_undo_undo_command_finalize (GObject *object)
{
  GtkUndoUndoCommandPrivate *priv = gtk_undo_undo_command_get_instance_private (GTK_UNDO_UNDO_COMMAND (object));

  g_sequence_free (priv->commands);

  G_OBJECT_CLASS (gtk_undo_undo_command_parent_class)->finalize (object);
}

static void
gtk_undo_undo_command_class_init (GtkUndoUndoCommandClass *klass)
{
  GtkUndoCommandClass *undo_class = GTK_UNDO_COMMAND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_undo_undo_command_finalize;

  undo_class->undo = gtk_undo_undo_command_undo;
  undo_class->redo = gtk_undo_undo_command_redo;
  undo_class->should_merge = gtk_undo_undo_command_should_merge;
}

static void
gtk_undo_undo_command_init (GtkUndoUndoCommand *command)
{
  GtkUndoUndoCommandPrivate *priv = gtk_undo_undo_command_get_instance_private (GTK_UNDO_UNDO_COMMAND (command));

  priv->commands = g_sequence_new (g_object_unref);
}

GtkUndoCommand *
gtk_undo_undo_command_new (GSequenceIter *begin_iter,
                           GSequenceIter *end_iter)
{
  GtkUndoUndoCommand *result;
  GtkUndoUndoCommandPrivate *priv;
  GSequenceIter *iter;
  char *title;

  g_return_val_if_fail (begin_iter != NULL, NULL);
  g_return_val_if_fail (end_iter != NULL, NULL);

  result = g_object_new (GTK_TYPE_UNDO_UNDO_COMMAND, NULL);

  priv = gtk_undo_undo_command_get_instance_private (result);
  for (iter = begin_iter; iter != end_iter; iter = g_sequence_iter_next (iter))
    {
      g_sequence_prepend (priv->commands, g_object_ref (g_sequence_get (iter)));
    }

  title = g_strdup_printf (_("Undo last %u commands"), g_sequence_get_length (priv->commands));
  gtk_undo_command_set_title (GTK_UNDO_COMMAND (result), title);
  g_free (title);

  return GTK_UNDO_COMMAND (result);
}

guint
gtk_undo_undo_command_get_n_items (GtkUndoUndoCommand *command)
{
  GtkUndoUndoCommandPrivate *priv = gtk_undo_undo_command_get_instance_private (GTK_UNDO_UNDO_COMMAND (command));

  g_return_val_if_fail (GTK_IS_UNDO_UNDO_COMMAND (command), 0);

  return g_sequence_get_length (priv->commands);
}

