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

  GHashTable *render_pass_cache;

  VkCommandPool vk_command_pool;
  DescriptorPools descriptor_pools;
  gsize last_pool;
  VkSampler vk_samplers[GSK_GPU_SAMPLER_N_SAMPLERS];
  VkDescriptorSetLayout vk_image_set_layout;
  VkDescriptorSetLayout vk_image_set_layout_for_mask;
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
gsk_vulkan_device_create_vk_image_set_layout (GskVulkanDevice *self,
                                              gsize            n_descriptors)
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
                                                         .descriptorCount = n_descriptors,
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
                                            .setLayoutCount = 3,
                                            .pSetLayouts = (VkDescriptorSetLayout[3]) {
                                                image1_layout,
                                                image2_layout,
                                                self->vk_image_set_layout_for_mask,
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
                                          GdkMemoryFormat format,
                                          gboolean        is_srgb,
                                          gsize           width,
                                          gsize           height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_offscreen (self,
                                             with_mipmap,
                                             format,
                                             is_srgb,
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
gsk_vulkan_device_create_upload_image (GskGpuDevice     *device,
                                       gboolean          with_mipmap,
                                       GdkMemoryFormat   format,
                                       GskGpuConversion  conv,
                                       gsize             width,
                                       gsize             height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_upload (self,
                                          with_mipmap,
                                          format,
                                          conv,
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
                                       FALSE,
                                       width,
                                       height);
  if (image != NULL)
    return image;
#endif

  image = gsk_vulkan_image_new_for_offscreen (self,
                                              FALSE,
                                              gdk_memory_depth_get_format (depth),
                                              FALSE,
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
  vkDestroyDescriptorSetLayout (vk_device,
                                self->vk_image_set_layout_for_mask,
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
  self->render_pass_cache = g_hash_table_new (render_pass_cache_key_hash, render_pass_cache_key_equal);

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

  self->vk_image_set_layout = gsk_vulkan_device_create_vk_image_set_layout (self, 3);
  self->vk_image_set_layout_for_mask = gsk_vulkan_device_create_vk_image_set_layout (self, 1);

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
                        GSK_GPU_DEVICE_DEFAULT_TILE_SIZE,
                        vk_props.properties.limits.minUniformBufferOffsetAlignment);
}

GskGpuDevice *
gsk_vulkan_device_get_for_display (GdkDisplay  *display,
                                   GError     **error)
{
  GskVulkanDevice *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-vulkan-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_display_prepare_vulkan (display, error))
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
      [GSK_GPU_SAMPLER_REFLECT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
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

  cached_result = g_memdup2 (&cache_key, sizeof (RenderPassCacheKey));
  cached_result->render_pass = render_pass;

  g_hash_table_insert (self->render_pass_cache, cached_result, cached_result);

  return render_pass;
}

GskVulkanAllocator *
gsk_vulkan_device_get_allocator (GskVulkanDevice    *self,
                                 gsize               index)
{
  if (self->allocators[index] == NULL)
    {
      VkPhysicalDeviceMemoryProperties properties;

      vkGetPhysicalDeviceMemoryProperties (gsk_vulkan_device_get_vk_physical_device (self),
                                           &properties);

      self->allocators[index] = gsk_vulkan_direct_allocator_new (gsk_vulkan_device_get_vk_device (self),
                                                                 index,
                                                                 &properties.memoryTypes[index]);
      self->allocators[index] = gsk_vulkan_buddy_allocator_new (self->allocators[index],
                                                                1024 * 1024);
      //allocators[index] = gsk_vulkan_stats_allocator_new (allocators[index]);
    }

  return self->allocators[index];
}

/* following code found in
 * https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceMemoryProperties.html */
 gsize
 gsk_vulkan_device_find_allocator (GskVulkanDevice        *self,
                                   uint32_t                allowed_types,
                                   VkMemoryPropertyFlags   required_flags,
                                   VkMemoryPropertyFlags   desired_flags)
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
        {
          found = i;
          break;
        }
  }

  g_assert (found < properties.memoryTypeCount);

  return found;
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
