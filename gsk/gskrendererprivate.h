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

#ifndef __GSK_RENDERER_PRIVATE_H__
#define __GSK_RENDERER_PRIVATE_H__

#include "gskrenderer.h"

G_BEGIN_DECLS

#define GSK_RENDERER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_RENDERER, GskRendererClass))
#define GSK_IS_RENDERER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_RENDERER))
#define GSK_RENDERER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_RENDERER, GskRendererClass))

struct _GskRenderer
{
  GObject parent_instance;
};

struct _GskRendererClass
{
  GObjectClass parent_class;

  gboolean (* realize) (GskRenderer *renderer);
  void (* unrealize) (GskRenderer *renderer);

  void (* resize_viewport) (GskRenderer *renderer,
                            const graphene_rect_t *viewport);
  void (* update) (GskRenderer *renderer,
                   const graphene_matrix_t *modelview,
                   const graphene_matrix_t *projection);
  void (* validate_tree) (GskRenderer *renderer,
                          GskRenderNode *root);
  void (* clear) (GskRenderer *renderer);
  void (* render) (GskRenderer *renderer);
};

gboolean gsk_renderer_is_realized (GskRenderer *renderer);

void gsk_renderer_maybe_resize_viewport (GskRenderer *renderer);
void gsk_renderer_maybe_update (GskRenderer *renderer);
void gsk_renderer_maybe_validate_tree (GskRenderer *renderer);
void gsk_renderer_maybe_clear (GskRenderer *renderer);

G_END_DECLS

#endif /* __GSK_RENDERER_PRIVATE_H__ */
