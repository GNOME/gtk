#pragma once

#include "gskcolornode.h"

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_color_node_new2                     (const GdkColor         *color,
                                                                 const graphene_rect_t  *bounds);
const GdkColor *        gsk_color_node_get_gdk_color            (const GskRenderNode    *node);

G_END_DECLS
