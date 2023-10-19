#include "config.h"

#include "gskvulkanimageprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanmemoryprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkvulkancontextprivate.h"
#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

#include <fcntl.h>
#include <string.h>

struct _GskVulkanImage
{
  GskGpuImage parent_instance;

  GdkDisplay *display;

  VkFormat vk_format;
  VkImageTiling vk_tiling;
  VkImageUsageFlags vk_usage;
  VkImage vk_image;
  VkImageView vk_image_view;
  VkFramebuffer vk_framebuffer;
  VkSampler vk_sampler;
  GskVulkanImagePostprocess postprocess;

  VkPipelineStageFlags vk_pipeline_stage;
  VkImageLayout vk_image_layout;
  VkAccessFlags vk_access;

  GskVulkanAllocator *allocator;
  GskVulkanAllocation allocation;
};

G_DEFINE_TYPE (GskVulkanImage, gsk_vulkan_image, GSK_TYPE_GPU_IMAGE)

typedef struct _GskMemoryFormatInfo GskMemoryFormatInfo;

struct _GskMemoryFormatInfo
{
  VkFormat format;
  VkComponentMapping components;
  GskVulkanImagePostprocess postprocess;
};

static const GskMemoryFormatInfo *
gsk_memory_format_get_vk_format_infos (GdkMemoryFormat format)
{
#define SWIZZLE(a, b, c, d) { VK_COMPONENT_SWIZZLE_ ## a, VK_COMPONENT_SWIZZLE_ ## b, VK_COMPONENT_SWIZZLE_ ## c, VK_COMPONENT_SWIZZLE_ ## d }
#define DEFAULT_SWIZZLE SWIZZLE (R, G, B, A)
  switch (format)
    {
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(A, B, G, R), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_B8G8R8A8_UNORM, DEFAULT_SWIZZLE,     0 },
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(B, G, R, A), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(G, B, A, R), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, DEFAULT_SWIZZLE, 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_B8G8R8A8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_B8G8R8A8_UNORM, DEFAULT_SWIZZLE,     GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(B, G, R, A), GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A8R8G8B8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(G, B, A, R), GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R8G8B8A8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, DEFAULT_SWIZZLE, GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A8B8G8R8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(A, B, G, R), GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_X8B8G8R8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(A, B, G, ONE), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }
    case GDK_MEMORY_B8G8R8X8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_B8G8R8A8_UNORM, SWIZZLE(R, G, B, ONE), 0 },
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(B, G, R, ONE), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_X8R8G8B8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(G, B, A, ONE), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R8G8B8X8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(R, G, B, ONE), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R8G8B8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8_UNORM, DEFAULT_SWIZZLE, 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_B8G8R8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_B8G8R8_UNORM, DEFAULT_SWIZZLE,     0 },
          { VK_FORMAT_R8G8B8_UNORM, SWIZZLE(B, G, R, A), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16_UNORM, DEFAULT_SWIZZLE, 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16A16_UNORM, DEFAULT_SWIZZLE, 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16A16:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16A16_UNORM, DEFAULT_SWIZZLE, GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16_FLOAT:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16_SFLOAT, DEFAULT_SWIZZLE, 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16A16_SFLOAT, DEFAULT_SWIZZLE, 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16A16_FLOAT:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16A16_SFLOAT, DEFAULT_SWIZZLE, GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R32G32B32_FLOAT:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R32G32B32_SFLOAT, DEFAULT_SWIZZLE, 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R32G32B32A32_SFLOAT, DEFAULT_SWIZZLE, 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R32G32B32A32_FLOAT:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R32G32B32A32_SFLOAT, DEFAULT_SWIZZLE, GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_G8A8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8_UNORM, SWIZZLE (R, R, R, G), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_G8A8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8_UNORM, SWIZZLE (R, R, R, G), GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_G8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8_UNORM, SWIZZLE (R, R, R, ONE), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_G16A16_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16_UNORM, SWIZZLE (R, R, R, G), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_G16A16:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16_UNORM, SWIZZLE (R, R, R, G), GSK_VULKAN_IMAGE_PREMULTIPLY },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_G16:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16_UNORM, SWIZZLE (R, R, R, ONE), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8_UNORM, SWIZZLE (R, R, R, R), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A16:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16_UNORM, SWIZZLE (R, R, R, R), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A16_FLOAT:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16_SFLOAT, SWIZZLE (R, R, R, R), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A32_FLOAT:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R32_SFLOAT, SWIZZLE (R, R, R, R), 0 },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return NULL;
    }
