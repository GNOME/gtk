#include "config.h"

#include "gskvulkanrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeiter.h"
#include "gskrendernodeprivate.h"
#include "gsktextureprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanpipelineprivate.h"

#include <graphene.h>

typedef struct _GskVulkanTarget GskVulkanTarget;

#ifdef G_ENABLE_DEBUG
typedef struct {
  GQuark cpu_time;
  GQuark gpu_time;
} ProfileTimers;
#endif

struct _GskVulkanRenderer
{
  GskRenderer parent_instance;

  GdkVulkanContext *vulkan;

  guint n_targets;
  GskVulkanTarget **targets;

  VkRenderPass render_pass;
  VkCommandPool command_pool;
  VkFence command_pool_fence;

  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;  

  GskVulkanPipeline *pipeline;

  VkSampler sampler;

#ifdef G_ENABLE_DEBUG
  ProfileTimers profile_timers;
#endif
};

struct _GskVulkanRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskVulkanRenderer, gsk_vulkan_renderer, GSK_TYPE_RENDERER)

struct _GskVulkanTarget {
  VkImage image;
  VkImageView image_view;
  VkFramebuffer framebuffer;
}; 

static GskVulkanTarget *
gsk_vulkan_target_new_for_image (GskVulkanRenderer *self,
                                 VkImage            image)
{
  GskVulkanTarget *target;
  GdkWindow *window;
  VkDevice device;

  device = gdk_vulkan_context_get_device (self->vulkan);
  window = gdk_draw_context_get_window (GDK_DRAW_CONTEXT (self->vulkan));

  target = g_slice_new0 (GskVulkanTarget);

  target->image = image;

  GSK_VK_CHECK (vkCreateImageView, device,
                                   &(VkImageViewCreateInfo) {
                                       .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                       .image = target->image,
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = gdk_vulkan_context_get_image_format (self->vulkan),
                                       .components = {
                                           .r = VK_COMPONENT_SWIZZLE_R,
                                           .g = VK_COMPONENT_SWIZZLE_G,
                                           .b = VK_COMPONENT_SWIZZLE_B,
                                           .a = VK_COMPONENT_SWIZZLE_A,
                                       },
                                       .subresourceRange = {
                                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .baseMipLevel = 0,
                                           .levelCount = 1,
                                           .baseArrayLayer = 0,
                                           .layerCount = 1,
                                       },
                                   },
                                   NULL,
                                   &target->image_view);
  GSK_VK_CHECK (vkCreateFramebuffer, device,
                                     &(VkFramebufferCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                         .renderPass = self->render_pass,
                                         .attachmentCount = 1,
                                         .pAttachments = &target->image_view,
                                         .width = gdk_window_get_width (window),
                                         .height = gdk_window_get_height (window),
                                         .layers = 1
                                     },
                                     NULL,
                                     &target->framebuffer);

  return target;
}

static void
gsk_vulkan_target_free (GskVulkanRenderer *self,
                        GskVulkanTarget   *target)
{
  VkDevice device;

  device = gdk_vulkan_context_get_device (self->vulkan);

  vkDestroyFramebuffer (device,
                        target->framebuffer,
                        NULL);
  vkDestroyImageView (device,
                      target->image_view,
                      NULL);

  g_slice_free (GskVulkanTarget, target);
}

static void
gsk_vulkan_renderer_free_targets (GskVulkanRenderer *self)
{
  guint i;

  for (i = 0; i < self->n_targets; i++)
    {
      gsk_vulkan_target_free (self, self->targets[i]);
    }

  g_clear_pointer (&self->targets, g_free);
  self->n_targets = 0;
}

static void
gsk_vulkan_renderer_update_images_cb (GdkVulkanContext  *context,
                                      GskVulkanRenderer *self)
{
  guint i;

  gsk_vulkan_renderer_free_targets (self);

  self->n_targets = gdk_vulkan_context_get_n_images (context);
  self->targets = g_new (GskVulkanTarget *, self->n_targets);

  for (i = 0; i < self->n_targets; i++)
    {
      self->targets[i] = gsk_vulkan_target_new_for_image (self,
                                                          gdk_vulkan_context_get_image (context, i));
    }
}

