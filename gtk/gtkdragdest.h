/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_DRAG_DEST_H__
#define __GTK_DRAG_DEST_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

typedef struct _GtkDropTarget GtkDropTarget;


#define GTK_TYPE_DROP_TARGET         (gtk_drop_target_get_type ())
#define GTK_DROP_TARGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_DROP_TARGET, GtkDropTarget))
#define GTK_DROP_TARGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_DROP_TARGET, GtkDropTargetClass))
#define GTK_IS_DROP_TARGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_DROP_TARGET))
#define GTK_IS_DROP_TARGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_DROP_TARGET))
#define GTK_DROP_TARGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_DROP_TARGET, GtkDropTargetClass))

typedef struct _GtkDropTargetClass GtkDropTargetClass;

GDK_AVAILABLE_IN_ALL
GType              gtk_drop_target_get_type         (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkDropTarget       *gtk_drop_target_new            (GdkContentFormats *formats,
                                                     GdkDragAction      actions);

GDK_AVAILABLE_IN_ALL
void               gtk_drop_target_set_formats      (GtkDropTarget     *dest,
                                                     GdkContentFormats *formats);
GDK_AVAILABLE_IN_ALL
GdkContentFormats *gtk_drop_target_get_formats      (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
void               gtk_drop_target_set_actions      (GtkDropTarget     *dest,
                                                     GdkDragAction      actions);
GDK_AVAILABLE_IN_ALL
GdkDragAction      gtk_drop_target_get_actions      (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
GdkDrop           *gtk_drop_target_get_drop         (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
const char        *gtk_drop_target_find_mimetype    (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
void                gtk_drop_target_deny_drop      (GtkDropTarget       *dest,
                                                    GdkDrop             *drop);


G_END_DECLS

#endif /* __GTK_DRAG_DEST_H__ */