#undef DEFAULT_SWIZZLE
#undef SWIZZLE
}

static gboolean
gsk_memory_format_info_is_framebuffer_compatible (const GskMemoryFormatInfo *format)
{
  if (format->postprocess)
    return FALSE;

  if (format->components.r != VK_COMPONENT_SWIZZLE_R ||
      format->components.g != VK_COMPONENT_SWIZZLE_G ||
      format->components.b != VK_COMPONENT_SWIZZLE_B ||
      format->components.a != VK_COMPONENT_SWIZZLE_A)
    return FALSE;

  return TRUE;
}

static GdkMemoryFormat
gsk_memory_format_get_fallback (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_R8G8B8:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;

    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_X8B8G8R8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_B8G8R8:
      return GDK_MEMORY_R8G8B8;

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16A16:
      return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;

    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;

    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
      return GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;

    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;

    case GDK_MEMORY_R32G32B32_FLOAT:
    case  GDK_MEMORY_R32G32B32A32_FLOAT:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;

    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_G8A8:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;

    case GDK_MEMORY_G8:
      return GDK_MEMORY_R8G8B8;

    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16A16:
      return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;

    case GDK_MEMORY_G16:
      return GDK_MEMORY_R16G16B16;

    case GDK_MEMORY_A8:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_A16:
      return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
    case GDK_MEMORY_A16_FLOAT:
      return GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
    case GDK_MEMORY_A32_FLOAT:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;

    case GDK_MEMORY_N_FORMATS:
    default:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    }
}

static gboolean
gsk_vulkan_device_supports_format (GskVulkanDevice   *device,
                                   VkFormat           format,
                                   uint64_t           modifier,
                                   guint              n_planes,
                                   VkImageTiling      tiling,
                                   VkImageUsageFlags  usage,
                                   gsize              width,
                                   gsize              height)
{
  VkDrmFormatModifierPropertiesEXT drm_mod_properties[100];
  VkDrmFormatModifierPropertiesListEXT drm_properties;
  VkPhysicalDevice vk_phys_device;
  VkFormatProperties2 properties;
  VkImageFormatProperties2 image_properties;
  VkFormatFeatureFlags features, required;
  VkResult res;
  gsize i;

  vk_phys_device = gsk_vulkan_device_get_vk_physical_device (device);

  drm_properties = (VkDrmFormatModifierPropertiesListEXT) {
    .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    .drmFormatModifierCount = G_N_ELEMENTS (drm_mod_properties),
    .pDrmFormatModifierProperties = drm_mod_properties,
  };
  properties = (VkFormatProperties2) {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    .pNext = (tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) ? NULL : &drm_properties
  };
  vkGetPhysicalDeviceFormatProperties2 (vk_phys_device,
                                        format,
                                        &properties);

  switch ((int) tiling)
    {
      case VK_IMAGE_TILING_OPTIMAL:
        features = properties.formatProperties.optimalTilingFeatures;
        break;
      case VK_IMAGE_TILING_LINEAR:
        features = properties.formatProperties.linearTilingFeatures;
        break;
      case VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT:
        features = 0;
        for (i = 0; i < drm_properties.drmFormatModifierCount; i++)
          {
            if (drm_mod_properties[i].drmFormatModifier == modifier &&
                drm_mod_properties[i].drmFormatModifierPlaneCount == n_planes)
              {
                features = drm_mod_properties[i].drmFormatModifierTilingFeatures;
                break;
              }
          }
        if (features == 0)
          return FALSE;
        break;
      default:
        return FALSE;
    }
  required = 0;
  if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    required |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;

  if ((features & required) != required)
    return FALSE;

  image_properties = (VkImageFormatProperties2) {
    .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
  };
  res = vkGetPhysicalDeviceImageFormatProperties2 (vk_phys_device,
                                                   &(VkPhysicalDeviceImageFormatInfo2) {
                                                     .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
                                                     .format = format,
                                                     .type = VK_IMAGE_TYPE_2D,
                                                     .tiling = tiling,
                                                     .usage = usage,
                                                     .flags = 0,
                                                     .pNext = (tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) ? NULL : &(VkPhysicalDeviceImageDrmFormatModifierInfoEXT) {
                                                         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
                                                         .drmFormatModifier = modifier,
                                                         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                         .queueFamilyIndexCount = 1,
                                                         .pQueueFamilyIndices = (uint32_t[1]) { gsk_vulkan_device_get_vk_queue_family_index (device) },
                                                      }
                                                   },
                                                   &image_properties);
  if (res != VK_SUCCESS)
    return FALSE;

  if (image_properties.imageFormatProperties.maxExtent.width < width ||
      image_properties.imageFormatProperties.maxExtent.height < height)
    return FALSE;

  return TRUE;
}

