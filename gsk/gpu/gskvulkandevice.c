#include "config.h"

#include "gskvulkandeviceprivate.h"

#include "gskgpuglobalsopprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanycbcrprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkvulkancontextprivate.h"
#include "gdk/gdkprofilerprivate.h"

#define GDK_ARRAY_NAME descriptor_pools
#define GDK_ARRAY_TYPE_NAME DescriptorPools
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorPool
#define GDK_ARRAY_PREALLOC 4
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

struct _GskVulkanDevice
{
  GskGpuDevice parent_instance;

  GskVulkanAllocator *allocators[VK_MAX_MEMORY_TYPES];
  GskVulkanAllocator *external_allocator;
  GdkVulkanFeatures features;

  GHashTable *ycbcr_cache;
  GHashTable *render_pass_cache;
  GHashTable *pipeline_cache;

  VkCommandPool vk_command_pool;
  DescriptorPools descriptor_pools;
  gsize last_pool;
  VkSampler vk_samplers[GSK_GPU_SAMPLER_N_SAMPLERS];
  VkDescriptorSetLayout vk_image_set_layout;
  VkPipelineLayout default_vk_pipeline_layout;
};

struct _GskVulkanDeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskVulkanDevice, gsk_vulkan_device, GSK_TYPE_GPU_DEVICE)

typedef struct _ConversionCacheEntry ConversionCacheEntry;
typedef struct _PipelineCacheKey PipelineCacheKey;
typedef struct _RenderPassCacheKey RenderPassCacheKey;

struct _ConversionCacheEntry
{
  VkFormat vk_format;

  /* actual data */
  VkSamplerYcbcrConversion vk_conversion;
  VkSampler vk_sampler;
};

struct _PipelineCacheKey
{
  const GskGpuShaderOpClass *op_class;
  GskGpuShaderFlags flags;
  GskGpuColorStates color_states;
  guint32 variation;
  GskGpuBlend blend;
  VkFormat vk_format;
  VkPipelineLayout vk_layout;
  VkPipeline vk_pipeline;
};

struct _RenderPassCacheKey
{
  VkFormat format;
  VkAttachmentLoadOp vk_load_op;
  VkImageLayout from_layout;
  VkImageLayout to_layout;
  VkRenderPass render_pass;
};

static guint
pipeline_cache_key_hash (gconstpointer data)
{
  const PipelineCacheKey *key = data;

  return GPOINTER_TO_UINT (key->op_class) ^
         key->flags ^
         (key->color_states << 8) ^
         (key->variation << 16) ^
         (key->blend << 24) ^
         GPOINTER_TO_UINT (key->vk_layout) ^
         (key->vk_format << 21) ^ (key->vk_format >> 11);
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
         keya->vk_layout == keyb->vk_layout &&
         keya->vk_format == keyb->vk_format;
}

static guint
render_pass_cache_key_hash (gconstpointer data)
{
  const RenderPassCacheKey *key = data;

  return (key->from_layout << 26) ^
         (key->to_layout << 18) ^
         (key->vk_load_op << 16) ^
         (key->format);
}

static gboolean
render_pass_cache_key_equal (gconstpointer a,
                             gconstpointer b)
{
  const RenderPassCacheKey *keya = a;
  const RenderPassCacheKey *keyb = b;

  return keya->from_layout == keyb->from_layout &&
         keya->to_layout == keyb->to_layout &&
         keya->vk_load_op == keyb->vk_load_op &&
         keya->format == keyb->format;
}

static VkDescriptorSetLayout
gsk_vulkan_device_create_vk_image_set_layout (GskVulkanDevice *self)
{
  VkDevice vk_device;
  VkDescriptorSetLayout result;

  vk_device = gsk_vulkan_device_get_vk_device (self);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, vk_device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = 0,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                     }
                                                 },
                                             },
                                             NULL,
                                             &result);

  return result;
}

