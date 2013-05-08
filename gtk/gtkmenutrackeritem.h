/*
 * Copyright © 2011, 2013 Canonical Limited
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

#ifndef __GTK_MENU_TRACKER_ITEM_H__
#define __GTK_MENU_TRACKER_ITEM_H__

#include "gactionobservable.h"

#define GTK_TYPE_MENU_TRACKER_ITEM                          (gtk_menu_tracker_item_get_type ())
#define GTK_MENU_TRACKER_ITEM(inst)                         (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                                             GTK_TYPE_MENU_TRACKER_ITEM, GtkMenuTrackerItem))
#define GTK_IS_MENU_TRACKER_ITEM(inst)                      (G_TYPE_CHECK_INSTANCE_TYPE ((inst), \
                                                             GTK_TYPE_MENU_TRACKER_ITEM))

typedef struct _GtkMenuTrackerItem GtkMenuTrackerItem;

#define GTK_TYPE_MENU_TRACKER_ITEM_ROLE                     (gtk_menu_tracker_item_role_get_type ())

typedef enum  {
  GTK_MENU_TRACKER_ITEM_ROLE_NORMAL,
  GTK_MENU_TRACKER_ITEM_ROLE_TOGGLE,
  GTK_MENU_TRACKER_ITEM_ROLE_RADIO,
  GTK_MENU_TRACKER_ITEM_ROLE_SEPARATOR
} GtkMenuTrackerItemRole;

G_GNUC_INTERNAL
GType                   gtk_menu_tracker_item_get_type                  (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GType                   gtk_menu_tracker_item_role_get_type             (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkMenuTrackerItem *    gtk_menu_tracker_item_new                       (GActionObservable *observable,
                                                                         GMenuModel        *model,
                                                                         gint               item_index,
                                                                         const gchar       *action_namespace,
                                                                         gboolean           is_separator);

G_GNUC_INTERNAL
GActionObservable *     gtk_menu_tracker_item_get_observable            (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
const gchar *           gtk_menu_tracker_item_get_label                 (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
GIcon *                 gtk_menu_tracker_item_get_icon                  (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
gboolean                gtk_menu_tracker_item_get_sensitive             (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
gboolean                gtk_menu_tracker_item_get_visible               (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
GtkMenuTrackerItemRole  gtk_menu_tracker_item_get_role                  (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
gboolean                gtk_menu_tracker_item_get_toggled               (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
const gchar *           gtk_menu_tracker_item_get_accel                 (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
GMenuModel *            gtk_menu_tracker_item_get_submenu               (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
gchar *                 gtk_menu_tracker_item_get_submenu_namespace     (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
gboolean                gtk_menu_tracker_item_get_should_request_show   (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
void                    gtk_menu_tracker_item_activated                 (GtkMenuTrackerItem *self);

G_GNUC_INTERNAL
void                    gtk_menu_tracker_item_request_submenu_shown     (GtkMenuTrackerItem *self,
                                                                         gboolean            shown);

#endif
