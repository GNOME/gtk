#include "config.h"

#include "gskvulkandeviceprivate.h"

#include "gskgpuglobalsopprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

#define DESCRIPTOR_POOL_MAXITEMS 50000

struct _GskVulkanDevice
{
  GskGpuDevice parent_instance;

  GskVulkanAllocator *allocators[VK_MAX_MEMORY_TYPES];
  GskVulkanAllocator *external_allocator;
  GdkVulkanFeatures features;

  GHashTable *conversion_cache;
  GHashTable *render_pass_cache;
  GHashTable *pipeline_layouts;
  GskVulkanPipelineLayout *pipeline_layout_cache;

  VkDescriptorSetLayout vk_buffer_set_layout;
  VkCommandPool vk_command_pool;
  VkSampler vk_samplers[GSK_GPU_SAMPLER_N_SAMPLERS];
};

struct _GskVulkanDeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskVulkanDevice, gsk_vulkan_device, GSK_TYPE_GPU_DEVICE)

typedef struct _ConversionCacheEntry ConversionCacheEntry;
typedef struct _PipelineCacheKey PipelineCacheKey;
typedef struct _RenderPassCacheKey RenderPassCacheKey;

struct _GskVulkanPipelineLayout
{
  gint ref_count;

  VkDescriptorSetLayout vk_image_set_layout;
  VkPipelineLayout vk_pipeline_layout;
  GHashTable *pipeline_cache;

  gsize n_samplers;
  VkSampler samplers[];
};

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
  GskGpuShaderClip clip;
  VkFormat format;
};

struct _RenderPassCacheKey
{
  VkFormat format;
  VkImageLayout from_layout;
  VkImageLayout to_layout;
};

static guint
conversion_cache_entry_hash (gconstpointer data)
{
  const ConversionCacheEntry *key = data;

  return key->vk_format;
}

static gboolean
conversion_cache_entry_equal (gconstpointer a,
                              gconstpointer b)
{
  const ConversionCacheEntry *keya = a;
  const ConversionCacheEntry *keyb = b;

  return keya->vk_format == keyb->vk_format;
}

static guint
pipeline_cache_key_hash (gconstpointer data)
{
  const PipelineCacheKey *key = data;

  return GPOINTER_TO_UINT (key->op_class) ^
         key->clip ^
         (key->format << 2);
}

static gboolean
pipeline_cache_key_equal (gconstpointer a,
                          gconstpointer b)
{
  const PipelineCacheKey *keya = a;
  const PipelineCacheKey *keyb = b;

  return keya->op_class == keyb->op_class &&
         keya->clip == keyb->clip &&
         keya->format == keyb->format;
}

static guint
render_pass_cache_key_hash (gconstpointer data)
{
  const RenderPassCacheKey *key = data;

  return (key->from_layout << 20) ^
         (key->to_layout << 16) ^
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
         keya->format == keyb->format;
}

static GskGpuImage *
gsk_vulkan_device_create_offscreen_image (GskGpuDevice   *device,
                                          GdkMemoryDepth  depth,
                                          gsize           width,
                                          gsize           height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_offscreen (self,
                                             GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                                             width,
                                             height);
}

