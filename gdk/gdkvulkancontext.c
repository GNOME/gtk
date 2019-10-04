/* GDK - The GIMP Drawing Kit
 *
 * gdkvulkancontext.c: Vulkan wrappers
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

#include "gdkvulkancontext.h"

#include "gdkvulkancontextprivate.h"

#include "gdkdisplayprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"

/**
 * SECTION:gdkvulkancontext
 * @Title: GdkVulkanContext
 * @Short_description: Vulkan draw context
 *
 * #GdkVulkanContext is an object representing the platform-specific
 * Vulkan draw context.
 *
 * #GdkVulkanContexts are created for a #GdkSurface using
 * gdk_surface_create_vulkan_context(), and the context will match the
 * the characteristics of the surface.
 *
 * Support for #GdkVulkanContext is platform-specific, context creation
 * can fail, returning %NULL context.
 */

/**
 * GdkVulkanContext:
 *
 * The GdkVulkanContext struct contains only private fields and should not
 * be accessed directly.
 */

typedef struct _GdkVulkanContextPrivate GdkVulkanContextPrivate;

struct _GdkVulkanContextPrivate {
#ifdef GDK_RENDERING_VULKAN
  VkSurfaceKHR surface;
  VkSurfaceFormatKHR image_format;

  VkSwapchainKHR swapchain;
  VkSemaphore draw_semaphore;

  guint n_images;
  VkImage *images;
  cairo_region_t **regions;

  gboolean has_present_region;

#endif

  guint32 draw_index;

  guint vulkan_ref: 1;
};

enum {
  IMAGES_UPDATED,

  LAST_SIGNAL
};

G_DEFINE_QUARK (gdk-vulkan-error-quark, gdk_vulkan_error)

static guint signals[LAST_SIGNAL] = { 0 };

static void gdk_vulkan_context_initable_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GdkVulkanContext, gdk_vulkan_context, GDK_TYPE_DRAW_CONTEXT,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gdk_vulkan_context_initable_init)
                                  G_ADD_PRIVATE (GdkVulkanContext))

#ifdef GDK_RENDERING_VULKAN

const char *
gdk_vulkan_strerror (VkResult result)
{
  /* If your compiler brought you here with a warning about missing
   * enumeration values, you're running a newer Vulkan version than
   * the GTK developers )or you are a GTK developer) and have
   * encountered a newly added Vulkan error message.
   * You want to add it to this enum now.
   *
   * Becuse the Vulkan people don't make adding this too easy, here's
   * the process to manage it:
   * 1. go to
   *    https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/blame/master/include/vulkan/vulkan.h
   * 2. Find the line where this enum value was added.
   * 3. Click the commit that added this line.
   * 4. The commit you're looking at now should also change
   *    VK_HEADER_VERSION, find that number.
   * 5. Use that number in the #ifdef when adding the enum value to
   *    this enum.
   * 6. For the error message, look at the specification (the one
   *    that includes all extensions) at
   *    https://www.khronos.org/registry/vulkan/specs/1.0-extensions/html/vkspec.html#VkResult
   * 7. If this value has not been added to the specification yet,
   *    search for the error message in the text of specification.
   *    Often it will have a description that can be used as an error
   *    message.
   * 8. If that didn't lead to one (or you are lazy), just use the
   *    literal string of the enum value as the error message. A
   *    GTK developer will add the correct one once it's added to the
   *    specification.
   */
  switch (result)
  {
    case VK_SUCCESS:
      return "Command successfully completed.";
    case VK_NOT_READY:
      return "A fence or query has not yet completed.";
    case VK_TIMEOUT:
      return "A wait operation has not completed in the specified time.";
    case VK_EVENT_SET:
      return "An event is signaled.";
    case VK_EVENT_RESET:
      return "An event is unsignaled.";
    case VK_INCOMPLETE:
      return "A return array was too small for the result.";
    case VK_SUBOPTIMAL_KHR:
      return "A swapchain no longer matches the surface properties exactly, but can still be used to present to the surface successfully.";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "A host memory allocation has failed.";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "A device memory allocation has failed.";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "Initialization of an object could not be completed for implementation-specific reasons.";
    case VK_ERROR_DEVICE_LOST:
      return "The logical or physical device has been lost.";
    case VK_ERROR_MEMORY_MAP_FAILED:
      return "Mapping of a memory object has failed.";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "A requested layer is not present or could not be loaded.";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "A requested extension is not supported.";
    case VK_ERROR_FEATURE_NOT_PRESENT:
      return "A requested feature is not supported.";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "The requested version of Vulkan is not supported by the driver or is otherwise incompatible for implementation-specific reasons.";
    case VK_ERROR_TOO_MANY_OBJECTS:
      return "Too many objects of the type have already been created.";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "A requested format is not supported on this device.";
#if VK_HEADER_VERSION >= 24
    case VK_ERROR_FRAGMENTED_POOL:
      return "A requested pool allocation has failed due to fragmentation of the pool’s memory.";
#endif
    case VK_ERROR_SURFACE_LOST_KHR:
      return "A surface is no longer available.";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
      return "The requested window is already in use by Vulkan or another API in a manner which prevents it from being used again.";
    case VK_ERROR_OUT_OF_DATE_KHR:
      return "A surface has changed in such a way that it is no longer compatible with the swapchain.";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
      return "The display used by a swapchain does not use the same presentable image layout, or is incompatible in a way that prevents sharing an image.";
    case VK_ERROR_VALIDATION_FAILED_EXT:
      return "The application caused the validation layer to fail.";
    case VK_ERROR_INVALID_SHADER_NV:
      return "One or more shaders failed to compile or link.";
#if VK_HEADER_VERSION >= 39
    case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
      return "A pool memory allocation has failed.";
#endif
#if VK_HEADER_VERSION >= 54
    case VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR:
      return "An external handle is not a valid handle of the specified type.";
#endif
#if VK_HEADER_VERSION >= 64
    case VK_ERROR_NOT_PERMITTED_EXT:
      return "The caller does not have sufficient privileges.";
#endif
#if VK_HEADER_VERSION >= 72
    case VK_ERROR_FRAGMENTATION_EXT:
      return "A descriptor pool creation has failed due to fragmentation";
#endif
#if VK_HEADER_VERSION >= 89
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
      return "Invalid DRM format modifier plane layout";
#endif
#if VK_HEADER_VERSION >= 97
    case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
      return "Invalid device address";
#endif

    case VK_RESULT_RANGE_SIZE:
    case VK_RESULT_MAX_ENUM:
    default:
      return "Unknown Vulkan error.";
  }
}

