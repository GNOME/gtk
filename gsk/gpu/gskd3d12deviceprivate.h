#pragma once

#include "gskgpudeviceprivate.h"

#include "gskdebugprivate.h"

#include "gdk/gdkdisplayprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_D3D12_DEVICE (gsk_d3d12_device_get_type ())

G_DECLARE_FINAL_TYPE(GskD3d12Device, gsk_d3d12_device, GSK, D3D12_DEVICE, GskGpuDevice)

GskGpuDevice *          gsk_d3d12_device_get_for_display                (GdkDisplay             *display,
                                                                         GError                **error);

ID3D12Device *          gsk_d3d12_device_get_d3d12_device               (GskD3d12Device         *self);
ID3D12RootSignature *   gsk_d3d12_device_get_d3d12_root_signature       (GskD3d12Device         *self);


G_END_DECLS
