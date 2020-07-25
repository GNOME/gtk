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

#ifndef __GTK_APPLICATION_LIST_H__
#define __GTK_APPLICATION_LIST_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gio/gio.h>
/* for GDK_AVAILABLE_IN_ALL */
#include <gdk/gdk.h>


G_BEGIN_DECLS

#define GTK_TYPE_APPLICATION_LIST (gtk_application_list_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkApplicationList, gtk_application_list, GTK, APPLICATION_LIST, GObject)

GDK_AVAILABLE_IN_ALL
GtkApplicationList *    gtk_application_list_new  (void);

G_END_DECLS

#endif /* __GTK_APPLICATION_LIST_H__ */
