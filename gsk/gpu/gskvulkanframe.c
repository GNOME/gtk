#include "config.h"

#include "gskvulkanframeprivate.h"

#include "gskgpuopprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkandeviceprivate.h"
#include "gskvulkanimageprivate.h"

#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkglcontextprivate.h"
#include "gdk/gdkgltextureprivate.h"

#define GDK_ARRAY_NAME gsk_semaphores
#define GDK_ARRAY_TYPE_NAME GskSemaphores
#define GDK_ARRAY_ELEMENT_TYPE VkSemaphore
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_pipeline_stages
#define GDK_ARRAY_TYPE_NAME GskPipelineStages
#define GDK_ARRAY_ELEMENT_TYPE VkPipelineStageFlags
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

struct _GskVulkanSemaphores
{
  GskSemaphores wait_semaphores;
  GskPipelineStages wait_stages;
  GskSemaphores signal_semaphores;
};

struct _GskVulkanFrame
{
  GskGpuFrame parent_instance;

  VkSemaphore vk_acquire_semaphore;
  VkFence vk_fence;
  VkCommandBuffer vk_command_buffer;

  gsize pool_n_sets;
  gsize pool_n_images;
  gsize pool_n_buffers;
};

struct _GskVulkanFrameClass
{
  GskGpuFrameClass parent_class;
};

G_DEFINE_TYPE (GskVulkanFrame, gsk_vulkan_frame, GSK_TYPE_GPU_FRAME)

static gboolean
gsk_vulkan_frame_is_busy (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  VkDevice device;
  VkResult res;

  device = gsk_vulkan_device_get_vk_device (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)));

  res = vkGetFenceStatus (device, self->vk_fence);
  if (res == VK_NOT_READY)
    return TRUE;

  gsk_vulkan_handle_result (res, "vkGetFenceStatus");
  return res;
}

static void
gsk_vulkan_frame_wait (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  VkDevice vk_device;

  vk_device = gsk_vulkan_device_get_vk_device (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)));

  GSK_VK_CHECK (vkWaitForFences, vk_device,
                                 1,
                                 &self->vk_fence,
                                 VK_FALSE,
                                 INT64_MAX);
}

static void
gsk_vulkan_frame_setup (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkCommandPool vk_command_pool;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  vk_command_pool = gsk_vulkan_device_get_vk_command_pool (device);

  GSK_VK_CHECK (vkAllocateCommandBuffers, vk_device,
                                          &(VkCommandBufferAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = vk_command_pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1,
                                          },
                                          &self->vk_command_buffer);

  GDK_VK_CHECK (vkCreateSemaphore, vk_device,
                                   &(VkSemaphoreCreateInfo) {
                                       .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                   },
                                   NULL,
                                   &self->vk_acquire_semaphore);

  GSK_VK_CHECK (vkCreateFence, vk_device,
                               &(VkFenceCreateInfo) {
                                   .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                               },
                               NULL,
                               &self->vk_fence);
}

static void
gsk_vulkan_frame_cleanup (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanDevice *device;
  VkDevice vk_device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  GSK_VK_CHECK (vkWaitForFences, vk_device,
                                 1,
                                 &self->vk_fence,
                                 VK_TRUE,
                                 INT64_MAX);

  GSK_VK_CHECK (vkResetFences, vk_device,
                               1,
                               &self->vk_fence);

  GSK_VK_CHECK (vkResetCommandBuffer, self->vk_command_buffer,
                                      0);

  GSK_GPU_FRAME_CLASS (gsk_vulkan_frame_parent_class)->cleanup (frame);
}

static void
gsk_vulkan_frame_begin (GskGpuFrame           *frame,
                        GdkDrawContext        *context,
                        GdkMemoryDepth         depth,
                        const cairo_region_t  *region,
                        const graphene_rect_t *opaque)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);

  gdk_vulkan_context_set_draw_semaphore (GDK_VULKAN_CONTEXT (context), self->vk_acquire_semaphore);
  GSK_GPU_FRAME_CLASS (gsk_vulkan_frame_parent_class)->begin (frame, context, depth, region, opaque);
}

