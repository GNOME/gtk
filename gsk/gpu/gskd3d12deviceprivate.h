#pragma once

#include "gskgpudeviceprivate.h"

#include "gskdebugprivate.h"

#include "gdk/gdkdisplayprivate.h"

G_BEGIN_DECLS

typedef enum
{
  GSK_D3D12_ROOT_PUSH_CONSTANTS,
  GSK_D3D12_ROOT_SAMPLER_0,
  GSK_D3D12_ROOT_SAMPLER_1,
  GSK_D3D12_ROOT_TEXTURE_0,
  GSK_D3D12_ROOT_TEXTURE_1,
  GSK_D3D12_ROOT_N_PARAMETERS
} GskD3d12RootParameter;

#define GSK_TYPE_D3D12_DEVICE (gsk_d3d12_device_get_type ())

G_DECLARE_FINAL_TYPE(GskD3d12Device, gsk_d3d12_device, GSK, D3D12_DEVICE, GskGpuDevice)

GskGpuDevice *          gsk_d3d12_device_get_for_display                (GdkDisplay                     *display,
                                                                         GError                        **error);

ID3D12Device *          gsk_d3d12_device_get_d3d12_device               (GskD3d12Device                 *self);
ID3D12RootSignature *   gsk_d3d12_device_get_d3d12_root_signature       (GskD3d12Device                 *self);
ID3D12DescriptorHeap *  gsk_d3d12_device_get_d3d12_sampler_heap         (GskD3d12Device                 *self);
ID3D12DescriptorHeap *  gsk_d3d12_device_get_d3d12_srv_heap             (GskD3d12Device                 *self);

ID3D12PipelineState *   gsk_d3d12_device_get_d3d12_pipeline_state       (GskD3d12Device                 *self,
                                                                         const GskGpuShaderOpClass      *op_class,
                                                                         GskGpuShaderFlags               flags,
                                                                         GskGpuColorStates               color_states,
                                                                         guint32                         variation,
                                                                         GskGpuBlend                     blend,
                                                                         DXGI_FORMAT                     rtv_format);

void                    gsk_d3d12_device_get_sampler                    (GskD3d12Device                 *self,
                                                                         GskGpuSampler                   sampler,
                                                                         D3D12_GPU_DESCRIPTOR_HANDLE    *out_gpu_handle);

void                    gsk_d3d12_device_alloc_rtv                      (GskD3d12Device                 *self,
                                                                         D3D12_CPU_DESCRIPTOR_HANDLE    *out_descriptor);
void                    gsk_d3d12_device_free_rtv                       (GskD3d12Device                 *self,
                                                                         D3D12_CPU_DESCRIPTOR_HANDLE    *descriptor);
void                    gsk_d3d12_device_alloc_srv                      (GskD3d12Device                 *self,
                                                                         D3D12_CPU_DESCRIPTOR_HANDLE     out_cpu[3],
                                                                         D3D12_GPU_DESCRIPTOR_HANDLE     out_gpu[3]);
void                    gsk_d3d12_device_free_srv                       (GskD3d12Device                 *self,
                                                                         D3D12_CPU_DESCRIPTOR_HANDLE     descriptor[3]);

G_END_DECLS
