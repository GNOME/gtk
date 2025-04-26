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

  ID3D12CommandAllocator *command_allocator;
  ID3D12GraphicsCommandList *command_list;

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

  hr_warn (ID3D12Device_CreateCommandAllocator (d3d12_device,
                                                D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                &IID_ID3D12CommandAllocator,
                                                (void **) &self->command_allocator));
  hr_warn (ID3D12Device_CreateCommandList (d3d12_device,
                                           0,
                                           D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           self->command_allocator,
                                           NULL,
                                           &IID_ID3D12GraphicsCommandList,
                                           (void **) &self->command_list));

  self->fence_wait = 0;
  hr_warn (ID3D12Device_CreateFence (d3d12_device,
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

  hr_warn (ID3D12CommandAllocator_Reset (self->command_allocator));
  hr_warn (ID3D12GraphicsCommandList_Reset (self->command_list,
                                            self->command_allocator,
                                            NULL));

  GSK_GPU_FRAME_CLASS (gsk_d3d12_frame_parent_class)->cleanup (frame);
}

static void
gsk_d3d12_frame_end (GskGpuFrame    *frame,
                     GdkDrawContext *context)
{
  GSK_GPU_FRAME_CLASS (gsk_d3d12_frame_parent_class)->end (frame, context);

  gsk_gpu_frame_sync (frame);
}

static void
gsk_d3d12_frame_sync (GskGpuFrame *frame)
{
  GskD3d12Frame *self = GSK_D3D12_FRAME (frame);
  ID3D12CommandQueue *queue;

  queue = gdk_d3d12_context_get_command_queue (GDK_D3D12_CONTEXT (gsk_gpu_frame_get_context (frame)));

  self->fence_wait++;
  hr_warn (ID3D12CommandQueue_Signal (queue, self->fence, self->fence_wait));
}

static GskGpuImage *
gsk_d3d12_frame_upload_texture (GskGpuFrame  *frame,
                                gboolean      with_mipmap,
                                GdkTexture   *texture)
{
  return GSK_GPU_FRAME_CLASS (gsk_d3d12_frame_parent_class)->upload_texture (frame, with_mipmap, texture);
}

static GskGpuBuffer *
gsk_d3d12_frame_create_vertex_buffer (GskGpuFrame *frame,
                                       gsize        size)
{
  return NULL;
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
  return NULL;
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
  GskD3d12CommandState state = {
    .command_list = self->command_list,
  };
  ID3D12CommandQueue *queue;

  queue = gdk_d3d12_context_get_command_queue (GDK_D3D12_CONTEXT (gsk_gpu_frame_get_context (frame)));

  while (op)
    {
      op = gsk_gpu_op_d3d12_command (op, frame, &state);
    }

  hr_warn (ID3D12GraphicsCommandList_Close (self->command_list));

  ID3D12CommandQueue_ExecuteCommandLists (queue, 1, (ID3D12CommandList **) &self->command_list);
}

static void
gsk_d3d12_frame_finalize (GObject *object)
{
  GskD3d12Frame *self = GSK_D3D12_FRAME (object);

  gdk_win32_com_clear (&self->fence);
  gdk_win32_com_clear (&self->command_list);
  gdk_win32_com_clear (&self->command_allocator);

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
  gpu_frame_class->end = gsk_d3d12_frame_end;
  gpu_frame_class->sync = gsk_d3d12_frame_sync;
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