static void
gdk_vulkan_context_dispose (GObject *gobject)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (gobject);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
  GdkDisplay *display;
  VkDevice device;
  guint i;

  for (i = 0; i < priv->n_images; i++)
    {
      cairo_region_destroy (priv->regions[i]);
    }
  g_clear_pointer (&priv->regions, g_free);
  g_clear_pointer (&priv->images, g_free);
  priv->n_images = 0;

  device = gdk_vulkan_context_get_device (context);

  if (priv->draw_semaphore != VK_NULL_HANDLE)
    {
      vkDestroySemaphore (device,
                           priv->draw_semaphore,
                           NULL);
      priv->draw_semaphore = VK_NULL_HANDLE;
    }

  if (priv->swapchain != VK_NULL_HANDLE)
    {
      vkDestroySwapchainKHR (device,
                             priv->swapchain,
                             NULL);
      priv->swapchain = VK_NULL_HANDLE;
    }

  if (priv->surface != VK_NULL_HANDLE)
    {
      vkDestroySurfaceKHR (gdk_vulkan_context_get_instance (context),
                           priv->surface,
                           NULL);
      priv->surface = VK_NULL_HANDLE;
    }

  /* display will be unset in gdk_draw_context_dispose() */
  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  if (display && priv->vulkan_ref)
    gdk_display_unref_vulkan (display);

  G_OBJECT_CLASS (gdk_vulkan_context_parent_class)->dispose (gobject);
}

