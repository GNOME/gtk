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
  char *title;
};

enum {
  PROP_0,
  PROP_TIMESTAMP,
  PROP_TITLE,
  /* add more */
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE_WITH_CODE (GtkUndoCommand, gtk_undo_command, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkUndoCommand))

static void
gtk_undo_command_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkUndoCommandPrivate *priv = gtk_undo_command_get_instance_private (GTK_UNDO_COMMAND (object));

  switch (property_id)
    {
    case PROP_TIMESTAMP:
      g_value_set_int64 (value, priv->timestamp);
      break;

    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_undo_command_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkUndoCommand *command = GTK_UNDO_COMMAND (object);
  GtkUndoCommandPrivate *priv = gtk_undo_command_get_instance_private (command);

  switch (property_id)
    {
    case PROP_TIMESTAMP:
      priv->timestamp = g_value_get_int64 (value);
      if (priv->timestamp == 0)
        priv->timestamp = g_get_real_time ();
      break;

    case PROP_TITLE:
      gtk_undo_command_set_title (command, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_undo_command_finalize (GObject *object)
{
  GtkUndoCommandPrivate *priv = gtk_undo_command_get_instance_private (GTK_UNDO_COMMAND (object));

  g_free (priv->title);

  G_OBJECT_CLASS (gtk_undo_command_parent_class)->finalize (object);
}

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

gboolean
gtk_undo_command_real_should_merge (GtkUndoCommand *command,
                                    GtkUndoCommand *followup)
{
  GtkUndoCommandPrivate *command_priv = gtk_undo_command_get_instance_private (command);
  GtkUndoCommandPrivate *followup_priv = gtk_undo_command_get_instance_private (followup);

  if (followup_priv->timestamp - command_priv->timestamp > 5 * G_USEC_PER_SEC)
    return FALSE;

  return TRUE;
}

static void
gtk_undo_command_class_init (GtkUndoCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_undo_command_set_property;
  object_class->get_property = gtk_undo_command_get_property;
  object_class->finalize = gtk_undo_command_finalize;

  klass->undo = gtk_undo_command_real_undo;
  klass->redo = gtk_undo_command_real_redo;
  klass->merge = gtk_undo_command_real_merge;
  klass->should_merge = gtk_undo_command_real_should_merge;

  /*
   * GtkUndoCommand:timestamp:
   *
   * Timestamp this command was recorded at. See
   * gtk_undo_command_get_timestamp() for details.
   */
  properties[PROP_TIMESTAMP] = g_param_spec_int64 ("timestamp", "Timestamp",
                                                   "Time at which this command was recorded",
                                                   0, G_MAXINT64, 0,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                   G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /*
   * GtkUndoCommand:title:
   *
   * Title of this command. See
   * gtk_undo_command_get_title() for details.
   */
  properties[PROP_TITLE] = g_param_spec_string ("title", "Title",
                                                "Title of this command",
                                                NULL,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_undo_command_init (GtkUndoCommand *command)
{
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

/*
 * gtk_undo_command_should_merge:
 * @command: The command that would be undone second
 * @followup_command: The command that would be undone first
 *
 * Determines if @command and @followup_comand should be merged for
 * undo. That is, it determines if when the user triggers an undo
 * (for example by pressing Ctrl-z), if both commands should be
 * undone at once or if they should require two seperate undos.
 *
 * Returns: %TRUE if the commands should be merged for UI purposes.
 */
gboolean
gtk_undo_command_should_merge (GtkUndoCommand *command,
                               GtkUndoCommand *followup_command)
{
  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (command), FALSE);
  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (followup_command), FALSE);

  return GTK_UNDO_COMMAND_GET_CLASS (command)->should_merge (command, followup_command);
}

/*
 * gtk_undo_command_get_title:
 * @command: The command
 *
 * Gets the title of the @command. The title is a translated string
 * for presentation in a user interface, for example a list of actions
 * to undo.
 *
 * Undo commands always have a title set, but that title is often generic.
 * Applications may want to update titles via gtk_undo_command_set_title()
 *
 * Returns: The title for the command
 */
const char *
gtk_undo_command_get_title (GtkUndoCommand *command)
{
  GtkUndoCommandPrivate *priv = gtk_undo_command_get_instance_private (command);

  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (command), NULL);

  return priv->title;
}

/*
 * gtk_undo_command_set_title:
 * @command: The command
 * @title: The new title
 *
 * Updates the title for the given @command. This function should be called 
 * when applications can provide a better title for an action than the generic
 * title provided upon creation of commands.
 */
void
gtk_undo_command_set_title (GtkUndoCommand *command,
                            const char     *title)
{
  GtkUndoCommandPrivate *priv = gtk_undo_command_get_instance_private (command);

  g_return_if_fail (GTK_IS_UNDO_COMMAND (command));

  if (title == NULL)
    /* translators: This is the (hopefully never used) fallback string for undo commands without a name */
    title = _("Unknown command");

  if (g_strcmp0 (priv->title, title) == 0)
    return;

  g_free (priv->title);
  priv->title = g_strdup (title);

  g_object_notify_by_pspec (G_OBJECT (command), properties[PROP_TITLE]);
}

/*
 * gtk_undo_command_get_timestamp:
 * @command: The command
 *
 * Returns the timestamp when this command was recorded. If multiple
 * commands get combined into one, the timestamp will be the timestamp
 * of the newest command.
 *
 * Returns: Timestamp as returned by g_get_real_time()
 **/
gint64
gtk_undo_command_get_timestamp (GtkUndoCommand *command)
{
  GtkUndoCommandPrivate *priv = gtk_undo_command_get_instance_private (command);

  g_return_val_if_fail (GTK_IS_UNDO_COMMAND (command), 0);

  return priv->timestamp;
}