VkPipelineLayout
gsk_vulkan_device_create_vk_pipeline_layout (GskVulkanDevice       *self,
                                             VkDescriptorSetLayout  image1_layout,
                                             VkDescriptorSetLayout  image2_layout)
{
  VkDevice vk_device;
  VkPipelineLayout result;

  vk_device = gsk_vulkan_device_get_vk_device (self);

  GSK_VK_CHECK (vkCreatePipelineLayout, vk_device,
                                        &(VkPipelineLayoutCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                            .setLayoutCount = 2,
                                            .pSetLayouts = (VkDescriptorSetLayout[2]) {
                                                image1_layout,
                                                image2_layout,
                                            },
                                            .pushConstantRangeCount = 1,
                                            .pPushConstantRanges = (VkPushConstantRange[1]) {
                                                {
                                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                    .offset = 0,
                                                    .size = sizeof (GskGpuGlobalsInstance)
                                                }
                                            }
                                        },
                                        NULL,
                                        &result);

  return result;
}

static GskGpuImage *
gsk_vulkan_device_create_offscreen_image (GskGpuDevice   *device,
                                          gboolean        with_mipmap,
                                          GdkMemoryDepth  depth,
                                          gsize           width,
                                          gsize           height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_offscreen (self,
                                             with_mipmap,
                                             gdk_memory_depth_get_format (depth),
                                             gdk_memory_depth_is_srgb (depth),
                                             width,
                                             height);
}

static GskGpuImage *
gsk_vulkan_device_create_atlas_image (GskGpuDevice *device,
                                      gsize         width,
                                      gsize         height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_atlas (self,
                                         width,
                                         height);
}

static GskGpuImage *
gsk_vulkan_device_create_upload_image (GskGpuDevice    *device,
                                       gboolean         with_mipmap,
                                       GdkMemoryFormat  format,
                                       gboolean         try_srgb,
                                       gsize            width,
                                       gsize            height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_upload (self,
                                          with_mipmap,
                                          format,
                                          try_srgb,
                                          width,
                                          height);
}

static GskGpuImage *
gsk_vulkan_device_create_download_image (GskGpuDevice   *device,
                                         GdkMemoryDepth  depth,
                                         gsize           width,
                                         gsize           height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);
  GskGpuImage *image;

#ifdef HAVE_DMABUF
  image = gsk_vulkan_image_new_dmabuf (self,
                                       gdk_memory_depth_get_format (depth),
                                       gdk_memory_depth_is_srgb (depth),
                                       width,
                                       height);
  if (image != NULL)
    return image;
#endif

  image = gsk_vulkan_image_new_for_offscreen (self,
                                              FALSE,
                                              gdk_memory_depth_get_format (depth),
                                              gdk_memory_depth_is_srgb (depth),
                                              width,
                                              height);

  return image;
}

static void
gsk_vulkan_device_make_current (GskGpuDevice *device)
{
}

static void
gsk_vulkan_device_finalize (GObject *object)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);
  VkDevice vk_device;
  GdkDisplay *display;
  GHashTableIter iter;
  gpointer key, value;
  gsize i;

  display = gsk_gpu_device_get_display (device);
  vk_device = gsk_vulkan_device_get_vk_device (self);

  g_object_steal_data (G_OBJECT (display), "-gsk-vulkan-device");

  g_assert (g_hash_table_size (self->ycbcr_cache) == 0);
  g_hash_table_unref (self->ycbcr_cache);

  g_hash_table_iter_init (&iter, self->pipeline_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      vkDestroyPipeline (vk_device,
                         ((PipelineCacheKey *)key)->vk_pipeline,
                         NULL);
      g_free (key);
    }
  g_hash_table_unref (self->pipeline_cache);

  g_hash_table_iter_init (&iter, self->render_pass_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      vkDestroyRenderPass (vk_device,
                           ((RenderPassCacheKey *)key)->render_pass,
                           NULL);
      g_free (key);
    }
  g_hash_table_unref (self->render_pass_cache);

  for (i = 0; i < G_N_ELEMENTS (self->vk_samplers); i++)
    {
      if (self->vk_samplers[i] != VK_NULL_HANDLE)
        vkDestroySampler (vk_device,
                          self->vk_samplers[i],
                          NULL);
    }

  vkDestroyPipelineLayout (vk_device,
                           self->default_vk_pipeline_layout,
                           NULL);
  vkDestroyDescriptorSetLayout (vk_device,
                                self->vk_image_set_layout,
                                NULL);
  for (i = 0; i < descriptor_pools_get_size (&self->descriptor_pools); i++)
    vkDestroyDescriptorPool (vk_device,
                             descriptor_pools_get (&self->descriptor_pools, i),
                             NULL);
  descriptor_pools_clear (&self->descriptor_pools);
  vkDestroyCommandPool (vk_device,
                        self->vk_command_pool,
                        NULL);

  for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    g_clear_pointer (&self->allocators[i], gsk_vulkan_allocator_unref);
  g_clear_pointer (&self->external_allocator, gsk_vulkan_allocator_unref);

  gdk_display_unref_vulkan (display);

  G_OBJECT_CLASS (gsk_vulkan_device_parent_class)->finalize (object);
}

