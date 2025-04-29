#include "config.h"

#include "gskd3d12deviceprivate.h"
#include "gskd3d12imageprivate.h"
#include "gskgpuglobalsopprivate.h"
#include "gskgpushaderopprivate.h"

#include "gdk/win32/gdkd3d12contextprivate.h"
#include "gdk/win32/gdkd3d12texture.h"
#include "gdk/win32/gdkdisplay-win32.h"

#include <d3dcompiler.h>

typedef struct _DescriptorHeap DescriptorHeap;
typedef struct _PipelineCacheKey PipelineCacheKey;

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

  GHashTable *pipeline_cache;
  DescriptorHeaps descriptor_heaps;
};

struct _GskD3d12DeviceClass
{
  GskGpuDeviceClass parent_class;
};
struct _PipelineCacheKey
{
  const GskGpuShaderOpClass *op_class;
  GskGpuShaderFlags flags;
  GskGpuColorStates color_states;
  guint32 variation;
  GskGpuBlend blend;
  DXGI_FORMAT rtv_format;
};

static guint
pipeline_cache_key_hash (gconstpointer data)
{
  const PipelineCacheKey *key = data;

  return GPOINTER_TO_UINT (key->op_class) ^
         key->flags ^
         (key->color_states << 4) ^
         (key->variation << 12) ^
         (key->blend << 20) ^
         (key->rtv_format << 24);
}

static gboolean
pipeline_cache_key_equal (gconstpointer a,
                          gconstpointer b)
{
  const PipelineCacheKey *keya = a;
  const PipelineCacheKey *keyb = b;

  return keya->op_class == keyb->op_class &&
         keya->flags == keyb->flags &&
         keya->color_states == keyb->color_states &&
         keya->variation == keyb->variation &&
         keya->blend == keyb->blend &&
         keya->rtv_format == keyb->rtv_format;
}

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

  g_hash_table_unref (self->pipeline_cache);

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

  self->pipeline_cache = g_hash_table_new_full (pipeline_cache_key_hash,
                                                pipeline_cache_key_equal,
                                                g_free,
                                                gdk_win32_com_release);
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
  ID3DBlob *signature, *error_msg;
  HRESULT hr;

  hr = D3D12SerializeRootSignature ((&(D3D12_ROOT_SIGNATURE_DESC) {
                                    .NumParameters = GSK_D3D12_ROOT_N_PARAMETERS,
                                    .pParameters = (D3D12_ROOT_PARAMETER[GSK_D3D12_ROOT_N_PARAMETERS]) {
                                      [GSK_D3D12_ROOT_PUSH_CONSTANTS] = {
                                        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
                                        .Constants = {
                                            .ShaderRegister = 0,
                                            .RegisterSpace = 0,
                                            .Num32BitValues = sizeof (GskGpuGlobalsInstance) / 4,
                                        },
                                        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
                                      },
                                      [GSK_D3D12_ROOT_SAMPLER_0] = {
                                        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                                        .DescriptorTable = {
                                            .NumDescriptorRanges = 1,
                                            .pDescriptorRanges = &(D3D12_DESCRIPTOR_RANGE) {
                                                .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                .NumDescriptors = 3,
                                                .BaseShaderRegister = 0,
                                                .RegisterSpace = 0,
                                                .OffsetInDescriptorsFromTableStart = 0,
                                            }
                                        }
                                      },
                                      [GSK_D3D12_ROOT_SAMPLER_1] = {
                                        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                                        .DescriptorTable = {
                                            .NumDescriptorRanges = 1,
                                            .pDescriptorRanges = &(D3D12_DESCRIPTOR_RANGE) {
                                                .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                .NumDescriptors = 3,
                                                .BaseShaderRegister = 3,
                                                .RegisterSpace = 0,
                                                .OffsetInDescriptorsFromTableStart = 0,
                                            }
                                        }
                                      },
                                      [GSK_D3D12_ROOT_TEXTURE_0] = {
                                        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                                        .DescriptorTable = {
                                            .NumDescriptorRanges = 1,
                                            .pDescriptorRanges = &(D3D12_DESCRIPTOR_RANGE) {
                                                .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                                .NumDescriptors = 3,
                                                .BaseShaderRegister = 0,
                                                .RegisterSpace = 0,
                                                .OffsetInDescriptorsFromTableStart = 0,
                                            }
                                        }
                                      },
                                      [GSK_D3D12_ROOT_TEXTURE_1] = {
                                        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                                        .DescriptorTable = {
                                            .NumDescriptorRanges = 1,
                                            .pDescriptorRanges = &(D3D12_DESCRIPTOR_RANGE) {
                                                .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                                .NumDescriptors = 3,
                                                .BaseShaderRegister = 3,
                                                .RegisterSpace = 0,
                                                .OffsetInDescriptorsFromTableStart = 0,
                                            }
                                        }
                                      },
                                    },
                                    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
                                }),
                                D3D_ROOT_SIGNATURE_VERSION_1,
                                &signature,
                                &error_msg);
  if (FAILED (hr))
    {
      g_critical ("%*s", (int) ID3D10Blob_GetBufferSize (error_msg), (const char *) ID3D10Blob_GetBufferPointer (error_msg));
      return;
    }
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