static void
gsk_vulkan_image_create_view (GskVulkanImage            *self,
                              VkSamplerYcbcrConversion   vk_conversion,
                              const GskMemoryFormatInfo *format)
{
  GSK_VK_CHECK (vkCreateImageView, self->display->vk_device,
                                 &(VkImageViewCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                     .image = self->vk_image,
                                     .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                     .format = format->format,
                                     .components = format->components,
                                     .subresourceRange = {
                                         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = VK_REMAINING_MIP_LEVELS,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1,
                                     },
                                     .pNext = vk_conversion == VK_NULL_HANDLE ? NULL : &(VkSamplerYcbcrConversionInfo) {
                                         .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
                                         .conversion = vk_conversion
                                     }
                                 },
                                 NULL,
                                 &self->vk_image_view);
}

static GskVulkanImage *
gsk_vulkan_image_new (GskVulkanDevice           *device,
                      GdkMemoryFormat            format,
                      gsize                      width,
                      gsize                      height,
                      GskVulkanImagePostprocess  allowed_postprocess,
                      VkImageTiling              tiling,
                      VkImageUsageFlags          usage,
                      VkPipelineStageFlags       stage,
                      VkImageLayout              layout,
                      VkAccessFlags              access,
                      VkMemoryPropertyFlags      memory)
{
  VkMemoryRequirements requirements;
  GskVulkanImage *self;
  VkDevice vk_device;
  const GskMemoryFormatInfo *vk_format;

  g_assert (width > 0 && height > 0);

  while (TRUE)
    {
      for (vk_format = gsk_memory_format_get_vk_format_infos (format);
           vk_format->format != VK_FORMAT_UNDEFINED;
           vk_format++)
        {
          if (vk_format->postprocess & ~allowed_postprocess)
            continue;

          if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT &&
              !gsk_memory_format_info_is_framebuffer_compatible (vk_format))
            continue;

          if (gsk_vulkan_device_supports_format (device,
                                                 vk_format->format,
                                                 0, 1,
                                                 tiling, usage,
                                                 width, height))
            break;

          if (tiling != VK_IMAGE_TILING_OPTIMAL &&
              gsk_vulkan_device_supports_format (device,
                                                 vk_format->format,
                                                 0, 1,
                                                 VK_IMAGE_TILING_OPTIMAL, usage,
                                                 width, height))
            {
              tiling = VK_IMAGE_TILING_OPTIMAL;
              break;
            }
        }
      if (vk_format->format != VK_FORMAT_UNDEFINED)
        break;

      if (format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
        return NULL;

      format = gsk_memory_format_get_fallback (format);
    }

  vk_device = gsk_vulkan_device_get_vk_device (device);

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->display = g_object_ref (gsk_gpu_device_get_display (GSK_GPU_DEVICE (device)));
  gdk_display_ref_vulkan (self->display);
  self->vk_format = vk_format->format;
  self->postprocess = vk_format->postprocess;
  self->vk_tiling = tiling;
  self->vk_usage = usage;
  self->vk_pipeline_stage = stage;
  self->vk_image_layout = layout;
  self->vk_access = access;

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self), 0, format, width, height);

  GSK_VK_CHECK (vkCreateImage, vk_device,
                                &(VkImageCreateInfo) {
                                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                    .flags = 0,
                                    .imageType = VK_IMAGE_TYPE_2D,
                                    .format = vk_format->format,
                                    .extent = { width, height, 1 },
                                    .mipLevels = 1,
                                    .arrayLayers = 1,
                                    .samples = VK_SAMPLE_COUNT_1_BIT,
                                    .tiling = tiling,
                                    .usage = usage,
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                    .initialLayout = self->vk_image_layout,
                                },
                                NULL,
                                &self->vk_image);

  vkGetImageMemoryRequirements (vk_device,
                                self->vk_image,
                                &requirements);

  self->allocator = gsk_vulkan_device_find_allocator (device,
                                                      requirements.memoryTypeBits,
                                                      0,
                                                      tiling == VK_IMAGE_TILING_LINEAR ? GSK_VULKAN_MEMORY_MAPPABLE : 0);
  gsk_vulkan_alloc (self->allocator,
                    requirements.size,
                    requirements.alignment,
                    &self->allocation);

  GSK_VK_CHECK (vkBindImageMemory, vk_device,
                                   self->vk_image,
                                   self->allocation.vk_memory,
                                   self->allocation.offset);

  gsk_vulkan_image_create_view (self, VK_NULL_HANDLE, vk_format);

  return self;
}