static gboolean
gsk_vulkan_renderer_realize (GskRenderer  *renderer,
                             GdkWindow    *window,
                             GError      **error)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  VkDevice device;

  self->vulkan = gdk_window_create_vulkan_context (window, error);
  if (self->vulkan == NULL)
    return FALSE;

  device = gdk_vulkan_context_get_device (self->vulkan);

  GSK_VK_CHECK (vkCreateRenderPass, gdk_vulkan_context_get_device (self->vulkan),
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = gdk_vulkan_context_get_image_format (self->vulkan),
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
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
                                   .flags = 0
                               },
                               NULL,
                               &self->command_pool_fence);

  self->pipeline = gsk_vulkan_pipeline_new (self->vulkan, self->render_pass);

  GSK_VK_CHECK (vkCreateDescriptorPool, device,
                                        &(VkDescriptorPoolCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .maxSets = 1,
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

  GSK_VK_CHECK (vkAllocateDescriptorSets, device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = self->descriptor_pool,
                                              .descriptorSetCount = 1,
                                              .pSetLayouts = (VkDescriptorSetLayout[1]) {
                                                  gsk_vulkan_pipeline_get_descriptor_set_layout (self->pipeline)
                                              }
                                          },
                                          &self->descriptor_set);

  GSK_VK_CHECK (vkCreateSampler, device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE
                                 },
                                 NULL,
                                 &self->sampler);

  g_signal_connect (self->vulkan,
                    "images-updated",
                    G_CALLBACK (gsk_vulkan_renderer_update_images_cb),
                    self);
  gsk_vulkan_renderer_update_images_cb (self->vulkan, self);

  return TRUE;
}

static void
gsk_vulkan_renderer_unrealize (GskRenderer *renderer)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  VkDevice device;

  device = gdk_vulkan_context_get_device (self->vulkan);

  gsk_vulkan_renderer_free_targets (self);
  g_signal_handlers_disconnect_by_func(self->vulkan,
                                       gsk_vulkan_renderer_update_images_cb,
                                       self);

  vkDestroySampler (device,
                    self->sampler,
                    NULL);
  self->sampler = VK_NULL_HANDLE;

  vkDestroyDescriptorPool (device,
                           self->descriptor_pool,
                           NULL);
  self->descriptor_pool = VK_NULL_HANDLE;
  self->descriptor_set = VK_NULL_HANDLE;

  g_clear_object (&self->pipeline);

  vkDestroyFence (device,
                  self->command_pool_fence,
                  NULL);
  self->command_pool_fence = VK_NULL_HANDLE;

  vkDestroyCommandPool (device,
                        self->command_pool,
                        NULL);
  self->command_pool = VK_NULL_HANDLE;

  vkDestroyRenderPass (device,
                       self->render_pass,
                       NULL);
  self->render_pass = VK_NULL_HANDLE;

  g_clear_object (&self->vulkan);
}

static GskVulkanImage *
gsk_vulkan_renderer_prepare_render (GskVulkanRenderer *self,
                                    VkCommandBuffer    command_buffer,
                                    GskRenderNode     *root)
{
  GdkWindow *window;
  GskRenderer *fallback;
  GskVulkanImage *image;
  cairo_surface_t *surface;
  cairo_t *cr;

  window = gsk_renderer_get_window (GSK_RENDERER (self));
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gdk_window_get_width (window),
                                        gdk_window_get_height (window));
  cr = cairo_create (surface);
  fallback = gsk_renderer_create_fallback (GSK_RENDERER (self),
                                           &GRAPHENE_RECT_INIT(
                                               0, 0,
                                               gdk_window_get_width (window),
                                               gdk_window_get_height (window)
                                           ),
                                           cr);

  gsk_renderer_render (fallback, root, NULL);
  g_object_unref (fallback);
  
  cairo_destroy (cr);

  image = gsk_vulkan_image_new_from_data (self->vulkan,
                                          command_buffer,
                                          cairo_image_surface_get_data (surface),
                                          cairo_image_surface_get_width (surface),
                                          cairo_image_surface_get_height (surface),
                                          cairo_image_surface_get_stride (surface));
  cairo_surface_destroy (surface);

  return image;
}

