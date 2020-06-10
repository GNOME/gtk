/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GTK_RECENT_FILES_LIST_H__
#define __GTK_RECENT_FILES_LIST_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gio/gio.h>
/* for GDK_AVAILABLE_IN_ALL */
#include <gdk/gdk.h>


G_BEGIN_DECLS

#define GTK_TYPE_RECENT_FILES_LIST (gtk_recent_files_list_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkRecentFilesList, gtk_recent_files_list, GTK, RECENT_FILES_LIST, GObject)

GDK_AVAILABLE_IN_ALL
GtkRecentFilesList *    gtk_recent_files_list_new (const char *application_id,
                                                   const char *attributes);

GDK_AVAILABLE_IN_ALL
void                    gtk_recent_files_list_set_application_id (GtkRecentFilesList       *self,
                                                                  const char               *application_id);

GDK_AVAILABLE_IN_ALL
const char *            gtk_recent_files_list_get_application_id (GtkRecentFilesList       *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_recent_files_list_set_attributes       (GtkRecentFilesList       *self,
                                                                    const char             *attributes);
GDK_AVAILABLE_IN_ALL
const char *            gtk_recent_files_list_get_attributes       (GtkRecentFilesList       *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_recent_files_list_set_io_priority      (GtkRecentFilesList       *self,
                                                                    int                     io_priority);
GDK_AVAILABLE_IN_ALL
int                     gtk_recent_files_list_get_io_priority      (GtkRecentFilesList       *self);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_recent_files_list_is_loading           (GtkRecentFilesList       *self);

G_END_DECLS

#endif /* __GTK_RECENT_FILES_LIST_H__ */