GskGpuImage *
gsk_vulkan_image_new_for_upload (GskVulkanDevice   *device,
                                 GdkMemoryFormat    format,
                                 gsize              width,
                                 gsize              height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (device,
                               format,
                               width,
                               height,
                               -1,
                               VK_IMAGE_TILING_LINEAR,
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_IMAGE_LAYOUT_PREINITIALIZED,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return GSK_GPU_IMAGE (self);
}

static gboolean
gsk_vulkan_image_can_map (GskVulkanImage *self)
{
  if (GSK_DEBUG_CHECK (STAGING))
    return FALSE;

  if (self->vk_tiling != VK_IMAGE_TILING_LINEAR)
    return FALSE;

  if (self->vk_image_layout != VK_IMAGE_LAYOUT_PREINITIALIZED &&
      self->vk_image_layout != VK_IMAGE_LAYOUT_GENERAL)
    return FALSE;

  return self->allocation.map != NULL;
}

guchar *
gsk_vulkan_image_get_data (GskVulkanImage *self,
                           gsize          *out_stride)
{
  VkImageSubresource image_res;
  VkSubresourceLayout image_layout;

  if (!gsk_vulkan_image_can_map (self))
    return NULL;

  image_res.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_res.mipLevel = 0;
  image_res.arrayLayer = 0;

  vkGetImageSubresourceLayout (self->display->vk_device,
                               self->vk_image, &image_res, &image_layout);

  *out_stride = image_layout.rowPitch;

  return self->allocation.map + image_layout.offset;
}

GskGpuImage *
gsk_vulkan_image_new_for_swapchain (GskVulkanDevice  *device,
                                    VkImage           image,
                                    VkFormat          format,
                                    gsize             width,
                                    gsize             height)
{
  GskVulkanImage *self;

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->display = g_object_ref (gsk_gpu_device_get_display (GSK_GPU_DEVICE (device)));
  gdk_display_ref_vulkan (self->display);
  self->vk_tiling = VK_IMAGE_TILING_OPTIMAL;
  self->vk_image = image;
  self->vk_format = format;
  self->vk_pipeline_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  self->vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  self->vk_access = 0;

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self), 0, GDK_MEMORY_DEFAULT, width, height);

  gsk_vulkan_image_create_view (self,
                                VK_NULL_HANDLE,
                                &(GskMemoryFormatInfo) {
                                  format,
                                  { VK_COMPONENT_SWIZZLE_R,
                                    VK_COMPONENT_SWIZZLE_G,
                                    VK_COMPONENT_SWIZZLE_B,
                                    VK_COMPONENT_SWIZZLE_A
                                   }
                                });

  return GSK_GPU_IMAGE (self);
}

