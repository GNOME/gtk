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

#include "gtkundorecorderprivate.h"

#include <glib/gi18n-lib.h>

#include "gtkundocommandchainprivate.h"

typedef struct _GtkUndoRecorderPrivate GtkUndoRecorderPrivate;
struct _GtkUndoRecorderPrivate {
  GSList *commands;
};

G_DEFINE_TYPE_WITH_CODE (GtkUndoRecorder, gtk_undo_recorder, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkUndoRecorder))

static void
gtk_undo_recorder_dispose (GObject *object)
{
  gtk_undo_recorder_clear (GTK_UNDO_RECORDER (object));

  G_OBJECT_CLASS (gtk_undo_recorder_parent_class)->dispose (object);
}

static void
gtk_undo_recorder_class_init (GtkUndoRecorderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_undo_recorder_dispose;
}

static void
gtk_undo_recorder_init (GtkUndoRecorder *recorder)
{
}

GtkUndoRecorder *
gtk_undo_recorder_new (void)
{
  return g_object_new (GTK_TYPE_UNDO_RECORDER, NULL);
}

void
gtk_undo_recorder_push (GtkUndoRecorder *recorder,
                        GtkUndoCommand  *command)
{
  GtkUndoRecorderPrivate *priv = gtk_undo_recorder_get_instance_private (recorder);

  g_return_if_fail (GTK_IS_UNDO_RECORDER (recorder));
  g_return_if_fail (GTK_IS_UNDO_COMMAND (command));

  if (priv->commands)
    {
      GtkUndoCommand *merge;

      merge = gtk_undo_command_merge (priv->commands->data, command);
      if (merge == NULL)
        {
          /* undo commands cancel out */
          g_object_unref (priv->commands->data);
          priv->commands = g_slist_remove (priv->commands, priv->commands->data);
          return;
        }
      else if (GTK_IS_UNDO_COMMAND_CHAIN (merge))
        {
        }
    }
  else
    {
      g_object_ref (command);
    }

  priv->commands = g_slist_prepend (priv->commands, command);
}

GtkUndoCommand *
gtk_undo_recorder_create_command_chain (const GSList *list)
{
  GtkUndoCommand **commands;
  GtkUndoCommand *result;
  gsize i, n_commands;

  n_commands = g_slist_length ((GSList *) list);
  commands = g_newa (GtkUndoCommand *, n_commands);

  for (i = 0; i < n_commands; i++)
    {
      commands[i] = list->data;
      list = list->next;
    }

  result = gtk_undo_command_chain_new (commands, n_commands);

  return result;
}

GtkUndoCommand *
gtk_undo_recorder_finish (GtkUndoRecorder *recorder)
{
  GtkUndoRecorderPrivate *priv = gtk_undo_recorder_get_instance_private (recorder);
  GtkUndoCommand *result;

  g_return_val_if_fail (GTK_IS_UNDO_RECORDER (recorder), NULL);

  if (priv->commands == NULL)
    {
      result = NULL;
    }
  else if (priv->commands->next == NULL)
    {
      result = g_object_ref (priv->commands->data);
    }
  else
    {
      result = gtk_undo_recorder_create_command_chain (priv->commands);
    }

  gtk_undo_recorder_clear (recorder);

  return result;
}

void
gtk_undo_recorder_clear (GtkUndoRecorder *recorder)
{
  GtkUndoRecorderPrivate *priv = gtk_undo_recorder_get_instance_private (recorder);

  g_return_if_fail (GTK_IS_UNDO_RECORDER (recorder));

  g_slist_free_full (priv->commands, g_object_unref);

  priv->commands = NULL;
}

