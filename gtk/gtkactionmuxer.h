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

#ifndef __GTK_ACTION_MUXER_H__
#define __GTK_ACTION_MUXER_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACTION_MUXER                               (gtk_action_muxer_get_type ())
#define GTK_ACTION_MUXER(inst)                              (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             GTK_TYPE_ACTION_MUXER, GtkActionMuxer))
#define GTK_IS_ACTION_MUXER(inst)                           (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             GTK_TYPE_ACTION_MUXER))

typedef struct _GtkActionMuxer                              GtkActionMuxer;

G_GNUC_INTERNAL
GType                   gtk_action_muxer_get_type                       (void);
G_GNUC_INTERNAL
GtkActionMuxer *        gtk_action_muxer_new                            (void);

G_GNUC_INTERNAL
void                    gtk_action_muxer_insert                         (GtkActionMuxer *muxer,
                                                                         const gchar    *prefix,
                                                                         GActionGroup   *group);

G_GNUC_INTERNAL
void                    gtk_action_muxer_remove                         (GtkActionMuxer *muxer,
                                                                         const gchar    *prefix);

G_GNUC_INTERNAL
GtkActionMuxer *        gtk_action_muxer_get_parent                     (GtkActionMuxer *muxer);

G_GNUC_INTERNAL
void                    gtk_action_muxer_set_parent                     (GtkActionMuxer *muxer,
                                                                         GtkActionMuxer *parent);

G_END_DECLS

#endif /* __GTK_ACTION_MUXER_H__ */
