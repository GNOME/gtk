#pragma once

#include "gskgpubufferprivate.h"

#include "gskd3d12deviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_D3D12_BUFFER (gsk_d3d12_buffer_get_type ())

G_DECLARE_FINAL_TYPE (GskD3d12Buffer, gsk_d3d12_buffer, GSK, D3D12_BUFFER, GskGpuBuffer)

GskGpuBuffer *          gsk_d3d12_buffer_new_from_resource              (GskD3d12Device         *device,
                                                                         ID3D12Resource         *resource,
                                                                         gsize                   size);

GskGpuBuffer *          gsk_d3d12_buffer_new_vertex                     (GskD3d12Device         *device,
                                                                         gsize                   size);
GskGpuBuffer *          gsk_d3d12_buffer_new_storage                    (GskD3d12Device         *device,
                                                                         gsize                   size);

ID3D12Resource *        gsk_d3d12_buffer_get_d3d12_resource             (GskD3d12Buffer         *self);

G_END_DECLS
