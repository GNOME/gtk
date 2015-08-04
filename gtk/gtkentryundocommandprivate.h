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

#ifndef __GTK_ENTRY_UNDO_COMMAND_PRIVATE_H__
#define __GTK_ENTRY_UNDO_COMMAND_PRIVATE_H__

#include <gtk/gtkentry.h>
#include <gtk/gtkundocommandprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_ENTRY_UNDO_COMMAND           (gtk_entry_undo_command_get_type ())
#define GTK_ENTRY_UNDO_COMMAND(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_ENTRY_UNDO_COMMAND, GtkEntryUndoCommand))
#define GTK_ENTRY_UNDO_COMMAND_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_ENTRY_UNDO_COMMAND, GtkEntryUndoCommandClass))
#define GTK_IS_ENTRY_UNDO_COMMAND(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_ENTRY_UNDO_COMMAND))
#define GTK_IS_ENTRY_UNDO_COMMAND_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_ENTRY_UNDO_COMMAND))
#define GTK_ENTRY_UNDO_COMMAND_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ENTRY_UNDO_COMMAND, GtkEntryUndoCommandClass))

typedef struct _GtkEntryUndoCommand           GtkEntryUndoCommand;
typedef struct _GtkEntryUndoCommandClass      GtkEntryUndoCommandClass;

struct _GtkEntryUndoCommand
{
  GtkUndoCommand parent;
};

struct _GtkEntryUndoCommandClass
{
  GtkUndoCommandClass parent_class;
};

GType                   gtk_entry_undo_command_get_type         (void) G_GNUC_CONST;

GtkUndoCommand *        gtk_entry_undo_command_new              (GtkEntry       *entry,
                                                                 int             position,
                                                                 const char     *text_deleted,
                                                                 const char     *text_entered);

G_END_DECLS

#endif /* __GTK_ENTRY_UNDO_COMMAND_PRIVATE_H__ */
