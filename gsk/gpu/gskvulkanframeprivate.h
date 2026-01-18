#pragma once

#include "gskgpuframeprivate.h"

#include "gskgpuopprivate.h"
#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_FRAME         (gsk_vulkan_frame_get_type ())
#define GSK_VULKAN_FRAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_VULKAN_FRAME, GskVulkanFrame))
#define GSK_VULKAN_FRAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_VULKAN_FRAME, GskVulkanFrameClass))
#define GSK_IS_VULKAN_FRAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_VULKAN_FRAME))
#define GSK_IS_VULKAN_FRAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_VULKAN_FRAME))
#define GSK_VULKAN_FRAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_VULKAN_FRAME, GskVulkanFrameClass))

typedef struct _GskVulkanFrame GskVulkanFrame;
typedef struct _GskVulkanFrameClass GskVulkanFrameClass;

struct _GskVulkanFrame
{
  GskGpuFrame parent_instance;
};

struct _GskVulkanFrameClass
{
  GskGpuFrameClass parent_class;
};

GType                   gsk_vulkan_frame_get_type                       (void) G_GNUC_CONST;

void                    gsk_vulkan_semaphores_add_wait                  (GskVulkanSemaphores    *self,
                                                                         VkSemaphore             semaphore,
                                                                         uint64_t                semaphore_wait,
                                                                         VkPipelineStageFlags    stage);
void                    gsk_vulkan_semaphores_add_signal                (GskVulkanSemaphores    *self,
                                                                         VkSemaphore             semaphore);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskVulkanFrame, g_object_unref)

G_END_DECLS