static void
gsk_vulkan_renderer_do_render_commands (GskVulkanRenderer *self,
                                        VkCommandBuffer    command_buffer)
{
  GskVulkanBuffer *buffer;
  GdkWindow *window = gsk_renderer_get_window (GSK_RENDERER (self));
  int width = gdk_window_get_width (window);
  int height = gdk_window_get_height (window);
  float pts[] = {
    0.0,   0.0,     0.0, 0.0,
    width, 0.0,     1.0, 0.0,
    0.0,   height,  0.0, 1.0,

    0.0,   height,  0.0, 1.0,
    width, 0.0,     1.0, 0.0,
    width, height,  1.0, 1.0
  };
  guchar *data;

  buffer = gsk_vulkan_buffer_new (self->vulkan, sizeof (pts));

  data = gsk_vulkan_buffer_map (buffer);
  memcpy (data, pts, sizeof (pts));
  gsk_vulkan_buffer_unmap (buffer);

  vkCmdBindPipeline (command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_pipeline_get_pipeline (self->pipeline));

  vkCmdBindDescriptorSets (command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           gsk_vulkan_pipeline_get_pipeline_layout (self->pipeline),
                           0,
                           1,
                           &self->descriptor_set,
                           0,
                           NULL);

  vkCmdBindVertexBuffers (command_buffer,
                          0,
                          1,
                          (VkBuffer[1]) {
                              gsk_vulkan_buffer_get_buffer (buffer)
                          },
                          (VkDeviceSize[1]) { 0 });

  vkCmdDraw (command_buffer,
             6, 1,
             0, 0);

  gsk_vulkan_buffer_free (buffer);
}

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