static void
gsk_vulkan_device_class_init (GskVulkanDeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_vulkan_device_create_offscreen_image;
  gpu_device_class->create_atlas_image = gsk_vulkan_device_create_atlas_image;
  gpu_device_class->create_upload_image = gsk_vulkan_device_create_upload_image;
  gpu_device_class->create_download_image = gsk_vulkan_device_create_download_image;
  gpu_device_class->make_current = gsk_vulkan_device_make_current;

  object_class->finalize = gsk_vulkan_device_finalize;
}

static void
gsk_vulkan_device_init (GskVulkanDevice *self)
{
  self->ycbcr_cache = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->render_pass_cache = g_hash_table_new (render_pass_cache_key_hash, render_pass_cache_key_equal);
  self->pipeline_cache = g_hash_table_new (pipeline_cache_key_hash, pipeline_cache_key_equal);

  descriptor_pools_init (&self->descriptor_pools);
}

static void
gsk_vulkan_device_create_vk_objects (GskVulkanDevice *self)
{
  VkDevice vk_device;

  vk_device = gsk_vulkan_device_get_vk_device (self);

  GSK_VK_CHECK (vkCreateCommandPool, vk_device,
                                     &(const VkCommandPoolCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .queueFamilyIndex = gsk_vulkan_device_get_vk_queue_family_index (self),
                                         .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                     },
                                     NULL,
                                     &self->vk_command_pool);

  self->vk_image_set_layout = gsk_vulkan_device_create_vk_image_set_layout (self);

  self->default_vk_pipeline_layout = gsk_vulkan_device_create_vk_pipeline_layout (self,
                                                                                  self->vk_image_set_layout,
                                                                                  self->vk_image_set_layout);
}

static void
gsk_vulkan_device_setup (GskVulkanDevice *self,
                         GdkDisplay      *display)
{
  VkPhysicalDeviceProperties2 vk_props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    .pNext = NULL
  };

  vkGetPhysicalDeviceProperties2 (display->vk_physical_device, &vk_props);
  gsk_gpu_device_setup (GSK_GPU_DEVICE (self),
                        display,
                        vk_props.properties.limits.maxImageDimension2D,
                        GSK_GPU_DEVICE_DEFAULT_TILE_SIZE);
}

GskGpuDevice *
gsk_vulkan_device_get_for_display (GdkDisplay  *display,
                                   GError     **error)
{
  GskVulkanDevice *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-vulkan-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_display_init_vulkan (display, error))
    return NULL;

  self = g_object_new (GSK_TYPE_VULKAN_DEVICE, NULL);
  self->features = display->vulkan_features;

  gsk_vulkan_device_setup (self, display);

  gsk_vulkan_device_create_vk_objects (self);

  g_object_set_data (G_OBJECT (display), "-gsk-vulkan-device", self);

  return GSK_GPU_DEVICE (self);
}

gboolean
gsk_vulkan_device_has_feature (GskVulkanDevice   *self,
                               GdkVulkanFeatures  feature)
{
  return (self->features & feature) == feature;
}

