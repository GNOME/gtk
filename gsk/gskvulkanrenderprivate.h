#ifndef __GSK_VULKAN_RENDER_PRIVATE_H__
#define __GSK_VULKAN_RENDER_PRIVATE_H__

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gsk/gskvulkanimageprivate.h"
#include "gsk/gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanRender GskVulkanRender;
typedef struct _GskVulkanVertex GskVulkanVertex;

struct _GskVulkanRender
{
  GskRenderer *renderer;
  GdkVulkanContext *vulkan;

  graphene_matrix_t mvp;
  int scale_factor;
  VkExtent2D size;
  VkRect2D scissor;


  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;

  GSList *render_passes;
  GSList *cleanup_images;
};

struct _GskVulkanVertex
{
  float x;
  float y;
  float tex_x;
  float tex_y;
};

GskVulkanRender *       gsk_vulkan_render_new                           (GskRenderer            *renderer,
                                                                         GdkVulkanContext       *context,
                                                                         VkCommandPool           command_pool);
void                    gsk_vulkan_render_free                          (GskVulkanRender        *self);

void                    gsk_vulkan_render_add_cleanup_image             (GskVulkanRender        *self,
                                                                         GskVulkanImage         *image);

void                    gsk_vulkan_render_add_node                      (GskVulkanRender        *self,
                                                                         GskRenderNode          *node);

void                    gsk_vulkan_render_upload                        (GskVulkanRender        *self);

void                    gsk_vulkan_render_draw                          (GskVulkanRender        *self,
                                                                         GskVulkanPipeline      *pipeline,
                                                                         VkRenderPass            render_pass,
                                                                         VkFramebuffer           framebuffer,
                                                                         VkDescriptorSet         descriptor_set,
                                                                         VkSampler               sampler);

void                    gsk_vulkan_render_submit                        (GskVulkanRender        *self,
                                                                         VkFence                 fence);

G_END_DECLS

#endif /* __GSK_VULKAN_RENDER_PRIVATE_H__ */