static void
gsk_vulkan_renderer_do_render_pass (GskVulkanRenderer *self,
                                    VkCommandBuffer    command_buffer,
                                    GskVulkanImage    *image)
{
  GdkRectangle extents;
  GdkWindow *window;
  int scale_factor, unscaled_width, unscaled_height;
  graphene_matrix_t modelview, projection, mvp;

  window = gsk_renderer_get_window (GSK_RENDERER (self));
  cairo_region_get_extents (gdk_drawing_context_get_clip (gsk_renderer_get_drawing_context (GSK_RENDERER (self))),
                            &extents);
  scale_factor = gsk_renderer_get_scale_factor (GSK_RENDERER (self));
  unscaled_width = gdk_window_get_width (window) * scale_factor;
  unscaled_height = gdk_window_get_height (window) * scale_factor;

  vkUpdateDescriptorSets (gdk_vulkan_context_get_device (self->vulkan),
                          1,
                          (VkWriteDescriptorSet[1]) {
                              {
                                  .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                  .dstSet = self->descriptor_set,
                                  .dstBinding = 0,
                                  .dstArrayElement = 0,
                                  .descriptorCount = 1,
                                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .pImageInfo = &(VkDescriptorImageInfo) {
                                      .sampler = self->sampler,
                                      .imageView = gsk_vulkan_image_get_image_view (image),
                                      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                  }
                              }
                          },
                          0, NULL);

  vkCmdBeginRenderPass (command_buffer,
                        &(VkRenderPassBeginInfo) {
                            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                            .renderPass = self->render_pass,
                            .framebuffer = self->targets[gdk_vulkan_context_get_draw_index (self->vulkan)]->framebuffer,
                            .renderArea = { 
                                { 0, 0 },
                                { unscaled_width, unscaled_height }
                            },
                            .clearValueCount = 1,
                            .pClearValues = (VkClearValue [1]) {
                                { .color = { .float32 = { 0.f, 0.f, 0.f, 0.f } } }
                            }
                        },
                        VK_SUBPASS_CONTENTS_INLINE);

  vkCmdSetViewport (command_buffer,
                    0,
                    1,
                    &(VkViewport) {
                      .x = 0,
                      .y = 0,
                      .width = unscaled_width,
                      .height = unscaled_height,
                      .minDepth = 0,
                      .maxDepth = 1
                    });

  vkCmdSetScissor (command_buffer,
                   0,
                   1,
                   &(VkRect2D) {
                       { extents.x * scale_factor, extents.y * scale_factor },
                       { extents.width * scale_factor, extents.height * scale_factor }
                   });

  graphene_matrix_init_scale (&modelview, scale_factor, scale_factor, 1.0);
  graphene_matrix_init_ortho (&projection,
                              0, unscaled_width,
                              0, unscaled_height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_matrix_multiply (&modelview, &projection, &mvp);

  vkCmdPushConstants (command_buffer,
                      gsk_vulkan_pipeline_get_pipeline_layout (self->pipeline),
                      VK_SHADER_STAGE_VERTEX_BIT,
                      0,
                      sizeof (graphene_matrix_t),
                      &mvp);

  gsk_vulkan_renderer_do_render_commands (self, command_buffer);

  vkCmdEndRenderPass (command_buffer);
}

static void
gsk_vulkan_renderer_render (GskRenderer   *renderer,
                            GskRenderNode *root)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  VkCommandBuffer command_buffer;
  GskVulkanImage *image;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  GSK_VK_CHECK (vkAllocateCommandBuffers, gdk_vulkan_context_get_device (self->vulkan),
                                          &(VkCommandBufferAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = self->command_pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1,
                                          },
                                          &command_buffer);

  GSK_VK_CHECK (vkBeginCommandBuffer, command_buffer,
                                      &(VkCommandBufferBeginInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = 0
                                      });

  image = gsk_vulkan_renderer_prepare_render (self, command_buffer, root);

  gsk_vulkan_renderer_do_render_pass (self, command_buffer, image);

  GSK_VK_CHECK (vkEndCommandBuffer, command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gdk_vulkan_context_get_queue (self->vulkan),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .waitSemaphoreCount = 1,
                                  .pWaitSemaphores = (VkSemaphore[1]) {
                                      gdk_vulkan_context_get_draw_semaphore (self->vulkan),
                                  },
                                  .pWaitDstStageMask = (VkPipelineStageFlags []) {
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  },
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &command_buffer,
                               },
                               self->command_pool_fence);

  GSK_VK_CHECK (vkWaitForFences, gdk_vulkan_context_get_device (self->vulkan),
                                 1,
                                 &self->command_pool_fence,
                                 true,
                                 INT64_MAX);
  GSK_VK_CHECK (vkResetFences, gdk_vulkan_context_get_device (self->vulkan),
                               1,
                               &self->command_pool_fence);

  GSK_VK_CHECK (vkResetCommandPool, gdk_vulkan_context_get_device (self->vulkan),
                                    self->command_pool,
                                    0);
   
  gsk_vulkan_image_free (image);

#ifdef G_ENABLE_DEBUG
  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);
#endif
}

static GdkDrawingContext *
gsk_vulkan_renderer_begin_draw_frame (GskRenderer          *renderer,
                                      const cairo_region_t *region)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  cairo_region_t *whole_window;
  GdkDrawingContext *result;
  GdkWindow *window;

  window = gsk_renderer_get_window (renderer);
  
  whole_window = cairo_region_create_rectangle (&(GdkRectangle) {
                                                    0, 0,
                                                    gdk_window_get_width (window),
                                                    gdk_window_get_height (window)
                                                });

  result = gdk_window_begin_draw_frame (window,
                                        GDK_DRAW_CONTEXT (self->vulkan),
                                        whole_window);

  cairo_region_destroy (whole_window);

  return result;
}

static void
gsk_vulkan_renderer_class_init (GskVulkanRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_vulkan_renderer_realize;
  renderer_class->unrealize = gsk_vulkan_renderer_unrealize;
  renderer_class->render = gsk_vulkan_renderer_render;
  renderer_class->begin_draw_frame = gsk_vulkan_renderer_begin_draw_frame;
}

static void
gsk_vulkan_renderer_init (GskVulkanRenderer *self)
{
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));
#endif

  gsk_ensure_resources ();

#ifdef G_ENABLE_DEBUG
  self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
#endif
}
