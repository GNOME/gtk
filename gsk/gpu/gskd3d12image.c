#include "config.h"

#include "gskd3d12imageprivate.h"

#include "gskd3d12frameprivate.h"
#include "gskgpuutilsprivate.h"

#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/win32/gdkprivate-win32.h"

struct _GskD3d12Image
{
  GskGpuImage parent_instance;

  GskD3d12Device *device;

  ID3D12Resource *resource;
  guint swizzle; /* aka shader4ComponentMapping */
  D3D12_RESOURCE_STATES state;

  D3D12_CPU_DESCRIPTOR_HANDLE rtv;
  D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu[3];
  D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu[3];
};

G_DEFINE_TYPE (GskD3d12Image, gsk_d3d12_image, GSK_TYPE_GPU_IMAGE)

static gboolean
swizzle_is_framebuffer_compatible (guint swizzle)
{
  if (D3D12_DECODE_SHADER_4_COMPONENT_MAPPING (0, swizzle) != D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0 ||
      D3D12_DECODE_SHADER_4_COMPONENT_MAPPING (1, swizzle) != D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1 ||
      D3D12_DECODE_SHADER_4_COMPONENT_MAPPING (2, swizzle) != D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2 ||
      (D3D12_DECODE_SHADER_4_COMPONENT_MAPPING (3, swizzle) != D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3 &&
       D3D12_DECODE_SHADER_4_COMPONENT_MAPPING (3, swizzle) != D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1))
    return FALSE;

  return TRUE;
}

static gboolean
gsk_d3d12_device_supports_format (GskD3d12Device   *device,
                                  DXGI_FORMAT       format,
                                  GskGpuImageFlags *out_flags)
{
  D3D12_FEATURE_DATA_FORMAT_SUPPORT support = { .Format = format, };
  static const D3D12_FORMAT_SUPPORT1 mandatory = D3D12_FORMAT_SUPPORT1_TEXTURE2D;

  if (format == DXGI_FORMAT_UNKNOWN)
    return FALSE;

  if (FAILED (ID3D12Device_CheckFeatureSupport (gsk_d3d12_device_get_d3d12_device (device),
                                                D3D12_FEATURE_FORMAT_SUPPORT,
                                                &support,
                                                sizeof (support))))
    return FALSE;

  if ((support.Support1 & mandatory) != mandatory )
    return FALSE;

  *out_flags = GSK_GPU_IMAGE_DOWNLOADABLE;
  if (support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)
    *out_flags |= GSK_GPU_IMAGE_FILTERABLE;
  if (support.Support1 & D3D12_FORMAT_SUPPORT1_MIP)
    *out_flags |= GSK_GPU_IMAGE_CAN_MIPMAP;
  if (support.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)
    *out_flags |= GSK_GPU_IMAGE_RENDERABLE;

  return TRUE;
}

