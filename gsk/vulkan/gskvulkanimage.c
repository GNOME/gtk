#include "config.h"

#include "gskvulkanimageprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanmemoryprivate.h"
#include "gskvulkanpipelineprivate.h"

#include "gdk/gdkmemoryformatprivate.h"

#include "gskrendernodeprivate.h"

#include <string.h>

struct _GskVulkanUploader
{
  GdkVulkanContext *vulkan;

  GskVulkanCommandPool *command_pool;

  GArray *before_buffer_barriers;
  GArray *before_image_barriers;
  VkCommandBuffer copy_buffer;
  GArray *after_buffer_barriers;
  GArray *after_image_barriers;

  GSList *staging_image_free_list;
  GSList *staging_buffer_free_list;
};

struct _GskVulkanImage
{
  GObject parent_instance;

  GdkVulkanContext *vulkan;

  GdkMemoryFormat format;
  VkFormat vk_format;
  gsize width;
  gsize height;
  VkImageUsageFlags vk_usage;
  VkImage vk_image;
  VkImageView vk_image_view;
  VkImageLayout vk_image_layout;
  VkAccessFlags vk_access;

  GskVulkanMemory *memory;
};

G_DEFINE_TYPE (GskVulkanImage, gsk_vulkan_image, G_TYPE_OBJECT)

GskVulkanUploader *
gsk_vulkan_uploader_new (GdkVulkanContext     *context,
                         GskVulkanCommandPool *command_pool)
{
  GskVulkanUploader *self;

  self = g_new0 (GskVulkanUploader, 1);

  self->vulkan = g_object_ref (context);
  self->command_pool = command_pool;

  self->before_buffer_barriers = g_array_new (FALSE, FALSE, sizeof (VkBufferMemoryBarrier));
  self->after_buffer_barriers = g_array_new (FALSE, FALSE, sizeof (VkBufferMemoryBarrier));

  self->before_image_barriers = g_array_new (FALSE, FALSE, sizeof (VkImageMemoryBarrier));
  self->after_image_barriers = g_array_new (FALSE, FALSE, sizeof (VkImageMemoryBarrier));

  return self;
}

void
gsk_vulkan_uploader_free (GskVulkanUploader *self)
{
  gsk_vulkan_uploader_reset (self);

  g_array_unref (self->after_buffer_barriers);
  g_array_unref (self->before_buffer_barriers);
  g_array_unref (self->after_image_barriers);
  g_array_unref (self->before_image_barriers);

  g_object_unref (self->vulkan);

  g_free (self);
}

static void
gsk_vulkan_uploader_add_image_barrier (GskVulkanUploader          *self,
                                       gboolean                    after,
                                       GskVulkanImage             *image,
                                       VkImageLayout               new_layout,
                                       VkAccessFlags               new_access)
{
  GArray *array;
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = image->vk_access,
    .dstAccessMask = new_access,
    .oldLayout = image->vk_image_layout,
    .newLayout = new_layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image->vk_image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  if (after)
    array = self->after_image_barriers;
  else
    array = self->before_image_barriers;

  g_array_append_val (array, barrier);

  image->vk_image_layout = new_layout;
  image->vk_access = new_access;
}

static void
gsk_vulkan_uploader_add_buffer_barrier (GskVulkanUploader           *self,
                                        gboolean                     after,
                                        const VkBufferMemoryBarrier *barrier)
{
  GArray *array;

  if (after)
    array = self->after_buffer_barriers;
  else
    array = self->before_buffer_barriers;

  g_array_append_val (array, *barrier);
}

static VkCommandBuffer
gsk_vulkan_uploader_get_copy_buffer (GskVulkanUploader *self)
{
  if (self->copy_buffer == VK_NULL_HANDLE)
    self->copy_buffer = gsk_vulkan_command_pool_get_buffer (self->command_pool);

  return self->copy_buffer;
}