VkDevice
gsk_vulkan_device_get_vk_device (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_device;
}

VkPhysicalDevice
gsk_vulkan_device_get_vk_physical_device (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_physical_device;
}

VkQueue
gsk_vulkan_device_get_vk_queue (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_queue;
}

uint32_t
gsk_vulkan_device_get_vk_queue_family_index (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_queue_family_index;
}

VkDescriptorSetLayout
gsk_vulkan_device_get_vk_image_set_layout (GskVulkanDevice *self)
{
  return self->vk_image_set_layout;
}

VkPipelineLayout
gsk_vulkan_device_get_default_vk_pipeline_layout (GskVulkanDevice *self)
{
  return self->default_vk_pipeline_layout;
}

VkPipelineLayout
gsk_vulkan_device_get_vk_pipeline_layout (GskVulkanDevice *self,
                                          GskVulkanYcbcr  *ycbcr0,
                                          GskVulkanYcbcr  *ycbcr1)
{
  if (ycbcr0 == VK_NULL_HANDLE)
    {
      if (ycbcr1 == VK_NULL_HANDLE)
        return self->default_vk_pipeline_layout;
      else
        return gsk_vulkan_ycbcr_get_vk_pipeline_layout (ycbcr1, 1);
    }
  else
    {
      if (ycbcr1 == VK_NULL_HANDLE)
        return gsk_vulkan_ycbcr_get_vk_pipeline_layout (ycbcr0, 0);
      else
        {
          /* FIXME: someone write a test plz */
          g_assert_not_reached ();
        }
    }
}

VkCommandPool
gsk_vulkan_device_get_vk_command_pool (GskVulkanDevice *self)
{
  return self->vk_command_pool;
}

VkDescriptorSet
gsk_vulkan_device_allocate_descriptor (GskVulkanDevice             *self,
                                       const VkDescriptorSetLayout  layout,
                                       gsize                       *out_pool_id)
{
  VkDescriptorSet result;
  VkDevice vk_device;
  VkDescriptorPool new_pool;
  gsize i, n;

  vk_device = gsk_vulkan_device_get_vk_device (self);
  n = descriptor_pools_get_size (&self->descriptor_pools);

  for (i = 0; i < n; i++)
    {
      gsize pool_id = (i + self->last_pool) % n;
      VkResult res = vkAllocateDescriptorSets (vk_device,
                                               &(VkDescriptorSetAllocateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                   .descriptorPool = descriptor_pools_get (&self->descriptor_pools, pool_id),
                                                   .descriptorSetCount = 1,
                                                   .pSetLayouts = (VkDescriptorSetLayout[1]) {
                                                       layout,
                                                   },
                                              },
                                              &result);
      if (res == VK_SUCCESS)
        {
          self->last_pool = pool_id;
          *out_pool_id = self->last_pool;
          return result;
        }
      else if (res == VK_ERROR_OUT_OF_POOL_MEMORY ||
               res == VK_ERROR_FRAGMENTED_POOL)
        {
          continue;
        }
      else
        {
          gsk_vulkan_handle_result (res, "vkAllocateDescriptorSets");
        }
    }

  GSK_VK_CHECK (vkCreateDescriptorPool, vk_device,
                                        &(VkDescriptorPoolCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                            .maxSets = 100,
                                            .poolSizeCount = 1,
                                            .pPoolSizes = (VkDescriptorPoolSize[1]) {
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                    .descriptorCount = 100,
                                                },
                                            }
                                        },
                                        NULL,
                                        &new_pool);
  descriptor_pools_append (&self->descriptor_pools, new_pool);
  GSK_VK_CHECK (vkAllocateDescriptorSets, vk_device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = new_pool,
                                              .descriptorSetCount = 1,
                                              .pSetLayouts = (VkDescriptorSetLayout[1]) {
                                                  layout,
                                              },
                                          },
                                          &result);

  self->last_pool = descriptor_pools_get_size (&self->descriptor_pools) - 1;
  *out_pool_id = self->last_pool;

  return result;
}