static gboolean
gdk_vulkan_context_check_swapchain (GdkVulkanContext  *context,
                                    GError           **error)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
  VkSurfaceCapabilitiesKHR capabilities;
  VkCompositeAlphaFlagBitsKHR composite_alpha;
  VkSwapchainKHR new_swapchain;
  VkResult res;
  VkDevice device;
  guint i;

  device = gdk_vulkan_context_get_device (context);

  res = GDK_VK_CHECK (vkGetPhysicalDeviceSurfaceCapabilitiesKHR, gdk_vulkan_context_get_physical_device (context),
                                                                 priv->surface,
                                                                 &capabilities);
  if (res != VK_SUCCESS)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                   "Could not query surface capabilities: %s", gdk_vulkan_strerror (res));
      return FALSE;
    }

  if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
    composite_alpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
  else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    {
      /* let's hope the backend knows what it's doing */
      composite_alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
  else
    {
      GDK_DISPLAY_NOTE (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context)),
                        VULKAN, g_warning ("Vulkan swapchain doesn't do transparency. Using opaque swapchain instead."));
      composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

  /*
   * Per https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#VkSurfaceCapabilitiesKHR
   * the current extent may assume a special value, meaning that the extend should assume whatever
   * value the surface has.
   */
  if (capabilities.currentExtent.width == -1 || capabilities.currentExtent.height == -1)
    {
      capabilities.currentExtent.width = gdk_surface_get_width (surface) * gdk_surface_get_scale_factor (surface);
      capabilities.currentExtent.height = gdk_surface_get_height (surface) * gdk_surface_get_scale_factor (surface);
    }

  res = GDK_VK_CHECK (vkCreateSwapchainKHR, device,
                                            &(VkSwapchainCreateInfoKHR) {
                                                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                                .pNext = NULL,
                                                .flags = 0,
                                                .surface = priv->surface,
                                                .minImageCount = CLAMP (4,
                                                                        capabilities.minImageCount,
                                                                        capabilities.maxImageCount ? capabilities.maxImageCount : G_MAXUINT32),
                                                .imageFormat = priv->image_format.format,
                                                .imageColorSpace = priv->image_format.colorSpace,
                                                .imageExtent = capabilities.currentExtent,
                                                .imageArrayLayers = 1,
                                                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                .queueFamilyIndexCount = 1,
                                                .pQueueFamilyIndices = (uint32_t[1]) {
                                                    gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context))->vk_queue_family_index
                                                },
                                                .preTransform = capabilities.currentTransform,
                                                .compositeAlpha = composite_alpha,
                                                .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                                .clipped = VK_FALSE,
                                                .oldSwapchain = priv->swapchain
                                            },
                                            NULL,
                                            &new_swapchain);

  if (priv->swapchain != VK_NULL_HANDLE)
    {
      vkDestroySwapchainKHR (device,
                             priv->swapchain,
                             NULL);
      for (i = 0; i < priv->n_images; i++)
        {
          cairo_region_destroy (priv->regions[i]);
        }
      g_clear_pointer (&priv->regions, g_free);
      g_clear_pointer (&priv->images, g_free);
      priv->n_images = 0;
    }

  if (res == VK_SUCCESS)
    {
      priv->swapchain = new_swapchain;

      GDK_VK_CHECK (vkGetSwapchainImagesKHR, device,
                                             priv->swapchain,
                                             &priv->n_images,
                                             NULL);
      priv->images = g_new (VkImage, priv->n_images);
      GDK_VK_CHECK (vkGetSwapchainImagesKHR, device,
                                             priv->swapchain,
                                             &priv->n_images,
                                             priv->images);
      priv->regions = g_new (cairo_region_t *, priv->n_images);
      for (i = 0; i < priv->n_images; i++)
        {
          priv->regions[i] = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                                                0, 0,
                                                                gdk_surface_get_width (surface),
                                                                gdk_surface_get_height (surface),
                                                            });
        }
    }
  else
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                   "Could not create swapchain for this surface: %s", gdk_vulkan_strerror (res));
      priv->swapchain = VK_NULL_HANDLE;
      return FALSE;
    }

  g_signal_emit (context, signals[IMAGES_UPDATED], 0);

  return res == VK_SUCCESS;
}

static gboolean
device_supports_incremental_present (VkPhysicalDevice device)
{
  VkExtensionProperties *extensions;
  uint32_t n_device_extensions;

  vkEnumerateDeviceExtensionProperties (device, NULL, &n_device_extensions, NULL);

  extensions = g_newa (VkExtensionProperties, n_device_extensions);
  vkEnumerateDeviceExtensionProperties (device, NULL, &n_device_extensions, extensions);

  for (uint32_t i = 0; i < n_device_extensions; i++)
    {
      if (g_str_equal (extensions[i].extensionName, VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME))
        return TRUE;
    }

  return FALSE;
}

static void
gdk_vulkan_context_begin_frame (GdkDrawContext *draw_context,
                                cairo_region_t *region)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (draw_context);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
  guint i;

  for (i = 0; i < priv->n_images; i++)
    {
      cairo_region_union (priv->regions[i], region);
    }

  GDK_VK_CHECK (vkAcquireNextImageKHR, gdk_vulkan_context_get_device (context),
                                       priv->swapchain,
                                       UINT64_MAX,
                                       priv->draw_semaphore,
                                       VK_NULL_HANDLE,
                                       &priv->draw_index);

  cairo_region_union (region, priv->regions[priv->draw_index]);
}

