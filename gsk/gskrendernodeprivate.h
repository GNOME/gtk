#ifndef __GSK_RENDER_NODE_PRIVATE_H__
#define __GSK_RENDER_NODE_PRIVATE_H__

#include "gskrendernode.h"
#include <cairo.h>

G_BEGIN_DECLS

typedef struct _GskRenderNodeClass GskRenderNodeClass;

#define GSK_IS_RENDER_NODE_TYPE(node,type) (GSK_IS_RENDER_NODE (node) && (node)->node_class->node_type == (type))

struct _GskRenderNode
{
  const GskRenderNodeClass *node_class;

  volatile int ref_count;

  /* Use for debugging */
  char *name;

  /* Scaling filters */
  GskScalingFilter min_filter;
  GskScalingFilter mag_filter;

  graphene_rect_t bounds;
};

struct _GskRenderNodeClass
{
  GskRenderNodeType node_type;
  gsize struct_size;
  const char *type_name;
  void (* finalize) (GskRenderNode *node);
  void (* draw) (GskRenderNode *node,
                 cairo_t       *cr);
  GVariant * (* serialize) (GskRenderNode *node);
  GskRenderNode * (* deserialize) (GVariant  *variant,
                                   GError   **error);
};

GskRenderNode *gsk_render_node_new (const GskRenderNodeClass *node_class, gsize extra_size);

GVariant * gsk_render_node_serialize_node (GskRenderNode *node);
GskRenderNode * gsk_render_node_deserialize_node (GskRenderNodeType type, GVariant *variant, GError **error);

double gsk_opacity_node_get_opacity (GskRenderNode *node);

GskRenderNode * gsk_color_matrix_node_get_child (GskRenderNode *node);
const graphene_matrix_t * gsk_color_matrix_node_peek_color_matrix (GskRenderNode *node);
const graphene_vec4_t * gsk_color_matrix_node_peek_color_offset (GskRenderNode *node);

GskRenderNode * gsk_repeat_node_get_child (GskRenderNode *node);
const graphene_rect_t * gsk_repeat_node_peek_child_bounds (GskRenderNode *node);

const graphene_point_t * gsk_linear_gradient_node_peek_start (GskRenderNode *node);
const graphene_point_t * gsk_linear_gradient_node_peek_end (GskRenderNode *node);
const gsize gsk_linear_gradient_node_get_n_color_stops (GskRenderNode *node);
const GskColorStop * gsk_linear_gradient_node_peek_color_stops (GskRenderNode *node);

const GskRoundedRect * gsk_border_node_peek_outline (GskRenderNode *node);
const float * gsk_border_node_peek_widths (GskRenderNode *node);
const GdkRGBA * gsk_border_node_peek_colors (GskRenderNode *node);

const GskRoundedRect * gsk_inset_shadow_node_peek_outline (GskRenderNode *node);
const GdkRGBA * gsk_inset_shadow_node_peek_color (GskRenderNode *node);
float gsk_inset_shadow_node_get_dx (GskRenderNode *node);
float gsk_inset_shadow_node_get_dy (GskRenderNode *node);
float gsk_inset_shadow_node_get_spread (GskRenderNode *node);
float gsk_inset_shadow_node_get_blur_radius (GskRenderNode *node);

const GskRoundedRect * gsk_outset_shadow_node_peek_outline (GskRenderNode *node);
const GdkRGBA * gsk_outset_shadow_node_peek_color (GskRenderNode *node);
float gsk_outset_shadow_node_get_dx (GskRenderNode *node);
float gsk_outset_shadow_node_get_dy (GskRenderNode *node);
float gsk_outset_shadow_node_get_spread (GskRenderNode *node);
float gsk_outset_shadow_node_get_blur_radius (GskRenderNode *node);

GskRenderNode *gsk_cairo_node_new_for_surface (const graphene_rect_t *bounds, cairo_surface_t *surface);
cairo_surface_t *gsk_cairo_node_get_surface (GskRenderNode *node);

GskTexture *gsk_texture_node_get_texture (GskRenderNode *node);

const GdkRGBA *gsk_color_node_peek_color (GskRenderNode *node);

const graphene_rect_t * gsk_clip_node_peek_clip (GskRenderNode *node);

const GskRoundedRect * gsk_rounded_clip_node_peek_clip (GskRenderNode *node);

GskRenderNode * gsk_shadow_node_get_child (GskRenderNode *node);
const GskShadow * gsk_shadow_node_peek_shadow (GskRenderNode *node, gsize i);
gsize gsk_shadow_node_get_n_shadows (GskRenderNode *node);

void gsk_transform_node_get_transform (GskRenderNode *node, graphene_matrix_t *transform);

GskRenderNode * gsk_blend_node_get_bottom_child (GskRenderNode *node);
GskRenderNode * gsk_blend_node_get_top_child (GskRenderNode *node);
GskBlendMode gsk_blend_node_get_blend_node (GskRenderNode *node);

GskRenderNode * gsk_cross_fade_node_get_start_child (GskRenderNode *node);
GskRenderNode * gsk_cross_fade_node_get_end_child (GskRenderNode *node);
double gsk_cross_fade_node_get_progress (GskRenderNode *node);

G_END_DECLS

#endif /* __GSK_RENDER_NODE_PRIVATE_H__ */
