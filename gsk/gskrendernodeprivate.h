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

  void            (* finalize)    (GskRenderNode  *node);
  void            (* draw)        (GskRenderNode  *node,
                                   cairo_t        *cr);
  GVariant *      (* serialize)   (GskRenderNode  *node);
  GskRenderNode * (* deserialize) (GVariant       *variant,
                                   GError        **error);
};

GskRenderNode * gsk_render_node_new              (const GskRenderNodeClass  *node_class,
                                                  gsize                      extra_size);

GVariant *      gsk_render_node_serialize_node   (GskRenderNode             *node);
GskRenderNode * gsk_render_node_deserialize_node (GskRenderNodeType          type,
                                                  GVariant                  *variant,
                                                  GError                   **error);

G_END_DECLS

#endif /* __GSK_RENDER_NODE_PRIVATE_H__ */
