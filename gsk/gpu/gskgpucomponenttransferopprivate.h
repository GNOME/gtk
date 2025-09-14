#pragma once

#include "gskgpushaderopprivate.h"
#include "gskcomponenttransfer.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_component_transfer_op                   (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GskGpuColorStates               color_states,
                                                                         const graphene_point_t         *offset,
                                                                         float                           opacity,
                                                                         const GskGpuShaderImage        *image,
                                                                         const GskComponentTransfer     *red,
                                                                         const GskComponentTransfer     *green,
                                                                         const GskComponentTransfer     *blue,
                                                                         const GskComponentTransfer     *alpha);

G_END_DECLS

