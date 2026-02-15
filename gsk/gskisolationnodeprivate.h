#pragma once

#include "gskisolationnode.h"

#include "gskrendernodeprivate.h"

G_BEGIN_DECLS

GskIsolation            gsk_isolation_features_simplify_for_node(GskIsolation            features,
                                                                 GskRenderNode          *node) G_GNUC_PURE;

G_END_DECLS