void
gsk_vulkan_device_free_descriptor (GskVulkanDevice *self,
                                   gsize            pool_id,
                                   VkDescriptorSet  set)
{
  vkFreeDescriptorSets (gsk_vulkan_device_get_vk_device (self),
                        descriptor_pools_get (&self->descriptor_pools, pool_id),
                        1,
                        &set);
}

static VkSampler
gsk_vulkan_device_create_sampler (GskVulkanDevice          *self,
                                  VkSamplerYcbcrConversion  vk_conversion,
                                  VkFilter                  vk_filter,
                                  VkSamplerAddressMode      vk_address_mode,
                                  VkSamplerMipmapMode       vk_mipmap_mode,
                                  float                     max_lod)
{
  VkSampler result;

  GSK_VK_CHECK (vkCreateSampler, gsk_vulkan_device_get_vk_device (self),
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = vk_filter,
                                     .minFilter = vk_filter,
                                     .mipmapMode = vk_mipmap_mode,
                                     .addressModeU = vk_address_mode,
                                     .addressModeV = vk_address_mode,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                     .minLod = 0.0,
                                     .maxLod = max_lod,
                                     .pNext = vk_conversion == VK_NULL_HANDLE ? NULL : &(VkSamplerYcbcrConversionInfo) {
                                         .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
                                         .conversion = vk_conversion
                                     }
                                 },
                                 NULL,
                                 &result);

  return result;
}

VkSampler
gsk_vulkan_device_get_vk_sampler (GskVulkanDevice     *self,
                                  GskGpuSampler        sampler)
{
  const struct {
    VkFilter filter;
    VkSamplerAddressMode address_mode;
    VkSamplerMipmapMode mipmap_mode;
    float max_lod;
  } filter_attrs[GSK_GPU_SAMPLER_N_SAMPLERS] = {
      [GSK_GPU_SAMPLER_DEFAULT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .max_lod = 0.0f,
      },
      [GSK_GPU_SAMPLER_TRANSPARENT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .max_lod = 0.0f,
      },
      [GSK_GPU_SAMPLER_REPEAT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .max_lod = 0.0f,
      },
      [GSK_GPU_SAMPLER_NEAREST] = {
          .filter = VK_FILTER_NEAREST,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .max_lod = 0.0f,
      },
      [GSK_GPU_SAMPLER_MIPMAP_DEFAULT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
          .max_lod = VK_LOD_CLAMP_NONE,
      },
  };

  if (self->vk_samplers[sampler] == VK_NULL_HANDLE)
    {
      self->vk_samplers[sampler] = gsk_vulkan_device_create_sampler (self,
                                                                     VK_NULL_HANDLE,
                                                                     filter_attrs[sampler].filter,
                                                                     filter_attrs[sampler].address_mode,
                                                                     filter_attrs[sampler].mipmap_mode,
                                                                     filter_attrs[sampler].max_lod);
    }

  return self->vk_samplers[sampler];
}

GskVulkanYcbcr *
gsk_vulkan_device_get_ycbcr (GskVulkanDevice *self,
                             VkFormat         vk_format)
{
  GskVulkanYcbcr *ycbcr;

  ycbcr = g_hash_table_lookup (self->ycbcr_cache, GSIZE_TO_POINTER(vk_format));
  if (ycbcr)
    return ycbcr;

  ycbcr = gsk_vulkan_ycbcr_new (self, vk_format);
  g_hash_table_insert (self->ycbcr_cache, GSIZE_TO_POINTER(vk_format), ycbcr);
  return ycbcr;
}

void
gsk_vulkan_device_remove_ycbcr (GskVulkanDevice *self,
                                VkFormat         vk_format)
{
  g_hash_table_remove (self->ycbcr_cache, GSIZE_TO_POINTER(vk_format));
}