GskGpuImage *
gsk_vulkan_image_new_for_atlas (GskVulkanDevice *device,
                                gsize            width,
                                gsize            height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (device,
                               GDK_MEMORY_DEFAULT,
                               width,
                               height,
                               0,
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               0,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return GSK_GPU_IMAGE (self);
}

GskGpuImage *
gsk_vulkan_image_new_for_offscreen (GskVulkanDevice *device,
                                    GdkMemoryFormat  preferred_format,
                                    gsize            width,
                                    gsize            height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (device,
                               preferred_format,
                               width,
                               height,
                               0,
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return GSK_GPU_IMAGE (self);
}

#ifdef HAVE_DMABUF
GskGpuImage *
gsk_vulkan_image_new_for_dmabuf (GskVulkanDevice *device,
                                 GdkTexture      *texture)
{
  GskVulkanImage *self;
  VkDevice vk_device;
  VkFormat vk_format;
  VkComponentMapping vk_components;
  VkSamplerYcbcrConversion vk_conversion;
  PFN_vkGetMemoryFdPropertiesKHR func_vkGetMemoryFdPropertiesKHR;
  gsize i;
  int fd;
  gsize width, height;
  const GdkDmabuf *dmabuf;
  VkResult res;
  gboolean is_yuv;

  if (!gsk_vulkan_device_has_feature (device, GDK_VULKAN_FEATURE_DMABUF))
    {
      GDK_DEBUG (DMABUF, "Vulkan does not support dmabufs");
      return NULL;
    }

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  func_vkGetMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR) vkGetDeviceProcAddr (vk_device, "vkGetMemoryFdPropertiesKHR");

  vk_format = gdk_dmabuf_get_vk_format (dmabuf->fourcc, &vk_components);
  if (vk_format == VK_FORMAT_UNDEFINED)
    {
      GDK_DEBUG (DMABUF, "GTK's Vulkan doesn't support fourcc %.4s", (char *) &dmabuf->fourcc);
      return NULL;
    }
  if (!gdk_dmabuf_fourcc_is_yuv (dmabuf->fourcc, &is_yuv))
    {
      g_assert_not_reached ();
    }

  /* FIXME: Add support for disjoint images */
  if (gdk_dmabuf_is_disjoint (dmabuf))
    {
      GDK_DEBUG (DMABUF, "FIXME: Add support for disjoint dmabufs to Vulkan");
      return NULL;
    }

  if (!gsk_vulkan_device_supports_format (device,
                                          vk_format,
                                          dmabuf->modifier,
                                          dmabuf->n_planes,
                                          VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                                          VK_IMAGE_USAGE_SAMPLED_BIT,
                                          width, height))
    {
      GDK_DEBUG (DMABUF, "Vulkan driver does not support format %.4s::%016llx with %u planes",
                 (char *) &dmabuf->fourcc, (unsigned long long) dmabuf->modifier, dmabuf->n_planes);
      return NULL;
    }

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->display = g_object_ref (gsk_gpu_device_get_display (GSK_GPU_DEVICE (device)));
  gdk_display_ref_vulkan (self->display);
  self->vk_tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  self->vk_format = vk_format;
  self->vk_pipeline_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  self->vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  self->vk_access = 0;

  res  = vkCreateImage (vk_device,
                        &(VkImageCreateInfo) {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                            .flags = 0, //disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0,
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = vk_format,
                            .extent = { width, height, 1 },
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                            .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                            .initialLayout = self->vk_image_layout,
                            .pNext = &(VkExternalMemoryImageCreateInfo) {
                                .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                                .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                .pNext = &(VkImageDrmFormatModifierExplicitCreateInfoEXT) {
                                    .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
                                    .drmFormatModifier = dmabuf->modifier,
                                    .drmFormatModifierPlaneCount = dmabuf->n_planes,
                                    .pPlaneLayouts = (VkSubresourceLayout[4]) {
                                        {
                                            .offset = dmabuf->planes[0].offset,
                                            .rowPitch = dmabuf->planes[0].stride,
                                        },
                                        {
                                            .offset = dmabuf->planes[1].offset,
                                            .rowPitch = dmabuf->planes[1].stride,
                                        },
                                        {
                                            .offset = dmabuf->planes[2].offset,
                                            .rowPitch = dmabuf->planes[2].stride,
                                        },
                                        {
                                            .offset = dmabuf->planes[3].offset,
                                            .rowPitch = dmabuf->planes[3].stride,
                                        },
                                    },
                                }
                            },
                        },
                        NULL,
                        &self->vk_image);
  if (res != VK_SUCCESS)
    {
      gsk_vulkan_handle_result (res, "vkCreateImage");
      GDK_DEBUG (DMABUF, "vkCreateImage() failed: %s", gdk_vulkan_strerror (res));
      return NULL;
    }

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       is_yuv ? GSK_GPU_IMAGE_EXTERNAL : 0,
                       gdk_texture_get_format (texture),
                       width, height);
  gsk_gpu_image_toggle_ref_texture (GSK_GPU_IMAGE (self), texture);

  self->allocator = gsk_vulkan_device_get_external_allocator (device);
  gsk_vulkan_allocator_ref (self->allocator);

  fd = fcntl (dmabuf->planes[0].fd, F_DUPFD_CLOEXEC, (int) 3);
  if (fd < 0)
    {
      GDK_DEBUG (DMABUF, "Vulkan failed to dup() fd: %s", g_strerror (errno));
      vkDestroyImage (vk_device, self->vk_image, NULL);
      return NULL;
    }

  for (i = 0; i < 1 /* disjoint ? dmabuf->n_planes : 1 */; i++)
    {
      VkMemoryFdPropertiesKHR fd_props = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
      };
      VkMemoryRequirements2 requirements = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
      };

      GSK_VK_CHECK (func_vkGetMemoryFdPropertiesKHR, vk_device,
                                                     VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                                     fd,
                                                     &fd_props);

      vkGetImageMemoryRequirements2 (vk_device,
                                     &(VkImageMemoryRequirementsInfo2) {
                                         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
                                         .image = self->vk_image,
                                         //.pNext = !disjoint ? NULL : &(VkImagePlaneMemoryRequirementsInfo) {
                                         //    .sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
                                         //    .planeAspect = aspect_flags[i]
                                         //},
                                     },
                                     &requirements);

      gsk_vulkan_alloc (self->allocator,
                        requirements.memoryRequirements.size,
                        requirements.memoryRequirements.alignment,
                        &self->allocation);
      GSK_VK_CHECK (vkAllocateMemory, vk_device,
                                      &(VkMemoryAllocateInfo) {
                                          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                          .allocationSize = requirements.memoryRequirements.size,
                                          .memoryTypeIndex = g_bit_nth_lsf (fd_props.memoryTypeBits, -1),
                                          .pNext = &(VkImportMemoryFdInfoKHR) {
                                              .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
                                              .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                              .fd = fd,
                                              .pNext = &(VkMemoryDedicatedAllocateInfo) {
                                                  .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                                                  .image = self->vk_image,
                                              }
                                          }
                                      },
                                      NULL,
                                      &self->allocation.vk_memory);
    }