static GskGpuImage *
gsk_vulkan_frame_upload_texture (GskGpuFrame  *frame,
                                 gboolean      with_mipmap,
                                 GdkTexture   *texture)
{
#ifdef HAVE_DMABUF
  if (GDK_IS_GL_TEXTURE (texture))
    {
      GdkGLTexture *gltexture = GDK_GL_TEXTURE (texture);
      GdkDisplay *display;
      GdkGLContext *glcontext;
      GdkDmabuf dmabuf;

      display = gsk_gpu_device_get_display (gsk_gpu_frame_get_device (frame));
      glcontext = gdk_display_get_gl_context (display);
      if (gdk_gl_context_is_shared (glcontext, gdk_gl_texture_get_context (gltexture)))
        {
          gdk_gl_context_make_current (glcontext);

          if (gdk_gl_context_export_dmabuf (gdk_gl_texture_get_context (gltexture),
                                            gdk_gl_texture_get_id (gltexture),
                                            &dmabuf))
            {
              GskGpuImage *image;

              image = gsk_vulkan_image_new_for_dmabuf (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                       gdk_texture_get_width (texture),
                                                       gdk_texture_get_height (texture),
                                                       &dmabuf,
                                                       gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_PREMULTIPLIED);

              /* Vulkan import dups the fds, so we can close these */
              gdk_dmabuf_close_fds (&dmabuf);

              if (image)
                {
                  gsk_gpu_image_toggle_ref_texture (image, texture);
                  return image;
                }
            }
        }
    }

  if (GDK_IS_DMABUF_TEXTURE (texture))
    {
      GskGpuImage *image;

      image = gsk_vulkan_image_new_for_dmabuf (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                               gdk_texture_get_width (texture),
                                               gdk_texture_get_height (texture),
                                               gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture)),
                                               gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_PREMULTIPLIED);
      if (image)
        {
          gsk_gpu_image_toggle_ref_texture (image, texture);
          return image;
        }
    }
#endif

  return GSK_GPU_FRAME_CLASS (gsk_vulkan_frame_parent_class)->upload_texture (frame, with_mipmap, texture);
}

static GskGpuBuffer *
gsk_vulkan_frame_create_vertex_buffer (GskGpuFrame *frame,
                                       gsize        size)
{
  return gsk_vulkan_buffer_new_vertex (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)), size);
}

static GskGpuBuffer *
gsk_vulkan_frame_create_globals_buffer (GskGpuFrame *frame,
                                        gsize        size)
{
  return NULL;
}

static GskGpuBuffer *
gsk_vulkan_frame_create_storage_buffer (GskGpuFrame *frame,
                                        gsize        size)
{
  return gsk_vulkan_buffer_new_storage (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)), size);
}

static void
gsk_vulkan_frame_write_texture_vertex_data (GskGpuFrame    *self,
                                            guchar         *data,
                                            GskGpuImage   **images,
                                            GskGpuSampler  *samplers,
                                            gsize           n_images)
{
}

