#include "config.h"

#include "gskd3d12frameprivate.h"

#include "gskgpudeviceprivate.h"
#include "gskgpuopprivate.h"
#include "gskgpuutilsprivate.h"
//#include "gskd3d12bufferprivate.h"
#include "gskd3d12deviceprivate.h"
#include "gskd3d12imageprivate.h"

#include "gdk/win32/gdkd3d12contextprivate.h"
#include "gdk/win32/gdkd3d12utilsprivate.h"
#include "gdk/win32/gdkprivate-win32.h"

struct _GskD3d12Frame
{
  GskGpuFrame parent_instance;

  ID3D12Fence *fence;
  guint64 fence_wait;
};

struct _GskD3d12FrameClass
{
  GskGpuFrameClass parent_class;
};

G_DEFINE_TYPE (GskD3d12Frame, gsk_d3d12_frame, GSK_TYPE_GPU_FRAME)

static gboolean
gsk_d3d12_frame_is_busy (GskGpuFrame *frame)
{
  GskD3d12Frame *self = GSK_D3D12_FRAME (frame);

  if (ID3D12Fence_GetCompletedValue (self->fence) < self->fence_wait)
    return TRUE;

  return FALSE;
}

static void
gsk_d3d12_frame_wait (GskGpuFrame *frame)
{
  GskD3d12Frame *self = GSK_D3D12_FRAME (frame);

  gdk_d3d12_fence_wait_sync (self->fence, self->fence_wait);
}

static void
gsk_d3d12_frame_setup (GskGpuFrame *frame)
{
  GskD3d12Frame *self = GSK_D3D12_FRAME (frame);
  ID3D12Device *d3d12_device;

  d3d12_device = gsk_d3d12_device_get_d3d12_device (GSK_D3D12_DEVICE (gsk_gpu_frame_get_device (frame)));

  self->fence_wait = 0;
  hr_return_if_fail (ID3D12Device_CreateFence (d3d12_device,
                                               self->fence_wait,
                                               D3D12_FENCE_FLAG_NONE,
                                               &IID_ID3D12Fence,
                                               (void **) &self->fence));
}

static void
gsk_d3d12_frame_cleanup (GskGpuFrame *frame)
{
  GskD3d12Frame *self = GSK_D3D12_FRAME (frame);

  if (ID3D12Fence_GetCompletedValue (self->fence) < self->fence_wait)
    gdk_d3d12_fence_wait_sync (self->fence, self->fence_wait);

  GSK_GPU_FRAME_CLASS (gsk_d3d12_frame_parent_class)->cleanup (frame);
}

static void
gsk_d3d12_frame_begin (GskGpuFrame           *frame,
                       GdkDrawContext        *context,
                       GdkMemoryDepth         depth,
                       const cairo_region_t  *region,
                       const graphene_rect_t *opaque)
{
  GSK_GPU_FRAME_CLASS (gsk_d3d12_frame_parent_class)->begin (frame, context, depth, region, opaque);
}

static GskGpuImage *
gsk_d3d12_frame_upload_texture (GskGpuFrame  *frame,
                                gboolean      with_mipmap,
                                GdkTexture   *texture)
{
#if 0
  if (GDK_IS_D3D12_TEXTURE (texture))
    {
      GdkD3D12Texture *d3d_texture = GDK_D3D12_TEXTURE (texture);
      GskGpuImage *image;

      image = gsk_d3d12_image_new_for_d3d12resource (GSK_D3D12_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                      gdk_d3d12_texture_get_resource (d3d_texture),
                                                      gdk_d3d12_texture_get_resource_handle (d3d_texture),
                                                      gdk_d3d12_texture_get_fence (d3d_texture),
                                                      gdk_d3d12_texture_get_fence_handle (d3d_texture),
                                                      gdk_d3d12_texture_get_fence_wait (d3d_texture),
                                                      gdk_memory_format_alpha (gdk_texture_get_format (texture)) != GDK_MEMORY_ALPHA_STRAIGHT);
      if (image)
        {
          gsk_gpu_image_toggle_ref_texture (image, texture);
          return image;
        }
    }
#endif

  return GSK_GPU_FRAME_CLASS (gsk_d3d12_frame_parent_class)->upload_texture (frame, with_mipmap, texture);
}

static GskGpuBuffer *
gsk_d3d12_frame_create_vertex_buffer (GskGpuFrame *frame,
                                       gsize        size)
{
  return NULL; //gsk_d3d12_buffer_new_vertex (GSK_D3D12_DEVICE (gsk_gpu_frame_get_device (frame)), size);
}

static GskGpuBuffer *
gsk_d3d12_frame_create_globals_buffer (GskGpuFrame *frame,
                                       gsize        size)
{
  return NULL;
}

static GskGpuBuffer *
gsk_d3d12_frame_create_storage_buffer (GskGpuFrame *frame,
                                        gsize        size)
{
  return NULL; //gsk_d3d12_buffer_new_storage (GSK_D3D12_DEVICE (gsk_gpu_frame_get_device (frame)), size);
}

