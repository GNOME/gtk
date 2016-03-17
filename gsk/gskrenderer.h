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

#ifndef __GSK_RENDERER_H__
#define __GSK_RENDERER_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GSK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>
#include <gsk/gskrendernode.h>

G_BEGIN_DECLS

#define GSK_TYPE_RENDERER (gsk_renderer_get_type ())

#define GSK_RENDERER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_RENDERER, GskRenderer))
#define GSK_IS_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_RENDERER))

typedef struct _GskRenderer             GskRenderer;
typedef struct _GskRendererClass        GskRendererClass;

GDK_AVAILABLE_IN_3_22
GType gsk_renderer_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_22
GskRenderer *           gsk_renderer_get_for_display            (GdkDisplay              *display);

GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_viewport               (GskRenderer             *renderer,
                                                                 const graphene_rect_t   *viewport);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_get_viewport               (GskRenderer             *renderer,
                                                                 graphene_rect_t         *viewport);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_projection             (GskRenderer             *renderer,
                                                                 const graphene_matrix_t *projection);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_get_projection             (GskRenderer             *renderer,
                                                                 graphene_matrix_t       *projection);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_modelview              (GskRenderer             *renderer,
                                                                 const graphene_matrix_t *modelview);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_get_modelview              (GskRenderer             *renderer,
                                                                 graphene_matrix_t       *modelview);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_scaling_filters        (GskRenderer             *renderer,
                                                                 GskScalingFilter         min_filter,
                                                                 GskScalingFilter         mag_filter);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_get_scaling_filters        (GskRenderer             *renderer,
                                                                 GskScalingFilter        *min_filter,
                                                                 GskScalingFilter        *mag_filter);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_auto_clear             (GskRenderer             *renderer,
                                                                 gboolean                 clear);
GDK_AVAILABLE_IN_3_22
gboolean                gsk_renderer_get_auto_clear             (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_root_node              (GskRenderer             *renderer,
                                                                 GskRenderNode           *root);
GDK_AVAILABLE_IN_3_22
GskRenderNode *         gsk_renderer_get_root_node              (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_surface                (GskRenderer             *renderer,
                                                                 cairo_surface_t         *surface);
GDK_AVAILABLE_IN_3_22
cairo_surface_t *       gsk_renderer_get_surface                (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_window                 (GskRenderer             *renderer,
                                                                 GdkWindow               *window);
GDK_AVAILABLE_IN_3_22
GdkWindow *             gsk_renderer_get_window                 (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_draw_context           (GskRenderer             *renderer,
                                                                 cairo_t                 *cr);
GDK_AVAILABLE_IN_3_22
cairo_t *               gsk_renderer_get_draw_context           (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_22
GdkDisplay *            gsk_renderer_get_display                (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_set_use_alpha              (GskRenderer             *renderer,
                                                                 gboolean                 use_alpha);
GDK_AVAILABLE_IN_3_22
gboolean                gsk_renderer_get_use_alpha              (GskRenderer             *renderer);

GDK_AVAILABLE_IN_3_22
gboolean                gsk_renderer_realize                    (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_unrealize                  (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_22
void                    gsk_renderer_render                     (GskRenderer             *renderer);

G_END_DECLS

#endif /* __GSK_RENDERER_H__ */
