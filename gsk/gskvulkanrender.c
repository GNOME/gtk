#include "config.h"

#include "gskvulkanrenderprivate.h"

#include "gskrendererprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanpipelineprivate.h"
#include "gskvulkanrenderpassprivate.h"

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

#define DESCRIPTOR_POOL_MAXSETS 128
#define DESCRIPTOR_POOL_MAXSETS_INCREASE 128

struct _GskVulkanRender
{
  GskRenderer *renderer;
  GdkVulkanContext *vulkan;

  graphene_matrix_t mvp;
  int scale_factor;
  VkExtent2D size;
  VkRect2D scissor;

  GHashTable *framebuffers;
  VkCommandPool command_pool;
  VkFence fence;
  VkRenderPass render_pass;

  GHashTable *descriptor_set_indexes;
  VkDescriptorPool descriptor_pool;
  uint32_t descriptor_pool_maxsets;
  VkDescriptorSet *descriptor_sets;  
  gsize n_descriptor_sets;
  GskVulkanPipeline *pipeline;

  VkCommandBuffer command_buffer;

  GskVulkanImage *target;

  GSList *render_passes;
  GSList *cleanup_images;
};

static void
gsk_vulkan_render_compute_mvp (GskVulkanRender *self)
{
  GdkWindow *window = gsk_renderer_get_window (self->renderer);
  graphene_matrix_t modelview, projection;
  cairo_rectangle_int_t extents;

  cairo_region_get_extents (gdk_drawing_context_get_clip (gsk_renderer_get_drawing_context (self->renderer)),
                            &extents);

  self->scale_factor = gsk_renderer_get_scale_factor (self->renderer);
  self->size.width = gdk_window_get_width (window) * self->scale_factor;
  self->size.height = gdk_window_get_height (window) * self->scale_factor;
  self->scissor.offset.x = extents.x * self->scale_factor;
  self->scissor.offset.y = extents.y * self->scale_factor;
  self->scissor.extent.width = extents.width * self->scale_factor;
  self->scissor.extent.height = extents.height * self->scale_factor;

  graphene_matrix_init_scale (&modelview, self->scale_factor, self->scale_factor, 1.0);
  graphene_matrix_init_ortho (&projection,
                              0, self->size.width,
                              0, self->size.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_matrix_multiply (&modelview, &projection, &self->mvp);
}

GskVulkanRender *
gsk_vulkan_render_new (GskRenderer      *renderer,
                       GdkVulkanContext *context)
{
  GskVulkanRender *self;
  VkDevice device;

  self = g_slice_new0 (GskVulkanRender);

  self->vulkan = context;
  self->renderer = renderer;
  self->framebuffers = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->descriptor_set_indexes = g_hash_table_new (g_direct_hash, g_direct_equal);

  device = gdk_vulkan_context_get_device (self->vulkan);

  GSK_VK_CHECK (vkCreateCommandPool, device,
                                     &(const VkCommandPoolCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .queueFamilyIndex = gdk_vulkan_context_get_queue_family_index (self->vulkan),
                                         .flags = 0
                                     },
                                     NULL,
                                     &self->command_pool);

  GSK_VK_CHECK (vkCreateFence, device,
                               &(VkFenceCreateInfo) {
                                   .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT
                               },
                               NULL,
                               &self->fence);

  self->descriptor_pool_maxsets = DESCRIPTOR_POOL_MAXSETS;
  GSK_VK_CHECK (vkCreateDescriptorPool, device,
                                        &(VkDescriptorPoolCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .maxSets = self->descriptor_pool_maxsets,
                                            .poolSizeCount = 1,
                                            .pPoolSizes = (VkDescriptorPoolSize[1]) {
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                    .descriptorCount = 1
                                                }
                                            }
                                        },
                                        NULL,
                                        &self->descriptor_pool);

  GSK_VK_CHECK (vkCreateRenderPass, gdk_vulkan_context_get_device (self->vulkan),
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = gdk_vulkan_context_get_image_format (self->vulkan),
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                              .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                           }
                                        },
                                        .subpassCount = 1,
                                        .pSubpasses = (VkSubpassDescription []) {
                                           {
                                              .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                              .inputAttachmentCount = 0,
                                              .colorAttachmentCount = 1,
                                              .pColorAttachments = (VkAttachmentReference []) {
                                                 {
                                                    .attachment = 0,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pResolveAttachments = (VkAttachmentReference []) {
                                                  {
                                                     .attachment = VK_ATTACHMENT_UNUSED,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pDepthStencilAttachment = NULL,
                                               .preserveAttachmentCount = 1,
                                               .pPreserveAttachments = (uint32_t []) { 0 },
                                            }
                                         },
                                         .dependencyCount = 0
                                      },
                                      NULL,
                                      &self->render_pass);

  self->pipeline = gsk_vulkan_pipeline_new (self->vulkan, self->render_pass);

  return self;
}

typedef struct {
  VkFramebuffer framebuffer;
} HashFramebufferEntry;

static void
gsk_vulkan_render_remove_framebuffer_from_image (gpointer  data,
                                                 GObject  *image)
{
  GskVulkanRender *self = data;
  HashFramebufferEntry *fb;

  fb = g_hash_table_lookup (self->framebuffers, image);
  g_hash_table_remove (self->framebuffers, image);

  vkDestroyFramebuffer (gdk_vulkan_context_get_device (self->vulkan),
                        fb->framebuffer,
                        NULL);

  g_slice_free (HashFramebufferEntry, fb);
}

static VkFramebuffer
gsk_vulkan_render_get_framebuffer (GskVulkanRender *self,
                                   GskVulkanImage  *image)
{
  HashFramebufferEntry *fb;

  fb = g_hash_table_lookup (self->framebuffers, image);
  if (fb)
    return fb->framebuffer;

  fb = g_slice_new0 (HashFramebufferEntry);
  GSK_VK_CHECK (vkCreateFramebuffer, gdk_vulkan_context_get_device (self->vulkan),
                                     &(VkFramebufferCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                         .renderPass = self->render_pass,
                                         .attachmentCount = 1,
                                         .pAttachments = (VkImageView[1]) {
                                             gsk_vulkan_image_get_image_view (image)
                                         },
                                         .width = gsk_vulkan_image_get_width (image),
                                         .height = gsk_vulkan_image_get_height (image),
                                         .layers = 1
                                     },
                                     NULL,
                                     &fb->framebuffer);
  g_hash_table_insert (self->framebuffers, image, fb);
  g_object_weak_ref (G_OBJECT (image), gsk_vulkan_render_remove_framebuffer_from_image, self);

  return fb->framebuffer;
}

void
gsk_vulkan_render_add_cleanup_image (GskVulkanRender *self,
                                     GskVulkanImage  *image)
{
  self->cleanup_images = g_slist_prepend (self->cleanup_images, image);
}

void
gsk_vulkan_render_add_node (GskVulkanRender *self,
                            GskRenderNode   *node)
{
  GskVulkanRenderPass *pass = gsk_vulkan_render_pass_new (self->vulkan);

  self->render_passes = g_slist_prepend (self->render_passes, pass);

  gsk_vulkan_render_pass_add (pass, self, &self->mvp, node);
}

void
gsk_vulkan_render_upload (GskVulkanRender *self)
{
  GSList *l;

  for (l = self->render_passes; l; l = l->next)
    {
      gsk_vulkan_render_pass_upload (l->data, self, self->command_buffer);
    }
}

static gsize
gsk_vulkan_renderer_count_vertices (GskVulkanRender *self)
{
  gsize count;
  GSList *l;

  count = 0;
  for (l = self->render_passes; l; l = l->next)
    {
      count += gsk_vulkan_render_pass_count_vertices (l->data);
    }

  return count;
}

static GskVulkanBuffer *
gsk_vulkan_render_collect_vertices (GskVulkanRender *self)
{
  GskVulkanBuffer *buffer;
  GskVulkanVertex *vertices;
  GSList *l;
  gsize offset, count;
  
  offset = 0;
  count = gsk_vulkan_renderer_count_vertices (self);
  buffer = gsk_vulkan_buffer_new (self->vulkan, sizeof (GskVulkanVertex) * count);
  vertices = (GskVulkanVertex *) gsk_vulkan_buffer_map (buffer);

  for (l = self->render_passes; l; l = l->next)
    {
      offset += gsk_vulkan_render_pass_collect_vertices (l->data, vertices, offset, count - offset);
      g_assert (offset <= count);
    }

  gsk_vulkan_buffer_unmap (buffer);

  return buffer;
}

VkDescriptorSet
gsk_vulkan_render_get_descriptor_set (GskVulkanRender *self,
                                      gsize            id)
{
  g_assert (id < self->n_descriptor_sets);

  return self->descriptor_sets[id];
}

gsize
gsk_vulkan_render_reserve_descriptor_set (GskVulkanRender *self,
                                          GskVulkanImage  *source)
{
  gpointer id_plus_one;

  id_plus_one = g_hash_table_lookup (self->descriptor_set_indexes, source);
  if (id_plus_one)
    return GPOINTER_TO_SIZE (id_plus_one) - 1;

  id_plus_one = GSIZE_TO_POINTER (g_hash_table_size (self->descriptor_set_indexes) + 1);
  g_hash_table_insert (self->descriptor_set_indexes, source, id_plus_one);
  
  return GPOINTER_TO_SIZE (id_plus_one) - 1;
}

static void
gsk_vulkan_render_prepare_descriptor_sets (GskVulkanRender *self,
                                           VkSampler        sampler)
{
  GHashTableIter iter;
  gpointer key, value;
  VkDevice device;
  GSList *l;
  guint i, needed_sets;

  device = gdk_vulkan_context_get_device (self->vulkan);

  for (l = self->render_passes; l; l = l->next)
    {
      gsk_vulkan_render_pass_reserve_descriptor_sets (l->data, self);
    }
  
  needed_sets = g_hash_table_size (self->descriptor_set_indexes);
  if (needed_sets > self->n_descriptor_sets)
    {
      if (needed_sets > self->descriptor_pool_maxsets)
        {
          guint added_sets = needed_sets - self->descriptor_pool_maxsets;
          added_sets = added_sets + DESCRIPTOR_POOL_MAXSETS_INCREASE - 1;
          added_sets -= added_sets % DESCRIPTOR_POOL_MAXSETS_INCREASE;

          vkDestroyDescriptorPool (device,
                                   self->descriptor_pool,
                                   NULL);
          self->descriptor_pool_maxsets += added_sets;
          GSK_VK_CHECK (vkCreateDescriptorPool, device,
                                                &(VkDescriptorPoolCreateInfo) {
                                                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                    .maxSets = self->descriptor_pool_maxsets,
                                                    .poolSizeCount = 1,
                                                    .pPoolSizes = (VkDescriptorPoolSize[1]) {
                                                        {
                                                            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                            .descriptorCount = 1
                                                        }
                                                    }
                                                },
                                                NULL,
                                                &self->descriptor_pool);
        }
      else
        {
          GSK_VK_CHECK (vkResetDescriptorPool, device,
                                               self->descriptor_pool,
                                               0);
        }

      self->n_descriptor_sets = needed_sets;
      self->descriptor_sets = g_renew (VkDescriptorSet, self->descriptor_sets, needed_sets);

      VkDescriptorSetLayout layouts[needed_sets];
      for (i = 0; i < needed_sets; i++)
        {
          layouts[i] = gsk_vulkan_pipeline_get_descriptor_set_layout (self->pipeline);
        }
      GSK_VK_CHECK (vkAllocateDescriptorSets, device,
                                              &(VkDescriptorSetAllocateInfo) {
                                                  .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                  .descriptorPool = self->descriptor_pool,
                                                  .descriptorSetCount = needed_sets,
                                                  .pSetLayouts = layouts
                                              },
                                              self->descriptor_sets);
    }

  g_hash_table_iter_init (&iter, self->descriptor_set_indexes);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GskVulkanImage *image = key;
      gsize id = GPOINTER_TO_SIZE (value) - 1;

      vkUpdateDescriptorSets (device,
                              1,
                              (VkWriteDescriptorSet[1]) {
                                  {
                                      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      .dstSet = self->descriptor_sets[id],
                                      .dstBinding = 0,
                                      .dstArrayElement = 0,
                                      .descriptorCount = 1,
                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      .pImageInfo = &(VkDescriptorImageInfo) {
                                          .sampler = sampler,
                                          .imageView = gsk_vulkan_image_get_image_view (image),
                                          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      }
                                  }
                              },
                              0, NULL);
    }
}

