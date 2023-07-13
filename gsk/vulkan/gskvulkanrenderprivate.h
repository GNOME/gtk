#pragma once

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gskvulkanimageprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanrenderpassprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_VULKAN_SAMPLER_DEFAULT,
  GSK_VULKAN_SAMPLER_REPEAT,
  GSK_VULKAN_SAMPLER_NEAREST
} GskVulkanRenderSampler;

GskVulkanRender *       gsk_vulkan_render_new                           (GskRenderer            *renderer,
                                                                         GdkVulkanContext       *context);
void                    gsk_vulkan_render_free                          (GskVulkanRender        *self);

gboolean                gsk_vulkan_render_is_busy                       (GskVulkanRender        *self);
void                    gsk_vulkan_render_reset                         (GskVulkanRender        *self,
                                                                         GskVulkanImage         *target,
                                                                         const graphene_rect_t  *rect,
                                                                         const cairo_region_t   *clip,
                                                                         GskRenderNode          *node);

GskRenderer *           gsk_vulkan_render_get_renderer                  (GskVulkanRender        *self);
GdkVulkanContext *      gsk_vulkan_render_get_context                   (GskVulkanRender        *self);

gpointer                gsk_vulkan_render_alloc_op                      (GskVulkanRender        *self,
                                                                         gsize                   size);

VkPipeline              gsk_vulkan_render_get_pipeline                  (GskVulkanRender        *self,
                                                                         const GskVulkanOpClass *op_class,
                                                                         const char             *clip_type,
                                                                         VkFormat                format,
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

void                    gsk_vulkan_render_draw                          (GskVulkanRender        *self);

GdkTexture *            gsk_vulkan_render_download_target               (GskVulkanRender        *self);
VkFence                 gsk_vulkan_render_get_fence                     (GskVulkanRender        *self);

G_END_DECLS

