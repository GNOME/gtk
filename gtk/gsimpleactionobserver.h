/*
 * Copyright © 2012 Canonical Limited
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_SIMPLE_ACTION_OBSERVER_H__
#define __G_SIMPLE_ACTION_OBSERVER_H__

#include "gactionobserver.h"

G_BEGIN_DECLS

#define G_TYPE_SIMPLE_ACTION_OBSERVER                       (g_simple_action_observer_get_type ())
#define G_SIMPLE_ACTION_OBSERVER(inst)                      (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_SIMPLE_ACTION_OBSERVER,                          \
                                                             GSimpleActionObserver))
#define G_IS_SIMPLE_ACTION_OBSERVER(inst)                   (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_SIMPLE_ACTION_OBSERVER))

typedef struct _GSimpleActionObserver                       GSimpleActionObserver;

G_GNUC_INTERNAL
GType                   g_simple_action_observer_get_type               (void);
G_GNUC_INTERNAL
GSimpleActionObserver * g_simple_action_observer_new                    (GActionObservable     *observable,
                                                                         const gchar           *action_name,
                                                                         GVariant              *target);
G_GNUC_INTERNAL
void                    g_simple_action_observer_activate               (GSimpleActionObserver *observer);
G_GNUC_INTERNAL
gboolean                g_simple_action_observer_get_active             (GSimpleActionObserver *observer);
G_GNUC_INTERNAL
gboolean                g_simple_action_observer_get_enabled            (GSimpleActionObserver *observer);

G_END_DECLS

#endif /* __G_SIMPLE_ACTION_OBSERVER_H__ */
