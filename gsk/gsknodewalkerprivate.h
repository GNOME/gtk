#pragma once

#include "gskrendernode.h"

G_BEGIN_DECLS

typedef struct _GskNodeWalker GskNodeWalker;

GskNodeWalker *         gsk_node_walker_new                             (GskRenderNode *         node);
void                    gsk_node_walker_free                            (GskNodeWalker          *self);

gboolean                gsk_node_walker_start_node                      (GskNodeWalker          *self,
                                                                         GskRenderNode          *node,
                                                                         gsize                   pos) G_GNUC_WARN_UNUSED_RESULT;
void                    gsk_node_walker_end_node                        (GskNodeWalker          *self);

G_END_DECLS