static void
gdk_vulkan_context_end_frame (GdkDrawContext *draw_context,
                              cairo_region_t *painted)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (draw_context);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
  VkPresentRegionsKHR *regionsptr = VK_NULL_HANDLE;
  VkPresentRegionsKHR regions;

  if (priv->has_present_region)
    {
      cairo_rectangle_int_t extents;

      cairo_region_get_extents (priv->regions[priv->draw_index], &extents);

      regions = (VkPresentRegionsKHR) {
          .sType = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR,
          .swapchainCount = 1,
          .pRegions = &(VkPresentRegionKHR) {
              .rectangleCount = 1,
              .pRectangles = &(VkRectLayerKHR) {
                  .offset.x = extents.x,
                  .offset.y = extents.y,
                  .extent.width = extents.width,
                  .extent.height = extents.height,
              }
          },
      };

      regionsptr = &regions;
    }

  GDK_VK_CHECK (vkQueuePresentKHR, gdk_vulkan_context_get_queue (context),
                                   &(VkPresentInfoKHR) {
                                       .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                       .waitSemaphoreCount = 1,
                                       .pWaitSemaphores = (VkSemaphore[]) {
                                           priv->draw_semaphore
                                       },
                                       .swapchainCount = 1,
                                       .pSwapchains = (VkSwapchainKHR[]) { 
                                           priv->swapchain
                                       },
                                       .pImageIndices = (uint32_t[]) {
                                           priv->draw_index
                                       },
                                       .pNext = regionsptr,
                                   });

  cairo_region_destroy (priv->regions[priv->draw_index]);
  priv->regions[priv->draw_index] = cairo_region_create ();
}

static void
gdk_vulkan_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (draw_context);
  GError *error = NULL;

  if (!gdk_vulkan_context_check_swapchain (context, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }
}

static void
gdk_vulkan_context_class_init (GdkVulkanContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_vulkan_context_dispose;

  draw_context_class->begin_frame = gdk_vulkan_context_begin_frame;
  draw_context_class->end_frame = gdk_vulkan_context_end_frame;
  draw_context_class->surface_resized = gdk_vulkan_context_surface_resized;

  /**
   * GdkVulkanContext::images-updated:
   * @context: the object on which the signal is emitted
   *
   * This signal is emitted when the images managed by this context have
   * changed. Usually this means that the swapchain had to be recreated,
   * for example in response to a change of the surface size.
   */
  signals[IMAGES_UPDATED] =
    g_signal_new (g_intern_static_string ("images-updated"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
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
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  VkResult res;
  VkBool32 supported;
  uint32_t i;

  priv->vulkan_ref = gdk_display_ref_vulkan (display, error);
  if (!priv->vulkan_ref)
    return FALSE;

  res = GDK_VULKAN_CONTEXT_GET_CLASS (context)->create_surface (context, &priv->surface);
  if (res != VK_SUCCESS)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                   "Could not create surface for this surface: %s", gdk_vulkan_strerror (res));
      return FALSE;
    }

  res = GDK_VK_CHECK (vkGetPhysicalDeviceSurfaceSupportKHR, gdk_vulkan_context_get_physical_device (context),
                                                            gdk_vulkan_context_get_queue_family_index (context),
                                                            priv->surface,
                                                            &supported);
  if (res != VK_SUCCESS)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                   "Could not check if queue family supports this surface: %s", gdk_vulkan_strerror (res));
    }
  else if (!supported)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                   "FIXME: Queue family does not support surface. Write code to try different queue family.");
    }
  else
    {
      uint32_t n_formats;
      GDK_VK_CHECK (vkGetPhysicalDeviceSurfaceFormatsKHR, gdk_vulkan_context_get_physical_device (context),
                                                          priv->surface,
                                                          &n_formats, NULL);
      VkSurfaceFormatKHR *formats = g_newa (VkSurfaceFormatKHR, n_formats);
      GDK_VK_CHECK (vkGetPhysicalDeviceSurfaceFormatsKHR, gdk_vulkan_context_get_physical_device (context),
                                                          priv->surface,
                                                          &n_formats, formats);
      for (i = 0; i < n_formats; i++)
        {
          if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
            break;
        }
      if (i == n_formats)
        {
          g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                               "No supported image format found.");
          goto out_surface;  
        }
      priv->image_format = formats[i];
      priv->has_present_region = device_supports_incremental_present (display->vk_physical_device);

      if (!gdk_vulkan_context_check_swapchain (context, error))
        goto out_surface;

      GDK_VK_CHECK (vkCreateSemaphore, gdk_vulkan_context_get_device (context),
                                       &(VkSemaphoreCreateInfo) {
                                           .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                       },
                                       NULL,
                                       &priv->draw_semaphore);

      return TRUE;
    }

