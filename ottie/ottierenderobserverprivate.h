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

#ifndef __OTTIE_RENDER_OBSERVER_PRIVATE_H__
#define __OTTIE_RENDER_OBSERVER_PRIVATE_H__

#include "ottie/ottietypesprivate.h"

G_BEGIN_DECLS

#define OTTIE_TYPE_RENDER_OBSERVER         (ottie_render_observer_get_type ())
#define OTTIE_RENDER_OBSERVER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_RENDER_OBSERVER, OttieRenderObserver))
#define OTTIE_RENDER_OBSERVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_RENDER_OBSERVER, OttieRenderObserverClass))
#define OTTIE_IS_RENDER_OBSERVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_RENDER_OBSERVER))
#define OTTIE_IS_RENDER_OBSERVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_RENDER_OBSERVER))
#define OTTIE_RENDER_OBSERVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_RENDER_OBSERVER, OttieRenderObserverClass))

typedef struct _OttieRenderObserverClass OttieRenderObserverClass;

struct _OttieRenderObserver
{
  GObject parent;
};

struct _OttieRenderObserverClass
{
  GObjectClass parent_class;

  void                  (* start)                              (OttieRenderObserver     *self,
                                                                OttieRender             *render,
                                                                double                   timestamp);
  void                  (* end)                                (OttieRenderObserver     *self,
                                                                OttieRender             *render,
                                                                GskRenderNode           *node);
  void                  (* start_object)                       (OttieRenderObserver     *self,
                                                                OttieRender             *render,
                                                                OttieObject             *object,
                                                                double                   timestamp);
  void                  (* end_object)                         (OttieRenderObserver     *self,
                                                                OttieRender             *render,
                                                                OttieObject             *object);

  void                  (* add_node)                           (OttieRenderObserver     *self,
                                                                OttieRender             *render,
                                                                GskRenderNode           *node);
};

GType                   ottie_render_observer_get_type                   (void) G_GNUC_CONST;

static inline void
ottie_render_observer_start (OttieRenderObserver *self,
                             OttieRender         *render,
                             double               timestamp)
{
  OTTIE_RENDER_OBSERVER_GET_CLASS (self)->start (self, render, timestamp);
}

static inline void
ottie_render_observer_end (OttieRenderObserver *self,
                           OttieRender         *render,
                           GskRenderNode       *node)
{
  OTTIE_RENDER_OBSERVER_GET_CLASS (self)->end (self, render, node);
}

static inline void
ottie_render_observer_start_object (OttieRenderObserver *self,
                                    OttieRender         *render,
                                    OttieObject         *object,
                                    double               timestamp)
{
  OTTIE_RENDER_OBSERVER_GET_CLASS (self)->start_object (self, render, object, timestamp);
}

static inline void
ottie_render_observer_end_object (OttieRenderObserver *self,
                                  OttieRender         *render,
                                  OttieObject         *object)
{
  OTTIE_RENDER_OBSERVER_GET_CLASS (self)->end_object (self, render, object);
}

static inline void
ottie_render_observer_add_node (OttieRenderObserver *self,
                                OttieRender         *render,
                                GskRenderNode       *node)
{
  OTTIE_RENDER_OBSERVER_GET_CLASS (self)->add_node (self, render, node);
}

G_END_DECLS

#endif /* __OTTIE_RENDER_OBSERVER_PRIVATE_H__ */
