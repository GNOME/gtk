/*
 * Copyright (c) 2015 Benjamin Otte <otte@gnome.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef _GTK_INSPECTOR_SNAPSHOT_H_
#define _GTK_INSPECTOR_SNAPSHOT_H_

#include <gtk/gtkwindow.h>

#include "gtkrenderoperation.h"

G_BEGIN_DECLS

#define GTK_TYPE_INSPECTOR_SNAPSHOT            (gtk_inspector_snapshot_get_type())
#define GTK_INSPECTOR_SNAPSHOT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_SNAPSHOT, GtkInspectorSnapshot))
#define GTK_INSPECTOR_SNAPSHOT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_SNAPSHOT, GtkInspectorSnapshotClass))
#define GTK_IS_INSPECTOR_SNAPSHOT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_SNAPSHOT))
#define GTK_IS_INSPECTOR_SNAPSHOT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_SNAPSHOT))
#define GTK_INSPECTOR_SNAPSHOT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_SNAPSHOT, GtkInspectorSnapshotClass))

typedef struct
{
  GtkWindow parent;

  GtkRenderOperation *operation;

  GtkWidget *operations_listbox;
  GtkWidget *image;
} GtkInspectorSnapshot;

typedef struct
{
  GtkWindowClass parent;
} GtkInspectorSnapshotClass;

GType                   gtk_inspector_snapshot_get_type         (void);

GtkWidget *             gtk_inspector_snapshot_new              (GtkRenderOperation     *oper);

void                    gtk_inspector_snapshot_set_operation    (GtkInspectorSnapshot   *snapshot,
                                                                 GtkRenderOperation     *oper);
GtkRenderOperation *    gtk_inspector_snapshot_get_operation    (GtkInspectorSnapshot   *snapshot);

G_END_DECLS


#endif // _GTK_INSPECTOR_SNAPSHOT_H_

// vim: set et sw=2 ts=2:
