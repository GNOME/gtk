#include "config.h"

#include "gskvulkanycbcrprivate.h"

#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"

struct _GskVulkanYcbcr
{
  GskGpuCached parent;

  int ref_count;

  GskVulkanYcbcrInfo info;

  VkSamplerYcbcrConversion vk_conversion;
  VkSampler vk_sampler;
  VkDescriptorSetLayout vk_descriptor_set_layout;
  VkPipelineLayout vk_pipeline_layouts[2];
};

static guint
gsk_vulkan_ycbcr_info_hash (gconstpointer info_)
{
  const GskVulkanYcbcrInfo *info = info_;

  return ((info->vk_components.r << 29) |
          (info->vk_components.g << 26) |
          (info->vk_components.b << 23) |
          (info->vk_components.a << 20) |
          (info->vk_ycbcr_model << 17) |
          (info->vk_ycbcr_range << 16)) ^
         info->vk_format;
}

static gboolean
gsk_vulkan_ycbcr_info_equal (gconstpointer info1_,
                             gconstpointer info2_)
{
  const GskVulkanYcbcrInfo *info1 = info1_;
  const GskVulkanYcbcrInfo *info2 = info2_;

  return info1->vk_format == info2->vk_format &&
         info1->vk_components.r == info2->vk_components.r && 
         info1->vk_components.g == info2->vk_components.g && 
         info1->vk_components.b == info2->vk_components.b && 
         info1->vk_components.a == info2->vk_components.a && 
         info1->vk_ycbcr_model == info2->vk_ycbcr_model &&
         info1->vk_ycbcr_range == info2->vk_ycbcr_range;
}

static void
gsk_vulkan_ycbcr_free (GskGpuCached *cached)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);
  GskVulkanYcbcr *self = (GskVulkanYcbcr *) cached;
  GskGpuCache *cache = cached->cache;
  GskVulkanDevice *device;
  VkDevice vk_device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_cache_get_device (cache));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  g_assert (self->ref_count == 0);

  g_hash_table_remove (priv->ycbcr_cache, &self->info);

  vkDestroySampler (vk_device, self->vk_sampler, NULL);
  vkDestroySamplerYcbcrConversion (vk_device, self->vk_conversion, NULL);
  vkDestroyDescriptorSetLayout (vk_device, self->vk_descriptor_set_layout, NULL);
  vkDestroyPipelineLayout (vk_device, self->vk_pipeline_layouts[0], NULL);
  vkDestroyPipelineLayout (vk_device, self->vk_pipeline_layouts[1], NULL);

  g_free (self);
}

static gboolean
gsk_vulkan_ycbcr_should_collect (GskGpuCached *cached,
                                 gint64        cache_timeout,
                                 gint64        timestamp)
{
  GskVulkanYcbcr *self = (GskVulkanYcbcr *) cached;

  if (self->ref_count > 0)
    return FALSE;

  return gsk_gpu_cached_is_old (cached, cache_timeout, timestamp);
}

static const GskGpuCachedClass GSK_VULKAN_YCBCR_CLASS =
{
  sizeof (GskVulkanYcbcr),
  "Vulkan Ycbcr",
  gsk_vulkan_ycbcr_free,
  gsk_vulkan_ycbcr_should_collect
};

void
gsk_vulkan_ycbcr_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  priv->ycbcr_cache = g_hash_table_new (gsk_vulkan_ycbcr_info_hash, gsk_vulkan_ycbcr_info_equal);
}

void
gsk_vulkan_ycbcr_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_assert (g_hash_table_size (priv->ycbcr_cache) == 0);
  g_hash_table_unref (priv->ycbcr_cache);
}

GskVulkanYcbcr *
gsk_vulkan_ycbcr_get (GskVulkanDevice          *device,
                      const GskVulkanYcbcrInfo *info)
{
  GskGpuCache *cache = gsk_gpu_device_get_cache (GSK_GPU_DEVICE (device));
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  VkDevice vk_device = gsk_vulkan_device_get_vk_device (device);
  VkDescriptorSetLayout vk_image_set_layout;
  GskVulkanYcbcr *self;

  self = g_hash_table_lookup (priv->ycbcr_cache, info);
  if (self)
    return self;

  self = gsk_gpu_cached_new (cache, &GSK_VULKAN_YCBCR_CLASS);

  self->info = *info;

  GSK_VK_CHECK (vkCreateSamplerYcbcrConversion, vk_device,
                                                &(VkSamplerYcbcrConversionCreateInfo) {
                                                    .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
                                                    .format = self->info.vk_format,
                                                    .ycbcrModel = self->info.vk_ycbcr_model,
                                                    .ycbcrRange = self->info.vk_ycbcr_range,
                                                    .components = self->info.vk_components,
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

  g_hash_table_insert (priv->ycbcr_cache, &self->info, self);

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

  gsk_gpu_cached_use ((GskGpuCached *) self);
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
