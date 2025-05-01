#pragma once

#include "gskgpuimageprivate.h"
#include "gskd3d12deviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_D3D12_IMAGE (gsk_d3d12_image_get_type ())

G_DECLARE_FINAL_TYPE (GskD3d12Image, gsk_d3d12_image, GSK, D3D12_IMAGE, GskGpuImage)

GskGpuImage *           gsk_d3d12_image_new                             (GskD3d12Device                 *device,
                                                                         GdkMemoryFormat                 format,
                                                                         gboolean                        with_mipmap,
                                                                         GskGpuConversion                conv,
                                                                         gsize                           width,
                                                                         gsize                           height,
                                                                         D3D12_RESOURCE_STATES           initial_state,
                                                                         D3D12_HEAP_FLAGS                heap_flags,
                                                                         D3D12_RESOURCE_FLAGS            resource_flags);
GskGpuImage *           gsk_d3d12_image_new_for_resource                (GskD3d12Device                 *device,
                                                                         ID3D12Resource                 *resource,
                                                                         D3D12_RESOURCE_STATES           initial_state,
                                                                         gboolean                        premultiplied);

ID3D12Resource *        gsk_d3d12_image_get_resource                    (GskD3d12Image                  *self);

const D3D12_CPU_DESCRIPTOR_HANDLE *
                        gsk_d3d12_image_get_rtv                         (GskD3d12Image                  *self);

void                    gsk_d3d12_image_transition                      (GskD3d12Image                  *self,
                                                                         ID3D12GraphicsCommandList      *command_list,
                                                                         D3D12_RESOURCE_STATES           state);

G_END_DECLS