VkRenderPass
gsk_vulkan_device_get_vk_render_pass (GskVulkanDevice    *self,
                                      VkFormat            format,
                                      VkAttachmentLoadOp  vk_load_op,
                                      VkImageLayout       from_layout,
                                      VkImageLayout       to_layout)
{
  RenderPassCacheKey cache_key;
  RenderPassCacheKey *cached_result;
  VkRenderPass render_pass;
  GdkDisplay *display;

  cache_key = (RenderPassCacheKey) {
    .format = format,
    .vk_load_op = vk_load_op,
    .from_layout = from_layout,
    .to_layout = to_layout,
  };
  cached_result = g_hash_table_lookup (self->render_pass_cache, &cache_key);
  if (cached_result)
    return cached_result->render_pass;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateRenderPass, display->vk_device,
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = format,
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = vk_load_op,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              .initialLayout = from_layout,
                                              .finalLayout = to_layout
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
                                            }
                                         },
                                         .dependencyCount = 0,
                                      },
                                      NULL,
                                      &render_pass);

  cached_result = g_memdup (&cache_key, sizeof (RenderPassCacheKey));
  cached_result->render_pass = render_pass;

  g_hash_table_insert (self->render_pass_cache, cached_result, cached_result);

  return render_pass;
}

typedef struct _GskVulkanShaderSpecialization GskVulkanShaderSpecialization;
struct _GskVulkanShaderSpecialization
{
  guint32 flags;
  guint32 color_states;
  guint32 variation;
};

static VkPipelineColorBlendAttachmentState blend_attachment_states[4] = {
  [GSK_GPU_BLEND_NONE] = {
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_OVER] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_ADD] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_CLEAR] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
};

VkPipeline
gsk_vulkan_device_get_vk_pipeline (GskVulkanDevice           *self,
                                   VkPipelineLayout           vk_layout,
                                   const GskGpuShaderOpClass *op_class,
                                   GskGpuShaderFlags          flags,
                                   GskGpuColorStates          color_states,
                                   guint32                    variation,
                                   GskGpuBlend                blend,
                                   VkFormat                   vk_format,
                                   VkRenderPass               render_pass)
{
  PipelineCacheKey cache_key;
  PipelineCacheKey *cached_result;
  VkPipeline vk_pipeline;
  GdkDisplay *display;
  char *vertex_shader_name, *fragment_shader_name;
  G_GNUC_UNUSED gint64 begin_time = GDK_PROFILER_CURRENT_TIME;
  const char *blend_name[] = { "NONE", "OVER", "ADD", "CLEAR" };

  cache_key = (PipelineCacheKey) {
    .vk_layout = vk_layout,
    .op_class = op_class,
    .color_states = color_states,
    .variation = variation,
    .flags = flags,
    .blend = blend,
    .vk_format = vk_format,
  };
  cached_result = g_hash_table_lookup (self->pipeline_cache, &cache_key);
  if (cached_result)
    return cached_result->vk_pipeline;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  vertex_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/",
                                    op_class->shader_name,
                                    ".vert.spv",
                                    NULL);
  fragment_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/",
                                      op_class->shader_name,
                                      ".frag.spv",
                                      NULL);

  GSK_VK_CHECK (vkCreateGraphicsPipelines, display->vk_device,
                                           display->vk_pipeline_cache,
                                           1,
                                           &(VkGraphicsPipelineCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                               .stageCount = 2,
                                               .pStages = (VkPipelineShaderStageCreateInfo[2]) {
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                       .module = gdk_display_get_vk_shader_module (display, vertex_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 3,
                                                           .pMapEntries = (VkSpecializationMapEntry[6]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, flags),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, color_states),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 2,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, variation),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .flags = flags,
                                                               .color_states = color_states,
                                                               .variation = variation,
                                                           },
                                                       },
                                                   },
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       .module = gdk_display_get_vk_shader_module (display, fragment_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 3,
                                                           .pMapEntries = (VkSpecializationMapEntry[6]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, flags),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, color_states),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 2,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, variation),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .flags = flags,
                                                               .color_states = color_states,
                                                               .variation = variation,
                                                           },
                                                       },
                                                   },
                                               },

                                               .pVertexInputState = op_class->vertex_input_state,
                                               .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                   .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                   .primitiveRestartEnable = VK_FALSE,
                                               },
                                               .pTessellationState = NULL,
                                               .pViewportState = &(VkPipelineViewportStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                   .viewportCount = 1,
                                                   .scissorCount = 1
                                               },
                                               .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                   .depthClampEnable = VK_FALSE,
                                                   .rasterizerDiscardEnable = VK_FALSE,
                                                   .polygonMode = VK_POLYGON_MODE_FILL,
                                                   .cullMode = VK_CULL_MODE_NONE,
                                                   .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                                   .lineWidth = 1.0f,
                                               },
                                               .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                   .rasterizationSamples = 1,
                                               },
                                               .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
                                               },
                                               .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                   .attachmentCount = 1,
                                                   .pAttachments = &blend_attachment_states[blend],
                                               },
                                               .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = 2,
                                                   .pDynamicStates = (VkDynamicState[2]) {
                                                       VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR
                                                   },
                                               },
                                               .layout = vk_layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &vk_pipeline);

  gdk_profiler_end_markf (begin_time,
                          "Create Vulkan pipeline", "%s color states=%u variation=%u clip=%u blend=%s format=%u",
                          op_class->shader_name,
                          flags,
                          color_states, 
                          variation,
                          blend_name[blend],
                          vk_format);

  GSK_DEBUG (SHADERS,
             "Create Vulkan pipeline (%s, %u/%u/%u/%s/%u)",
             op_class->shader_name,
             flags,
             color_states, 
             variation,
             blend_name[blend],
             vk_format);

  g_free (fragment_shader_name);
  g_free (vertex_shader_name);

  cached_result = g_memdup (&cache_key, sizeof (PipelineCacheKey));
  cached_result->vk_pipeline = vk_pipeline;
  g_hash_table_add (self->pipeline_cache, cached_result);
  gdk_display_vulkan_pipeline_cache_updated (display);

  return vk_pipeline;
}