static void
gsk_vulkan_frame_submit (GskGpuFrame       *frame,
                         GskRenderPassType  pass_type,
                         GskGpuBuffer      *vertex_buffer,
                         GskGpuBuffer      *globals_buffer,
                         GskGpuOp          *op)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanSemaphores semaphores;
  GskVulkanCommandState state = { 0, };

  GSK_VK_CHECK (vkBeginCommandBuffer, self->vk_command_buffer,
                                      &(VkCommandBufferBeginInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                      });

  if (vertex_buffer)
    vkCmdBindVertexBuffers (self->vk_command_buffer,
                            0,
                            1,
                            (VkBuffer[1]) {
                                gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (vertex_buffer))
                            },
                            (VkDeviceSize[1]) { 0 });

  gsk_semaphores_init (&semaphores.wait_semaphores);
  gsk_pipeline_stages_init (&semaphores.wait_stages);
  gsk_semaphores_init (&semaphores.signal_semaphores);

  if (pass_type == GSK_RENDER_PASS_PRESENT)
    {
      gsk_vulkan_semaphores_add_wait (&semaphores,
                                      self->vk_acquire_semaphore,
                                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }

  state.vk_command_buffer = self->vk_command_buffer;
  state.vk_render_pass = VK_NULL_HANDLE;
  state.vk_format = VK_FORMAT_UNDEFINED;
  state.blend = GSK_GPU_BLEND_OVER; /* should we have a BLEND_NONE? */
  state.semaphores = &semaphores;

  while (op)
    {
      op = gsk_gpu_op_vk_command (op, frame, &state);
    }

  GSK_VK_CHECK (vkEndCommandBuffer, self->vk_command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gsk_vulkan_device_get_vk_queue (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame))),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &self->vk_command_buffer,
                                  .pWaitSemaphores = gsk_semaphores_get_data (&semaphores.wait_semaphores),
                                  .pWaitDstStageMask = gsk_pipeline_stages_get_data (&semaphores.wait_stages),
                                  .waitSemaphoreCount = gsk_semaphores_get_size (&semaphores.wait_semaphores),
                                  .pSignalSemaphores = gsk_semaphores_get_data (&semaphores.signal_semaphores),
                                  .signalSemaphoreCount = gsk_semaphores_get_size (&semaphores.signal_semaphores),
                               },
                               self->vk_fence);

  gsk_semaphores_clear (&semaphores.wait_semaphores);
  gsk_pipeline_stages_clear (&semaphores.wait_stages);
  gsk_semaphores_clear (&semaphores.signal_semaphores);
}

static void
gsk_vulkan_frame_finalize (GObject *object)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (object);
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkCommandPool vk_command_pool;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  vk_command_pool = gsk_vulkan_device_get_vk_command_pool (device);

  vkFreeCommandBuffers (vk_device,
                        vk_command_pool,
                        1, &self->vk_command_buffer);
  vkDestroySemaphore (vk_device,
                      self->vk_acquire_semaphore,
                      NULL);
  vkDestroyFence (vk_device,
                  self->vk_fence,
                  NULL);

  G_OBJECT_CLASS (gsk_vulkan_frame_parent_class)->finalize (object);
}

static void
gsk_vulkan_frame_class_init (GskVulkanFrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_frame_class->is_busy = gsk_vulkan_frame_is_busy;
  gpu_frame_class->wait = gsk_vulkan_frame_wait;
  gpu_frame_class->setup = gsk_vulkan_frame_setup;
  gpu_frame_class->cleanup = gsk_vulkan_frame_cleanup;
  gpu_frame_class->begin = gsk_vulkan_frame_begin;
  gpu_frame_class->upload_texture = gsk_vulkan_frame_upload_texture;
  gpu_frame_class->create_vertex_buffer = gsk_vulkan_frame_create_vertex_buffer;
  gpu_frame_class->create_globals_buffer = gsk_vulkan_frame_create_globals_buffer;
  gpu_frame_class->create_storage_buffer = gsk_vulkan_frame_create_storage_buffer;
  gpu_frame_class->write_texture_vertex_data = gsk_vulkan_frame_write_texture_vertex_data;
  gpu_frame_class->submit = gsk_vulkan_frame_submit;

  object_class->finalize = gsk_vulkan_frame_finalize;
}

static void
gsk_vulkan_frame_init (GskVulkanFrame *self)
{
  self->pool_n_sets = 4;
  self->pool_n_images = 8;
  self->pool_n_buffers = 8;
}

VkFence
gsk_vulkan_frame_get_vk_fence (GskVulkanFrame *self)
{
  return self->vk_fence;
}

void
gsk_vulkan_semaphores_add_wait (GskVulkanSemaphores  *self,
                                VkSemaphore           semaphore,
                                VkPipelineStageFlags  stage)
{
  gsk_semaphores_append (&self->wait_semaphores, semaphore);
  gsk_pipeline_stages_append (&self->wait_stages, stage);
}

void
gsk_vulkan_semaphores_add_signal (GskVulkanSemaphores  *self,
                                  VkSemaphore           semaphore)
{
  gsk_semaphores_append (&self->signal_semaphores, semaphore);
}

