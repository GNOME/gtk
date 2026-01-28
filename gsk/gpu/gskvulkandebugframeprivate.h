#pragma once

#include "gskvulkanframeprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_DEBUG_FRAME (gsk_vulkan_debug_frame_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanDebugFrame, gsk_vulkan_debug_frame, GSK, VULKAN_DEBUG_FRAME, GskVulkanFrame)


G_END_DECLS
