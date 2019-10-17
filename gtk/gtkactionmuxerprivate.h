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
#include "gtkwidget.h"

G_BEGIN_DECLS

#define GTK_TYPE_ACTION_MUXER                               (gtk_action_muxer_get_type ())
#define GTK_ACTION_MUXER(inst)                              (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             GTK_TYPE_ACTION_MUXER, GtkActionMuxer))
#define GTK_IS_ACTION_MUXER(inst)                           (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             GTK_TYPE_ACTION_MUXER))

typedef struct {
  char *name;
  GType owner;

  const GVariantType *parameter_type;
  GtkWidgetActionActivateFunc activate;

  const GVariantType *state_type;
  GParamSpec *pspec;
} GtkWidgetAction;

typedef struct _GtkActionMuxer                              GtkActionMuxer;

GType                   gtk_action_muxer_get_type                       (void);
GtkActionMuxer *        gtk_action_muxer_new                            (GtkWidget      *widget,
                                                                         GPtrArray      *actions);

void                    gtk_action_muxer_insert                         (GtkActionMuxer *muxer,
                                                                         const gchar    *prefix,
                                                                         GActionGroup   *action_group);

void                    gtk_action_muxer_remove                         (GtkActionMuxer *muxer,
                                                                         const gchar    *prefix);
GActionGroup *          gtk_action_muxer_find                           (GtkActionMuxer *muxer,
                                                                         const char     *action_name,
                                                                         const char    **unprefixed_name);
GtkActionMuxer *        gtk_action_muxer_get_parent                     (GtkActionMuxer *muxer);

void                    gtk_action_muxer_set_parent                     (GtkActionMuxer *muxer,
                                                                         GtkActionMuxer *parent);

void                    gtk_action_muxer_set_primary_accel              (GtkActionMuxer *muxer,
                                                                         const gchar    *action_and_target,
                                                                         const gchar    *primary_accel);

const gchar *           gtk_action_muxer_get_primary_accel              (GtkActionMuxer *muxer,
                                                                         const gchar    *action_and_target);

void
gtk_action_muxer_action_enabled_changed (GtkActionMuxer *muxer,
                                         const char     *action_name,
                                         gboolean        enabled);
void
gtk_action_muxer_action_state_changed (GtkActionMuxer *muxer,
                                       const gchar    *action_name,
                                       GVariant       *state);


/* No better place for these... */
gchar *                 gtk_print_action_and_target                     (const gchar    *action_namespace,
                                                                         const gchar    *action_name,
                                                                         GVariant       *target);

gchar *                 gtk_normalise_detailed_action_name              (const gchar *detailed_action_name);

G_END_DECLS

#endif /* __GTK_ACTION_MUXER_H__ */
