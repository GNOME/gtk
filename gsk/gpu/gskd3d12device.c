#include "config.h"

#include "gskd3d12deviceprivate.h"
#include "gskd3d12imageprivate.h"
#include "gskgpuglobalsopprivate.h"

#include "gdk/win32/gdkd3d12contextprivate.h"
#include "gdk/win32/gdkd3d12texture.h"
#include "gdk/win32/gdkdisplay-win32.h"

typedef struct _DescriptorHeap DescriptorHeap;
struct _DescriptorHeap
{
  ID3D12DescriptorHeap *heap;
  guint64 free_mask;
};

#define GDK_ARRAY_NAME descriptor_heaps
#define GDK_ARRAY_TYPE_NAME DescriptorHeaps
#define GDK_ARRAY_ELEMENT_TYPE DescriptorHeap
#define GDK_ARRAY_PREALLOC 8
#define GDK_ARRAY_NO_MEMSET 1
#define GDK_ARRAY_BY_VALUE 1
#include "gdk/gdkarrayimpl.c"
struct _GskD3d12Device
{
  GskGpuDevice parent_instance;

  ID3D12Device *device;

  ID3D12RootSignature *root_signature;

  DescriptorHeaps descriptor_heaps;
};

struct _GskD3d12DeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskD3d12Device, gsk_d3d12_device, GSK_TYPE_GPU_DEVICE)

static GskGpuImage *
gsk_d3d12_device_create_offscreen_image (GskGpuDevice   *device,
                                         gboolean        with_mipmap,
                                         GdkMemoryFormat format,
                                         gboolean        is_srgb,
                                         gsize           width,
                                         gsize           height)
{
  GskD3d12Device *self = GSK_D3D12_DEVICE (device);

  g_warn_if_fail (with_mipmap == FALSE);

  return gsk_d3d12_image_new (self,
                              format,
                              width,
                              height,
                              D3D12_RESOURCE_STATE_RENDER_TARGET,
                              0,
                              D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
}

static GskGpuImage *
gsk_d3d12_device_create_atlas_image (GskGpuDevice *device,
                                     gsize         width,
                                     gsize         height)
{
  g_return_val_if_reached (NULL);
}

static GskGpuImage *
gsk_d3d12_device_create_upload_image (GskGpuDevice     *device,
                                      gboolean          with_mipmap,
                                      GdkMemoryFormat   format,
                                      GskGpuConversion  conv,
                                      gsize             width,
                                      gsize             height)
{
  g_return_val_if_reached (NULL);
}

static GskGpuImage *
gsk_d3d12_device_create_download_image (GskGpuDevice   *device,
                                        GdkMemoryDepth  depth,
                                        gsize           width,
                                        gsize           height)
{
  GskD3d12Device *self = GSK_D3D12_DEVICE (device);

  return gsk_d3d12_image_new (self,
                              gdk_memory_depth_get_format (depth),
                              width,
                              height,
                              D3D12_RESOURCE_STATE_RENDER_TARGET,
                              D3D12_HEAP_FLAG_SHARED,
                              D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
}

static void
gsk_d3d12_device_make_current (GskGpuDevice *device)
{
}

static void
gsk_d3d12_device_finalize (GObject *object)
{
  GskD3d12Device *self = GSK_D3D12_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);
  GdkDisplay *display;
  gsize i;

  display = gsk_gpu_device_get_display (device);
  g_object_steal_data (G_OBJECT (display), "-gsk-d3d12-device");

  for (i = 0; i < descriptor_heaps_get_size (&self->descriptor_heaps); i++)
    {
      DescriptorHeap *heap = descriptor_heaps_get (&self->descriptor_heaps, i);
      ID3D12DescriptorHeap_Release (heap->heap);
    }
  descriptor_heaps_clear (&self->descriptor_heaps);

  gdk_win32_com_clear (&self->root_signature);
  gdk_win32_com_clear (&self->device);

  G_OBJECT_CLASS (gsk_d3d12_device_parent_class)->finalize (object);
}

static void
gsk_d3d12_device_class_init (GskD3d12DeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_d3d12_device_create_offscreen_image;
  gpu_device_class->create_atlas_image = gsk_d3d12_device_create_atlas_image;
  gpu_device_class->create_upload_image = gsk_d3d12_device_create_upload_image;
  gpu_device_class->create_download_image = gsk_d3d12_device_create_download_image;
  gpu_device_class->make_current = gsk_d3d12_device_make_current;

  object_class->finalize = gsk_d3d12_device_finalize;
}

static void
gsk_d3d12_device_init (GskD3d12Device *self)
{
  descriptor_heaps_init (&self->descriptor_heaps);
}

static void
gsk_d3d12_device_setup (GskD3d12Device *self,
                        GdkDisplay     *display)
{
  self->device = gdk_win32_display_get_d3d12_device (GDK_WIN32_DISPLAY (display));
  ID3D12Device_AddRef (self->device);

  gsk_gpu_device_setup (GSK_GPU_DEVICE (self),
                        display,
                        D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION,
                        GSK_GPU_DEVICE_DEFAULT_TILE_SIZE,
                        /* not used */ 1);
}

static void
gsk_d3d12_device_create_d3d12_objects (GskD3d12Device *self)
{
  ID3DBlob *signature;

  hr_warn (D3D12SerializeRootSignature ((&(D3D12_ROOT_SIGNATURE_DESC) {
                                            .NumParameters = 1,
                                            .pParameters = (D3D12_ROOT_PARAMETER[1]) {
                                              {
                                                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
                                                .Constants = {
                                                    .ShaderRegister = 0,
                                                    .RegisterSpace = 0,
                                                    .Num32BitValues = sizeof (GskGpuGlobalsInstance) / 4,
                                                },
                                                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
                                              },
                                            },
                                        }),
                                        D3D_ROOT_SIGNATURE_VERSION_1,
                                        &signature,
                                        NULL));
  hr_warn (ID3D12Device_CreateRootSignature (self->device,
                                             0,
                                             ID3D10Blob_GetBufferPointer (signature),
                                             ID3D10Blob_GetBufferSize (signature),
                                             &IID_ID3D12RootSignature,
                                             (void **) &self->root_signature));

  gdk_win32_com_clear (&signature);
}

GskGpuDevice *
gsk_d3d12_device_get_for_display (GdkDisplay  *display,
                                   GError    **error)
{
  GskD3d12Device *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-d3d12-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_win32_display_get_d3d12_device (GDK_WIN32_DISPLAY (display)))
    {
      if (!gdk_has_feature (GDK_FEATURE_D3D12))
        g_set_error (error, GDK_D3D12_ERROR, GDK_D3D12_ERROR_NOT_AVAILABLE, "D3D12 disabled via GDK_DISABLE");
      else
        g_set_error (error, GDK_D3D12_ERROR, GDK_D3D12_ERROR_NOT_AVAILABLE, "D3D12 is not available");

      return NULL;
    }
  self = g_object_new (GSK_TYPE_D3D12_DEVICE, NULL);

  gsk_d3d12_device_setup (self, display);

  gsk_d3d12_device_create_d3d12_objects (self);

  g_object_set_data (G_OBJECT (display), "-gsk-d3d12-device", self);

  return GSK_GPU_DEVICE (self);
}

