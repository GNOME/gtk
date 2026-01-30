#pragma once

#include "gskdebugnode.h"

G_BEGIN_DECLS

typedef struct _GskDebugProfile GskDebugProfile;

struct _GskDebugProfile {
  struct {
    /* recorded by the frame */
    guint64 cpu_ns;
    guint64 cpu_record_ns;
    guint64 cpu_submit_ns;
    guint64 gpu_ns;
    guint64 gpu_pixels;
    /* recorded while processing nodes */
    guint64 offscreen_pixels;
    guint64 upload_pixels;
  } total, self;
};

GskRenderNode *         gsk_debug_node_new_profile              (GskRenderNode                  *child,
                                                                 const GskDebugProfile          *profile,
                                                                 char *                          message);

const GskDebugProfile * gsk_debug_node_get_profile              (GskRenderNode                  *node) G_GNUC_PURE;


G_END_DECLS