typedef enum
{
  VERTEX_SHADER,
  FRAGMENT_SHADER,
  N_STAGES
} ShaderStage;

static const char *
get_target_for_shader_stage (ShaderStage stage)
{
  switch (stage)
  {
    case VERTEX_SHADER:
      return "vs_5_0";
    case FRAGMENT_SHADER:
      return "ps_5_0";
    case N_STAGES:
    default:
      g_assert_not_reached ();
      return "ps_5_0";
  }
}

static const char *
get_extension_for_shader_stage (ShaderStage stage)
{
  switch (stage)
  {
    case VERTEX_SHADER:
      return ".vert.hlsl";
    case FRAGMENT_SHADER:
      return ".frag.hlsl";
    case N_STAGES:
    default:
      g_assert_not_reached ();
      return ".frag.hlsl";
  }
}

static ID3D10Blob *
gsk_d3d12_device_compile_shader (const char         *shader_name,
                                 ShaderStage         stage,
                                 GskGpuShaderFlags   flags,
                                 GskGpuColorStates   color_states,
                                 guint32             variation,
                                 GError            **error)
{
  char *flags_str, *color_states_str, *variation_str;
  char *resource_path;
  GBytes *bytes;
  ID3D10Blob *shader, *error_msg = NULL;
  HRESULT hr;
  UINT extra_compile_flags[] = {
    0,
    D3DCOMPILE_SKIP_VALIDATION,
    D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_SKIP_OPTIMIZATION
  };
  gsize i;

  resource_path = g_strconcat ("/org/gtk/libgsk/shaders/d3d12/",
                               shader_name,
                               get_extension_for_shader_stage (stage),
                               NULL);

  bytes = g_resources_lookup_data (resource_path, 0, error);
  if (bytes == NULL)
    {
      g_free (resource_path);
      return NULL;
    }

  flags_str = g_strdup_printf ("%uu", flags);
  color_states_str = g_strdup_printf ("%uu", color_states);
  variation_str = g_strdup_printf ("%uu", variation);

  for (i = 0; i < G_N_ELEMENTS (extra_compile_flags); i++)
    {
      /* clear potential error from previous loop */
      gdk_win32_com_clear (&error_msg);

      hr = D3DCompile (g_bytes_get_data (bytes, NULL),
                      g_bytes_get_size (bytes),
                      resource_path,
                      ((D3D_SHADER_MACRO[]) {
                        {
                          .Name = "SPIRV_CROSS_CONSTANT_ID_0",
                          .Definition = flags_str,
                        },
                        {
                          .Name = "SPIRV_CROSS_CONSTANT_ID_1",
                          .Definition = color_states_str,
                        },
                        {
                          .Name = "SPIRV_CROSS_CONSTANT_ID_2",
                          .Definition = variation_str,
                        },
                        {
                          .Name = NULL,
                          .Definition = NULL,
                        }
                      }),
                      NULL,
                      "main",
                      get_target_for_shader_stage (stage),
                      extra_compile_flags[i],
                      0,
                      &shader,
                      &error_msg);

      if (SUCCEEDED (hr))
        break;
    }

  g_free (flags_str);
  g_free (color_states_str);
  g_free (variation_str);
  g_bytes_unref (bytes);
  g_free (resource_path);

  if (!gdk_win32_check_hresult (hr, error, "%s", ""))
    {
      if (error)
        {
          char *new_msg = g_strdup_printf ("%s\n%*s",
                                           (*error)->message,
                                           (int) ID3D10Blob_GetBufferSize (error_msg),
                                           (const char *) ID3D10Blob_GetBufferPointer (error_msg));
          g_free ((*error)->message);
          (*error)->message = new_msg;
        }
      gdk_win32_com_clear (&error_msg);
      return NULL;
    }

  return shader;
}