static void
gsk_d3d12_device_find_format (GskD3d12Device   *device,
                              GdkMemoryFormat   format,
                              gboolean          try_srgb,
                              GskGpuImageFlags  required_flags,
                              GdkMemoryFormat  *out_format,
                              DXGI_FORMAT      *out_dxgi_format,
                              GskGpuImageFlags *out_flags,
                              guint            *out_swizzle)
{
  DXGI_FORMAT dxgi_format, dxgi_srgb_format = DXGI_FORMAT_UNKNOWN;

  /* First, try the actual format */
  dxgi_format = gdk_memory_format_get_dxgi_format (format, out_swizzle);
  if (try_srgb)
    dxgi_srgb_format = gdk_memory_format_get_dxgi_srgb_format (format);
  if (gsk_d3d12_device_supports_format (device, dxgi_srgb_format, out_flags) &&
      (*out_flags & required_flags) == required_flags)
    {
      *out_dxgi_format = dxgi_srgb_format;
      *out_format = format;
    }
  else if (gsk_d3d12_device_supports_format (device, dxgi_format, out_flags) &&
           (*out_flags & required_flags) == required_flags)
  {
      *out_dxgi_format = dxgi_format;
      *out_format = format;
    }
  else
    {
      GdkMemoryFormat rgba_format;
      GdkSwizzle rgba_swizzle;

      if (gdk_memory_format_get_rgba_format (format, &rgba_format, &rgba_swizzle))
        {
          dxgi_format = gdk_memory_format_get_dxgi_format (format, NULL);
          *out_swizzle = gdk_swizzle_to_d3d12 (rgba_swizzle); 
        }
      else
        dxgi_format = DXGI_FORMAT_UNKNOWN;
        
      if (dxgi_format == DXGI_FORMAT_UNKNOWN)
        dxgi_srgb_format = DXGI_FORMAT_UNKNOWN;
      else if (try_srgb)
        dxgi_srgb_format = gdk_memory_format_get_dxgi_srgb_format (rgba_format);
      if (gsk_d3d12_device_supports_format (device, dxgi_srgb_format, out_flags) &&
          (*out_flags & required_flags) == required_flags)
        {
          *out_dxgi_format = dxgi_srgb_format;
          *out_format = format;
        }
      else if (gsk_d3d12_device_supports_format (device, dxgi_format, out_flags) &&
               (*out_flags & required_flags) == required_flags)
        {
          *out_dxgi_format = dxgi_format;
          *out_format = format;
        }
      else
        {
          const GdkMemoryFormat *fallbacks;
          gsize i;

          /* Next, try the fallbacks */
          fallbacks = gdk_memory_format_get_fallbacks (format);
          for (i = 0; fallbacks[i] != -1; i++)
            {
              dxgi_format = gdk_memory_format_get_dxgi_format (fallbacks[i], out_swizzle);
              if (try_srgb)
                dxgi_srgb_format = gdk_memory_format_get_dxgi_srgb_format (fallbacks[i]);
              if (gsk_d3d12_device_supports_format (device, dxgi_srgb_format, out_flags) &&
                  (*out_flags & required_flags) == required_flags)
                {
                  *out_dxgi_format = dxgi_srgb_format;
                  *out_format = fallbacks[i];
                  format = fallbacks[i];
                  break;
                }
              else if (gsk_d3d12_device_supports_format (device, dxgi_format, out_flags) &&
                       (*out_flags & required_flags) == required_flags)
                {
                  *out_dxgi_format = dxgi_format;
                  *out_format = fallbacks[i];
                  break;
                }
            }

          /* No format found. This should never happen. */
          g_assert (fallbacks[i] != -1);
        }
    }

  if (!swizzle_is_framebuffer_compatible (*out_swizzle))
    *out_flags &= ~GSK_GPU_IMAGE_RENDERABLE;
}

GskGpuImage *
gsk_d3d12_image_new_for_resource (GskD3d12Device        *device,
                                  ID3D12Resource        *resource,
                                  D3D12_RESOURCE_STATES  initial_state,
                                  gboolean               premultiplied)
{
  GskD3d12Image *self;
  D3D12_RESOURCE_DESC desc;
  GdkMemoryFormat format;
  GskGpuImageFlags flags;

  ID3D12Resource_GetDesc (resource, &desc);

  if (!gdk_memory_format_find_by_dxgi_format (desc.Format, premultiplied, &format))
    return NULL;

  if (!gsk_d3d12_device_supports_format (device, desc.Format, &flags))
    return NULL;

  self = g_object_new (GSK_TYPE_D3D12_IMAGE, NULL);
  self->device = g_object_ref (device);
  self->state = initial_state;

  ID3D12Resource_AddRef (resource);
  self->resource = resource;
  self->swizzle = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       flags,
                       GSK_GPU_CONVERSION_NONE,
                       gdk_memory_format_get_default_shader_op (format),
                       format,
                       desc.Width,
                       desc.Height);

  return GSK_GPU_IMAGE (self);
}

