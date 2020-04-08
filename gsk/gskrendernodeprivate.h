#ifndef __GSK_RENDER_NODE_PRIVATE_H__
#define __GSK_RENDER_NODE_PRIVATE_H__

#include "gskrendernode.h"
#include <cairo.h>

G_BEGIN_DECLS

typedef struct _GskRenderNodeClass GskRenderNodeClass;

/* Keep this in sync with the GskRenderNodeType enumeration.
 *
 * We don't add an "n-types" value to avoid having to handle
 * it in every single switch.
 */
#define GSK_RENDER_NODE_TYPE_N_TYPES    (GSK_DEBUG_NODE + 1)

extern GType gsk_render_node_types[];

#define GSK_IS_RENDER_NODE_TYPE(node,type) \
  (G_TYPE_INSTANCE_GET_CLASS ((node), GSK_TYPE_RENDER_NODE, GskRenderNodeClass)->node_type == (type))

struct _GskRenderNode
{
  GTypeInstance parent_instance;

  gatomicrefcount ref_count;

  graphene_rect_t bounds;
};

struct _GskRenderNodeClass
{
  GTypeClass parent_class;

  GskRenderNodeType node_type;

  void            (* finalize)    (GskRenderNode  *node);
  void            (* draw)        (GskRenderNode  *node,
                                   cairo_t        *cr);
  gboolean        (* can_diff)    (const GskRenderNode  *node1,
                                   const GskRenderNode  *node2);
  void            (* diff)        (GskRenderNode  *node1,
                                   GskRenderNode  *node2,
                                   cairo_region_t *region);
};

/*< private >
 * GskRenderNodeTypeInfo:
 * @node_type: the render node type in the #GskRenderNodeType enumeration
 * @instance_size: the size of the render node instance
 * @instance_init: (nullable): the instance initialization function
 * @finalize: (nullable): the instance finalization function; must chain up to the
 *   implementation of the parent class
 * @draw: the function called by gsk_render_node_draw()
 * @can_diff: (nullable): the function called by gsk_render_node_can_diff(); if
 *   unset, gsk_render_node_can_diff_true() will be used
 * @diff: (nullable): the function called by gsk_render_node_diff(); if unset,
 *   gsk_render_node_diff_impossible() will be used
 *
 * A struction that contains the type information for a #GskRenderNode subclass,
 * to be used by gsk_render_node_type_register_static().
 */
typedef struct
{
  GskRenderNodeType node_type;

  gsize instance_size;

  void            (* instance_init) (GskRenderNode        *node);
  void            (* finalize)      (GskRenderNode        *node);
  void            (* draw)          (GskRenderNode        *node,
                                     cairo_t              *cr);
  gboolean        (* can_diff)      (const GskRenderNode  *node1,
                                     const GskRenderNode  *node2);
  void            (* diff)          (GskRenderNode        *node1,
                                     GskRenderNode        *node2,
                                     cairo_region_t       *region);
} GskRenderNodeTypeInfo;

void            gsk_render_node_init_types              (void);

GType           gsk_render_node_type_register_static    (const char                  *node_name,
                                                         const GskRenderNodeTypeInfo *node_info);

gpointer        gsk_render_node_alloc                   (GskRenderNodeType            node_type);

gboolean        gsk_render_node_can_diff                (const GskRenderNode         *node1,
                                                         const GskRenderNode         *node2) G_GNUC_PURE;
void            gsk_render_node_diff                    (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         cairo_region_t              *region);
void            gsk_render_node_diff_impossible         (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         cairo_region_t              *region);

G_END_DECLS

#endif /* __GSK_RENDER_NODE_PRIVATE_H__ */