static const D3D12_BLEND_DESC blend_descs[4] = {
  [GSK_GPU_BLEND_NONE] = {
    .AlphaToCoverageEnable = false,
    .IndependentBlendEnable = false,
    .RenderTarget = {
      {
        .BlendEnable = false,
        .LogicOpEnable = false,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
      },
    }
  },
  [GSK_GPU_BLEND_OVER] = {
    .AlphaToCoverageEnable = false,
    .IndependentBlendEnable = false,
    .RenderTarget = {
      {
        .BlendEnable = true,
        .LogicOpEnable = false,
        .SrcBlend = D3D12_BLEND_ONE,
        .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ONE,
        .DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
      },
    }
  },
  [GSK_GPU_BLEND_ADD] = {
    .AlphaToCoverageEnable = false,
    .IndependentBlendEnable = false,
    .RenderTarget = {
      {
        .BlendEnable = true,
        .LogicOpEnable = false,
        .SrcBlend = D3D12_BLEND_ONE,
        .DestBlend = D3D12_BLEND_ONE,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ONE,
        .DestBlendAlpha = D3D12_BLEND_ONE,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
      },
    }
  },
  [GSK_GPU_BLEND_CLEAR] = {
    .AlphaToCoverageEnable = false,
    .IndependentBlendEnable = false,
    .RenderTarget = {
      {
        .BlendEnable = true,
        .LogicOpEnable = false,
        .SrcBlend = D3D12_BLEND_ZERO,
        .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ZERO,
        .DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
      },
    }
  },
};

ID3D12PipelineState *
gsk_d3d12_device_get_d3d12_pipeline_state (GskD3d12Device            *self,
                                           const GskGpuShaderOpClass *op_class,
                                           GskGpuShaderFlags          flags,
                                           GskGpuColorStates          color_states,
                                           guint32                    variation,
                                           GskGpuBlend                blend,
                                           DXGI_FORMAT                rtv_format)
{
  PipelineCacheKey cache_key;
  ID3D12PipelineState *result;
  ID3D10Blob *shaders[N_STAGES];
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
  const char *blend_name[] = { "NONE", "OVER", "ADD", "CLEAR" };
  GError *error = NULL;
  gsize i;

  cache_key = (PipelineCacheKey) {
    .op_class = op_class,
    .color_states = color_states,
    .variation = variation,
    .flags = flags,
    .blend = blend,
    .rtv_format = rtv_format,
  };
  result = g_hash_table_lookup (self->pipeline_cache, &cache_key);
  if (result)
    return result;

  for (i = 0; i < N_STAGES; i++)
    {
      shaders[i] = gsk_d3d12_device_compile_shader (op_class->shader_name,
                                                    i,
                                                    flags,
                                                    color_states,
                                                    variation,
                                                    &error);
      if (shaders[i] == NULL)
      {
        g_critical ("%s", error->message);
        g_clear_error (&error);
      }
    }

  desc = (D3D12_GRAPHICS_PIPELINE_STATE_DESC) {
    .pRootSignature = self->root_signature,
    .VS = (D3D12_SHADER_BYTECODE) {
        .pShaderBytecode = ID3D10Blob_GetBufferPointer (shaders[VERTEX_SHADER]),
        .BytecodeLength = ID3D10Blob_GetBufferSize (shaders[VERTEX_SHADER]),
    },
    .PS = (D3D12_SHADER_BYTECODE) {
      .pShaderBytecode = ID3D10Blob_GetBufferPointer (shaders[FRAGMENT_SHADER]),
      .BytecodeLength = ID3D10Blob_GetBufferSize (shaders[FRAGMENT_SHADER]),
    },
    .BlendState = blend_descs[blend],
    .SampleMask = UINT_MAX,
    .RasterizerState = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_NONE,
        .FrontCounterClockwise = FALSE,
        .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
        .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        .DepthClipEnable = TRUE,
        .MultisampleEnable = FALSE,
        .AntialiasedLineEnable = FALSE,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
    },
    .DepthStencilState = {
        .DepthEnable = FALSE,
        .StencilEnable = FALSE,
    },
    .InputLayout = *op_class->d3d12_input_layout,
    .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = { rtv_format, 0, 0, 0, 0, 0, 0, 0 },
    .DSVFormat = 0,
    .SampleDesc = {
        .Count = 1,
        .Quality = 0,
    },
    .NodeMask = 0,
    .CachedPSO = {
      .pCachedBlob = NULL,
      .CachedBlobSizeInBytes = 0
    },
    .Flags = 0,
  };

  hr_warn (ID3D12Device_CreateGraphicsPipelineState (self->device,
                                                     &desc,
                                                     &IID_ID3D12PipelineState,
                                                     (void **) &result));
  GSK_DEBUG (SHADERS,
             "Create D3D12 pipeline (%s, %u/%u/%u/%s/%u)",
             op_class->shader_name,
             flags,
             color_states,
             variation,
             blend_name[blend],
             rtv_format);

  for (i = 0; i < N_STAGES; i++)
    {
       gdk_win32_com_clear (&shaders[i]);
    }

  g_hash_table_insert (self->pipeline_cache,
                       g_memdup2 (&cache_key, sizeof (PipelineCacheKey)),
                       result);

  return result;
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
