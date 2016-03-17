#ifndef __GSK_RENDER_NODE_PRIVATE_H__
#define __GSK_RENDER_NODE_PRIVATE_H__

#include "gskrendernode.h"
#include <cairo.h>

G_BEGIN_DECLS

#define GSK_RENDER_NODE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_RENDER_NODE, GskRenderNodeClass))
#define GSK_IS_RENDER_NODE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_RENDER_NODE))
#define GSK_RENDER_NODE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_RENDER_NODE, GskRenderNodeClass))

typedef enum {
  GSK_RENDER_NODE_CHANGES_UPDATE_BOUNDS = 1 << 0,
  GSK_RENDER_NODE_CHANGES_UPDATE_TRANSFORM = 1 << 1,
  GSK_RENDER_NODE_CHANGES_UPDATE_SURFACE = 1 << 2,
  GSK_RENDER_NODE_CHANGES_UPDATE_OPACITY = 1 << 3,
  GSK_RENDER_NODE_CHANGES_UPDATE_VISIBILITY = 1 << 4,
  GSK_RENDER_NODE_CHANEGS_UPDATE_HIERARCHY = 1 << 5
} GskRenderNodeChanges;

typedef void (* GskRenderNodeInvalidateFunc) (GskRenderNode *node,
                                              gpointer       data);

struct _GskRenderNode
{
  GObject parent_instance;

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

  /* The contents of the node */
  cairo_surface_t *surface;

  /* Paint opacity */
  double opacity;

  /* Clip rectangle */
  graphene_rect_t bounds;

  /* Transformations relative to the root of the scene */
  graphene_matrix_t world_matrix;

  /* Transformations applied to the node */
  graphene_matrix_t transform;

  /* Transformations applied to the children of the node */
  graphene_matrix_t child_transform;

  /* Invalidation function for root node */
  GskRenderNodeInvalidateFunc invalidate_func;
  gpointer func_data;
  GDestroyNotify destroy_func_data;

  /* Descendants that need to be validated; only for root node */
  GPtrArray *invalidated_descendants;

  GskRenderNodeChanges last_state_change;

  /* Bit fields; leave at the end */
  gboolean hidden : 1;
  gboolean opaque : 1;
  gboolean transform_set : 1;
  gboolean child_transform_set : 1;
  gboolean needs_resize : 1;
  gboolean needs_world_matrix_update : 1;
  gboolean needs_content_update : 1;
  gboolean needs_opacity_update : 1;
  gboolean needs_visibility_update : 1;
};

struct _GskRenderNodeClass
{
  GObjectClass parent_class;

  void (* resize) (GskRenderNode *node);
};

void gsk_render_node_get_bounds (GskRenderNode   *node,
                                 graphene_rect_t *frame);
void gsk_render_node_get_transform (GskRenderNode     *node,
                                    graphene_matrix_t *mv);
double gsk_render_node_get_opacity (GskRenderNode *node);

cairo_surface_t *gsk_render_node_get_surface (GskRenderNode *node);

GskRenderNode *gsk_render_node_get_toplevel (GskRenderNode *node);

void gsk_render_node_update_world_matrix (GskRenderNode *node,
                                          gboolean       force);

void gsk_render_node_get_world_matrix (GskRenderNode     *node,
                                       graphene_matrix_t *mv);

void gsk_render_node_queue_invalidate (GskRenderNode        *node,
                                       GskRenderNodeChanges  changes);

void gsk_render_node_set_invalidate_func (GskRenderNode *root,
                                          GskRenderNodeInvalidateFunc validate_func,
                                          gpointer data,
                                          GDestroyNotify notify);

void gsk_render_node_validate (GskRenderNode *node);

void gsk_render_node_maybe_resize (GskRenderNode *node);

GskRenderNodeChanges gsk_render_node_get_current_state (GskRenderNode *node);
GskRenderNodeChanges gsk_render_node_get_last_state (GskRenderNode *node);

G_END_DECLS

#endif /* __GSK_RENDER_NODE_PRIVATE_H__ */
