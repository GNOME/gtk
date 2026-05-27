#pragma once

#include "gskroundedclipnode.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_rounded_clip_node_new2              (GskRenderNode          *child,
                                                                 const GskRoundedRect   *clip,
                                                                 GskRectSnap             snap);

G_END_DECLS
