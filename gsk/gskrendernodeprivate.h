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

  /* The graph */
  GskRenderNode *parent;
  GskRenderNode *first_child;
  GskRenderNode *last_child;
  GskRenderNode *prev_sibling;
  GskRenderNode *next_sibling;

  int n_children;

  /* Use for debugging */
  char *name;

  /* Tag updated when adding/removing children */
  gint64 age;

  /* Paint opacity */
  double opacity;

  /* Blend mode */
  GskBlendMode blend_mode;

  /* Scaling filters */
  GskScalingFilter min_filter;
  GskScalingFilter mag_filter;

  /* Clip rectangle */
  graphene_rect_t bounds;

  /* Transformations relative to the root of the scene */
  graphene_matrix_t world_matrix;

  /* Transformations applied to the node */
  graphene_matrix_t transform;

  /* Bit fields; leave at the end */
  gboolean is_mutable : 1;
  gboolean transform_set : 1;
  gboolean needs_world_matrix_update : 1;
};

struct _GskRenderNodeClass
{
  GskRenderNodeType node_type;
  gsize struct_size;
  const char *type_name;
  void (* finalize) (GskRenderNode *node);
  void (* make_immutable) (GskRenderNode *node);
};

GskRenderNode *gsk_render_node_new (const GskRenderNodeClass *node_class);

void gsk_render_node_make_immutable (GskRenderNode *node);

void gsk_render_node_get_bounds (GskRenderNode   *node,
                                 graphene_rect_t *frame);
void gsk_render_node_get_transform (GskRenderNode     *node,
                                    graphene_matrix_t *mv);
double gsk_render_node_get_opacity (GskRenderNode *node);

cairo_surface_t *gsk_cairo_node_get_surface (GskRenderNode *node);

GskTexture *gsk_texture_node_get_texture (GskRenderNode *node);

GskBlendMode gsk_render_node_get_blend_mode (GskRenderNode *node);

GskRenderNode *gsk_render_node_get_toplevel (GskRenderNode *node);

void gsk_render_node_update_world_matrix (GskRenderNode *node,
                                          gboolean       force);

void gsk_render_node_get_world_matrix (GskRenderNode     *node,
                                       graphene_matrix_t *mv);

G_END_DECLS

#endif /* __GSK_RENDER_NODE_PRIVATE_H__ */
