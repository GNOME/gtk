#include "config.h"

#include "gskvulkanycbcrprivate.h"

#include "gskgpucacheprivate.h"

struct _GskVulkanYcbcr
{
  GskGpuCached parent;

  int ref_count;

  VkFormat vk_format;
  VkSamplerYcbcrConversion vk_conversion;
  VkSampler vk_sampler;
  VkDescriptorSetLayout vk_descriptor_set_layout;
  VkPipelineLayout vk_pipeline_layouts[2];
};

static void
gsk_vulkan_ycbcr_free (GskGpuCache  *cache,
                       GskGpuCached *cached)
{
  GskVulkanYcbcr *self = (GskVulkanYcbcr *) cached;
  GskVulkanDevice *device;
  VkDevice vk_device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_cache_get_device (cache));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  g_assert (self->ref_count == 0);

  gsk_vulkan_device_remove_ycbcr (device, self->vk_format);

  vkDestroySampler (vk_device, self->vk_sampler, NULL);
  vkDestroySamplerYcbcrConversion (vk_device, self->vk_conversion, NULL);
  vkDestroyDescriptorSetLayout (vk_device, self->vk_descriptor_set_layout, NULL);
  vkDestroyPipelineLayout (vk_device, self->vk_pipeline_layouts[0], NULL);
  vkDestroyPipelineLayout (vk_device, self->vk_pipeline_layouts[1], NULL);

  g_free (self);
}

static inline gboolean
gsk_vulkan_ycbcr_cached_is_old (GskGpuCache  *self,
                                GskGpuCached *cached,
                                gint64        cache_timeout,
                                gint64        timestamp)
{
  if (cache_timeout < 0)
    return -1;
  else
    return timestamp - cached->timestamp > cache_timeout;
}

static gboolean
gsk_vulkan_ycbcr_should_collect (GskGpuCache  *cache,
                                 GskGpuCached *cached,
                                 gint64        cache_timeout,
                                 gint64        timestamp)
{
  GskVulkanYcbcr *self = (GskVulkanYcbcr *) cached;

  if (self->ref_count > 0)
    return FALSE;

  return gsk_vulkan_ycbcr_cached_is_old (cache, cached, cache_timeout, timestamp);
}

static const GskGpuCachedClass GSK_VULKAN_YCBCR_CLASS =
{
  sizeof (GskVulkanYcbcr),
  "Vulkan Ycbcr",
  gsk_vulkan_ycbcr_free,
  gsk_vulkan_ycbcr_should_collect
};

GskVulkanYcbcr *
gsk_vulkan_ycbcr_new (GskVulkanDevice *device,
                      VkFormat         vk_format)
{
  GskGpuCache *cache = gsk_gpu_device_get_cache (GSK_GPU_DEVICE (device));
  VkDevice vk_device = gsk_vulkan_device_get_vk_device (device);
  VkDescriptorSetLayout vk_image_set_layout;
  GskVulkanYcbcr *self;

  self = gsk_gpu_cached_new (cache, &GSK_VULKAN_YCBCR_CLASS);

  self->vk_format = vk_format;

  GSK_VK_CHECK (vkCreateSamplerYcbcrConversion, vk_device,
                                                &(VkSamplerYcbcrConversionCreateInfo) {
                                                    .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
                                                    .format = vk_format,
                                                    .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601,
                                                    .ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW,
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
                                                &self->vk_conversion);

  GSK_VK_CHECK (vkCreateSampler, vk_device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                     .minLod = 0.0,
                                     .maxLod = 0.0f,
                                     .pNext = &(VkSamplerYcbcrConversionInfo) {
                                         .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
                                         .conversion = self->vk_conversion
                                     }
                                 },
                                 NULL,
                                 &self->vk_sampler);

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
                                                         .pImmutableSamplers = (VkSampler[1]) {
                                                             self->vk_sampler,
                                                         },
                                                     }
                                                 },
                                             },
                                             NULL,
                                             &self->vk_descriptor_set_layout);

  vk_image_set_layout = gsk_vulkan_device_get_vk_image_set_layout (device);
  self->vk_pipeline_layouts[0] = gsk_vulkan_device_create_vk_pipeline_layout (device,
                                                                              self->vk_descriptor_set_layout,
                                                                              vk_image_set_layout);
  self->vk_pipeline_layouts[1] = gsk_vulkan_device_create_vk_pipeline_layout (device,
                                                                              vk_image_set_layout,
                                                                              self->vk_descriptor_set_layout);

  return self;
}

GskVulkanYcbcr *
gsk_vulkan_ycbcr_ref (GskVulkanYcbcr *self)
{
  self->ref_count++;

  return self;
}

void
gsk_vulkan_ycbcr_unref (GskVulkanYcbcr *self)
{
  self->ref_count--;
}

VkSamplerYcbcrConversion
gsk_vulkan_ycbcr_get_vk_conversion (GskVulkanYcbcr *self)
{
  return self->vk_conversion;
}

VkSampler
gsk_vulkan_ycbcr_get_vk_sampler (GskVulkanYcbcr *self)
{
  return self->vk_sampler;
}

VkDescriptorSetLayout
gsk_vulkan_ycbcr_get_vk_descriptor_set_layout (GskVulkanYcbcr *self)
{
  return self->vk_descriptor_set_layout;
}

VkPipelineLayout
gsk_vulkan_ycbcr_get_vk_pipeline_layout (GskVulkanYcbcr  *self,
                                         gsize            id)
{
  g_assert (id < 2);

  return self->vk_pipeline_layouts[id];
}