ID3D12Device *
gsk_d3d12_device_get_d3d12_device (GskD3d12Device *self)
{
  return self->device;
}

ID3D12RootSignature *
gsk_d3d12_device_get_d3d12_root_signature (GskD3d12Device *self)
{
  return self->root_signature;
}

/* taken from glib source code, adapted to guint64 */

static inline int
my_bit_nth_lsf (guint64 mask,
                int     nth_bit)
{
  if (G_UNLIKELY (nth_bit < -1))
    nth_bit = -1;
  while (nth_bit < (((int) sizeof (guint64) * 8) - 1))
    {
      nth_bit++;
      if (mask & (((guint64) 1U) << nth_bit))
        return nth_bit;
    }
  return -1;
}

void
gsk_d3d12_device_alloc_rtv (GskD3d12Device              *self,
                            D3D12_CPU_DESCRIPTOR_HANDLE *out_descriptor)
{
  DescriptorHeap *heap = NULL; /* poor MSVC */
  gsize i;
  int pos;

  for (i = 0; i < descriptor_heaps_get_size (&self->descriptor_heaps); i++)
    {
      D3D12_DESCRIPTOR_HEAP_DESC desc;

      heap = descriptor_heaps_get (&self->descriptor_heaps, i);
      if (heap->free_mask == 0)
        continue;
      ID3D12DescriptorHeap_GetDesc (heap->heap, &desc);
      if (desc.Type != D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
        continue;

      break;
    }

  if (i == descriptor_heaps_get_size (&self->descriptor_heaps))
    {
      descriptor_heaps_set_size (&self->descriptor_heaps, i + 1);
      heap = descriptor_heaps_get (&self->descriptor_heaps, i);

      heap->free_mask = G_MAXUINT64;
      hr_warn (ID3D12Device_CreateDescriptorHeap (self->device,
                                                  (&(D3D12_DESCRIPTOR_HEAP_DESC) {
                                                      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                                      .NumDescriptors = sizeof (heap->free_mask) * 8,
                                                      .Flags = 0,
                                                      .NodeMask = 0,
                                                  }),
                                                  &IID_ID3D12DescriptorHeap,
                                                  (void **) &heap->heap));
    }

  pos = my_bit_nth_lsf (heap->free_mask, -1);
  g_assert (pos >= 0);
  heap->free_mask &= ~(((guint64) 1U) << pos);

  ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart (heap->heap, out_descriptor);
  out_descriptor->ptr += pos * ID3D12Device_GetDescriptorHandleIncrementSize (self->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void
gsk_d3d12_device_free_rtv (GskD3d12Device              *self,
                           D3D12_CPU_DESCRIPTOR_HANDLE *descriptor)
{
  DescriptorHeap *heap;
  gsize i, size;

  size = ID3D12Device_GetDescriptorHandleIncrementSize (self->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  for (i = 0; i < descriptor_heaps_get_size (&self->descriptor_heaps); i++)
    {
      D3D12_CPU_DESCRIPTOR_HANDLE start;
      SIZE_T offset;

      heap = descriptor_heaps_get (&self->descriptor_heaps, i);
      ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart (heap->heap, &start);

      offset = descriptor->ptr - start.ptr;
      offset /= size;
      if (offset < sizeof (guint64) * 8)
        {
          /* entry must not be marked free yet */
          g_assert (!(heap->free_mask & (((guint64) 1U) << offset)));
          heap->free_mask |= ((guint64) 1U) << offset;
          return;
        }
    }

  /* We didn't find the heap this handle belongs to. Something went seriously wrong. */
  g_assert_not_reached ();
}
