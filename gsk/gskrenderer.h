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

typedef struct _GskRendererClass        GskRendererClass;

GDK_AVAILABLE_IN_3_90
GType gsk_renderer_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_90
GskRenderer *           gsk_renderer_new_for_window             (GdkWindow               *window);

GDK_AVAILABLE_IN_3_90
void                    gsk_renderer_set_viewport               (GskRenderer             *renderer,
                                                                 const graphene_rect_t   *viewport);
GDK_AVAILABLE_IN_3_90
void                    gsk_renderer_get_viewport               (GskRenderer             *renderer,
                                                                 graphene_rect_t         *viewport);
GDK_AVAILABLE_IN_3_90
void                    gsk_renderer_set_scale_factor           (GskRenderer             *renderer,
                                                                 int                      scale_factor);
GDK_AVAILABLE_IN_3_90
int                     gsk_renderer_get_scale_factor           (GskRenderer             *renderer);
GDK_AVAILABLE_IN_3_90
GdkWindow *             gsk_renderer_get_window                 (GskRenderer             *renderer);

GDK_AVAILABLE_IN_3_90
GdkDisplay *            gsk_renderer_get_display                (GskRenderer             *renderer);

GDK_AVAILABLE_IN_3_90
gboolean                gsk_renderer_realize                    (GskRenderer             *renderer,
                                                                 GdkWindow               *window,
                                                                 GError                 **error);
GDK_AVAILABLE_IN_3_90
void                    gsk_renderer_unrealize                  (GskRenderer             *renderer);

GDK_AVAILABLE_IN_3_90
GskTexture *            gsk_renderer_render_texture             (GskRenderer             *renderer,
                                                                 GskRenderNode           *root,
                                                                 const graphene_rect_t   *viewport);

GDK_AVAILABLE_IN_3_90
GdkDrawingContext *     gsk_renderer_begin_draw_frame           (GskRenderer             *renderer,
                                                                 const cairo_region_t    *region);
GDK_AVAILABLE_IN_3_90
void                    gsk_renderer_render                     (GskRenderer             *renderer,
                                                                 GskRenderNode           *root,
                                                                 GdkDrawingContext       *context);
GDK_AVAILABLE_IN_3_90
void                    gsk_renderer_end_draw_frame             (GskRenderer             *renderer,
                                                                 GdkDrawingContext       *context);

G_END_DECLS

#endif /* __GSK_RENDERER_H__ */
