/*
 * Copyright © 2013 Canonical Limited
 * Copyright © 2016 Sébastien Wilmet
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Sébastien Wilmet <swilmet@gnome.org>
 */

#ifndef __GTK_APPLICATION_ACCELS_H__
#define __GTK_APPLICATION_ACCELS_H__

#include <gio/gio.h>
#include "gtkwindowprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_APPLICATION_ACCELS (gtk_application_accels_get_type ())
G_DECLARE_FINAL_TYPE (GtkApplicationAccels, gtk_application_accels,
                      GTK, APPLICATION_ACCELS,
                      GObject)

GtkApplicationAccels *
                gtk_application_accels_new                          (void);

void            gtk_application_accels_set_accels_for_action        (GtkApplicationAccels *accels,
                                                                     const gchar          *detailed_action_name,
                                                                     const gchar * const  *accelerators);

gchar **        gtk_application_accels_get_accels_for_action        (GtkApplicationAccels *accels,
                                                                     const gchar          *detailed_action_name);

gchar **        gtk_application_accels_get_actions_for_accel        (GtkApplicationAccels *accels,
                                                                     const gchar          *accel);

gchar **        gtk_application_accels_list_action_descriptions     (GtkApplicationAccels *accels);

void            gtk_application_accels_foreach_key                  (GtkApplicationAccels     *accels,
                                                                     GtkWindow                *window,
                                                                     GtkWindowKeysForeachFunc  callback,
                                                                     gpointer                  user_data);

gboolean        gtk_application_accels_activate                     (GtkApplicationAccels *accels,
                                                                     GActionGroup         *action_group,
                                                                     guint                 key,
                                                                     GdkModifierType       modifier);

G_END_DECLS

#endif /* __GTK_APPLICATION_ACCELS_H__ */