out_surface:
  vkDestroySurfaceKHR (gdk_vulkan_context_get_instance (context),
                       priv->surface,
                       NULL);
  priv->surface = VK_NULL_HANDLE;
  return FALSE;
}

static void
gdk_vulkan_context_initable_init (GInitableIface *iface)
{
  iface->init = gdk_vulkan_context_real_init;
}

/**
 * gdk_vulkan_context_get_instance:
 * @context: a #GdkVulkanContext
 *
 * Gets the Vulkan instance that is associated with @context.
 *
 * Returns: (transfer none): the VkInstance
 */
VkInstance
gdk_vulkan_context_get_instance (GdkVulkanContext *context)
{
  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), NULL);

  return gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context))->vk_instance;
}

/**
 * gdk_vulkan_context_get_physical_device:
 * @context: a #GdkVulkanContext
 *
 * Gets the Vulkan physical device that this context is using.
 *
 * Returns: (transfer none): the VkPhysicalDevice
 */
VkPhysicalDevice
gdk_vulkan_context_get_physical_device (GdkVulkanContext *context)
{
  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), NULL);

  return gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context))->vk_physical_device;
}

/**
 * gdk_vulkan_context_get_device:
 * @context: a #GdkVulkanContext
 *
 * Gets the Vulkan device that this context is using.
 *
 * Returns: (transfer none): the VkDevice
 */
VkDevice
gdk_vulkan_context_get_device (GdkVulkanContext *context)
{
  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), NULL);

  return gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context))->vk_device;
}

/**
 * gdk_vulkan_context_get_queue:
 * @context: a #GdkVulkanContext
 *
 * Gets the Vulkan queue that this context is using.
 *
 * Returns: (transfer none): the VkQueue
 */
VkQueue
gdk_vulkan_context_get_queue (GdkVulkanContext *context)
{
  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), NULL);

  return gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context))->vk_queue;
}

/**
 * gdk_vulkan_context_get_queue_family_index:
 * @context: a #GdkVulkanContext
 *
 * Gets the family index for the queue that this context is using.
 * See vkGetPhysicalDeviceQueueFamilyProperties().
 *
 * Returns: the index
 */
uint32_t
gdk_vulkan_context_get_queue_family_index (GdkVulkanContext *context)
{
  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), 0);

  return gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context))->vk_queue_family_index;
}

/**
 * gdk_vulkan_context_get_image_format:
 * @context: a #GdkVulkanContext
 *
 * Gets the image format that this context is using.
 *
 * Returns: (transfer none): the VkFormat
 */
VkFormat
gdk_vulkan_context_get_image_format (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), VK_FORMAT_UNDEFINED);

  return priv->image_format.format;
}

/**
 * gdk_vulkan_context_get_n_images:
 * @context: a #GdkVulkanContext
 *
 * Gets the number of images that this context is using in its swap chain.
 *
 * Returns: the number of images
 */
uint32_t
gdk_vulkan_context_get_n_images (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), 0);

  return priv->n_images;
}

/**
 * gdk_vulkan_context_get_image:
 * @context: a #GdkVulkanContext
 * @id: the index of the image to return
 *
 * Gets the image with index @id that this context is using.
 *
 * Returns: (transfer none): the VkImage
 */
VkImage
gdk_vulkan_context_get_image (GdkVulkanContext *context,
                              guint             id)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), VK_NULL_HANDLE);
  g_return_val_if_fail (id < priv->n_images, VK_NULL_HANDLE);

  return priv->images[id];
}

/**
 * gdk_vulkan_context_get_draw_index:
 * @context: a #GdkVulkanContext
 *
 * Gets the index of the image that is currently being drawn.
 *
 * This function can only be used between gdk_draw_context_begin_frame() and
 * gdk_draw_context_end_frame() calls.
 *
 * Returns: the index of the images that is being drawn
 */
uint32_t
gdk_vulkan_context_get_draw_index (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), 0);
  g_return_val_if_fail (gdk_draw_context_is_in_frame (GDK_DRAW_CONTEXT (context)), 0);

  return priv->draw_index;
}

/**
 * gdk_vulkan_context_get_draw_semaphore:
 * @context: a #GdkVulkanContext
 *
 * Gets the Vulkan semaphore that protects access to the image that is
 * currently being drawn.
 *
 * This function can only be used between gdk_draw_context_begin_frame() and
 * gdk_draw_context_end_frame() calls.
 *
 * Returns: (transfer none): the VkSemaphore
 */
