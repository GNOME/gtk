#pragma once

#include "gskgldeviceprivate.h"

G_BEGIN_DECLS


void                            gsk_gl_pipeline_init_cache              (GskGpuCache                    *cache);
void                            gsk_gl_pipeline_finish_cache            (GskGpuCache                    *cache);

void                            gsk_gl_pipeline_use                     (GskGLDevice                    *device,
                                                                         const GskGpuShaderOpClass      *op_class,
                                                                         GskGpuShaderFlags               flags,
                                                                         GskGpuColorStates               color_states,
                                                                         guint32                         variation);


G_END_DECLS