GskGpuImage *
gsk_d3d12_image_new (GskD3d12Device       *device,
                     GdkMemoryFormat       format,
                     gboolean              with_mipmap,
                     GskGpuConversion      conv,
                     gsize                 width,
                     gsize                 height,
                     D3D12_RESOURCE_STATES initial_state,
                     D3D12_HEAP_FLAGS      heap_flags,
                     D3D12_RESOURCE_FLAGS  resource_flags)
{
  GskD3d12Image *self;
  ID3D12Resource *resource;
  HRESULT hr;
  DXGI_FORMAT dxgi_format;
  guint swizzle;
  GskGpuImageFlags flags;
  ID3D12Device *d3d12_device;

  if (width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
      height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    return NULL;

  gsk_d3d12_device_find_format (device,
                                format,
                                conv == GSK_GPU_CONVERSION_SRGB,
                                ((resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? GSK_GPU_IMAGE_RENDERABLE : 0) |
                                (with_mipmap ? GSK_GPU_IMAGE_CAN_MIPMAP : 0),
                                &format,
                                &dxgi_format,
                                &flags,
                                &swizzle);

  if (!with_mipmap)
    flags &= ~GSK_GPU_IMAGE_CAN_MIPMAP;

  dxgi_format = gdk_memory_format_get_dxgi_format (format, &swizzle);
  d3d12_device = gsk_d3d12_device_get_d3d12_device (device);

  hr = ID3D12Device_CreateCommittedResource (d3d12_device,
                                             (&(D3D12_HEAP_PROPERTIES) {
                                                 .Type = D3D12_HEAP_TYPE_DEFAULT,
                                                 .CreationNodeMask = 1,
                                                 .VisibleNodeMask = 1,
                                             }),
                                             heap_flags,
                                             (&(D3D12_RESOURCE_DESC) {
                                                 .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                                 .Width = width,
                                                 .Height = height,
                                                 .DepthOrArraySize = 1,
                                                 .MipLevels = (flags & GSK_GPU_IMAGE_CAN_MIPMAP) ? gsk_gpu_mipmap_levels (width, height) : 1,
                                                 .Format = dxgi_format,
                                                 .SampleDesc = {
                                                     .Count = 1,
                                                     .Quality = 0,
                                                 },
                                                 .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                                 .Flags = resource_flags,
                                             }),
                                             initial_state,
                                             NULL,
                                             &IID_ID3D12Resource,
                                             (void **) &resource);
  if (FAILED (hr))
    {
      GDK_DEBUG (D3D12, "Failed to create %zuz %zu %s image", width, height, gdk_memory_format_get_name (format));
      return NULL;
    }

  self = g_object_new (GSK_TYPE_D3D12_IMAGE, NULL);
  self->device = g_object_ref (device);
  self->state = initial_state;
  self->resource = resource;
  self->swizzle = swizzle;

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       flags,
                       GSK_GPU_CONVERSION_NONE,
                       gdk_memory_format_get_default_shader_op (format),
                       format,
                       width,
                       height);

  return GSK_GPU_IMAGE (self);
}

static void
gsk_d3d12_image_get_projection_matrix (GskGpuImage       *image,
                                       graphene_matrix_t *out_projection)
{
  graphene_matrix_t scale_z;

  GSK_GPU_IMAGE_CLASS (gsk_d3d12_image_parent_class)->get_projection_matrix (image, out_projection);

  graphene_matrix_init_from_float (&scale_z,
                                   (float[16]) {
                                       1,   0,   0,   0,
                                       0,   1,   0,   0,
                                       0,   0, 0.5,   0,
                                       0,   0, 0.5,   1
                                   });
  graphene_matrix_multiply (out_projection, &scale_z, out_projection);
  graphene_matrix_scale (out_projection, 1.f, -1.f, 1.f);
}

static void
gsk_d3d12_image_finalize (GObject *object)
{
  GskD3d12Image *self = GSK_D3D12_IMAGE (object);

  if (self->rtv.ptr)
    gsk_d3d12_device_free_rtv (self->device, &self->rtv);
  if (self->srv_cpu[0].ptr)
    gsk_d3d12_device_free_srv (self->device, self->srv_cpu);

  gdk_win32_com_clear (&self->resource);

  g_object_unref (self->device);

  G_OBJECT_CLASS (gsk_d3d12_image_parent_class)->finalize (object);
}

static void
gsk_d3d12_image_class_init (GskD3d12ImageClass *klass)
{
  GskGpuImageClass *image_class = GSK_GPU_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_projection_matrix = gsk_d3d12_image_get_projection_matrix;

  object_class->finalize = gsk_d3d12_image_finalize;
}

static void
gsk_d3d12_image_init (GskD3d12Image *self)
{
}

ID3D12Resource *
gsk_d3d12_image_get_resource (GskD3d12Image *self)
{
  return self->resource;
}

const D3D12_CPU_DESCRIPTOR_HANDLE *
gsk_d3d12_image_get_rtv (GskD3d12Image *self)
{
  if (self->rtv.ptr == 0)
    {
      gsk_d3d12_device_alloc_rtv (self->device, &self->rtv);
      ID3D12Device_CreateRenderTargetView (gsk_d3d12_device_get_d3d12_device (self->device),
                                           self->resource,
                                           NULL,
                                           self->rtv);
    }
  return &self->rtv;
}

const D3D12_GPU_DESCRIPTOR_HANDLE *
gsk_d3d12_image_get_srv (GskD3d12Image *self)
{
  if (self->srv_cpu[0].ptr == 0)
    {
      D3D12_RESOURCE_DESC desc;
      GdkMemoryFormat format;
      GdkShaderOp shader_op;
      gsize i;

      ID3D12Resource_GetDesc (self->resource, &desc);
      format = gsk_gpu_image_get_format (GSK_GPU_IMAGE (self));

      shader_op = gdk_memory_format_get_default_shader_op (format);
      for (i = 0; i < gdk_shader_op_get_n_shaders (shader_op); i++)
        {
          gsize unused;
          DXGI_FORMAT dxgi_format;
          guint swizzle, plane_slice;

          plane_slice = gdk_memory_format_get_shader_plane (format, i, &unused, &unused, &unused);
          
          dxgi_format = gdk_memory_format_get_dxgi_srv_format (format, i, &swizzle);
          if (shader_op == GDK_SHADER_DEFAULT || shader_op == GDK_SHADER_STRAIGHT)
            swizzle = self->swizzle;
          
          gsk_d3d12_device_alloc_srv (self->device, self->srv_cpu, self->srv_gpu);
          ID3D12Device_CreateShaderResourceView (gsk_d3d12_device_get_d3d12_device (self->device),
                                                 self->resource,
                                                 (&(D3D12_SHADER_RESOURCE_VIEW_DESC) {
                                                     .Format = dxgi_format,
                                                     .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                                                     .Shader4ComponentMapping = swizzle,
                                                     .Texture2D = {
                                                         .MostDetailedMip = 0,
                                                         .MipLevels = desc.MipLevels,
                                                         .PlaneSlice = plane_slice,
                                                         .ResourceMinLODClamp = 0.0f
                                                     }
                                                 }),
                                                 self->srv_cpu[i]);
      }
    }

  if (self->srv_cpu[0].ptr == 0)
    return NULL;

  return self->srv_gpu;
}

void
gsk_d3d12_image_transition (GskD3d12Image             *self,
                            ID3D12GraphicsCommandList *command_list,
                            D3D12_RESOURCE_STATES      state)
{
  if (self->state == state)
    return;

  ID3D12GraphicsCommandList_ResourceBarrier (command_list,
                                            1,
                                            ((D3D12_RESOURCE_BARRIER[1]) {
                                              {
                                                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                                                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                                                .Transition = {
                                                    .pResource = self->resource,
                                                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                                    .StateBefore = self->state,
                                                    .StateAfter = state,
                                                }
                                              }
                                            }));
  self->state = state;
}