static GskVulkanPipelineLayout *
gsk_vulkan_pipeline_layout_new (GskVulkanDevice *self,
                                VkSampler       *immutable_samplers,
                                gsize            n_immutable_samplers)
{
  GskVulkanPipelineLayout *layout;
  GdkDisplay *display;

  layout = g_malloc (sizeof (GskVulkanPipelineLayout) + (n_immutable_samplers + 1) * sizeof (VkSampler));
  layout->ref_count = 1;

  layout->n_samplers = n_immutable_samplers;
  memcpy (layout->samplers, immutable_samplers, (n_immutable_samplers + 1) * sizeof (VkSampler));
  layout->pipeline_cache = g_hash_table_new (pipeline_cache_key_hash, pipeline_cache_key_equal);

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, display->vk_device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 2,
                                                 .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[2]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = layout->n_samplers,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = layout->samplers
                                                     },
                                                     {
                                                         .binding = 1,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = DESCRIPTOR_POOL_MAXITEMS,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     }
                                                 },
                                                 .pNext = &(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                   .bindingCount = 2,
                                                   .pBindingFlags = (VkDescriptorBindingFlags[2]) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                     | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &layout->vk_image_set_layout);

  GSK_VK_CHECK (vkCreatePipelineLayout, display->vk_device,
                                        &(VkPipelineLayoutCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                            .setLayoutCount = 2,
                                            .pSetLayouts = (VkDescriptorSetLayout[2]) {
                                                layout->vk_image_set_layout,
                                                self->vk_buffer_set_layout,
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
                                        &layout->vk_pipeline_layout);

  g_hash_table_insert (self->pipeline_layouts, layout->samplers, layout);

  return layout;
}

static void
gsk_vulkan_pipeline_layout_unref (GskVulkanDevice         *self,
                                  GskVulkanPipelineLayout *layout)
{
  GdkDisplay *display;
  GHashTableIter iter;
  gpointer key, value;

  layout->ref_count--;
  if (layout->ref_count)
    return;

  if (!g_hash_table_remove (self->pipeline_layouts, layout->samplers))
    {
      g_assert_not_reached ();
    }

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  vkDestroyDescriptorSetLayout (display->vk_device,
                                layout->vk_image_set_layout,
                                NULL);

  vkDestroyPipelineLayout (display->vk_device,
                           layout->vk_pipeline_layout,
                           NULL);

  g_hash_table_iter_init (&iter, layout->pipeline_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_free (key);
      vkDestroyPipeline (display->vk_device, value, NULL);
    }
  g_hash_table_unref (layout->pipeline_cache);

  g_free (layout);
}

static void
gsk_vulkan_pipeline_layout_ref (GskVulkanDevice         *self,
                                GskVulkanPipelineLayout *layout)
{
  layout->ref_count++;
}

static guint
gsk_vulkan_pipeline_layout_hash_samplers (gconstpointer data)
{
  const VkSampler *samplers = data;
  guint result = 0;

  while (*samplers)
    {
      result = (result << 13) ^
               GPOINTER_TO_SIZE (*samplers) ^
               (GPOINTER_TO_SIZE (*samplers) >> 32);
      samplers++;
    }

  return result;
}

static gboolean
pipeline_layout_equal_samplers (gconstpointer a,
                                gconstpointer b)
{
  const VkSampler *samplersa = a;
  const VkSampler *samplersb = b;

  while (TRUE)
    {
      if (*samplersa != *samplersb)
        return FALSE;

      if (*samplersa == NULL)
        return TRUE;

      samplersa++;
      samplersb++;
    }
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
                                       GdkMemoryFormat  format,
                                       gsize            width,
                                       gsize            height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_upload (self,
                                          format,
                                          width,
                                          height);
}

static void
gsk_vulkan_device_finalize (GObject *object)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);
  GdkDisplay *display;
  GHashTableIter iter;
  gpointer key, value;
  gsize i;

  g_object_steal_data (G_OBJECT (gsk_gpu_device_get_display (device)), "-gsk-vulkan-device");

  display = gsk_gpu_device_get_display (device);

  if (self->pipeline_layout_cache)
    gsk_vulkan_pipeline_layout_unref (self, self->pipeline_layout_cache);
  g_assert (g_hash_table_size (self->pipeline_layouts) == 0);
  g_hash_table_unref (self->pipeline_layouts);

  g_hash_table_iter_init (&iter, self->conversion_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ConversionCacheEntry *entry = key;
      vkDestroySamplerYcbcrConversion (display->vk_device, entry->vk_conversion, NULL);
      vkDestroySampler (display->vk_device, entry->vk_sampler, NULL);
      g_free (key);
    }
  g_hash_table_unref (self->conversion_cache);

  g_hash_table_iter_init (&iter, self->render_pass_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_free (key);
      vkDestroyRenderPass (display->vk_device, value, NULL);
    }
  g_hash_table_unref (self->render_pass_cache);

  for (i = 0; i < G_N_ELEMENTS (self->vk_samplers); i++)
    {
      if (self->vk_samplers[i] != VK_NULL_HANDLE)
        vkDestroySampler (display->vk_device,
                          self->vk_samplers[i],
                          NULL);
    }

  vkDestroyCommandPool (display->vk_device,
                        self->vk_command_pool,
                        NULL);

  vkDestroyDescriptorSetLayout (display->vk_device,
                                self->vk_buffer_set_layout,
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

  object_class->finalize = gsk_vulkan_device_finalize;
}

