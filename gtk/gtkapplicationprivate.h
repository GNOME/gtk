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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */


#ifndef __GTK_APPLICATION_PRIVATE_H__
#define __GTK_APPLICATION_PRIVATE_H__

#include "gsimpleactionobserver.h"
#include "gtkapplicationwindow.h"

G_GNUC_INTERNAL
gboolean                gtk_application_window_publish                  (GtkApplicationWindow *window,
                                                                         GDBusConnection      *session,
                                                                         const gchar          *object_path);

G_GNUC_INTERNAL
void                    gtk_application_window_unpublish                (GtkApplicationWindow *window);

G_GNUC_INTERNAL
GSimpleActionObserver * gtk_application_window_get_observer             (GtkApplicationWindow *window,
                                                                         const gchar          *action_name,
                                                                         GVariant             *target);

#endif /* __GTK_APPLICATION_PRIVATE_H__ */