void
gsk_vulkan_uploader_upload (GskVulkanUploader *self)
{
  VkPipelineStageFlagBits host_and_transfer_bits = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;

  if (self->before_buffer_barriers->len > 0 || self->before_image_barriers->len > 0)
    {
      VkCommandBuffer command_buffer;

      command_buffer = gsk_vulkan_command_pool_get_buffer (self->command_pool);
      vkCmdPipelineBarrier (command_buffer,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | host_and_transfer_bits,
                            VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                            0,
                            0, NULL,
                            self->before_buffer_barriers->len, (VkBufferMemoryBarrier *) self->before_buffer_barriers->data,
                            self->before_image_barriers->len, (VkImageMemoryBarrier *) self->before_image_barriers->data);
      gsk_vulkan_command_pool_submit_buffer (self->command_pool, command_buffer, 0, NULL, 0, NULL, VK_NULL_HANDLE);
      g_array_set_size (self->before_buffer_barriers, 0);
      g_array_set_size (self->before_image_barriers, 0);
    }

  /* append these to existing buffer */
  if (self->after_buffer_barriers->len > 0 || self->after_image_barriers->len > 0)
    {
      VkCommandBuffer command_buffer = gsk_vulkan_uploader_get_copy_buffer (self);
      vkCmdPipelineBarrier (command_buffer,
                            host_and_transfer_bits,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            0,
                            0, NULL,
                            self->after_buffer_barriers->len, (VkBufferMemoryBarrier *) self->after_buffer_barriers->data,
                            self->after_image_barriers->len, (VkImageMemoryBarrier *) self->after_image_barriers->data);
      g_array_set_size (self->after_buffer_barriers, 0);
      g_array_set_size (self->after_image_barriers, 0);
    }

  if (self->copy_buffer != VK_NULL_HANDLE)
    {
      gsk_vulkan_command_pool_submit_buffer (self->command_pool, self->copy_buffer, 0, NULL, 0, NULL, VK_NULL_HANDLE);
      self->copy_buffer = VK_NULL_HANDLE;
    }
}

void
gsk_vulkan_uploader_reset (GskVulkanUploader *self)
{
  g_array_set_size (self->before_image_barriers, 0);
  self->copy_buffer = VK_NULL_HANDLE;
  g_array_set_size (self->after_image_barriers, 0);

  g_slist_free_full (self->staging_image_free_list, g_object_unref);
  self->staging_image_free_list = NULL;
  g_slist_free_full (self->staging_buffer_free_list, (GDestroyNotify) gsk_vulkan_buffer_free);
  self->staging_buffer_free_list = NULL;
}

typedef struct _GskMemoryFormatInfo GskMemoryFormatInfo;

struct _GskMemoryFormatInfo
{
  VkFormat format;
  VkComponentMapping components;
};

static const GskMemoryFormatInfo *
gsk_memory_format_get_vk_format_infos (GdkMemoryFormat format)
{
#define SWIZZLE(a, b, c, d) { VK_COMPONENT_SWIZZLE_ ## a, VK_COMPONENT_SWIZZLE_ ## b, VK_COMPONENT_SWIZZLE_ ## c, VK_COMPONENT_SWIZZLE_ ## d }
#define DEFAULT_SWIZZLE SWIZZLE (R, G, B, A)
  switch (format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_B8G8R8A8_UNORM, DEFAULT_SWIZZLE },
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(B, G, R, A) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, SWIZZLE(G, B, A, R) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8A8_UNORM, DEFAULT_SWIZZLE },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

#if 0
  GDK_MEMORY_B8G8R8A8,
  GDK_MEMORY_A8R8G8B8,
  GDK_MEMORY_R8G8B8A8,
  GDK_MEMORY_A8B8G8R8,
#endif

    case GDK_MEMORY_R8G8B8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8B8_UNORM, DEFAULT_SWIZZLE },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_B8G8R8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_B8G8R8_UNORM, DEFAULT_SWIZZLE },
          { VK_FORMAT_R8G8B8_UNORM, SWIZZLE(B, G, R, A) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16_UNORM, DEFAULT_SWIZZLE },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16A16_UNORM, DEFAULT_SWIZZLE },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