static void
gsk_vulkan_device_init (GskVulkanDevice *self)
{
  self->conversion_cache = g_hash_table_new (conversion_cache_entry_hash, conversion_cache_entry_equal);
  self->render_pass_cache = g_hash_table_new (render_pass_cache_key_hash, render_pass_cache_key_equal);
  self->pipeline_layouts = g_hash_table_new (gsk_vulkan_pipeline_layout_hash_samplers, pipeline_layout_equal_samplers);
}

static void
gsk_vulkan_device_setup (GskVulkanDevice *self)
{
  GdkDisplay *display;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, display->vk_device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                         .descriptorCount = DESCRIPTOR_POOL_MAXITEMS,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     },
                                                 },
                                                 .pNext = &(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                   .bindingCount = 1,
                                                   .pBindingFlags = (VkDescriptorBindingFlags[1]) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                     | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &self->vk_buffer_set_layout);

  GSK_VK_CHECK (vkCreateCommandPool, display->vk_device,
                                     &(const VkCommandPoolCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .queueFamilyIndex = display->vk_queue_family_index,
                                         .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                     },
                                     NULL,
                                     &self->vk_command_pool);
}

GskGpuDevice *
gsk_vulkan_device_get_for_display (GdkDisplay  *display,
                                   GError     **error)
{
  VkPhysicalDeviceProperties vk_props;
  GskVulkanDevice *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-vulkan-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_display_init_vulkan (display, error))
    return NULL;

  self = g_object_new (GSK_TYPE_VULKAN_DEVICE, NULL);
  self->features = display->vulkan_features;

  vkGetPhysicalDeviceProperties (display->vk_physical_device, &vk_props);
  gsk_gpu_device_setup (GSK_GPU_DEVICE (self),
                        display,
                        vk_props.limits.maxImageDimension2D);
  gsk_vulkan_device_setup (self);

  g_object_set_data (G_OBJECT (display), "-gsk-vulkan-device", self);

  return GSK_GPU_DEVICE (self);
}

