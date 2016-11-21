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
  GdkWindow *window;
};

enum {
  PROP_0,

  PROP_DISPLAY,
  PROP_WINDOW,

  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };

G_DEFINE_QUARK (gdk-vulkan-error-quark, gdk_vulkan_error)

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkVulkanContext, gdk_vulkan_context, G_TYPE_OBJECT)

static void
gdk_vulkan_context_dispose (GObject *gobject)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (gobject);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_clear_object (&priv->window);

  G_OBJECT_CLASS (gdk_vulkan_context_parent_class)->dispose (gobject);
}

static void
gdk_vulkan_context_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (gobject);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  switch (prop_id)
    {
    case PROP_WINDOW:
      priv->window = g_value_dup_object (value);
      g_assert (priv->window != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_vulkan_context_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (gobject);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, gdk_vulkan_context_get_display (context));
      break;

    case PROP_WINDOW:
      g_value_set_object (value, priv->window);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_vulkan_context_class_init (GdkVulkanContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gdk_vulkan_context_set_property;
  gobject_class->get_property = gdk_vulkan_context_get_property;
  gobject_class->dispose = gdk_vulkan_context_dispose;

  /**
   * GdkVulkanContext:display:
   *
   * The #GdkDisplay used to create the #GdkVulkanContext.
   *
   * Since: 3.16
   */
  pspecs[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         P_("Display"),
                         P_("The GDK display used to create the Vulkan context"),
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkVulkanContext:window:
   *
   * The #GdkWindow the gl context is bound to.
   *
   * Since: 3.16
   */
  pspecs[PROP_WINDOW] =
    g_param_spec_object ("window",
                         P_("Window"),
                         P_("The GDK window bound to the Vulkan context"),
                         GDK_TYPE_WINDOW,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, pspecs);
}

static void
gdk_vulkan_context_init (GdkVulkanContext *self)
{
}

/**
 * gdk_vulkan_context_get_display:
 * @context: a #GdkVulkanContext
 *
 * Retrieves the #GdkDisplay the @context is created for
 *
 * Returns: (nullable) (transfer none): a #GdkDisplay or %NULL
 *
 * Since: 3.90
 */
GdkDisplay *
gdk_vulkan_context_get_display (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), NULL);

  return priv->window ? gdk_window_get_display (priv->window) : NULL;
}

/**
 * gdk_vulkan_context_get_window:
 * @context: a #GdkVulkanContext
 *
 * Retrieves the #GdkWindow used by the @context.
 *
 * Returns: (nullable) (transfer none): a #GdkWindow or %NULL
 *
 * Since: 3.90
 */
GdkWindow *
gdk_vulkan_context_get_window (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), NULL);

  return priv->window;
}

#ifdef GDK_WINDOWING_VULKAN

static gboolean
gdk_display_create_vulkan_device (GdkDisplay *display)
{
  uint32_t i, j;

  uint32_t n_devices;
  GDK_VK_CHECK(vkEnumeratePhysicalDevices, display->vk_instance, &n_devices, NULL);
  VkPhysicalDevice devices[n_devices];
  GDK_VK_CHECK(vkEnumeratePhysicalDevices, display->vk_instance, &n_devices, devices);

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

  return FALSE;
}

static gboolean
gdk_display_create_vulkan_instance (GdkDisplay *display,
                                    const char *wsi_extension_name)
{
  uint32_t i;

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
                                           1,
                                           &wsi_extension_name },
                                       NULL,
                                       &display->vk_instance) != VK_SUCCESS)
    return FALSE;

  return gdk_display_create_vulkan_device (display);
}

gboolean
gdk_display_ref_vulkan (GdkDisplay *display,
                        const char *wsi_extension_name)
{
  if (display->vulkan_refcount == 0)
    {
      if (!gdk_display_create_vulkan_instance (display, wsi_extension_name))
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
