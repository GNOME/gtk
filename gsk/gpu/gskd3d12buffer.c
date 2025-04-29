#include "config.h"

#include "gskd3d12bufferprivate.h"

#include "gskd3d12deviceprivate.h"

#include "gdk/win32/gdkprivate-win32.h"

struct _GskD3d12Buffer
{
  GskGpuBuffer parent_instance;

  ID3D12Resource *d3d12_resource;
};

G_DEFINE_TYPE (GskD3d12Buffer, gsk_d3d12_buffer, GSK_TYPE_GPU_BUFFER)

static void
gsk_d3d12_buffer_finalize (GObject *object)
{
  GskD3d12Buffer *self = GSK_D3D12_BUFFER (object);

  gdk_win32_com_clear (&self->d3d12_resource);

  G_OBJECT_CLASS (gsk_d3d12_buffer_parent_class)->finalize (object);
}

static guchar *
gsk_d3d12_buffer_map (GskGpuBuffer *buffer)
{
  GskD3d12Buffer *self = GSK_D3D12_BUFFER (buffer);
  guchar *data;

  hr_warn (ID3D12Resource_Map (self->d3d12_resource,
                               0,
                               (&(D3D12_RANGE) {
                                   0,
                                   gsk_gpu_buffer_get_size (buffer)
                               }),
                               (void **) &data));
  return data;
}

static void
gsk_d3d12_buffer_unmap (GskGpuBuffer *buffer,
                        gsize         size)
{
  GskD3d12Buffer *self = GSK_D3D12_BUFFER (buffer);

  ID3D12Resource_Unmap (self->d3d12_resource,
                        0,
                        (&(D3D12_RANGE) {
                            0,
                            size
                        }));
}

static void
gsk_d3d12_buffer_class_init (GskD3d12BufferClass *klass)
{
  GskGpuBufferClass *buffer_class = GSK_GPU_BUFFER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  buffer_class->map = gsk_d3d12_buffer_map;
  buffer_class->unmap = gsk_d3d12_buffer_unmap;

  gobject_class->finalize = gsk_d3d12_buffer_finalize;
}

static void
gsk_d3d12_buffer_init (GskD3d12Buffer *self)
{
}

GskGpuBuffer *
gsk_d3d12_buffer_new_from_resource (GskD3d12Device *device,
                                    ID3D12Resource *resource,
                                    gsize           size)
{
  GskD3d12Buffer *self;

  self = g_object_new (GSK_TYPE_D3D12_BUFFER, NULL);

  self->d3d12_resource = resource;

  gsk_gpu_buffer_setup (GSK_GPU_BUFFER (self), size);

  return GSK_GPU_BUFFER (self);
 }

static GskGpuBuffer *
gsk_d3d12_buffer_new_internal (GskD3d12Device   *device,
                               gsize             size)
{
  ID3D12Resource *resource;

  hr_warn (ID3D12Device_CreateCommittedResource (gsk_d3d12_device_get_d3d12_device (device),
                                                 (&(D3D12_HEAP_PROPERTIES) {
                                                     .Type = D3D12_HEAP_TYPE_UPLOAD,
                                                     .CreationNodeMask = 1,
                                                     .VisibleNodeMask = 1,
                                                 }),
                                                 D3D12_HEAP_FLAG_NONE,
                                                 (&(D3D12_RESOURCE_DESC) {
                                                     .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                                     .Width = size,
                                                     .Height = 1,
                                                     .DepthOrArraySize = 1,
                                                     .MipLevels = 1,
                                                     .SampleDesc = {
                                                         .Count = 1,
                                                         .Quality = 0,
                                                     },
                                                     .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                                 }),
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 NULL,
                                                 &IID_ID3D12Resource,
                                                 (void **) &resource));

  return gsk_d3d12_buffer_new_from_resource (device, resource, size);
}

GskGpuBuffer *
gsk_d3d12_buffer_new_vertex (GskD3d12Device *device,
                             gsize            size)
{
  return gsk_d3d12_buffer_new_internal (device, size);
}

#if 0
GskGpuBuffer *
gsk_d3d12_buffer_new_storage (GskD3d12Device *device,
                              gsize            size)
{
  return gsk_d3d12_buffer_new_internal (device, size);
}
#endif

ID3D12Resource *
gsk_d3d12_buffer_get_d3d12_resource (GskD3d12Buffer *self)
{
  return self->d3d12_resource;
}
