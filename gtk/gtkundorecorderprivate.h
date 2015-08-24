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

#ifndef __GTK_UNDO_RECORDER_PRIVATE_H__
#define __GTK_UNDO_RECORDER_PRIVATE_H__

#include <gtk/gtkundocommandprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_UNDO_RECORDER           (gtk_undo_recorder_get_type ())
#define GTK_UNDO_RECORDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_UNDO_RECORDER, GtkUndoRecorder))
#define GTK_UNDO_RECORDER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_UNDO_RECORDER, GtkUndoRecorderClass))
#define GTK_IS_UNDO_RECORDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_UNDO_RECORDER))
#define GTK_IS_UNDO_RECORDER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_UNDO_RECORDER))
#define GTK_UNDO_RECORDER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_UNDO_RECORDER, GtkUndoRecorderClass))

typedef struct _GtkUndoRecorder           GtkUndoRecorder;
typedef struct _GtkUndoRecorderClass      GtkUndoRecorderClass;

struct _GtkUndoRecorder
{
  GObject parent;
};

struct _GtkUndoRecorderClass
{
  GObjectClass parent_class;
};

GType                   gtk_undo_recorder_get_type       (void) G_GNUC_CONST;

GtkUndoRecorder *       gtk_undo_recorder_new            (void);

void                    gtk_undo_recorder_push           (GtkUndoRecorder                *recorder,
                                                          GtkUndoCommand                 *command);

GtkUndoCommand *        gtk_undo_recorder_finish         (GtkUndoRecorder                *recorder);
void                    gtk_undo_recorder_clear          (GtkUndoRecorder                *recorder);


G_END_DECLS

#endif /* __GTK_UNDO_RECORDER_PRIVATE_H__ */
