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

#include "gtkundostackprivate.h"

#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#include "gtkundoundocommandprivate.h"

typedef struct _GtkUndoStackPrivate GtkUndoStackPrivate;
struct _GtkUndoStackPrivate {
  GSequence *commands;          /* latest added command is at front of queue */
};

static void gtk_undo_stack_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkUndoStack, gtk_undo_stack, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkUndoStack);
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_undo_stack_list_model_iface_init));

static GType
gtk_undo_stack_list_model_get_item_type (GListModel *list)
{
  return GTK_TYPE_UNDO_COMMAND;
}

static guint
gtk_undo_stack_list_model_get_n_items (GListModel *list)
{
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (GTK_UNDO_STACK (list));

  return g_sequence_get_length (priv->commands);
}

static gpointer
gtk_undo_stack_list_model_get_item (GListModel *list,
                                    guint       position)
{
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (GTK_UNDO_STACK (list));
  guint len;

  /* XXX: This is sloooow. */
  len = g_sequence_get_length (priv->commands);
  if (position >= len)
    return NULL;

  return g_object_ref (g_sequence_get (g_sequence_get_iter_at_pos (priv->commands, len - position - 1)));
}

static void
gtk_undo_stack_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_undo_stack_list_model_get_item_type;
  iface->get_n_items = gtk_undo_stack_list_model_get_n_items;
  iface->get_item = gtk_undo_stack_list_model_get_item;
}

static void
gtk_undo_stack_dispose (GObject *object)
{
  GtkUndoStack *stack = GTK_UNDO_STACK (object);

  gtk_undo_stack_clear (stack);

  G_OBJECT_CLASS (gtk_undo_stack_parent_class)->dispose (object);
}

static void
gtk_undo_stack_finalize (GObject *object)
{
  GtkUndoStack *stack = GTK_UNDO_STACK (object);
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (stack);

  g_sequence_free (priv->commands);

  G_OBJECT_CLASS (gtk_undo_stack_parent_class)->finalize (object);
}

static void
gtk_undo_stack_class_init (GtkUndoStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_undo_stack_dispose;
  object_class->finalize = gtk_undo_stack_finalize;
}

static void
gtk_undo_stack_init (GtkUndoStack *stack)
{
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (stack);

  priv->commands = g_sequence_new (g_object_unref);
}

GtkUndoStack *
gtk_undo_stack_new (void)
{
  return g_object_new (GTK_TYPE_UNDO_STACK, NULL);
}

void
gtk_undo_stack_clear (GtkUndoStack *stack)
{
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (stack);
  guint len;

  g_return_if_fail (GTK_IS_UNDO_STACK (stack));

  len = g_sequence_get_length (priv->commands);

  g_sequence_remove_range (g_sequence_get_begin_iter (priv->commands),
                           g_sequence_get_end_iter (priv->commands));

  g_list_model_items_changed (G_LIST_MODEL (stack), 0, len, 0);
}

static void G_GNUC_UNUSED
gtk_undo_stack_dump (GtkUndoStack *stack)
{
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (stack);
  GSequenceIter *iter;

  g_print ("Undo Stack @ %p (%u items)\n", stack, g_sequence_get_length (priv->commands));

  for (iter = g_sequence_get_begin_iter (priv->commands);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      GtkUndoCommand *command = g_sequence_get (iter);

      g_print ("  %s\n", gtk_undo_command_get_title (command));
    }
}

static void
gtk_undo_stack_push_internal (GtkUndoStack   *stack,
                              GtkUndoCommand *command,
                              gboolean        replace)
{
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (stack);

  if (replace)
    g_sequence_remove (g_sequence_get_begin_iter (priv->commands));

  if (command)
    {
      g_object_ref (command);
      g_sequence_prepend (priv->commands, command);
    }

  g_list_model_items_changed (G_LIST_MODEL (stack), 0,
                              replace ? 1 : 0,
                              command ? 1 : 0);

  gtk_undo_stack_dump (stack);
}

void
gtk_undo_stack_push (GtkUndoStack   *stack,
                     GtkUndoCommand *command)
{
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (stack);
  GSequenceIter *begin_iter;
  GtkUndoCommand *previous, *merge;

  g_return_if_fail (GTK_IS_UNDO_STACK (stack));
  g_return_if_fail (GTK_IS_UNDO_COMMAND (command));

  begin_iter = g_sequence_get_begin_iter (priv->commands);
  if (!g_sequence_iter_is_end (begin_iter))
    {
      previous = g_sequence_get (begin_iter);
      if (gtk_undo_command_should_merge (previous, command))
        {
          merge = gtk_undo_command_merge (previous, command);
          gtk_undo_stack_push_internal (stack, merge, TRUE);
          if (merge)
            g_object_unref (merge);
          return;
        }
    }

  gtk_undo_stack_push_internal (stack, command, FALSE);
}

/* n_commands negative means we redo */
static gboolean
gtk_undo_stack_run_undo (GtkUndoStack *stack,
                         int           n_commands)
{
  GtkUndoStackPrivate *priv = gtk_undo_stack_get_instance_private (stack);
  GSequenceIter *begin_iter, *end_iter;
  GtkUndoCommand *command;
  int prev_commands, total_commands;
  gboolean replace = FALSE;

  begin_iter = g_sequence_get_begin_iter (priv->commands);
  if (g_sequence_iter_is_end (begin_iter))
    return FALSE;

  command = g_sequence_get (begin_iter);
  if (GTK_IS_UNDO_UNDO_COMMAND (command))
    {
      begin_iter = g_sequence_iter_next (begin_iter);
      prev_commands = gtk_undo_undo_command_get_n_items (GTK_UNDO_UNDO_COMMAND (command));
      total_commands = g_sequence_get_length (priv->commands) - 1;
      replace = TRUE;
    }
  else
    {
      prev_commands = 0;
      total_commands = g_sequence_get_length (priv->commands);
      replace = FALSE;
    }

  n_commands = CLAMP (prev_commands + n_commands, 0, total_commands);
  if (n_commands == prev_commands)
    return FALSE;

  end_iter = g_sequence_iter_move (begin_iter, prev_commands);
  while (prev_commands < n_commands)
    {
      GtkUndoCommand *undo = g_sequence_get (end_iter);
      gtk_undo_command_undo (undo);
      end_iter = g_sequence_iter_next (end_iter);
      prev_commands++;
    }
  while (prev_commands > n_commands)
    {
      GtkUndoCommand *redo;
      end_iter = g_sequence_iter_prev (end_iter);
      prev_commands--;
      redo = g_sequence_get (end_iter);
      gtk_undo_command_redo (redo);
    }

  command = gtk_undo_undo_command_new (begin_iter, end_iter);
  gtk_undo_stack_push_internal (stack, command, replace);
  g_object_unref (command);

  return TRUE;
}

gboolean
gtk_undo_stack_undo (GtkUndoStack *stack)
{
  g_return_val_if_fail (GTK_IS_UNDO_STACK (stack), FALSE);

  return gtk_undo_stack_run_undo (stack, 1);
}

gboolean
gtk_undo_stack_redo (GtkUndoStack *stack)
{
  g_return_val_if_fail (GTK_IS_UNDO_STACK (stack), FALSE);

  return gtk_undo_stack_run_undo (stack, -1);
}