VkSemaphore
gdk_vulkan_context_get_draw_semaphore (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), VK_NULL_HANDLE);
  g_return_val_if_fail (gdk_draw_context_is_in_frame (GDK_DRAW_CONTEXT (context)), VK_NULL_HANDLE);

  return priv->draw_semaphore;
}

static gboolean
gdk_display_create_vulkan_device (GdkDisplay  *display,
                                  GError     **error)
{
  uint32_t i, j, k;
  const char *override;
  gboolean list_devices;
  int first, last;

  uint32_t n_devices = 0;
  GDK_VK_CHECK(vkEnumeratePhysicalDevices, display->vk_instance, &n_devices, NULL);
  VkPhysicalDevice *devices;

  if (n_devices == 0)
    {
      /* Give a different error for 0 devices so people know their drivers suck. */
      g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                           "No Vulkan devices available.");
      return FALSE;
    }

  devices = g_newa (VkPhysicalDevice, n_devices);
  GDK_VK_CHECK(vkEnumeratePhysicalDevices, display->vk_instance, &n_devices, devices);

  first = 0;
  last = n_devices;

  override = g_getenv ("GDK_VULKAN_DEVICE");
  list_devices = FALSE;
  if (override)
    {
      if (g_strcmp0 (override, "list") == 0)
        list_devices = TRUE;
      else
        {
          gint64 device_idx;
          GError *error2 = NULL;

          if (!g_ascii_string_to_signed (override, 10, 0, G_MAXINT, &device_idx, &error2))
            {
              g_warning ("Failed to parse %s: %s", "GDK_VULKAN_DEVICE", error2->message);
              g_error_free (error2);
              device_idx = -1;
            }

          if (device_idx < 0 || device_idx >= n_devices)
            g_warning ("%s value out of range, ignoring", "GDK_VULKAN_DEVICE");
          else
            {
              first = device_idx;
              last = first + 1;
            }
        }
    }

  if (list_devices || GDK_DISPLAY_DEBUG_CHECK (display, VULKAN))
    {
      for (i = 0; i < n_devices; i++)
        {
          VkPhysicalDeviceProperties props;
          VkQueueFamilyProperties *queue_props;
          uint32_t n_queue_props;
          const char *device_type[] = {
            "Other", "Integrated GPU", "Discrete GPU", "Virtual GPU", "CPU"
          };
          struct {
            int bit;
            const char *name;
          } queue_caps[] = {
            { VK_QUEUE_GRAPHICS_BIT, "graphics" },
            { VK_QUEUE_COMPUTE_BIT, "compute" },
            { VK_QUEUE_TRANSFER_BIT, "transfer" },
            { VK_QUEUE_SPARSE_BINDING_BIT, "sparse binding" }
          };

          vkGetPhysicalDeviceProperties (devices[i], &props);
          vkGetPhysicalDeviceQueueFamilyProperties (devices[i], &n_queue_props, NULL);
          queue_props = g_newa (VkQueueFamilyProperties, n_queue_props);
          vkGetPhysicalDeviceQueueFamilyProperties (devices[i], &n_queue_props, queue_props);

          g_print ("Vulkan Device %u:\n", i);
          g_print ("    %s (%s)\n", props.deviceName, device_type[props.deviceType]);
          g_print ("    Vendor ID: 0x%Xu\n", props.vendorID);
          g_print ("    Device ID: 0x%Xu\n", props.deviceID);
          g_print ("    API version %u.%u.%u\n",
                   VK_VERSION_MAJOR (props.apiVersion),
                   VK_VERSION_MINOR (props.apiVersion),
                   VK_VERSION_PATCH (props.apiVersion));
          for (j = 0; j < n_queue_props; j++)
            {
              const char *sep = "";

              g_print ("    Queue %d: ", j);
              for (k = 0; k < G_N_ELEMENTS (queue_caps); k++)
                {
                  if (queue_props[j].queueFlags & queue_caps[k].bit)
                    {
                      g_print ("%s%s", sep, queue_caps[k].name);
                      sep = "/";
                    }
                }
              g_print ("\n");
            }
        }
    }

  for (i = first; i < last; i++)
    {
      uint32_t n_queue_props;
      vkGetPhysicalDeviceQueueFamilyProperties (devices[i], &n_queue_props, NULL);
      VkQueueFamilyProperties *queue_props = g_newa (VkQueueFamilyProperties, n_queue_props);
      vkGetPhysicalDeviceQueueFamilyProperties (devices[i], &n_queue_props, queue_props);
      for (j = 0; j < n_queue_props; j++)
        {
          if (queue_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
              GPtrArray *device_extensions;
              gboolean has_incremental_present;

              has_incremental_present = device_supports_incremental_present (devices[i]);

              device_extensions = g_ptr_array_new ();
              g_ptr_array_add (device_extensions, (gpointer) VK_KHR_SWAPCHAIN_EXTENSION_NAME);
              if (has_incremental_present)
                g_ptr_array_add (device_extensions, (gpointer) VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);

              GDK_DISPLAY_NOTE (display, VULKAN, g_print ("Using Vulkan device %u, queue %u\n", i, j));
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
                                                    device_extensions->len,
                                                    (const gchar * const *) device_extensions->pdata
                                                },
                                                NULL,
                                                &display->vk_device) != VK_SUCCESS)
                {
                  g_ptr_array_unref (device_extensions);
                  continue;
                }

              g_ptr_array_unref (device_extensions);

              display->vk_physical_device = devices[i];
              vkGetDeviceQueue(display->vk_device, j, 0, &display->vk_queue);
              display->vk_queue_family_index = j;
              return TRUE;
            }
        }
    }

  g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                       "Could not find a Vulkan device with the required features.");
  return FALSE;
}

