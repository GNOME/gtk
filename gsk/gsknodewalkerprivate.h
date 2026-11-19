#pragma once

#include "gskrendernode.h"

G_BEGIN_DECLS

typedef struct _GskNodeWalker GskNodeWalker;

GskNodeWalker *         gsk_node_walker_new                             (GskRenderNode          *node);
GskNodeWalker *         gsk_node_walker_new_replay                      (GskNodeWalker          *replay,
                                                                         gsize                   start_pos,
                                                                         gsize                   end_pos);
void                    gsk_node_walker_free                            (GskNodeWalker          *self);

gsize                   gsk_node_walker_get_n_nodes                     (GskNodeWalker          *self);
GskRenderNode *         gsk_node_walker_get_node                        (GskNodeWalker          *self,
                                                                         gsize                   depth);
gsize                   gsk_node_walker_get_pos                         (GskNodeWalker          *self,
                                                                         gsize                   depth);

gboolean                gsk_node_walker_start_node                      (GskNodeWalker          *self,
                                                                         GskRenderNode          *node,
                                                                         gsize                   pos) G_GNUC_WARN_UNUSED_RESULT;
void                    gsk_node_walker_end_node                        (GskNodeWalker          *self);

GskRenderNode *         gsk_node_walker_get_copy_node                   (GskNodeWalker          *self,
                                                                         gsize                   depth,
                                                                         gsize                  *out_pos);
gsize                   gsk_node_walker_get_replay_start                (GskNodeWalker          *self,
                                                                         gsize                   pos,
                                                                         GskTransform          **out_transform);

G_END_DECLS

