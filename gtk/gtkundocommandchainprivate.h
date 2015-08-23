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

#ifndef __GTK_UNDO_COMMAND_CHAIN_PRIVATE_H__
#define __GTK_UNDO_COMMAND_CHAIN_PRIVATE_H__

#include <gtk/gtkentry.h>
#include <gtk/gtkundocommandprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_UNDO_COMMAND_CHAIN           (gtk_undo_command_chain_get_type ())
#define GTK_UNDO_COMMAND_CHAIN(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_UNDO_COMMAND_CHAIN, GtkUndoCommandChain))
#define GTK_UNDO_COMMAND_CHAIN_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_UNDO_COMMAND_CHAIN, GtkUndoCommandChainClass))
#define GTK_IS_UNDO_COMMAND_CHAIN(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_UNDO_COMMAND_CHAIN))
#define GTK_IS_UNDO_COMMAND_CHAIN_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_UNDO_COMMAND_CHAIN))
#define GTK_UNDO_COMMAND_CHAIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_UNDO_COMMAND_CHAIN, GtkUndoCommandChainClass))

typedef struct _GtkUndoCommandChain           GtkUndoCommandChain;
typedef struct _GtkUndoCommandChainClass      GtkUndoCommandChainClass;

struct _GtkUndoCommandChain
{
  GtkUndoCommand parent;
};

struct _GtkUndoCommandChainClass
{
  GtkUndoCommandClass parent_class;
};

GType                   gtk_undo_command_chain_get_type         (void) G_GNUC_CONST;

GtkUndoCommand *        gtk_undo_command_chain_new              (GtkUndoCommand        **commnds,
                                                                 gsize                   n_commands);
GtkUndoCommand *        gtk_undo_command_chain_new_merge        (GtkUndoCommand         *command,
                                                                 GtkUndoCommand         *followup);

G_END_DECLS

#endif /* __GTK_UNDO_COMMAND_CHAIN_PRIVATE_H__ */
