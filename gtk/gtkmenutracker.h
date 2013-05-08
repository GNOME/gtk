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

#include "gtkmenutrackeritem.h"

#define GTK_TYPE_MENU_TRACKER                               (gtk_menu_tracker_get_type ())
#define GTK_MENU_TRACKER(inst)                              (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                                             GTK_TYPE_MENU_TRACKER, GtkMenuTracker))
#define GTK_IS_MENU_TRACKER(inst)                           (G_TYPE_CHECK_INSTANCE_TYPE ((inst), \
                                                             GTK_TYPE_MENU_TRACKER))

typedef struct _GtkMenuTracker GtkMenuTracker;

G_GNUC_INTERNAL
GType                   gtk_menu_tracker_get_type                       (void) G_GNUC_CONST;

typedef void         (* GtkMenuTrackerInsertFunc)       (GtkMenuTrackerItem       *item,
                                                         gint                      position,
                                                         gpointer                  user_data);

typedef void         (* GtkMenuTrackerRemoveFunc)       (gint                      position,
                                                         gpointer                  user_data);


G_GNUC_INTERNAL
GtkMenuTracker *        gtk_menu_tracker_new            (GActionObservable        *observer,
                                                         GMenuModel               *model,
                                                         gboolean                  with_separators,
                                                         const gchar              *action_namespace,
                                                         GtkMenuTrackerInsertFunc  insert_func,
                                                         GtkMenuTrackerRemoveFunc  remove_func,
                                                         gpointer                  user_data);

G_GNUC_INTERNAL
void                    gtk_menu_tracker_setup          (GtkMenuTracker           *tracker,
                                                         GActionObservable        *observer,
                                                         GMenuModel               *model,
                                                         gboolean                  with_separators,
                                                         const gchar              *action_namespace);

#endif /* __GTK_MENU_TRACKER_H__ */
