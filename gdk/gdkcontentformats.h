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

#ifndef __GTK_CONTENT_FORMATS_H__
#define __GTK_CONTENT_FORMATS_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif


#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

/**
 * GtkTargetList:
 *
 * A #GtkTargetList-struct is a reference counted list
 * of #GtkTargetPair and should be treated as
 * opaque.
 */
typedef struct _GtkTargetList  GtkTargetList;

#define GTK_TYPE_TARGET_LIST    (gtk_target_list_get_type ())

GDK_AVAILABLE_IN_ALL
GType          gtk_target_list_get_type  (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkTargetList *gtk_target_list_new       (const char          **targets,
                                          guint                 ntargets);
GDK_AVAILABLE_IN_ALL
GtkTargetList *gtk_target_list_ref       (GtkTargetList  *list);
GDK_AVAILABLE_IN_ALL
void           gtk_target_list_unref     (GtkTargetList  *list);
GDK_AVAILABLE_IN_3_94
void           gtk_target_list_merge     (GtkTargetList         *target,
                                          const GtkTargetList   *source);
GDK_AVAILABLE_IN_3_94
GdkAtom        gtk_target_list_intersects(const GtkTargetList   *first,
                                          const GtkTargetList   *second);
GDK_AVAILABLE_IN_ALL
void           gtk_target_list_add       (GtkTargetList  *list,
                                          const char     *target);
GDK_AVAILABLE_IN_ALL
void           gtk_target_list_add_table (GtkTargetList        *list,
                                          const char          **targets,
                                          guint                 ntargets);
GDK_AVAILABLE_IN_ALL
void           gtk_target_list_remove    (GtkTargetList  *list,
                                          const char     *target);
GDK_AVAILABLE_IN_ALL
gboolean       gtk_target_list_find      (GtkTargetList  *list,
                                          const char     *target);

G_END_DECLS

#endif /* __GTK_CONTENT_FORMATS_H__ */
