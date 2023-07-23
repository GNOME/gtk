#include "config.h"

#include "gskvulkandeviceprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

struct _GskVulkanDevice
{
  GObject parent_instance;

  GdkDisplay *display;
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

  g_object_steal_data (G_OBJECT (self->display), "-gsk-vulkan-device");

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