#if 1
  GSK_VK_CHECK (vkBindImageMemory2, self->display->vk_device,
                                    1,
                                    &(VkBindImageMemoryInfo) {
                                        .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                        .image = self->vk_image,
                                        .memory = self->allocation.vk_memory,
                                        .memoryOffset = self->allocation.offset,
                                    });
#else
  GSK_VK_CHECK (vkBindImageMemory2, self->display->vk_device,
                                    dmabuf->n_planes,
                                    (VkBindImageMemoryInfo[4]) {
                                        {
                                            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                            .image = self->vk_image,
                                            .memory = self->allocation.vk_memory,
                                            .memoryOffset = dmabuf->planes[0].offset,
                                            .pNext = &(VkBindImagePlaneMemoryInfo) {
                                                .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                                .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
                                            },
                                        },
                                        {
                                            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                            .image = self->vk_image,
                                            .memory = self->allocation.vk_memory,
                                            .memoryOffset = dmabuf->planes[1].offset,
                                            .pNext = &(VkBindImagePlaneMemoryInfo) {
                                                .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                                .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
                                            },

                                        },
                                        {
                                            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                            .image = self->vk_image,
                                            .memory = self->allocation.vk_memory,
                                            .memoryOffset = dmabuf->planes[2].offset,
                                            .pNext = &(VkBindImagePlaneMemoryInfo) {
                                                .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                                .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
                                            },
                                        },
                                        {
                                            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                            .image = self->vk_image,
                                            .memory = self->allocation.vk_memory,
                                            .memoryOffset = dmabuf->planes[3].offset,
                                            .pNext = &(VkBindImagePlaneMemoryInfo) {
                                                .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                                .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT,
                                            },
                                        }
                                    });
