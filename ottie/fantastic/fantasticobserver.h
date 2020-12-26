/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __FANTASTIC_OBSERVER_PRIVATE_H__
#define __FANTASTIC_OBSERVER_PRIVATE_H__

#include "ottie/ottierenderobserverprivate.h"

G_BEGIN_DECLS

#define FANTASTIC_TYPE_OBSERVER         (fantastic_observer_get_type ())
#define FANTASTIC_OBSERVER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), FANTASTIC_TYPE_OBSERVER, FantasticObserver))
#define FANTASTIC_OBSERVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), FANTASTIC_TYPE_OBSERVER, FantasticObserverClass))
#define FANTASTIC_IS_OBSERVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), FANTASTIC_TYPE_OBSERVER))
#define FANTASTIC_IS_OBSERVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), FANTASTIC_TYPE_OBSERVER))
#define FANTASTIC_OBSERVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), FANTASTIC_TYPE_OBSERVER, FantasticObserverClass))

typedef struct _FantasticObserver FantasticObserver;
typedef struct _FantasticObserverClass FantasticObserverClass;

GType                   fantastic_observer_get_type             (void) G_GNUC_CONST;

FantasticObserver *     fantastic_observer_new                  (void);

GskRenderNode *         fantastic_observer_pick_node            (FantasticObserver *self,
                                                                 double             x,
                                                                 double             y);
OttieObject *           fantastic_observer_pick                 (FantasticObserver *self,
                                                                 double             x,
                                                                 double             y);

G_END_DECLS

#endif /* __FANTASTIC_OBSERVER_PRIVATE_H__ */
