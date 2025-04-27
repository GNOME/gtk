#pragma once

#include "gskgpuimageprivate.h"
#include "gskd3d12deviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_D3D12_IMAGE (gsk_d3d12_image_get_type ())

G_DECLARE_FINAL_TYPE (GskD3d12Image, gsk_d3d12_image, GSK, D3D12_IMAGE, GskGpuImage)

GskGpuImage *           gsk_d3d12_image_new                             (GskD3d12Device                 *device,
                                                                         GdkMemoryFormat                 format,
                                                                         gsize                           width,
                                                                         gsize                           height,
                                                                         D3D12_RESOURCE_STATES           initial_state,
                                                                         D3D12_HEAP_FLAGS                heap_flags,
                                                                         D3D12_RESOURCE_FLAGS            resource_flags);
GskGpuImage *           gsk_d3d12_image_new_for_resource                (GskD3d12Device                 *device,
                                                                         ID3D12Resource                 *resource,
                                                                         gboolean                        premultiplied);

ID3D12Resource *        gsk_d3d12_image_get_resource                    (GskD3d12Image                  *self);

G_END_DECLS