void
gsk_vulkan_render_draw (GskVulkanRender   *self,
                        VkSampler          sampler)
{
  GskVulkanBuffer *buffer;
  GSList *l;

  gsk_vulkan_render_prepare_descriptor_sets (self, sampler);

  buffer = gsk_vulkan_render_collect_vertices (self);

  vkCmdSetViewport (self->command_buffer,
                    0,
                    1,
                    &(VkViewport) {
                      .x = 0,
                      .y = 0,
                      .width = self->size.width,
                      .height = self->size.height,
                      .minDepth = 0,
                      .maxDepth = 1
                    });

  vkCmdSetScissor (self->command_buffer,
                   0,
                   1,
                   &self->scissor);

  vkCmdBeginRenderPass (self->command_buffer,
                        &(VkRenderPassBeginInfo) {
                            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                            .renderPass = self->render_pass,
                            .framebuffer = gsk_vulkan_render_get_framebuffer (self, self->target),
                            .renderArea = { 
                                { 0, 0 },
                                { self->size.width, self->size.height }
                            },
                            .clearValueCount = 1,
                            .pClearValues = (VkClearValue [1]) {
                                { .color = { .float32 = { 0.f, 0.f, 0.f, 0.f } } }
                            }
                        },
                        VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline (self->command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_pipeline_get_pipeline (self->pipeline));

  vkCmdBindVertexBuffers (self->command_buffer,
                          0,
                          1,
                          (VkBuffer[1]) {
                              gsk_vulkan_buffer_get_buffer (buffer)
                          },
                          (VkDeviceSize[1]) { 0 });

  for (l = self->render_passes; l; l = l->next)
    {
      gsk_vulkan_render_pass_draw (l->data, self, self->pipeline, self->command_buffer);
    }

  vkCmdEndRenderPass (self->command_buffer);

  gsk_vulkan_buffer_free (buffer);
}

void
gsk_vulkan_render_submit (GskVulkanRender *self)
{
  GSK_VK_CHECK (vkEndCommandBuffer, self->command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gdk_vulkan_context_get_queue (self->vulkan),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .waitSemaphoreCount = 1,
                                  .pWaitSemaphores = (VkSemaphore[1]) {
                                      gdk_vulkan_context_get_draw_semaphore (self->vulkan)
                                  },
                                  .pWaitDstStageMask = (VkPipelineStageFlags []) {
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  },
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &self->command_buffer,
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = (VkSemaphore[1]) {
                                      gdk_vulkan_context_get_draw_semaphore (self->vulkan)
                                  }
                               },
                               self->fence);

  if (GSK_RENDER_MODE_CHECK (SYNC))
    {
      GSK_VK_CHECK (vkWaitForFences, gdk_vulkan_context_get_device (self->vulkan),
                                     1,
                                     &self->fence,
                                     VK_TRUE,
                                     INT64_MAX);
    }
}

