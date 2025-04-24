#pragma once

#include "gskgpuframeprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_D3D12_FRAME (gsk_d3d12_frame_get_type ())

G_DECLARE_FINAL_TYPE (GskD3d12Frame, gsk_d3d12_frame, GSK, D3D12_FRAME, GskGpuFrame)

G_END_DECLS
