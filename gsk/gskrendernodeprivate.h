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

  /* Bit fields; leave at the end */
  gboolean is_mutable : 1;
};

struct _GskRenderNodeClass
{
  GskRenderNodeType node_type;
  gsize struct_size;
  const char *type_name;
  void (* finalize) (GskRenderNode *node);
  void (* make_immutable) (GskRenderNode *node);
  void (* draw) (GskRenderNode *node,
                 cairo_t       *cr);
  void (* get_bounds) (GskRenderNode   *node,
                       graphene_rect_t *bounds);
};

GskRenderNode *gsk_render_node_new (const GskRenderNodeClass *node_class);

void gsk_render_node_make_immutable (GskRenderNode *node);

void gsk_render_node_get_bounds (GskRenderNode   *node,
                                 graphene_rect_t *frame);
double gsk_opacity_node_get_opacity (GskRenderNode *node);

cairo_surface_t *gsk_cairo_node_get_surface (GskRenderNode *node);

GskTexture *gsk_texture_node_get_texture (GskRenderNode *node);

const GdkRGBA *gsk_color_node_peek_color (GskRenderNode *node);

const graphene_rect_t * gsk_clip_node_peek_clip (GskRenderNode *node);

const GskRoundedRect * gsk_rounded_clip_node_peek_clip (GskRenderNode *node);

void gsk_transform_node_get_transform (GskRenderNode *node, graphene_matrix_t *transform);

GskRenderNode * gsk_blend_node_get_bottom_child (GskRenderNode *node);
GskRenderNode * gsk_blend_node_get_top_child (GskRenderNode *node);
GskBlendMode gsk_blend_node_get_blend_node (GskRenderNode *node);

GskRenderNode * gsk_cross_fade_node_get_start_child (GskRenderNode *node);
GskRenderNode * gsk_cross_fade_node_get_end_child (GskRenderNode *node);
double gsk_cross_fade_node_get_progress (GskRenderNode *node);

G_END_DECLS

#endif /* __GSK_RENDER_NODE_PRIVATE_H__ */