static void
gsk_vulkan_render_cleanup (GskVulkanRender *self)
{
  VkDevice device = gdk_vulkan_context_get_device (self->vulkan);

  /* XXX: Wait for fence here or just in reset()? */
  GSK_VK_CHECK (vkWaitForFences, device,
                                 1,
                                 &self->fence,
                                 VK_TRUE,
                                 INT64_MAX);

  GSK_VK_CHECK (vkResetFences, device,
                               1,
                               &self->fence);
  GSK_VK_CHECK (vkResetCommandPool, device,
                                    self->command_pool,
                                    0);

  g_hash_table_remove_all (self->descriptor_set_indexes);

  g_slist_free_full (self->render_passes, (GDestroyNotify) gsk_vulkan_render_pass_free);
  self->render_passes = NULL;
  g_slist_free_full (self->cleanup_images, g_object_unref);
  self->cleanup_images = NULL;

  g_clear_object (&self->target);
}

void
gsk_vulkan_render_free (GskVulkanRender *self)
{
  GHashTableIter iter;
  gpointer key, value;
  VkDevice device;
  
  gsk_vulkan_render_cleanup (self);

  device = gdk_vulkan_context_get_device (self->vulkan);

  g_hash_table_iter_init (&iter, self->framebuffers);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HashFramebufferEntry *fb = value;

      vkDestroyFramebuffer (gdk_vulkan_context_get_device (self->vulkan),
                            fb->framebuffer,
                            NULL);
      g_slice_free (HashFramebufferEntry, fb);
      g_object_weak_unref (G_OBJECT (key), gsk_vulkan_render_remove_framebuffer_from_image, self);
      g_hash_table_iter_remove (&iter);
    }
  g_hash_table_unref (self->framebuffers);

  g_clear_object (&self->pipeline);

  vkDestroyRenderPass (device,
                       self->render_pass,
                       NULL);

  vkDestroyDescriptorPool (device,
                           self->descriptor_pool,
                           NULL);
  g_free (self->descriptor_sets);
  g_hash_table_unref (self->descriptor_set_indexes);

  vkDestroyFence (device,
                  self->fence,
                  NULL);
  vkDestroyCommandPool (device,
                        self->command_pool,
                        NULL);

  g_slice_free (GskVulkanRender, self);
}

gboolean
gsk_vulkan_render_is_busy (GskVulkanRender *self)
{
  return vkGetFenceStatus (gdk_vulkan_context_get_device (self->vulkan), self->fence) != VK_SUCCESS;
}

void
gsk_vulkan_render_reset (GskVulkanRender *self,
                         GskVulkanImage  *target)
{
  VkDevice device;

  gsk_vulkan_render_cleanup (self);

  self->target = g_object_ref (target);

  gsk_vulkan_render_compute_mvp (self);

  device = gdk_vulkan_context_get_device (self->vulkan);

  GSK_VK_CHECK (vkAllocateCommandBuffers, device,
                                          &(VkCommandBufferAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = self->command_pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1,
                                          },
                                          &self->command_buffer);

  GSK_VK_CHECK (vkBeginCommandBuffer, self->command_buffer,
                                      &(VkCommandBufferBeginInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = 0
                                      });
}

GskRenderer *
gsk_vulkan_render_get_renderer (GskVulkanRender *self)
{
  return self->renderer;
}