static VkBool32 VKAPI_CALL
gdk_vulkan_debug_report (VkDebugReportFlagsEXT      flags,
                         VkDebugReportObjectTypeEXT objectType,
                         uint64_t                   object,
                         size_t                     location,
                         int32_t                    messageCode,
                         const char*                pLayerPrefix,
                         const char*                pMessage,
                         void*                      pUserData)
{
  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    g_critical ("Vulkan: %s: %s", pLayerPrefix, pMessage);
  else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    g_critical ("Vulkan: %s: %s", pLayerPrefix, pMessage);
  else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    g_warning ("Vulkan: %s: %s", pLayerPrefix, pMessage);
  else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    g_debug ("Vulkan: %s: %s", pLayerPrefix, pMessage);
  else
    g_info ("Vulkan: %s: %s", pLayerPrefix, pMessage);

  return VK_FALSE;
}

static gboolean
gdk_display_create_vulkan_instance (GdkDisplay  *display,
                                    GError     **error)
{
  uint32_t i;
  GPtrArray *used_extensions;
  GPtrArray *used_layers;
  gboolean validate = FALSE, have_debug_report = FALSE;
  VkResult res;

  if (GDK_DISPLAY_GET_CLASS (display)->vk_extension_name == NULL)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                   "The %s backend has no Vulkan support.", G_OBJECT_TYPE_NAME (display));
      return FALSE;
    }

  uint32_t n_extensions;
  GDK_VK_CHECK (vkEnumerateInstanceExtensionProperties, NULL, &n_extensions, NULL);
  VkExtensionProperties *extensions = g_newa (VkExtensionProperties, n_extensions);
  GDK_VK_CHECK (vkEnumerateInstanceExtensionProperties, NULL, &n_extensions, extensions);

  used_extensions = g_ptr_array_new ();
  g_ptr_array_add (used_extensions, (gpointer) VK_KHR_SURFACE_EXTENSION_NAME); 
  g_ptr_array_add (used_extensions, (gpointer) GDK_DISPLAY_GET_CLASS (display)->vk_extension_name); 

  for (i = 0; i < n_extensions; i++)
    {
      if (GDK_DISPLAY_DEBUG_CHECK (display, VULKAN))
        g_print ("Extension available: %s v%u.%u.%u\n",
                                 extensions[i].extensionName,
                                 VK_VERSION_MAJOR (extensions[i].specVersion),
                                 VK_VERSION_MINOR (extensions[i].specVersion),
                                 VK_VERSION_PATCH (extensions[i].specVersion));

      if (g_str_equal (extensions[i].extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
        {
          g_ptr_array_add (used_extensions, (gpointer) VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
          have_debug_report = TRUE;
        }
    }

  uint32_t n_layers;
  GDK_VK_CHECK (vkEnumerateInstanceLayerProperties, &n_layers, NULL);
  VkLayerProperties *layers = g_newa (VkLayerProperties, n_layers);
  GDK_VK_CHECK (vkEnumerateInstanceLayerProperties, &n_layers, layers);

  used_layers = g_ptr_array_new ();

  for (i = 0; i < n_layers; i++)
    {
      if (GDK_DISPLAY_DEBUG_CHECK (display, VULKAN))
        g_print ("Layer available: %s v%u.%u.%u (%s)\n",
                                 layers[i].layerName,
                                 VK_VERSION_MAJOR (layers[i].specVersion),
                                 VK_VERSION_MINOR (layers[i].specVersion),
                                 VK_VERSION_PATCH (layers[i].specVersion),
                                 layers[i].description);
      if (GDK_DISPLAY_DEBUG_CHECK (display, VULKAN_VALIDATE) &&
          g_str_equal (layers[i].layerName, "VK_LAYER_LUNARG_standard_validation"))
        {
          g_ptr_array_add (used_layers, (gpointer) "VK_LAYER_LUNARG_standard_validation");
          validate = TRUE;
        }
    }

  if (GDK_DISPLAY_DEBUG_CHECK (display, VULKAN_VALIDATE) && !validate)
    {
      g_warning ("Vulkan validation layers were requested, but not found. Running without.");
    }

  res = GDK_VK_CHECK (vkCreateInstance, &(VkInstanceCreateInfo) {
                                             .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                             .pNext = NULL,
                                             .flags = 0,
                                             .pApplicationInfo = &(VkApplicationInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                                 .pNext = NULL,
                                                 .pApplicationName = g_get_application_name (),
                                                 .applicationVersion = 0,
                                                 .pEngineName = "GTK",
                                                 .engineVersion = VK_MAKE_VERSION (GDK_MAJOR_VERSION, GDK_MINOR_VERSION, GDK_MICRO_VERSION),
                                                 .apiVersion = VK_API_VERSION_1_0
                                             },
                                             .enabledLayerCount = used_layers->len,
                                             .ppEnabledLayerNames = (const char * const *) used_layers->pdata,
                                             .enabledExtensionCount = used_extensions->len,
                                             .ppEnabledExtensionNames = (const char * const *) used_extensions->pdata
                                         },
                                         NULL,
                                         &display->vk_instance);
  g_ptr_array_free (used_layers, TRUE);
  g_ptr_array_free (used_extensions, TRUE);

  if (res != VK_SUCCESS)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                   "Could not create a Vulkan instance: %s", gdk_vulkan_strerror (res));
      return FALSE;
    }

  if (have_debug_report)
    {
      PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
      
      vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr (display->vk_instance, "vkCreateDebugReportCallbackEXT" );
      GDK_VK_CHECK (vkCreateDebugReportCallbackEXT, display->vk_instance,
                                                    &(VkDebugReportCallbackCreateInfoEXT) {
                                                        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
                                                        .pNext = NULL,
                                                        .flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT
                                                               | VK_DEBUG_REPORT_WARNING_BIT_EXT
                                                               | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
                                                               | VK_DEBUG_REPORT_ERROR_BIT_EXT
                                                               | VK_DEBUG_REPORT_DEBUG_BIT_EXT,
                                                        .pfnCallback = gdk_vulkan_debug_report,
                                                        .pUserData = NULL
                                                    },
                                                    NULL,
                                                    &display->vk_debug_callback);
    }

  if (!gdk_display_create_vulkan_device (display, error))
    {
      if (display->vk_debug_callback != VK_NULL_HANDLE)
        {
          PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;

          vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr (display->vk_instance, "vkDestroyDebugReportCallbackEXT");
          vkDestroyDebugReportCallbackEXT (display->vk_instance,
                                           display->vk_debug_callback,
                                           NULL);
          display->vk_debug_callback = VK_NULL_HANDLE;
        }
      vkDestroyInstance (display->vk_instance, NULL);
      display->vk_instance = VK_NULL_HANDLE;
      return FALSE;
    }

  return TRUE;
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
  display->vk_device = VK_NULL_HANDLE;
  if (display->vk_debug_callback != VK_NULL_HANDLE)
    {
      PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;

      vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr (display->vk_instance, "vkDestroyDebugReportCallbackEXT");
      vkDestroyDebugReportCallbackEXT (display->vk_instance,
                                       display->vk_debug_callback,
                                       NULL);
      display->vk_debug_callback = VK_NULL_HANDLE;
    }
  vkDestroyInstance (display->vk_instance, NULL);
  display->vk_instance = VK_NULL_HANDLE;
}

#else /* GDK_RENDERING_VULKAN */

static void
gdk_vulkan_context_class_init (GdkVulkanContextClass *klass)
{
  signals[IMAGES_UPDATED] =
    g_signal_new (g_intern_static_string ("images-updated"),
		  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
gdk_vulkan_context_init (GdkVulkanContext *self)
{
}

static void
gdk_vulkan_context_initable_init (GInitableIface *iface)
{
}

#endif /* GDK_RENDERING_VULKAN */