gsize
gsk_vulkan_device_get_max_descriptors (GskVulkanDevice *self)
{
  return DESCRIPTOR_POOL_MAXITEMS;
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

VkDescriptorSetLayout
gsk_vulkan_device_get_vk_image_set_layout (GskVulkanDevice         *self,
                                           GskVulkanPipelineLayout *layout)
{
  return layout->vk_image_set_layout;
}

VkDescriptorSetLayout
gsk_vulkan_device_get_vk_buffer_set_layout (GskVulkanDevice *self)
{
  return self->vk_buffer_set_layout;
}

VkPipelineLayout
gsk_vulkan_device_get_vk_pipeline_layout (GskVulkanDevice         *self,
                                          GskVulkanPipelineLayout *layout)
{
  return layout->vk_pipeline_layout;
}

VkCommandPool
gsk_vulkan_device_get_vk_command_pool (GskVulkanDevice *self)
{
  return self->vk_command_pool;
}

static VkSampler
gsk_vulkan_device_create_sampler (GskVulkanDevice          *self,
                                  VkSamplerYcbcrConversion  vk_conversion,
                                  VkFilter                  vk_filter,
                                  VkSamplerAddressMode      vk_address_mode)
{
  VkSampler result;

  GSK_VK_CHECK (vkCreateSampler, gsk_vulkan_device_get_vk_device (self),
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = vk_filter,
                                     .minFilter = vk_filter,
                                     .addressModeU = vk_address_mode,
                                     .addressModeV = vk_address_mode,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
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
  } filter_attrs[GSK_GPU_SAMPLER_N_SAMPLERS] = {
      [GSK_GPU_SAMPLER_DEFAULT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      },
      [GSK_GPU_SAMPLER_TRANSPARENT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      },
      [GSK_GPU_SAMPLER_REPEAT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      },
      [GSK_GPU_SAMPLER_NEAREST] = {
          .filter = VK_FILTER_NEAREST,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      },
  };

  if (self->vk_samplers[sampler] == VK_NULL_HANDLE)
    {
      self->vk_samplers[sampler] = gsk_vulkan_device_create_sampler (self,
                                                                     VK_NULL_HANDLE,
                                                                     filter_attrs[sampler].filter,
                                                                     filter_attrs[sampler].address_mode);
    }

  return self->vk_samplers[sampler];
}

VkSamplerYcbcrConversion
gsk_vulkan_device_get_vk_conversion (GskVulkanDevice *self,
                                     VkFormat         vk_format,
                                     VkSampler       *out_sampler)
{
  ConversionCacheEntry lookup;
  ConversionCacheEntry *entry;
  GdkDisplay *display;

  lookup = (ConversionCacheEntry) {
    .vk_format = vk_format,
  };
  entry = g_hash_table_lookup (self->conversion_cache, &lookup);
  if (entry)
    {
      if (out_sampler)
        *out_sampler = entry->vk_sampler;

      return entry->vk_conversion;
    }

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  entry = g_memdup (&lookup, sizeof (ConversionCacheEntry));

  GSK_VK_CHECK (vkCreateSamplerYcbcrConversion, display->vk_device,
                                                &(VkSamplerYcbcrConversionCreateInfo) {
                                                    .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
                                                    .format = vk_format,
                                                    .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601,
                                                    .ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
                                                    .components = (VkComponentMapping) {
                                                        VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY
                                                    },
                                                    .xChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN,
                                                    .yChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN,
                                                    .chromaFilter = VK_FILTER_LINEAR,
                                                    .forceExplicitReconstruction = VK_FALSE
                                                },
                                                NULL,
                                                &entry->vk_conversion);

  entry->vk_sampler = gsk_vulkan_device_create_sampler (self,
                                                        entry->vk_conversion,
                                                        VK_FILTER_LINEAR,
                                                        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

  g_hash_table_insert (self->conversion_cache, entry, entry);

  if (out_sampler)
    *out_sampler = entry->vk_sampler;

  return entry->vk_conversion;
}

VkRenderPass
gsk_vulkan_device_get_vk_render_pass (GskVulkanDevice *self,
                                      VkFormat         format,
                                      VkImageLayout    from_layout,
                                      VkImageLayout    to_layout)
{
  RenderPassCacheKey cache_key;
  VkRenderPass render_pass;
  GdkDisplay *display;

  cache_key = (RenderPassCacheKey) {
    .format = format,
    .from_layout = from_layout,
    .to_layout = to_layout,
  };
  render_pass = g_hash_table_lookup (self->render_pass_cache, &cache_key);
  if (render_pass)
    return render_pass;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateRenderPass, display->vk_device,
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = format,
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
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

  g_hash_table_insert (self->render_pass_cache, g_memdup (&cache_key, sizeof (RenderPassCacheKey)), render_pass);

  return render_pass;
}

typedef struct _GskVulkanShaderSpecialization GskVulkanShaderSpecialization;
struct _GskVulkanShaderSpecialization
{
  guint32 clip;
  guint32 n_immutable_samplers;
};

VkPipeline
gsk_vulkan_device_get_vk_pipeline (GskVulkanDevice           *self,
                                   GskVulkanPipelineLayout   *layout,
                                   const GskGpuShaderOpClass *op_class,
                                   GskGpuShaderClip           clip,
                                   VkFormat                   format,
                                   VkRenderPass               render_pass)
{
  PipelineCacheKey cache_key;
  VkPipeline pipeline;
  GdkDisplay *display;
  char *vertex_shader_name, *fragment_shader_name;

  cache_key = (PipelineCacheKey) {
    .op_class = op_class,
    .clip = clip,
    .format = format,
  };
  pipeline = g_hash_table_lookup (layout->pipeline_cache, &cache_key);
  if (pipeline)
    return pipeline;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));
  vertex_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/", op_class->shader_name, ".vert.spv", NULL);
  fragment_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/", op_class->shader_name, ".frag.spv", NULL);

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
                                                           .mapEntryCount = 2,
                                                           .pMapEntries = (VkSpecializationMapEntry[2]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, clip),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, n_immutable_samplers),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .clip = clip,
                                                               .n_immutable_samplers = layout->n_samplers,
                                                           },
                                                       },
                                                   },
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       .module = gdk_display_get_vk_shader_module (display, fragment_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 2,
                                                           .pMapEntries = (VkSpecializationMapEntry[2]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, clip),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, n_immutable_samplers),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .clip = clip,
                                                               .n_immutable_samplers = layout->n_samplers,
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
                                                   .pAttachments = (VkPipelineColorBlendAttachmentState []) {
                                                       {
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
                                                   }
                                               },
                                               .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = 2,
                                                   .pDynamicStates = (VkDynamicState[2]) {
                                                       VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR
                                                   },
                                               },
                                               .layout = layout->vk_pipeline_layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &pipeline);

  g_free (fragment_shader_name);
  g_free (vertex_shader_name);

  g_hash_table_insert (layout->pipeline_cache, g_memdup (&cache_key, sizeof (PipelineCacheKey)), pipeline);
  //gdk_vulkan_context_pipeline_cache_updated (self->vulkan);

  return pipeline;
}