#endif

  if (is_yuv)
    vk_conversion = gsk_vulkan_device_get_vk_conversion (device, vk_format, &self->vk_sampler);
  else
    vk_conversion = VK_NULL_HANDLE;

  gsk_vulkan_image_create_view (self,
                                vk_conversion,
                                &(GskMemoryFormatInfo) {
                                  vk_format,
                                  vk_components,
                                });

  GDK_DEBUG (DMABUF, "Vulkan uploaded %zux%zu %.4s::%016llx %sdmabuf",
             width, height,
             (char *) &dmabuf->fourcc, (unsigned long long) dmabuf->modifier,
             is_yuv ? "YUV " : "");

  return GSK_GPU_IMAGE (self);
}
#endif

static void
gsk_vulkan_image_get_projection_matrix (GskGpuImage       *image,
                                        graphene_matrix_t *out_projection)
{
  graphene_matrix_t scale_z;

  GSK_GPU_IMAGE_CLASS (gsk_vulkan_image_parent_class)->get_projection_matrix (image, out_projection);

  graphene_matrix_init_from_float (&scale_z,
                                   (float[16]) {
                                       1,   0,   0,   0,
                                       0,   1,   0,   0,
                                       0,   0, 0.5,   0,
                                       0,   0, 0.5,   1
                                   });

  graphene_matrix_multiply (out_projection, &scale_z, out_projection);
}