#if 0
  GDK_MEMORY_R16G16B16A16,
#endif

    case GDK_MEMORY_R16G16B16_FLOAT:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16_SFLOAT, DEFAULT_SWIZZLE },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16B16A16_SFLOAT, DEFAULT_SWIZZLE },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

#if 0
  GDK_MEMORY_R16G16B16A16_FLOAT,
#endif

    case GDK_MEMORY_R32G32B32_FLOAT:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R32G32B32_SFLOAT, DEFAULT_SWIZZLE },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R32G32B32A32_SFLOAT, DEFAULT_SWIZZLE },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

#if 0
  GDK_MEMORY_R32G32B32A32_FLOAT,
#endif

    case GDK_MEMORY_G8A8_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8G8_UNORM, SWIZZLE (R, R, R, G) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

#if 0
  GDK_MEMORY_G8A8,
#endif

    case GDK_MEMORY_G8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8_UNORM, SWIZZLE (R, R, R, ONE) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_G16A16_PREMULTIPLIED:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16G16_UNORM, SWIZZLE (R, R, R, G) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

#if 0
  GDK_MEMORY_G16A16
#endif

    case GDK_MEMORY_G16:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16_UNORM, SWIZZLE (R, R, R, ONE) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A8:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R8_UNORM, SWIZZLE (R, R, R, R) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_A16:
      {
        static const GskMemoryFormatInfo info[] = {
          { VK_FORMAT_R16_UNORM, SWIZZLE (R, R, R, R) },
          { VK_FORMAT_UNDEFINED }
        };
        return info;
      }

    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G16A16:
      {
        static const GskMemoryFormatInfo info[] = {
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

static GdkMemoryFormat
gsk_memory_format_get_fallback (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_B8G8R8A8:
      return GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
    case GDK_MEMORY_A8R8G8B8:
      return GDK_MEMORY_A8R8G8B8_PREMULTIPLIED;
    case GDK_MEMORY_R8G8B8A8:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_A8B8G8R8:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_R8G8B8:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_B8G8R8:
      return GDK_MEMORY_R8G8B8;
    case GDK_MEMORY_R16G16B16:
      return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
    case GDK_MEMORY_R16G16B16A16:
      return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
    case GDK_MEMORY_R16G16B16_FLOAT:
      return GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
    case GDK_MEMORY_R16G16B16A16_FLOAT:
      return GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
    case GDK_MEMORY_R32G32B32_FLOAT:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case  GDK_MEMORY_R32G32B32A32_FLOAT:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_G8A8:
      return GDK_MEMORY_G8A8_PREMULTIPLIED;
    case GDK_MEMORY_G8:
      return GDK_MEMORY_R8G8B8;
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
      return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
    case GDK_MEMORY_G16A16:
      return GDK_MEMORY_G16A16_PREMULTIPLIED;
    case GDK_MEMORY_G16:
      return GDK_MEMORY_R16G16B16;
    case GDK_MEMORY_A8:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GDK_MEMORY_A16:
      return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;

    case GDK_MEMORY_N_FORMATS:
    default:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    }
}

static gboolean
gsk_vulkan_context_supports_format (GdkVulkanContext *context,
                                    VkFormat          format)
{
  VkFormatProperties properties;

  vkGetPhysicalDeviceFormatProperties (gdk_vulkan_context_get_physical_device (context),
                                       format,
                                       &properties);

  if ((properties.linearTilingFeatures & (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)) &&
      (properties.optimalTilingFeatures & (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)))
    return TRUE;

  return FALSE;
}

static void
gsk_vulkan_image_create_view (GskVulkanImage            *self,
                              const GskMemoryFormatInfo *format)
{
  GSK_VK_CHECK (vkCreateImageView, gdk_vulkan_context_get_device (self->vulkan),
                                 &(VkImageViewCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                     .image = self->vk_image,
                                     .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                     .format = format->format,
                                     .components = format->components,
                                     .subresourceRange = {
                                         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = 1,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1,
                                     },
                                 },
                                 NULL,
                                 &self->vk_image_view);
}

static GskVulkanImage *
gsk_vulkan_image_new (GdkVulkanContext      *context,
                      GdkMemoryFormat        format,
                      gsize                  width,
                      gsize                  height,
                      VkImageTiling          tiling,
                      VkImageUsageFlags      usage,
                      VkImageLayout          layout,
                      VkAccessFlags          access,
                      VkMemoryPropertyFlags  memory)
{
  VkMemoryRequirements requirements;
  GskVulkanImage *self;
  const GskMemoryFormatInfo *vk_format;

  g_assert (width > 0 && height > 0);

  while (TRUE)
    {
      for (vk_format = gsk_memory_format_get_vk_format_infos (format);
           vk_format->format != VK_FORMAT_UNDEFINED;
           vk_format++)
        {
          if (gsk_vulkan_context_supports_format (context, vk_format->format))
            break;
        }
      if (vk_format->format != VK_FORMAT_UNDEFINED)
        break;

      format = gsk_memory_format_get_fallback (format);
    }

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->vulkan = g_object_ref (context);
  self->format = format;
  self->vk_format = vk_format->format;
  self->width = width;
  self->height = height;
  self->vk_usage = usage;
  self->vk_image_layout = layout;
  self->vk_access = access;

  GSK_VK_CHECK (vkCreateImage, gdk_vulkan_context_get_device (context),
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

  vkGetImageMemoryRequirements (gdk_vulkan_context_get_device (context),
                                self->vk_image,
                                &requirements);

  self->memory = gsk_vulkan_memory_new (context,
                                        requirements.memoryTypeBits,
                                        memory,
                                        requirements.size);

  GSK_VK_CHECK (vkBindImageMemory, gdk_vulkan_context_get_device (context),
                                   self->vk_image,
                                   gsk_vulkan_memory_get_device_memory (self->memory),
                                   0);

  gsk_vulkan_image_create_view (self, vk_format);

  return self;
}

GskVulkanImage *
gsk_vulkan_image_new_from_texture (GskVulkanUploader *uploader,
                                   GdkTexture        *texture)
{
  GdkTextureDownloader *downloader;
  GskVulkanImage *result;
  GskVulkanImageMap map;

  downloader = gdk_texture_downloader_new (texture);
  result = gsk_vulkan_image_new_for_upload (uploader,
                                            gdk_texture_get_format (texture),
                                            gdk_texture_get_width (texture),
                                            gdk_texture_get_height (texture));
  gdk_texture_downloader_set_format (downloader, result->format);
  gsk_vulkan_image_map_memory (result, uploader, GSK_VULKAN_WRITE, &map);
  gdk_texture_downloader_download_into (downloader, map.data, map.stride);
  gsk_vulkan_image_unmap_memory (result, uploader, &map);
  gdk_texture_downloader_free (downloader);
  return result;
}

GskVulkanImage *
gsk_vulkan_image_new_for_upload (GskVulkanUploader *uploader,
                                 GdkMemoryFormat    format,
                                 gsize              width,
                                 gsize              height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (uploader->vulkan,
                               format,
                               width,
                               height,
                               VK_IMAGE_TILING_LINEAR,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return self;
}

static void
gsk_vulkan_image_map_memory_direct (GskVulkanImage    *self,
                                    GskVulkanUploader *uploader,
                                    GskVulkanMapMode   mode,
                                    GskVulkanImageMap *map)
{
  VkImageSubresource image_res;
  VkSubresourceLayout image_layout;

  if (self->vk_image_layout != VK_IMAGE_LAYOUT_PREINITIALIZED)
    {
      gsk_vulkan_uploader_add_image_barrier (uploader,
                                             FALSE,
                                             self,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             (mode & GSK_VULKAN_READ ? VK_ACCESS_MEMORY_READ_BIT : 0) |
                                             (mode & GSK_VULKAN_WRITE ? VK_ACCESS_MEMORY_WRITE_BIT : 0));

      if (mode & GSK_VULKAN_READ)
        {
          gsk_vulkan_uploader_upload (uploader);
          GSK_VK_CHECK (vkQueueWaitIdle, gdk_vulkan_context_get_queue (self->vulkan));
        }
    }

  image_res.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_res.mipLevel = 0;
  image_res.arrayLayer = 0;

  vkGetImageSubresourceLayout (gdk_vulkan_context_get_device (self->vulkan),
                               self->vk_image, &image_res, &image_layout);

  map->mode = mode;
  map->staging_buffer = NULL;
  map->data = gsk_vulkan_memory_map (self->memory) + image_layout.offset;
  map->stride = image_layout.rowPitch;
}

static void
gsk_vulkan_image_unmap_memory_direct (GskVulkanImage    *self,
                                      GskVulkanUploader *uploader,
                                      GskVulkanImageMap *map)
{
  gsk_vulkan_memory_unmap (self->memory);

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         TRUE,
                                         self,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         VK_ACCESS_SHADER_READ_BIT);
}

static void
gsk_vulkan_image_map_memory_indirect (GskVulkanImage    *self,
                                      GskVulkanUploader *uploader,
                                      GskVulkanMapMode   mode,
                                      GskVulkanImageMap *map)
{
  map->mode = mode;
  map->stride = self->width * gdk_memory_format_bytes_per_pixel (self->format);
  map->staging_buffer = gsk_vulkan_buffer_new_map (uploader->vulkan, self->height * map->stride, mode);

  if (self->vk_image_layout != VK_IMAGE_LAYOUT_PREINITIALIZED)
    {
      if (mode & GSK_VULKAN_READ)
        {
          gsk_vulkan_uploader_add_image_barrier (uploader,
                                                 FALSE,
                                                 self,
                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_ACCESS_TRANSFER_READ_BIT);

          vkCmdCopyImageToBuffer (gsk_vulkan_uploader_get_copy_buffer (uploader),
                                  self->vk_image,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  gsk_vulkan_buffer_get_buffer (map->staging_buffer),
                                  1,
                                  (VkBufferImageCopy[1]) {
                                       {
                                           .bufferOffset = 0,
                                           .imageSubresource = {
                                               .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                               .mipLevel = 0,
                                               .baseArrayLayer = 0,
                                               .layerCount = 1
                                           },
                                           .imageOffset = { 0, 0, 0 },
                                           .imageExtent = {
                                               .width = self->width,
                                               .height = self->height,
                                               .depth = 1
                                           }
                                       }
                                  });

          gsk_vulkan_uploader_upload (uploader);
          GSK_VK_CHECK (vkQueueWaitIdle, gdk_vulkan_context_get_queue (self->vulkan));
        }
    }

  map->data = gsk_vulkan_buffer_map (map->staging_buffer);
}

static void
gsk_vulkan_image_unmap_memory_indirect (GskVulkanImage    *self,
                                        GskVulkanUploader *uploader,
                                        GskVulkanImageMap *map)
{
  gsk_vulkan_buffer_unmap (map->staging_buffer);

  if (map->mode & GSK_VULKAN_WRITE)
    {
      gsk_vulkan_uploader_add_buffer_barrier (uploader,
                                              FALSE,
                                              &(VkBufferMemoryBarrier) {
                                                 .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                                 .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
                                                 .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                 .buffer = gsk_vulkan_buffer_get_buffer (map->staging_buffer),
                                                 .offset = 0,
                                                 .size = VK_WHOLE_SIZE,
                                             });

      gsk_vulkan_uploader_add_image_barrier (uploader,
                                             FALSE,
                                             self,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             VK_ACCESS_TRANSFER_WRITE_BIT);

      vkCmdCopyBufferToImage (gsk_vulkan_uploader_get_copy_buffer (uploader),
                              gsk_vulkan_buffer_get_buffer (map->staging_buffer),
                              self->vk_image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              1,
                              (VkBufferImageCopy[1]) {
                                   {
                                       .bufferOffset = 0,
                                       .imageSubresource = {
                                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .mipLevel = 0,
                                           .baseArrayLayer = 0,
                                           .layerCount = 1
                                       },
                                       .imageOffset = { 0, 0, 0 },
                                       .imageExtent = {
                                           .width = self->width,
                                           .height = self->height,
                                           .depth = 1
                                       }
                                   }
                              });

      uploader->staging_buffer_free_list = g_slist_prepend (uploader->staging_buffer_free_list,
                                                            map->staging_buffer);
    }
  else
    {
      gsk_vulkan_buffer_free (map->staging_buffer);
    }

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         TRUE,
                                         self,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         VK_ACCESS_SHADER_READ_BIT);
}

void
gsk_vulkan_image_map_memory (GskVulkanImage    *self,
                             GskVulkanUploader *uploader,
                             GskVulkanMapMode   mode,
                             GskVulkanImageMap *map)
{
  if (!GSK_DEBUG_CHECK (STAGING) && gsk_vulkan_memory_can_map (self->memory, TRUE))
    gsk_vulkan_image_map_memory_direct (self, uploader, mode, map);
  else
    gsk_vulkan_image_map_memory_indirect (self, uploader, mode, map);
}

void
gsk_vulkan_image_unmap_memory (GskVulkanImage    *self,
                               GskVulkanUploader *uploader,
                               GskVulkanImageMap *map)
{
  if (map->staging_buffer)
    gsk_vulkan_image_unmap_memory_indirect (self, uploader, map);
  else
    gsk_vulkan_image_unmap_memory_direct (self, uploader, map);
}

GskVulkanImage *
gsk_vulkan_image_new_for_swapchain (GdkVulkanContext *context,
                                    VkImage           image,
                                    VkFormat          format,
                                    gsize             width,
                                    gsize             height)
{
  GskVulkanImage *self;

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->vulkan = g_object_ref (context);
  self->width = width;
  self->height = height;
  self->vk_image = image;
  self->vk_format = format;

  gsk_vulkan_image_create_view (self,
                                &(GskMemoryFormatInfo) {
                                  format,
                                  { VK_COMPONENT_SWIZZLE_R,
                                    VK_COMPONENT_SWIZZLE_G,
                                    VK_COMPONENT_SWIZZLE_B,
                                    VK_COMPONENT_SWIZZLE_A
                                   }
                                });

  return self;
}

GskVulkanImage *
gsk_vulkan_image_new_for_atlas (GdkVulkanContext *context,
                                gsize             width,
                                gsize             height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (context,
                               GDK_MEMORY_DEFAULT,
                               width,
                               height,
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               0,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return self;
}

GskVulkanImage *
gsk_vulkan_image_new_for_offscreen (GdkVulkanContext *context,
                                    GdkMemoryFormat   preferred_format,
                                    gsize             width,
                                    gsize             height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (context,
                               preferred_format,
                               width,
                               height,
                               VK_IMAGE_TILING_LINEAR,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return self;
}

GdkTexture *
gsk_vulkan_image_download (GskVulkanImage    *self,
                           GskVulkanUploader *uploader)
{
  GskVulkanImageMap map;
  GdkTexture *texture;
  GBytes *bytes;

  gsk_vulkan_image_map_memory (self, uploader, GSK_VULKAN_READ, &map);
  bytes = g_bytes_new (map.data, map.stride * self->height);
  texture = gdk_memory_texture_new (self->width, self->height,
                                    self->format,
                                    bytes,
                                    map.stride);
  g_bytes_unref (bytes);
  gsk_vulkan_image_unmap_memory (self, uploader, &map);

  return texture;
}

void
gsk_vulkan_image_upload_regions (GskVulkanImage    *self,
                                 GskVulkanUploader *uploader,
                                 guint              num_regions,
                                 GskImageRegion    *regions)
{
  GskVulkanBuffer *staging;
  guchar *mem;
  guchar *m;
  gsize size;
  gsize offset;
  VkBufferImageCopy *bufferImageCopy;

  size = 0;
  for (int i = 0; i < num_regions; i++)
    size += regions[i].width * regions[i].height * 4;

  staging = gsk_vulkan_buffer_new_map (uploader->vulkan, size, GSK_VULKAN_WRITE);
  mem = gsk_vulkan_buffer_map (staging);

  bufferImageCopy = alloca (sizeof (VkBufferImageCopy) * num_regions);
  memset (bufferImageCopy, 0, sizeof (VkBufferImageCopy) * num_regions);

  offset = 0;
  for (int i = 0; i < num_regions; i++)
    {
      m = mem + offset;
      if (regions[i].stride == regions[i].width * 4)
        {
          memcpy (m, regions[i].data, regions[i].stride * regions[i].height);
        }
      else
        {
          for (gsize r = 0; r < regions[i].height; r++)
            memcpy (m + r * regions[i].width * 4, regions[i].data + r * regions[i].stride, regions[i].width * 4);
        }

      bufferImageCopy[i].bufferOffset = offset;
      bufferImageCopy[i].bufferRowLength = regions[i].width;
      bufferImageCopy[i].bufferImageHeight = regions[i].height;
      bufferImageCopy[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      bufferImageCopy[i].imageSubresource.mipLevel = 0;
      bufferImageCopy[i].imageSubresource.baseArrayLayer = 0;
      bufferImageCopy[i].imageSubresource.layerCount = 1;
      bufferImageCopy[i].imageOffset.x = regions[i].x;
      bufferImageCopy[i].imageOffset.y = regions[i].y;
      bufferImageCopy[i].imageOffset.z = 0;
      bufferImageCopy[i].imageExtent.width = regions[i].width;
      bufferImageCopy[i].imageExtent.height = regions[i].height;
      bufferImageCopy[i].imageExtent.depth = 1;

      offset += regions[i].width * regions[i].height * 4;
    }

  gsk_vulkan_buffer_unmap (staging);

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         FALSE,
                                         self,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_ACCESS_TRANSFER_WRITE_BIT);

  vkCmdCopyBufferToImage (gsk_vulkan_uploader_get_copy_buffer (uploader),
                          gsk_vulkan_buffer_get_buffer (staging),
                          self->vk_image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          num_regions,
                          bufferImageCopy);

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         TRUE,
                                         self,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         VK_ACCESS_SHADER_READ_BIT);

  uploader->staging_buffer_free_list = g_slist_prepend (uploader->staging_buffer_free_list, staging);
}

static void
gsk_vulkan_image_finalize (GObject *object)
{
  GskVulkanImage *self = GSK_VULKAN_IMAGE (object);

  if (self->vk_image_view != VK_NULL_HANDLE)
    {
      vkDestroyImageView (gdk_vulkan_context_get_device (self->vulkan),
                          self->vk_image_view,
                          NULL);
    }

  /* memory is NULL for for_swapchain() images, where we don't own
   * the VkImage */
  if (self->memory)
    {
      vkDestroyImage (gdk_vulkan_context_get_device (self->vulkan),
                      self->vk_image,
                      NULL);

      gsk_vulkan_memory_free (self->memory);
    }

  g_object_unref (self->vulkan);

  G_OBJECT_CLASS (gsk_vulkan_image_parent_class)->finalize (object);
}

static void
gsk_vulkan_image_class_init (GskVulkanImageClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_image_finalize;
}

static void
gsk_vulkan_image_init (GskVulkanImage *self)
{
}

gsize
gsk_vulkan_image_get_width (GskVulkanImage *self)
{
  return self->width;
}

gsize
gsk_vulkan_image_get_height (GskVulkanImage *self)
{
  return self->height;
}

VkImage
gsk_vulkan_image_get_image (GskVulkanImage *self)
{
  return self->vk_image;
}

VkImageView
gsk_vulkan_image_get_image_view (GskVulkanImage *self)
{
  return self->vk_image_view;
}

VkFormat
gsk_vulkan_image_get_vk_format (GskVulkanImage *self)
{
  return self->vk_format;
}