static void
gsk_d3d12_frame_write_texture_vertex_data (GskGpuFrame    *self,
                                           guchar         *data,
                                           GskGpuImage   **images,
                                           GskGpuSampler  *samplers,
                                           gsize           n_images)
{
}

static void
gsk_d3d12_frame_submit (GskGpuFrame       *frame,
                        GskRenderPassType  pass_type,
                        GskGpuBuffer      *vertex_buffer,
                        GskGpuBuffer      *globals_buffer,
                        GskGpuOp          *op)
{
  GskD3d12Frame *self = GSK_D3D12_FRAME (frame);
  GskD3d12CommandState state = { 0, };
  ID3D12CommandQueue *queue;

  queue = gdk_d3d12_context_get_command_queue (GDK_D3D12_CONTEXT (gsk_gpu_frame_get_context (frame)));

#if 0
  GskD3d12Frame *self = GSK_D3D12_FRAME (frame);
  GskD3d12Device *device;
  GskD3d12Semaphores semaphores;

  device = GSK_D3D12_DEVICE (gsk_gpu_frame_get_device (frame));

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
                                gsk_d3d12_buffer_get_vk_buffer (GSK_D3D12_BUFFER (vertex_buffer))
                            },
                            (VkDeviceSize[1]) { 0 });

  gsk_semaphores_init (&semaphores.wait_semaphores);
  gsk_semaphore_values_init (&semaphores.wait_semaphore_values);
  gsk_pipeline_stages_init (&semaphores.wait_stages);
  gsk_semaphores_init (&semaphores.signal_semaphores);

  if (pass_type == GSK_RENDER_PASS_PRESENT)
    {
      gsk_d3d12_semaphores_add_wait (&semaphores,
                                      self->vk_acquire_semaphore,
                                      0,
                                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }

  state.vk_command_buffer = self->vk_command_buffer;
  state.vk_render_pass = VK_NULL_HANDLE;
  state.vk_format = VK_FORMAT_UNDEFINED;
  state.blend = GSK_GPU_BLEND_OVER; /* should we have a BLEND_NONE? */
  state.semaphores = &semaphores;
#endif

  while (op)
    {
      op = gsk_gpu_op_d3d12_command (op, frame, &state);
    }

#if 0
  GSK_VK_CHECK (vkEndCommandBuffer, self->vk_command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gsk_d3d12_device_get_vk_queue (device),
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
                                  .pNext = gsk_d3d12_device_has_feature (device, GDK_D3D12_FEATURE_TIMELINE_SEMAPHORE) ? &(VkTimelineSemaphoreSubmitInfo) {
                                       .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
                                       .waitSemaphoreValueCount = gsk_semaphore_values_get_size (&semaphores.wait_semaphore_values),
                                       .pWaitSemaphoreValues = gsk_semaphore_values_get_data (&semaphores.wait_semaphore_values),
                                  } : NULL,
                               },
                               self->vk_fence);

  gsk_semaphores_clear (&semaphores.wait_semaphores);
  gsk_semaphore_values_clear (&semaphores.wait_semaphore_values);
  gsk_pipeline_stages_clear (&semaphores.wait_stages);
  gsk_semaphores_clear (&semaphores.signal_semaphores);
#endif

  self->fence_wait++;
  hr_return_if_fail (ID3D12CommandQueue_Signal (queue, self->fence, self->fence_wait));
}

static void
gsk_d3d12_frame_finalize (GObject *object)
{
  GskD3d12Frame *self = GSK_D3D12_FRAME (object);

  gdk_win32_com_clear (&self->fence);

  G_OBJECT_CLASS (gsk_d3d12_frame_parent_class)->finalize (object);
}

static void
gsk_d3d12_frame_class_init (GskD3d12FrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_frame_class->is_busy = gsk_d3d12_frame_is_busy;
  gpu_frame_class->wait = gsk_d3d12_frame_wait;
  gpu_frame_class->setup = gsk_d3d12_frame_setup;
  gpu_frame_class->cleanup = gsk_d3d12_frame_cleanup;
  gpu_frame_class->begin = gsk_d3d12_frame_begin;
  gpu_frame_class->upload_texture = gsk_d3d12_frame_upload_texture;
  gpu_frame_class->create_vertex_buffer = gsk_d3d12_frame_create_vertex_buffer;
  gpu_frame_class->create_globals_buffer = gsk_d3d12_frame_create_globals_buffer;
  gpu_frame_class->create_storage_buffer = gsk_d3d12_frame_create_storage_buffer;
  gpu_frame_class->write_texture_vertex_data = gsk_d3d12_frame_write_texture_vertex_data;
  gpu_frame_class->submit = gsk_d3d12_frame_submit;

  object_class->finalize = gsk_d3d12_frame_finalize;
}

static void
gsk_d3d12_frame_init (GskD3d12Frame *self)
{
}
