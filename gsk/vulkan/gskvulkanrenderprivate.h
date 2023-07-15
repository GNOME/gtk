#pragma once

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gskvulkanclipprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanrenderpassprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_VULKAN_SAMPLER_DEFAULT,
  GSK_VULKAN_SAMPLER_REPEAT,
  GSK_VULKAN_SAMPLER_NEAREST
} GskVulkanRenderSampler;

typedef void            (* GskVulkanDownloadFunc)                       (gpointer                user_data,
                                                                         GdkMemoryFormat         format,
                                                                         const guchar           *data,
                                                                         int                     width,
                                                                         int                     height,
                                                                         gsize                   stride);

GskVulkanRender *       gsk_vulkan_render_new                           (GskRenderer            *renderer,
                                                                         GdkVulkanContext       *context);
void                    gsk_vulkan_render_free                          (GskVulkanRender        *self);

gboolean                gsk_vulkan_render_is_busy                       (GskVulkanRender        *self);
void                    gsk_vulkan_render_render                        (GskVulkanRender        *self,
                                                                         GskVulkanImage         *target,
                                                                         const graphene_rect_t  *rect,
                                                                         const cairo_region_t   *clip,
                                                                         GskRenderNode          *node,
                                                                         GskVulkanDownloadFunc   download_func,
                                                                         gpointer                download_data);

GskRenderer *           gsk_vulkan_render_get_renderer                  (GskVulkanRender        *self);
GdkVulkanContext *      gsk_vulkan_render_get_context                   (GskVulkanRender        *self);

gpointer                gsk_vulkan_render_alloc_op                      (GskVulkanRender        *self,
                                                                         gsize                   size);

VkPipelineLayout        gsk_vulkan_render_get_pipeline_layout           (GskVulkanRender        *self);
VkPipeline              gsk_vulkan_render_get_pipeline                  (GskVulkanRender        *self,
                                                                         const GskVulkanOpClass *op_class,
                                                                         GskVulkanShaderClip     clip,
                                                                         VkRenderPass            render_pass);
VkRenderPass            gsk_vulkan_render_get_render_pass               (GskVulkanRender        *self,
                                                                         VkFormat                format,
                                                                         VkImageLayout           from_layout,
                                                                         VkImageLayout           to_layout);
gsize                   gsk_vulkan_render_get_image_descriptor          (GskVulkanRender        *self,
                                                                         GskVulkanImage         *source,
                                                                         GskVulkanRenderSampler  render_sampler);
gsize                   gsk_vulkan_render_get_buffer_descriptor         (GskVulkanRender        *self,
                                                                         GskVulkanBuffer        *buffer);
guchar *                gsk_vulkan_render_get_buffer_memory             (GskVulkanRender        *self,
                                                                         gsize                   size,
                                                                         gsize                   alignment,
                                                                         gsize                  *out_offset);

VkFence                 gsk_vulkan_render_get_fence                     (GskVulkanRender        *self);

G_END_DECLS

