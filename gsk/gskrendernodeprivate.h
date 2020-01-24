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

  graphene_rect_t bounds;
};

struct _GskRenderNodeClass
{
  GskRenderNodeType node_type;
  gsize struct_size;
  const char *type_name;

  void            (* finalize)    (GskRenderNode  *node);
  void            (* draw)        (GskRenderNode  *node,
                                   cairo_t        *cr);
  gboolean        (* can_diff)    (const GskRenderNode  *node1,
                                   const GskRenderNode  *node2);
  void            (* diff)        (GskRenderNode  *node1,
                                   GskRenderNode  *node2,
                                   cairo_region_t *region);
};

GskRenderNode * gsk_render_node_new              (const GskRenderNodeClass  *node_class,
                                                  gsize                      extra_size);

gboolean        gsk_render_node_can_diff         (const GskRenderNode       *node1,
                                                  const GskRenderNode       *node2) G_GNUC_PURE;
void            gsk_render_node_diff             (GskRenderNode             *node1,
                                                  GskRenderNode             *node2,
                                                  cairo_region_t            *region);
void            gsk_render_node_diff_impossible  (GskRenderNode             *node1,
                                                  GskRenderNode             *node2,
                                                  cairo_region_t            *region);


G_END_DECLS

#endif /* __GSK_RENDER_NODE_PRIVATE_H__ */
