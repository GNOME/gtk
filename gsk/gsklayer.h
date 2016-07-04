/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GSK_LAYER_H__
#define __GSK_LAYER_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GSK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>
#include <gsk/gskrenderer.h>

G_BEGIN_DECLS

#define GSK_TYPE_LAYER gsk_layer_get_type()

GDK_AVAILABLE_IN_3_22
G_DECLARE_DERIVABLE_TYPE (GskLayer, gsk_layer, GSK, LAYER, GInitiallyUnowned)

struct _GskLayerClass
{
  /*< private >*/
  GInitiallyUnowned parent_instance;

  /*< public >*/

  /* vfuncs */
  GskRenderNode *(* get_render_node)     (GskLayer         *layer,
                                          GskRenderer      *renderer);

  void           (* queue_resize)        (GskLayer         *layer,
                                          GskLayer         *origin);
  void           (* queue_redraw)        (GskLayer         *layer,
                                          GskLayer         *origin);

  /* signals */
  void           (* child_added)         (GskLayer         *layer,
                                          GskLayer         *child);
  void           (* child_removed)       (GskLayer         *layer,
                                          GskLayer         *child);
  void           (* destroy)             (GskLayer         *layer);


  /*< private >*/
  gpointer _padding[16];
};

GskLayer *              gsk_layer_new           (void);

G_END_DECLS

#endif /* __GSK_LAYER_H__ */
