/* GSK - The GTK Scene Kit
 *
 * Copyright 2026  Red Hat, Inc
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

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>
#include <gsk/gskenums.h>

G_BEGIN_DECLS

/*< private >
 * GskNoiseType:
 * @GSK_NOISE_FRACTAL_NOISE: Perlin noise (fractal Brownian motion)
 * @GSK_NOISE_TURBULENCE: Turbulence noise pattern
 *
 * The type of noise pattern to generate.
 */
typedef enum {
  GSK_NOISE_FRACTAL_NOISE,
  GSK_NOISE_TURBULENCE
} GskNoiseType;

typedef struct _GskTurbulenceNode                GskTurbulenceNode;

#define GSK_TYPE_TURBULENCE_NODE (gsk_turbulence_node_get_type())

GType                   gsk_turbulence_node_get_type             (void) G_GNUC_CONST;
GskRenderNode *         gsk_turbulence_node_new                  (const graphene_rect_t *bounds,
                                                                  GskRectSnap            snap,
                                                                  GdkColorState         *color_state,
                                                                  const graphene_size_t *base_frequency,
                                                                  unsigned int           num_octaves,
                                                                  int                    seed,
                                                                  GskNoiseType           noise_type,
                                                                  gboolean               stitch_tiles);
GdkColorState *         gsk_turbulence_node_get_color_state      (const GskRenderNode   *node) G_GNUC_PURE;
GskRectSnap             gsk_turbulence_node_get_snap             (const GskRenderNode   *node) G_GNUC_PURE;
const graphene_size_t * gsk_turbulence_node_get_base_frequency   (const GskRenderNode   *node) G_GNUC_PURE;
unsigned int            gsk_turbulence_node_get_num_octaves      (const GskRenderNode   *node) G_GNUC_PURE;
int                     gsk_turbulence_node_get_seed             (const GskRenderNode   *node) G_GNUC_PURE;
GskNoiseType            gsk_turbulence_node_get_noise_type       (const GskRenderNode   *node) G_GNUC_PURE;
gboolean                gsk_turbulence_node_get_stitch_tiles     (const GskRenderNode   *node) G_GNUC_PURE;

#define GSK_TURBULENCE_TABLE_WIDTH  514
#define GSK_TURBULENCE_TABLE_HEIGHT 4

const float *           gsk_turbulence_node_get_lookup_table     (const GskRenderNode   *node) G_GNUC_PURE;

G_END_DECLS