static GskVulkanAllocator *
gsk_vulkan_device_get_allocator (GskVulkanDevice    *self,
                                 gsize               index,
                                 const VkMemoryType *type)
{
  if (self->allocators[index] == NULL)
    {
      self->allocators[index] = gsk_vulkan_direct_allocator_new (gsk_vulkan_device_get_vk_device (self),
                                                                 index,
                                                                 type);
      self->allocators[index] = gsk_vulkan_buddy_allocator_new (self->allocators[index],
                                                                1024 * 1024);
      //allocators[index] = gsk_vulkan_stats_allocator_new (allocators[index]);
    }

  return self->allocators[index];
}

/* following code found in
 * https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceMemoryProperties.html */
GskVulkanAllocator *
gsk_vulkan_device_find_allocator (GskVulkanDevice       *self,
                                  uint32_t               allowed_types,
                                  VkMemoryPropertyFlags  required_flags,
                                  VkMemoryPropertyFlags  desired_flags)
{
  VkPhysicalDeviceMemoryProperties properties;
  uint32_t i, found;

  vkGetPhysicalDeviceMemoryProperties (gsk_vulkan_device_get_vk_physical_device (self),
                                       &properties);

  found = properties.memoryTypeCount;
  for (i = 0; i < properties.memoryTypeCount; i++)
    {
      if (!(allowed_types & (1 << i)))
        continue;

      if ((properties.memoryTypes[i].propertyFlags & required_flags) != required_flags)
        continue;

      found = MIN (i, found);

      if ((properties.memoryTypes[i].propertyFlags & desired_flags) == desired_flags)
        break;
  }

  g_assert (found < properties.memoryTypeCount);

  return gsk_vulkan_allocator_ref (gsk_vulkan_device_get_allocator (self, found, &properties.memoryTypes[found]));
}

GskVulkanAllocator *
gsk_vulkan_device_get_external_allocator (GskVulkanDevice *self)
{
  if (self->external_allocator == NULL)
    {
      self->external_allocator = gsk_vulkan_external_allocator_new (gsk_vulkan_device_get_vk_device (self));
      //self->external_allocator = gsk_vulkan_stats_allocator_new (self->external_allocator);
    }

  return self->external_allocator;
}
