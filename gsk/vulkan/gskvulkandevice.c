#include "config.h"

#include "gskvulkandeviceprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

struct _GskVulkanDevice
{
  GObject parent_instance;

  GdkDisplay *display;

  GskVulkanAllocator *allocators[VK_MAX_MEMORY_TYPES];
};

struct _GskVulkanDeviceClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GskVulkanDevice, gsk_vulkan_device, G_TYPE_OBJECT)

static void
gsk_vulkan_device_finalize (GObject *object)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (object);
  gsize i;

  g_object_steal_data (G_OBJECT (self->display), "-gsk-vulkan-device");

  for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    g_clear_pointer (&self->allocators[i], gsk_vulkan_allocator_free);

  gdk_display_unref_vulkan (self->display);
  g_object_unref (self->display);

  G_OBJECT_CLASS (gsk_vulkan_device_parent_class)->finalize (object);
}

static void
gsk_vulkan_device_class_init (GskVulkanDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_vulkan_device_finalize;
}

static void
gsk_vulkan_device_init (GskVulkanDevice *self)
{
}

GskVulkanDevice *
gsk_vulkan_device_get_for_display (GdkDisplay  *display,
                                   GError     **error)
{
  GskVulkanDevice *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-vulkan-device");
  if (self)
    return g_object_ref (self);

  if (!gdk_display_ref_vulkan (display, error))
    return NULL;

  self = g_object_new (GSK_TYPE_VULKAN_DEVICE, NULL);
  self->display = display;

  g_object_set_data (G_OBJECT (display), "-gsk-vulkan-device", self);

  return self;
}

VkDevice
gsk_vulkan_device_get_vk_device (GskVulkanDevice *self)
{
  return self->display->vk_device;
}

VkPhysicalDevice
gsk_vulkan_device_get_vk_physical_device (GskVulkanDevice *self)
{
  return self->display->vk_physical_device;
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

  return gsk_vulkan_device_get_allocator (self, i, &properties.memoryTypes[i]);
}

