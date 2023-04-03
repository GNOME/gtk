#pragma once

#include "gskrendernode.h"
#include <cairo.h>

G_BEGIN_DECLS

typedef struct _GskRenderNodeClass GskRenderNodeClass;

/* Keep this in sync with the GskRenderNodeType enumeration.
 *
 * We don't add an "n-types" value to avoid having to handle
 * it in every single switch.
 */
#define GSK_RENDER_NODE_TYPE_N_TYPES    (GSK_MASK_NODE + 1)

extern GType gsk_render_node_types[];

#define GSK_IS_RENDER_NODE_TYPE(node,type) \
  (G_TYPE_INSTANCE_GET_CLASS ((node), GSK_TYPE_RENDER_NODE, GskRenderNodeClass)->node_type == (type))

struct _GskRenderNode
{
  GTypeInstance parent_instance;

  gatomicrefcount ref_count;

  graphene_rect_t bounds;

  guint prefers_high_depth : 1;
  guint offscreen_for_opacity : 1;
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
  gboolean        (* get_opaque)  (GskRenderNode  *node,
                                   graphene_rect_t*opaque);
};

void            gsk_render_node_init_types              (void);

GType           gsk_render_node_type_register_static    (const char                  *node_name,
                                                         gsize                        instance_size,
                                                         GClassInitFunc               class_init);

gpointer        gsk_render_node_alloc                   (GskRenderNodeType            node_type);

gboolean        gsk_render_node_can_diff                (const GskRenderNode         *node1,
                                                         const GskRenderNode         *node2) G_GNUC_PURE;
void            gsk_render_node_diff                    (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         cairo_region_t              *region);
void            gsk_render_node_diff_impossible         (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         cairo_region_t              *region);
void            gsk_container_node_diff_with            (GskRenderNode               *container,
                                                         GskRenderNode               *other,
                                                         cairo_region_t              *region);

gboolean        gsk_render_node_get_opaque              (GskRenderNode               *node,
                                                         graphene_rect_t             *opaque);

bool            gsk_border_node_get_uniform             (const GskRenderNode         *self);
bool            gsk_border_node_get_uniform_color       (const GskRenderNode         *self);

void            gsk_text_node_serialize_glyphs          (GskRenderNode               *self,
                                                         GString                     *str);

GskRenderNode ** gsk_container_node_get_children        (const GskRenderNode *node,
                                                         guint               *n_children);

void             gsk_transform_node_get_translate       (const GskRenderNode *node,
                                                         float               *dx,
                                                         float               *dy);
gboolean       gsk_render_node_prefers_high_depth       (const GskRenderNode *node);

gboolean       gsk_container_node_is_disjoint           (const GskRenderNode *node);

gboolean       gsk_render_node_use_offscreen_for_opacity (const GskRenderNode *node);


G_END_DECLS