GskVulkanPipelineLayout *
gsk_vulkan_device_acquire_pipeline_layout (GskVulkanDevice *self,
                                           VkSampler       *immutable_samplers,
                                           gsize            n_immutable_samplers)
{
  GskVulkanPipelineLayout *layout;
  VkSampler fallback[2];

  /* It's mandatory to have at least 1 sampler, because GLSL can't deal with 0-sized arrays */
  if (n_immutable_samplers == 0)
    {
      fallback[0] = gsk_vulkan_device_get_vk_sampler (self, GSK_GPU_SAMPLER_DEFAULT);
      fallback[1] = NULL;
      immutable_samplers = fallback;
      n_immutable_samplers = 1;
    }

  /* We require null-termination for the hash table lookup */
  g_assert (immutable_samplers[n_immutable_samplers] == NULL);

  layout = g_hash_table_lookup (self->pipeline_layouts, immutable_samplers);
  if (layout)
    {
      gsk_vulkan_pipeline_layout_ref (self, layout);
      return layout;
    }

  return gsk_vulkan_pipeline_layout_new (self, immutable_samplers, n_immutable_samplers);
}

void
gsk_vulkan_device_release_pipeline_layout (GskVulkanDevice         *self,
                                           GskVulkanPipelineLayout *layout)
{
  if (self->pipeline_layout_cache)
    gsk_vulkan_pipeline_layout_unref (self, self->pipeline_layout_cache);

  self->pipeline_layout_cache = layout;
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

  return gsk_vulkan_allocator_ref (gsk_vulkan_device_get_allocator (self, i, &properties.memoryTypes[i]));
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
