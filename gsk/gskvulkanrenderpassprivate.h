#ifndef __GSK_VULKAN_RENDER_PASS_PRIVATE_H__
#define __GSK_VULKAN_RENDER_PASS_PRIVATE_H__

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gsk/gskvulkanbufferprivate.h"
#include "gsk/gskvulkanrenderprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanRenderPass GskVulkanRenderPass;

GskVulkanRenderPass *   gsk_vulkan_render_pass_new                      (GdkVulkanContext       *context);
void                    gsk_vulkan_render_pass_free                     (GskVulkanRenderPass    *self);

void                    gsk_vulkan_render_pass_add                      (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         const graphene_matrix_t*mvp,
                                                                         const graphene_rect_t  *viewport,
                                                                         GskRenderNode          *node);

void                    gsk_vulkan_render_pass_upload                   (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanUploader      *uploader);

gsize                   gsk_vulkan_render_pass_count_vertex_data        (GskVulkanRenderPass    *self);
gsize                   gsk_vulkan_render_pass_collect_vertex_data      (GskVulkanRenderPass    *self,
                                                                         guchar                 *data,
                                                                         gsize                   offset,
                                                                         gsize                   total);

void                    gsk_vulkan_render_pass_reserve_descriptor_sets  (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render);
void                    gsk_vulkan_render_pass_draw                     (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanBuffer        *vertex_buffer,
                                                                         GskVulkanPipelineLayout *layout,
                                                                         VkCommandBuffer         command_buffer);

G_END_DECLS

#endif /* __GSK_VULKAN_RENDER_PASS_PRIVATE_H__ */
