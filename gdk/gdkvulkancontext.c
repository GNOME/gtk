/* GDK - The GIMP Drawing Kit
 *
 * gdkvulkancontext-x11.c: X11 specific Vulkan wrappers
 *
 * Copyright © 2016  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkvulkancontextprivate.h"

#include <vulkan/vulkan.h>

#include "gdkdisplayprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"

typedef struct _GdkVulkanContextPrivate GdkVulkanContextPrivate;

struct _GdkVulkanContextPrivate {
  VkSurfaceKHR surface;
};

G_DEFINE_QUARK (gdk-vulkan-error-quark, gdk_vulkan_error)

static void gdk_vulkan_context_initable_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GdkVulkanContext, gdk_vulkan_context, GDK_TYPE_DRAW_CONTEXT,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gdk_vulkan_context_initable_init)
                                  G_ADD_PRIVATE (GdkVulkanContext))

static void
gdk_vulkan_context_dispose (GObject *gobject)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (gobject);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
  GdkDisplay *display;

  if (priv->surface != VK_NULL_HANDLE)
    {
      vkDestroySurfaceKHR (gdk_vulkan_context_get_instance (context),
                           priv->surface,
                           NULL);
      priv->surface = VK_NULL_HANDLE;
    }

  /* display will be unset in gdk_draw_context_dispose() */
  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  if (display)
    gdk_display_unref_vulkan (display);

  G_OBJECT_CLASS (gdk_vulkan_context_parent_class)->dispose (gobject);
}

static void
gdk_vulkan_context_class_init (GdkVulkanContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gdk_vulkan_context_dispose;
}

static void
gdk_vulkan_context_init (GdkVulkanContext *self)
{
}

static gboolean
gdk_vulkan_context_real_init (GInitable     *initable,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (initable);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  if (!gdk_display_ref_vulkan (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context)), error))
    return FALSE;

  if (GDK_VULKAN_CONTEXT_GET_CLASS (context)->create_surface (context, &priv->surface) != VK_SUCCESS)
    {
      g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                           "Vulkan support not available for this window.");
      return FALSE;
    }

  return TRUE;
}

static void
gdk_vulkan_context_initable_init (GInitableIface *iface)
{
  iface->init = gdk_vulkan_context_real_init;
}

#ifdef GDK_WINDOWING_VULKAN

VkInstance
gdk_vulkan_context_get_instance (GdkVulkanContext *context)
{
  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), NULL);

  return gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context))->vk_instance;
}

static gboolean
gdk_display_create_vulkan_device (GdkDisplay  *display,
                                  GError     **error)
{
  uint32_t i, j;

  uint32_t n_devices;
  GDK_VK_CHECK(vkEnumeratePhysicalDevices, display->vk_instance, &n_devices, NULL);
  VkPhysicalDevice devices[n_devices];
  GDK_VK_CHECK(vkEnumeratePhysicalDevices, display->vk_instance, &n_devices, devices);

  if (n_devices == 0)
    {
      /* Give a different error for 0 devices so people know their drivers suck. */
      g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                           "No Vulkan devices available.");
      return FALSE;
    }

  for (i = 0; i < n_devices; i++)
    {
      VkPhysicalDeviceProperties props;

      vkGetPhysicalDeviceProperties (devices[i], &props);

      GDK_NOTE (VULKAN, g_print ("Vulkan Device %u:\n", i));
      GDK_NOTE (VULKAN, g_print ("    %s (%u)\n", props.deviceName, props.deviceType));
      GDK_NOTE (VULKAN, g_print ("    vendor ID: 0x%Xu\n", props.vendorID));
      GDK_NOTE (VULKAN, g_print ("    device ID: 0x%Xu\n", props.deviceID));
      GDK_NOTE (VULKAN, g_print ("    API version %u.%u.%u\n",
                                 VK_VERSION_MAJOR (props.apiVersion),
                                 VK_VERSION_MINOR (props.apiVersion),
                                 VK_VERSION_PATCH (props.apiVersion)));
      GDK_NOTE (VULKAN, g_print ("    driver version %u.%u.%u\n",
                                 VK_VERSION_MAJOR (props.driverVersion),
                                 VK_VERSION_MINOR (props.driverVersion),
                                 VK_VERSION_PATCH (props.driverVersion)));

      uint32_t n_queue_props;
      vkGetPhysicalDeviceQueueFamilyProperties (devices[i], &n_queue_props, NULL);
      VkQueueFamilyProperties queue_props[n_queue_props];
      vkGetPhysicalDeviceQueueFamilyProperties (devices[i], &n_queue_props, queue_props);

      for (j = 0; j < n_queue_props; j++)
        {
          GDK_NOTE (VULKAN, g_print ("    queue %u/%u: %s\n", j, n_queue_props, queue_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT ? "graphics" : "no graphics"));
          if (queue_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
              GDK_NOTE (VULKAN, g_print ("    => trying this queue\n"));
              if (GDK_VK_CHECK (vkCreateDevice, devices[i],
                                                &(VkDeviceCreateInfo) {
                                                    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                                    NULL,
                                                    0,
                                                    1,
                                                    &(VkDeviceQueueCreateInfo) {
                                                        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                        .queueFamilyIndex = j,
                                                        .queueCount = 1,
                                                        .pQueuePriorities = (float []) { 1.0f },
                                                    },
                                                    0,
                                                    NULL,
                                                    1,
                                                    (const char * const []) {
                                                       VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                    },
                                                },
                                                NULL,
                                                &display->vk_device) != VK_SUCCESS)
                continue;

              display->vk_physical_device = devices[i];
              vkGetDeviceQueue(display->vk_device, j, 0, &display->vk_queue);
              return TRUE;
            }
        }
    }

  g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                       "Could not find a Vulkan device with the required features.");
  return FALSE;
}

