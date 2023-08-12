#include "config.h"

#include "gskvulkandeviceprivate.h"

#include "gskvulkanimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

struct _GskVulkanDevice
{
  GskGpuDevice parent_instance;

  GskVulkanAllocator *allocators[VK_MAX_MEMORY_TYPES];

  VkCommandPool vk_command_pool;
};

struct _GskVulkanDeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskVulkanDevice, gsk_vulkan_device, GSK_TYPE_GPU_DEVICE)

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
  gsize i;

  g_object_steal_data (G_OBJECT (gsk_gpu_device_get_display (device)), "-gsk-vulkan-device");

  display = gsk_gpu_device_get_display (device);

  vkDestroyCommandPool (display->vk_device,
                        self->vk_command_pool,
                        NULL);

  for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    g_clear_pointer (&self->allocators[i], gsk_vulkan_allocator_unref);

  gdk_display_unref_vulkan (display);

  G_OBJECT_CLASS (gsk_vulkan_device_parent_class)->finalize (object);
}

static void
gsk_vulkan_device_class_init (GskVulkanDeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_vulkan_device_create_offscreen_image;
  gpu_device_class->create_upload_image = gsk_vulkan_device_create_upload_image;

  object_class->finalize = gsk_vulkan_device_finalize;
}

static void
gsk_vulkan_device_init (GskVulkanDevice *self)
{
}

static void
gsk_vulkan_device_setup (GskVulkanDevice *self)
{
  GdkDisplay *display;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

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
  GskVulkanDevice *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-vulkan-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_display_init_vulkan (display, error))
    return NULL;

  self = g_object_new (GSK_TYPE_VULKAN_DEVICE, NULL);

  gsk_gpu_device_setup (GSK_GPU_DEVICE (self), display);
  gsk_vulkan_device_setup (self);

  g_object_set_data (G_OBJECT (display), "-gsk-vulkan-device", self);

  return GSK_GPU_DEVICE (self);
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

VkCommandPool
gsk_vulkan_device_get_vk_command_pool (GskVulkanDevice *self)
{
  return self->vk_command_pool;
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

