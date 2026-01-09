#pragma once

#include "gskinsetshadownode.h"

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_inset_shadow_node_new2              (const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);
const GdkColor *        gsk_inset_shadow_node_get_gdk_color     (const GskRenderNode    *node);
const graphene_point_t *gsk_inset_shadow_node_get_offset        (const GskRenderNode    *node);


G_END_DECLS