static gboolean
gdk_display_create_vulkan_instance (GdkDisplay  *display,
                                    GError     **error)
{
  uint32_t i;

  if (GDK_DISPLAY_GET_CLASS (display)->vk_extension_name == NULL)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                   "The %s backend has no Vulkan support.", G_OBJECT_TYPE_NAME (display));
      return FALSE;
    }

  uint32_t n_extensions;
  GDK_VK_CHECK (vkEnumerateInstanceExtensionProperties, NULL, &n_extensions, NULL);
  VkExtensionProperties extensions[n_extensions];
  GDK_VK_CHECK (vkEnumerateInstanceExtensionProperties, NULL, &n_extensions, extensions);

  for (i = 0; i < n_extensions; i++)
    {
      GDK_NOTE (VULKAN, g_print ("Extension available: %s v%u.%u.%u\n",
                                 extensions[i].extensionName,
                                 VK_VERSION_MAJOR (extensions[i].specVersion),
                                 VK_VERSION_MINOR (extensions[i].specVersion),
                                 VK_VERSION_PATCH (extensions[i].specVersion)));
    }

  uint32_t n_layers;
  GDK_VK_CHECK (vkEnumerateInstanceLayerProperties, &n_layers, NULL);
  VkLayerProperties layers[n_layers];
  GDK_VK_CHECK (vkEnumerateInstanceLayerProperties, &n_layers, layers);

  for (i = 0; i < n_layers; i++)
    {
      GDK_NOTE (VULKAN, g_print ("Layer available: %s v%u.%u.%u (%s)\n",
                                 layers[i].layerName,
                                 VK_VERSION_MAJOR (layers[i].specVersion),
                                 VK_VERSION_MINOR (layers[i].specVersion),
                                 VK_VERSION_PATCH (layers[i].specVersion),
                                 layers[i].description));
    }

  if (GDK_VK_CHECK (vkCreateInstance, &(VkInstanceCreateInfo) {
                                           VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                           NULL,
                                           0,
                                           &(VkApplicationInfo) {
                                               VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                               NULL,
                                               g_get_application_name (),
                                               0,
                                               "GTK+",
                                               VK_MAKE_VERSION (GDK_MAJOR_VERSION, GDK_MINOR_VERSION, GDK_MICRO_VERSION),
                                               VK_API_VERSION_1_0 },
                                           0,
                                           NULL,
                                           2,
                                           (const char *const *) &(const char *[2]) {
                                               VK_KHR_SURFACE_EXTENSION_NAME, 
                                               GDK_DISPLAY_GET_CLASS (display)->vk_extension_name
                                           },
                                       },
                                       NULL,
                                       &display->vk_instance) != VK_SUCCESS)
    {
      g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                           "Could not create a Vulkan instance.");
      return FALSE;
    }

  return gdk_display_create_vulkan_device (display, error);
}

gboolean
gdk_display_ref_vulkan (GdkDisplay *display,
                        GError     **error)
{
  if (display->vulkan_refcount == 0)
    {
      if (!gdk_display_create_vulkan_instance (display, error))
        return FALSE;
    }

  display->vulkan_refcount++;
    
  return TRUE;
}

void
gdk_display_unref_vulkan (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (display->vulkan_refcount > 0);

  display->vulkan_refcount--;
  if (display->vulkan_refcount > 0)
    return;
  
  vkDestroyDevice (display->vk_device, NULL);
  vkDestroyInstance (display->vk_instance, NULL);
}

#endif /* GDK_WINDOWING_VULKAN */
