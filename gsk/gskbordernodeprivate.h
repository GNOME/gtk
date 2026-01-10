#pragma once

#include "gskbordernode.h"

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

bool            gsk_border_node_get_uniform             (const GskRenderNode         *self) G_GNUC_PURE;
bool            gsk_border_node_get_uniform_color       (const GskRenderNode         *self) G_GNUC_PURE;

GskRenderNode *         gsk_border_node_new2                    (const GskRoundedRect   *outline,
                                                                 const float             border_width[4],
                                                                 const GdkColor          border_color[4]);
const GdkColor *        gsk_border_node_get_gdk_colors          (const GskRenderNode    *node);

G_END_DECLS
