/*
 * Copyright © 2013 Canonical Limited
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

#ifndef __GTK_MENU_TRACKER_H__
#define __GTK_MENU_TRACKER_H__

#include <gio/gio.h>

typedef struct _GtkMenuTracker GtkMenuTracker;

typedef void         (* GtkMenuTrackerInsertFunc)       (gint                      position,
                                                         GMenuModel               *model,
                                                         gint                      item_index,
                                                         const gchar              *action_namespace,
                                                         gboolean                  is_separator,
                                                         gpointer                  user_data);

typedef void         (* GtkMenuTrackerRemoveFunc)       (gint                      position,
                                                         gpointer                  user_data);


G_GNUC_INTERNAL
GtkMenuTracker *        gtk_menu_tracker_new            (GMenuModel               *model,
                                                         gboolean                  with_separators,
                                                         const gchar              *action_namespace,
                                                         GtkMenuTrackerInsertFunc  insert_func,
                                                         GtkMenuTrackerRemoveFunc  remove_func,
                                                         gpointer                  user_data);

G_GNUC_INTERNAL
void                    gtk_menu_tracker_free           (GtkMenuTracker           *tracker);

#endif /* __GTK_MENU_TRACKER_H__ */
