#pragma once

#include "gskdebugnode.h"

G_BEGIN_DECLS

typedef struct _GskDebugProfile GskDebugProfile;

struct _GskDebugProfile {
  guint64 elapsed_cpu_total_ns;
  guint64 elapsed_cpu_self_ns;
  guint64 elapsed_gpu_total_ns;
  guint64 elapsed_gpu_self_ns;
};

GskRenderNode *         gsk_debug_node_new_profile              (GskRenderNode                  *child,
                                                                 const GskDebugProfile          *profile,
                                                                 char *                          message);

const GskDebugProfile * gsk_debug_node_get_profile              (GskRenderNode                  *node) G_GNUC_PURE;


G_END_DECLS
