/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */


#ifndef __GTK_APPLICATION_PRIVATE_H__
#define __GTK_APPLICATION_PRIVATE_H__

#include "gtkapplicationwindow.h"

G_GNUC_INTERNAL
gboolean                gtk_application_window_publish                  (GtkApplicationWindow *window,
                                                                         GDBusConnection      *session,
                                                                         const gchar          *object_path,
                                                                         guint                 object_id);

G_GNUC_INTERNAL
void                    gtk_application_window_unpublish                (GtkApplicationWindow *window);

G_GNUC_INTERNAL
GtkAccelGroup         * gtk_application_window_get_accel_group          (GtkApplicationWindow *window);

G_GNUC_INTERNAL
const gchar *           gtk_application_get_app_menu_object_path        (GtkApplication       *application);
G_GNUC_INTERNAL
const gchar *           gtk_application_get_menubar_object_path         (GtkApplication       *application);

#endif /* __GTK_APPLICATION_PRIVATE_H__ */
