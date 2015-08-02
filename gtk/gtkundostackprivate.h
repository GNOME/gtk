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

#ifndef __GTK_UNDO_STACK_PRIVATE_H__
#define __GTK_UNDO_STACK_PRIVATE_H__

#include <gtk/gtkundocommandprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_UNDO_STACK           (gtk_undo_stack_get_type ())
#define GTK_UNDO_STACK(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_UNDO_STACK, GtkUndoStack))
#define GTK_UNDO_STACK_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_UNDO_STACK, GtkUndoStackClass))
#define GTK_IS_UNDO_STACK(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_UNDO_STACK))
#define GTK_IS_UNDO_STACK_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_UNDO_STACK))
#define GTK_UNDO_STACK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_UNDO_STACK, GtkUndoStackClass))

typedef struct _GtkUndoStack           GtkUndoStack;
typedef struct _GtkUndoStackClass      GtkUndoStackClass;

struct _GtkUndoStack
{
  GObject parent;
};

struct _GtkUndoStackClass
{
  GObjectClass parent_class;
};

GType                   gtk_undo_stack_get_type       (void) G_GNUC_CONST;

GtkUndoStack *          gtk_undo_stack_new            (void);

void                    gtk_undo_stack_clear          (GtkUndoStack     *stack);
void                    gtk_undo_stack_push           (GtkUndoStack     *stack,
                                                       GtkUndoCommand   *command);

gboolean                gtk_undo_stack_undo           (GtkUndoStack     *stack);
gboolean                gtk_undo_stack_redo           (GtkUndoStack     *stack);


G_END_DECLS

#endif /* __GTK_UNDO_STACK_PRIVATE_H__ */
