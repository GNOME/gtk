#pragma once

#include "gskshadownode.h"

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

typedef struct _GskShadowEntry GskShadowEntry;
struct _GskShadowEntry
{ 
  GdkColor color;
  graphene_point_t offset;
  float radius;
};

GskRenderNode * gsk_shadow_node_new2                    (GskRenderNode        *child,
                                                         const GskShadowEntry *shadows,
                                                         gsize                 n_shadows);

const GskShadowEntry *gsk_shadow_node_get_shadow_entry  (const GskRenderNode  *node,
                                                         gsize                 i);

G_END_DECLS