static void
gsk_vulkan_image_finalize (GObject *object)
{
  GskVulkanImage *self = GSK_VULKAN_IMAGE (object);
  VkDevice device;

  device = self->display->vk_device;

  if (self->vk_framebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer (device, self->vk_framebuffer, NULL);

  if (self->vk_image_view != VK_NULL_HANDLE)
    vkDestroyImageView (device, self->vk_image_view, NULL);

  /* memory is NULL for for_swapchain() images, where we don't own
   * the VkImage */
  if (self->allocator)
    {
      vkDestroyImage (device, self->vk_image, NULL);
      gsk_vulkan_free (self->allocator, &self->allocation);
      gsk_vulkan_allocator_unref (self->allocator);
    }

  gdk_display_unref_vulkan (self->display);
  g_object_unref (self->display);

  G_OBJECT_CLASS (gsk_vulkan_image_parent_class)->finalize (object);
}

static void
gsk_vulkan_image_class_init (GskVulkanImageClass *klass)
{
  GskGpuImageClass *image_class = GSK_GPU_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_projection_matrix = gsk_vulkan_image_get_projection_matrix;

  object_class->finalize = gsk_vulkan_image_finalize;
}

static void
gsk_vulkan_image_init (GskVulkanImage *self)
{
}

VkFramebuffer
gsk_vulkan_image_get_vk_framebuffer (GskVulkanImage *self,
                                     VkRenderPass    render_pass)
{
  if (self->vk_framebuffer)
    return self->vk_framebuffer;

  GSK_VK_CHECK (vkCreateFramebuffer, self->display->vk_device,
                                     &(VkFramebufferCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                         .renderPass = render_pass,
                                         .attachmentCount = 1,
                                         .pAttachments = (VkImageView[1]) {
                                             self->vk_image_view,
                                         },
                                         .width = gsk_gpu_image_get_width (GSK_GPU_IMAGE (self)),
                                         .height = gsk_gpu_image_get_height (GSK_GPU_IMAGE (self)),
                                         .layers = 1
                                     },
                                     NULL,
                                     &self->vk_framebuffer);

  return self->vk_framebuffer;
}

GskVulkanImagePostprocess
gsk_vulkan_image_get_postprocess (GskVulkanImage *self)
{
  return self->postprocess;
}

VkSampler
gsk_vulkan_image_get_vk_sampler (GskVulkanImage *self)
{
  return self->vk_sampler;
}

VkImage
gsk_vulkan_image_get_vk_image (GskVulkanImage *self)
{
  return self->vk_image;
}

VkImageView
gsk_vulkan_image_get_vk_image_view (GskVulkanImage *self)
{
  return self->vk_image_view;
}

VkPipelineStageFlags
gsk_vulkan_image_get_vk_pipeline_stage (GskVulkanImage *self)
{
  return self->vk_pipeline_stage;
}

VkImageLayout
gsk_vulkan_image_get_vk_image_layout (GskVulkanImage *self)
{
  return self->vk_image_layout;
}

VkAccessFlags
gsk_vulkan_image_get_vk_access (GskVulkanImage *self)
{
  return self->vk_access;
}

void
gsk_vulkan_image_set_vk_image_layout (GskVulkanImage       *self,
                                      VkPipelineStageFlags  stage,
                                      VkImageLayout         image_layout,
                                      VkAccessFlags         access)
{
  self->vk_pipeline_stage = stage;
  self->vk_image_layout = image_layout;
  self->vk_access = access;
}

void
gsk_vulkan_image_transition (GskVulkanImage       *self,
                             VkCommandBuffer       command_buffer,
                             VkPipelineStageFlags  stage,
                             VkImageLayout         image_layout,
                             VkAccessFlags         access)
{
  if (self->vk_pipeline_stage == stage &&
      self->vk_image_layout == image_layout &&
      self->vk_access == access)
    return;

  vkCmdPipelineBarrier (command_buffer,
                        self->vk_pipeline_stage,
                        stage,
                        0,
                        0, NULL,
                        0, NULL,
                        1, &(VkImageMemoryBarrier) {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                            .srcAccessMask = self->vk_access,
                            .dstAccessMask = access,
                            .oldLayout = self->vk_image_layout,
                            .newLayout = image_layout,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .image = self->vk_image,
                            .subresourceRange = {
                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .baseMipLevel = 0,
                              .levelCount = VK_REMAINING_MIP_LEVELS,
                              .baseArrayLayer = 0,
                              .layerCount = 1
                            },
                        });

  gsk_vulkan_image_set_vk_image_layout (self, stage, image_layout, access);
}

VkFormat
gsk_vulkan_image_get_vk_format (GskVulkanImage *self)
{
  return self->vk_format;
}

