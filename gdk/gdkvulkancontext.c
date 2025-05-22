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

#ifdef __APPLE__
#define VK_ENABLE_BETA_EXTENSIONS
#endif

#include "gdkvulkancontext.h"

#include "gdkvulkancontextprivate.h"

#include "gdkdebugprivate.h"
#include "gdkdmabufformatsbuilderprivate.h"
#include "gdkdmabuffourccprivate.h"
#include "gdkdmabuftextureprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkprofilerprivate.h"
#include "gdkmemorytexture.h"
#include <glib/gi18n-lib.h>
#include <math.h>

#ifdef GDK_RENDERING_VULKAN
const GdkDebugKey gdk_vulkan_feature_keys[] = {
  { "dmabuf", GDK_VULKAN_FEATURE_DMABUF, "Never import Dmabufs" },
  { "win32", GDK_VULKAN_FEATURE_WIN32, "Never import Windows resources" },
  { "ycbcr", GDK_VULKAN_FEATURE_YCBCR, "Do not support Ycbcr textures (also disables dmabufs)" },
  { "timeline-semaphore", GDK_VULKAN_FEATURE_TIMELINE_SEMAPHORE, "Disable timeline semaphore support (disables Windows sync)"},
  { "semaphore-export", GDK_VULKAN_FEATURE_SEMAPHORE_EXPORT, "Disable sync of exported dmabufs" },
  { "semaphore-import", GDK_VULKAN_FEATURE_SEMAPHORE_IMPORT, "Disable sync of imported dmabufs" },
  { "win32-semaphore", GDK_VULKAN_FEATURE_WIN32_SEMAPHORE, "Disable Windows sync support" },
  { "incremental-present", GDK_VULKAN_FEATURE_INCREMENTAL_PRESENT, "Do not send damage regions" },
  { "swapchain-maintenance", GDK_VULKAN_FEATURE_SWAPCHAIN_MAINTENANCE, "Do not use advanced swapchain features" },
  { "portability-subset", GDK_VULKAN_FEATURE_PORTABILITY_SUBSET, "Vulkan implementation is non-conformant" },
};
#endif

/**
 * GdkVulkanContext:
 *
 * Represents the platform-specific Vulkan draw context.
 *
 * `GdkVulkanContext`s are created for a surface using
 * [method@Gdk.Surface.create_vulkan_context], and the context will match
 * the characteristics of the surface.
 *
 * Support for `GdkVulkanContext` is platform-specific and context creation
 * can fail, returning %NULL context.
 *
 * Deprecated: 4.14: GTK does not expose any Vulkan internals. This
 *   struct is a leftover that was accidentally exposed.
 */

typedef struct _GdkVulkanContextPrivate GdkVulkanContextPrivate;

struct _GdkVulkanContextPrivate {
#ifdef GDK_RENDERING_VULKAN
  VkSurfaceKHR surface;
  struct {
    VkSurfaceFormatKHR vk_format;
    GdkMemoryFormat gdk_format;
  } formats[GDK_N_DEPTHS];
  GdkMemoryDepth current_depth;

  VkSwapchainKHR swapchain;

  guint n_images;
  VkImage *images;
  cairo_region_t **regions;
#endif

  guint32 draw_index;

  guint vulkan_ref: 1;
};

enum {
  IMAGES_UPDATED,

  LAST_SIGNAL
};

/**
 * gdk_vulkan_error_quark:
 *
 * Registers an error quark for [class@Gdk.VulkanContext] errors.
 *
 * Returns: the error quark
 **/
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
   * the GTK developers (or you are a GTK developer) and have
   * encountered a newly added Vulkan error message.
   * You want to add it to this enum now.
   *
   * Because the Vulkan people don't make adding this too easy, here's
   * the process to manage it:
   * 1. go to
   *    https://github.com/KhronosGroup/Vulkan-Headers/blob/main/include/vulkan/vulkan_core.h
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
      return "Command successfully completed. (VK_SUCCESS)";
    case VK_NOT_READY:
      return "A fence or query has not yet completed. (VK_NOT_READY)";
    case VK_TIMEOUT:
      return "A wait operation has not completed in the specified time. (VK_TIMEOUT)";
    case VK_EVENT_SET:
      return "An event is signaled. (VK_EVENT_SET)";
    case VK_EVENT_RESET:
      return "An event is unsignaled. (VK_EVENT_RESET)";
    case VK_INCOMPLETE:
      return "A return array was too small for the result. (VK_INCOMPLETE)";
    case VK_SUBOPTIMAL_KHR:
      return "A swapchain no longer matches the surface properties exactly, but can still be used to present to the surface successfully. (VK_SUBOPTIMAL_KHR)";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "A host memory allocation has failed. (VK_ERROR_OUT_OF_HOST_MEMORY)";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "A device memory allocation has failed. (VK_ERROR_OUT_OF_DEVICE_MEMORY)";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "Initialization of an object could not be completed for implementation-specific reasons. (VK_ERROR_INITIALIZATION_FAILED)";
    case VK_ERROR_DEVICE_LOST:
      return "The logical or physical device has been lost. (VK_ERROR_DEVICE_LOST)";
    case VK_ERROR_MEMORY_MAP_FAILED:
      return "Mapping of a memory object has failed. (VK_ERROR_MEMORY_MAP_FAILED)";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "A requested layer is not present or could not be loaded. (VK_ERROR_LAYER_NOT_PRESENT)";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "A requested extension is not supported. (VK_ERROR_EXTENSION_NOT_PRESENT)";
    case VK_ERROR_FEATURE_NOT_PRESENT:
      return "A requested feature is not supported. (VK_ERROR_FEATURE_NOT_PRESENT)";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "The requested version of Vulkan is not supported by the driver or is otherwise incompatible for implementation-specific reasons. (VK_ERROR_INCOMPATIBLE_DRIVER)";
    case VK_ERROR_TOO_MANY_OBJECTS:
      return "Too many objects of the type have already been created. (VK_ERROR_TOO_MANY_OBJECTS)";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "A requested format is not supported on this device. (VK_ERROR_FORMAT_NOT_SUPPORTED)";
#if VK_HEADER_VERSION >= 24
    case VK_ERROR_FRAGMENTED_POOL:
      return "A requested pool allocation has failed due to fragmentation of the pool’s memory. (VK_ERROR_FRAGMENTED_POOL)";
#endif
    case VK_ERROR_SURFACE_LOST_KHR:
      return "A surface is no longer available. (VK_ERROR_SURFACE_LOST_KHR)";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
      return "The requested window is already in use by Vulkan or another API in a manner which prevents it from being used again. (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)";
    case VK_ERROR_OUT_OF_DATE_KHR:
      return "A surface has changed in such a way that it is no longer compatible with the swapchain. (VK_ERROR_OUT_OF_DATE_KHR)";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
      return "The display used by a swapchain does not use the same presentable image layout, or is incompatible in a way that prevents sharing an image. (VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)";
    case VK_ERROR_VALIDATION_FAILED_EXT:
      return "The application caused the validation layer to fail. (VK_ERROR_VALIDATION_FAILED_EXT)";
    case VK_ERROR_INVALID_SHADER_NV:
      return "One or more shaders failed to compile or link. (VK_ERROR_INVALID_SHADER_NV)";
#if VK_HEADER_VERSION >= 39
    case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
      return "A pool memory allocation has failed. (VK_ERROR_OUT_OF_POOL_MEMORY_KHR)";
#endif
#if VK_HEADER_VERSION >= 54
    case VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR:
      return "An external handle is not a valid handle of the specified type. (VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR)";
#endif
#if VK_HEADER_VERSION >= 64
    case VK_ERROR_NOT_PERMITTED_EXT:
      return "The caller does not have sufficient privileges. (VK_ERROR_NOT_PERMITTED_EXT)";
#endif
#if VK_HEADER_VERSION >= 72
    case VK_ERROR_FRAGMENTATION_EXT:
      return "A descriptor pool creation has failed due to fragmentation. (VK_ERROR_FRAGMENTATION_EXT)";
#endif
#if VK_HEADER_VERSION >= 89
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
      return "Invalid DRM format modifier plane layout (VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)";
#endif
#if VK_HEADER_VERSION >= 97
    case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
      return "Invalid device address (VK_ERROR_INVALID_DEVICE_ADDRESS_EXT)";
#endif
#if VK_HEADER_VERSION >= 105
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
      return "An operation on a swapchain created with VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT failed as it did not have exclusive full-screen access. (VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)";
#endif
#if VK_HEADER_VERSION >= 131
    case VK_ERROR_UNKNOWN:
      return "An unknown error has occurred; either the application has provided invalid input, or an implementation failure has occurred. (VK_ERROR_UNKNOWN)";
#endif
#if VK_HEADER_VERSION >= 135
#if VK_HEADER_VERSION < 162
    case VK_ERROR_INCOMPATIBLE_VERSION_KHR:
      return "This error was removed by the Vulkan gods. (VK_ERROR_INCOMPATIBLE_VERSION_KHR)";
#endif
    case VK_THREAD_IDLE_KHR:
      return "A deferred operation is not complete but there is currently no work for this thread to do at the time of this call. (VK_THREAD_IDLE_KHR)";
    case VK_THREAD_DONE_KHR:
      return "A deferred operation is not complete but there is no work remaining to assign to additional threads. (VK_THREAD_DONE_KHR)";
    case VK_OPERATION_DEFERRED_KHR:
      return "A deferred operation was requested and at least some of the work was deferred. (VK_OPERATION_DEFERRED_KHR)";
    case VK_OPERATION_NOT_DEFERRED_KHR:
      return "A deferred operation was requested and no operations were deferred. (VK_OPERATION_NOT_DEFERRED_KHR)";
    case VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT:
      return "A requested pipeline creation would have required compilation, but the application requested compilation to not be performed. (VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT)";
#endif
#if VK_HEADER_VERSION >= 213
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
      return "An image creation failed because internal resources required for compression are exhausted. (VK_ERROR_COMPRESSION_EXHAUSTED_EXT)";
#endif
#if VK_HEADER_VERSION < 140
    case VK_RESULT_RANGE_SIZE:
#endif
#if VK_HEADER_VERSION >= 238
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
      return "The requested VkImageUsageFlags are not supported. (VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR)";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
      return "The requested video picture layout is not supported. (VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR)";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
      return "A video profile operation specified via VkVideoProfileInfoKHR::videoCodecOperation is not supported. (VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
      return "Format parameters in a requested VkVideoProfileInfoKHR chain are not supported. (VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR)";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
      return "Codec-specific parameters in a requested (VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR)";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
      return "The specified video Std header version is not supported. (VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR)";
#endif
#if VK_HEADER_VERSION >= 246
    case VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT:
      return "The provided binary shader code is not compatible with this device. (VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT)";
#endif
#if VK_HEADER_VERSION >= 274
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
      return "The specified Video Std parameters do not adhere to the syntactic or semantic requirements of the used video compression standard or implementation. (VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR)";
#endif
#if VK_HEADER_VERSION >= 294
    case VK_PIPELINE_BINARY_MISSING_KHR:
      return "The application attempted to create a pipeline binary by querying an internal cache, but the internal cache entry did not exist. (VK_PIPELINE_BINARY_MISSING_KHR)";
    case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
      return "The application did not provide enough space to return all the required data. (VK_ERROR_NOT_ENOUGH_SPACE_KHR)";
#endif

    case VK_RESULT_MAX_ENUM:
    default:
      return "Unknown Vulkan error.";
  }
}

static const char *
surface_present_mode_to_string (VkPresentModeKHR present_mode)
{
  switch (present_mode)
    {
    case VK_PRESENT_MODE_MAILBOX_KHR:
      return "VK_PRESENT_MODE_MAILBOX_KHR";
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
      return "VK_PRESENT_MODE_IMMEDIATE_KHR";
    case VK_PRESENT_MODE_FIFO_KHR:
      return "VK_PRESENT_MODE_FIFO_KHR";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
      return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
#if VK_HEADER_VERSION >= 304
    case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT:
      return "VK_PRESENT_MODE_FIFO_LATEST_READY_EXT";
#endif
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
    case VK_PRESENT_MODE_MAX_ENUM_KHR:
    default:
      return "(invalid)";
    }

  return "(unknown)";
}

static gboolean
gdk_vulkan_context_has_feature (GdkVulkanContext  *self,
                                GdkVulkanFeatures  feature)
{
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (self));

  return (display->vulkan_features & feature) ? TRUE : FALSE;
}

static const VkPresentModeKHR preferred_present_modes[] = {
  VK_PRESENT_MODE_MAILBOX_KHR,
  VK_PRESENT_MODE_IMMEDIATE_KHR,
};

static VkPresentModeKHR
find_best_surface_present_mode (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
  VkPresentModeKHR *available_present_modes;
  uint32_t n_present_modes;
  VkResult res;

  res = GDK_VK_CHECK (vkGetPhysicalDeviceSurfacePresentModesKHR, gdk_vulkan_context_get_physical_device (context),
                                                                 priv->surface,
                                                                 &n_present_modes,
                                                                 NULL);

  if (res != VK_SUCCESS)
    goto fallback_present_mode;

  available_present_modes = g_alloca (sizeof (VkPresentModeKHR) * n_present_modes);
  res = GDK_VK_CHECK (vkGetPhysicalDeviceSurfacePresentModesKHR, gdk_vulkan_context_get_physical_device (context),
                                                                 priv->surface,
                                                                 &n_present_modes,
                                                                 available_present_modes);

  if (res != VK_SUCCESS)
    goto fallback_present_mode;

  for (uint32_t i = 0; i < G_N_ELEMENTS (preferred_present_modes); i++)
    {
      for (uint32_t j = 0; j < n_present_modes; j++)
        {
          if (preferred_present_modes[i] == available_present_modes[j])
            return available_present_modes[j];
        }
    }

fallback_present_mode:
  return VK_PRESENT_MODE_FIFO_KHR;
}

static void
gdk_vulkan_context_dispose (GObject *gobject)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (gobject);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
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
  VkPresentModeKHR present_mode;
  VkSwapchainKHR new_swapchain;
  VkResult res;
  VkDevice device;
  VkExtent2D size;
  guint i;

  GDK_DEBUG (VULKAN, "(Re)creating the swapchain for surface of size %dx%d",
             gdk_surface_get_width (surface),
             gdk_surface_get_height (surface));

  device = gdk_vulkan_context_get_device (context);

  /*
   * Wait for device to be idle because this function is also called in window resizes.
   * And if we destroy old swapchain it also destroy the old VkImages, those images could
   * be in use by a vulkan render.
   */
  vkDeviceWaitIdle (device);

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
      GDK_DISPLAY_DEBUG (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context)), VULKAN,
                        "Vulkan swapchain doesn't do transparency. Using opaque swapchain instead.");
      composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

  present_mode = find_best_surface_present_mode (context);

  GDK_DEBUG (VULKAN, "Using surface present mode %s",
             surface_present_mode_to_string (present_mode));

  gdk_draw_context_get_buffer_size (GDK_DRAW_CONTEXT (context), &size.width, &size.height);

  /*
   * Per https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#VkSurfaceCapabilitiesKHR
   * the current extent may assume a special value, meaning that the extent should assume whatever
   * value the surface has.
   */
  if (capabilities.currentExtent.width == -1 || capabilities.currentExtent.height == -1)
    {
      GDK_DEBUG (VULKAN, "GTK selecting swapchain size as %ux%u",
                 (guint) size.width, (guint) size.height);
    }
  else if (capabilities.currentExtent.width == size.width &&
           capabilities.currentExtent.height == size.height)
    {
      GDK_DEBUG (VULKAN, "Vulkan and GTK agree on size as %ux%u",
                 (guint) size.width, (guint) size.height);
    }
  else
    {
      GDK_DEBUG (VULKAN, "Vulkan %ux%u and GTK %ux%u disagree on size, using GTK's",
                 (guint) capabilities.currentExtent.width, (guint) capabilities.currentExtent.height,
                 (guint) size.width, (guint) size.height);
    }

  if (capabilities.minImageExtent.width > size.width ||
      capabilities.minImageExtent.height > size.height ||
      capabilities.maxImageExtent.width < size.width ||
      capabilities.maxImageExtent.height < size.height)
    {
      /* Should this be a g_warning()? */
      GDK_DEBUG (VULKAN, "Clamping size %ux%u to fit into min %ux%u, max %ux%u",
                 (guint) size.width, (guint) size.height,
                 (guint) capabilities.minImageExtent.width, (guint) capabilities.minImageExtent.height,
                 (guint) capabilities.maxImageExtent.width, (guint) capabilities.maxImageExtent.height);
      size.width = CLAMP (size.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      size.height = CLAMP (size.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
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
                                                .imageFormat = priv->formats[priv->current_depth].vk_format.format,
                                                .imageColorSpace = priv->formats[priv->current_depth].vk_format.colorSpace,
                                                .imageExtent = size,
                                                .imageArrayLayers = 1,
                                                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                .queueFamilyIndexCount = 1,
                                                .pQueueFamilyIndices = (uint32_t[1]) {
                                                    gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context))->vk_queue_family_index
                                                },
                                                .preTransform = capabilities.currentTransform,
                                                .compositeAlpha = composite_alpha,
                                                .presentMode = present_mode,
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
                                                                size.width,
                                                                size.height
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
physical_device_supports_extension (VkPhysicalDevice  device,
                                    const char       *extension_name)
{
  VkExtensionProperties *extensions;
  uint32_t n_device_extensions;
  static gboolean first = TRUE;

  GDK_VK_CHECK (vkEnumerateDeviceExtensionProperties, device, NULL, &n_device_extensions, NULL);

  extensions = g_newa (VkExtensionProperties, n_device_extensions);
  GDK_VK_CHECK (vkEnumerateDeviceExtensionProperties, device, NULL, &n_device_extensions, extensions);

  if (first)
    {
      first = FALSE;
      for (uint32_t i = 0; i < n_device_extensions; i++)
        GDK_DEBUG (VULKAN, "%s", extensions[i].extensionName);
    }

  for (uint32_t i = 0; i < n_device_extensions; i++)
    {
      if (g_str_equal (extensions[i].extensionName, extension_name))
        return TRUE;
    }

  return FALSE;
}

static GdkVulkanFeatures
physical_device_check_features (VkPhysicalDevice device)
{
  VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maintenance1_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT,
  };
  VkPhysicalDeviceVulkan12Features v12_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &swapchain_maintenance1_features
  };
  VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
      .pNext = &v12_features
  };
  VkPhysicalDeviceFeatures2 v10_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &ycbcr_features
  };
  VkExternalSemaphoreProperties semaphore_props = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
  };
  GdkVulkanFeatures features;

  vkGetPhysicalDeviceFeatures2 (device, &v10_features);
  vkGetPhysicalDeviceExternalSemaphoreProperties (device,
                                                  &(VkPhysicalDeviceExternalSemaphoreInfo) {
		                                      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
		                                      .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
	                                          },
                                                  &semaphore_props);

  features = 0;

  if (ycbcr_features.samplerYcbcrConversion ||
      physical_device_supports_extension (device, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME))
    features |= GDK_VULKAN_FEATURE_YCBCR;

  if (physical_device_supports_extension (device, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) &&
      physical_device_supports_extension (device, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME) &&
      physical_device_supports_extension (device, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME))
    features |= GDK_VULKAN_FEATURE_DMABUF;

  if (v12_features.timelineSemaphore ||
      physical_device_supports_extension (device, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME))
    features |= GDK_VULKAN_FEATURE_TIMELINE_SEMAPHORE;

#ifdef GDK_WINDOWING_WIN32
  if (physical_device_supports_extension (device, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME))
    features |= GDK_VULKAN_FEATURE_WIN32;

  if ((features & GDK_VULKAN_FEATURE_TIMELINE_SEMAPHORE) &&
      physical_device_supports_extension (device, VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME))
    features |= GDK_VULKAN_FEATURE_WIN32_SEMAPHORE;
#endif

  if (physical_device_supports_extension (device, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME))
    {
      if (semaphore_props.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT)
        features |= GDK_VULKAN_FEATURE_SEMAPHORE_EXPORT;

      if (semaphore_props.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT)
        features |= GDK_VULKAN_FEATURE_SEMAPHORE_IMPORT;
    }

  if (physical_device_supports_extension (device, VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME))
    features |= GDK_VULKAN_FEATURE_INCREMENTAL_PRESENT;

  if (swapchain_maintenance1_features.swapchainMaintenance1 ||
      physical_device_supports_extension (device, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME))
    features |= GDK_VULKAN_FEATURE_SWAPCHAIN_MAINTENANCE;

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
  if (physical_device_supports_extension (device, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
    features |= GDK_VULKAN_FEATURE_PORTABILITY_SUBSET;
#endif

  return features;
}

static void
gdk_vulkan_context_begin_frame (GdkDrawContext  *draw_context,
                                gpointer         context_data,
                                GdkMemoryDepth   depth,
                                cairo_region_t  *region,
                                GdkColorState  **out_color_state,
                                GdkMemoryDepth  *out_depth)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (draw_context);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  GdkColorState *color_state;
  VkResult acquire_result;
  VkSemaphore draw_semaphore;
  guint i;

  g_assert (context_data != NULL);
  draw_semaphore = *(VkSemaphore *) context_data;

  color_state = gdk_surface_get_color_state (surface);
  depth = gdk_memory_depth_merge (depth, gdk_color_state_get_depth (color_state));

  g_assert (depth != GDK_MEMORY_U8_SRGB || gdk_color_state_get_no_srgb_tf (color_state) != NULL);

  if (depth != priv->current_depth && depth != GDK_MEMORY_NONE)
    {
      if (priv->formats[depth].vk_format.format != priv->formats[priv->current_depth].vk_format.format ||
          priv->formats[depth].vk_format.colorSpace != priv->formats[priv->current_depth].vk_format.colorSpace)
        {
          GdkMemoryDepth old_depth = priv->current_depth;
          GError *error = NULL;

          priv->current_depth = depth;
          if (!gdk_vulkan_context_check_swapchain (context, &error))
            {
              g_warning ("%s", error->message);
              g_error_free (error);
              priv->current_depth = old_depth;
            }
        }
    }
  for (i = 0; i < priv->n_images; i++)
    {
      cairo_region_union (priv->regions[i], region);
    }

  while (TRUE)
    {
      acquire_result = GDK_VK_CHECK (vkAcquireNextImageKHR, gdk_vulkan_context_get_device (context),
                                                            priv->swapchain,
                                                            UINT64_MAX,
                                                            draw_semaphore,
                                                            VK_NULL_HANDLE,
                                                            &priv->draw_index);
      if ((acquire_result == VK_ERROR_OUT_OF_DATE_KHR) ||
          (acquire_result == VK_SUBOPTIMAL_KHR))
        {
          GError *error = NULL;

          if (acquire_result == VK_SUBOPTIMAL_KHR)
            {
              const VkPipelineStageFlags mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

              vkQueueSubmit (gdk_vulkan_context_get_queue (context),
                             1,
                             &(VkSubmitInfo) {
                               .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .waitSemaphoreCount = 1,
                               .pWaitSemaphores = &draw_semaphore,
                               .pWaitDstStageMask = &mask,
                             },
                             VK_NULL_HANDLE);
              vkQueueWaitIdle (gdk_vulkan_context_get_queue (context));

              if (gdk_vulkan_context_has_feature (context, GDK_VULKAN_FEATURE_SWAPCHAIN_MAINTENANCE))
                {
                  PFN_vkReleaseSwapchainImagesEXT vkReleaseSwapchainImagesEXT;

                  vkReleaseSwapchainImagesEXT = (PFN_vkReleaseSwapchainImagesEXT) vkGetDeviceProcAddr (gdk_vulkan_context_get_device (context), "vkReleaseSwapchainImagesEXT");

                  vkReleaseSwapchainImagesEXT (gdk_vulkan_context_get_device (context),
                                            &(VkReleaseSwapchainImagesInfoEXT) {
                                              .sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT,
                                              .pNext = NULL,
                                              .swapchain = priv->swapchain,
                                              .imageIndexCount = 1,
                                              .pImageIndices = &priv->draw_index,
                                            });
                }
            }

          if (gdk_vulkan_context_check_swapchain (context, &error))
            continue;

          g_warning ("%s", error->message);
          g_error_free (error);
        }

      break;
    }

  cairo_region_union (region, priv->regions[priv->draw_index]);

  if (priv->current_depth == GDK_MEMORY_U8_SRGB)
    *out_color_state = gdk_color_state_get_no_srgb_tf (color_state);
  else
    *out_color_state = color_state;
  *out_depth = priv->current_depth;
}

static void
gdk_vulkan_context_end_frame (GdkDrawContext *draw_context,
                              gpointer        context_data,
                              cairo_region_t *painted)
{
  GdkVulkanContext *context = GDK_VULKAN_CONTEXT (draw_context);
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);
  VkPresentRegionsKHR present_regions;
  VkPresentRegionKHR present_region;
  VkSwapchainPresentFenceInfoEXT fence_info;
  void *pNext = NULL;

  g_assert (context_data != NULL);

  if (gdk_vulkan_context_has_feature (context, GDK_VULKAN_FEATURE_SWAPCHAIN_MAINTENANCE))
    {
      fence_info = (VkSwapchainPresentFenceInfoEXT) {
          .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
          .pNext = pNext,
          .swapchainCount = 1,
          .pFences = context_data,
      };
      pNext = &fence_info;
    }

  if (gdk_vulkan_context_has_feature (context, GDK_VULKAN_FEATURE_INCREMENTAL_PRESENT))
    {
      present_regions = (VkPresentRegionsKHR) {
          .sType = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR,
          .pNext = pNext,
          .swapchainCount = 1,
          .pRegions = &present_region
      };
      pNext = &present_regions;

      present_region.rectangleCount = cairo_region_num_rectangles (painted);
      present_region.pRectangles = g_alloca (sizeof (VkRectLayerKHR) * present_region.rectangleCount);

      for (int i = 0; i < present_region.rectangleCount; i++)
        {
          cairo_rectangle_int_t r;

          cairo_region_get_rectangle (painted, i, &r);

          ((VkRectLayerKHR *) present_region.pRectangles)[i] = (VkRectLayerKHR) {
              .offset.x = r.x,
              .offset.y = r.y,
              .extent.width = r.width,
              .extent.height = r.height
          };
        }
    }

  GDK_VK_CHECK (vkQueuePresentKHR, gdk_vulkan_context_get_queue (context),
                                   &(VkPresentInfoKHR) {
                                       .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                       .waitSemaphoreCount = 0,
                                       .pWaitSemaphores = NULL,
                                       .swapchainCount = 1,
                                       .pSwapchains = (VkSwapchainKHR[]) {
                                           priv->swapchain
                                       },
                                       .pImageIndices = (uint32_t[]) {
                                           priv->draw_index
                                       },
                                       .pNext = pNext,
                                   });

  cairo_region_destroy (priv->regions[priv->draw_index]);
  priv->regions[priv->draw_index] = cairo_region_create ();

  if (!gdk_vulkan_context_has_feature (context, GDK_VULKAN_FEATURE_SWAPCHAIN_MAINTENANCE))
    {
      GDK_VK_CHECK (vkQueueSubmit, gdk_vulkan_context_get_queue (context),
                                   0, NULL,
                                   *(VkFence *) context_data);
    }
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
   * Emitted when the images managed by this context have changed.
   *
   * Usually this means that the swapchain had to be recreated,
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
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
  VkResult res;
  VkBool32 supported;
  uint32_t i;

  if (!gdk_display_prepare_vulkan (display, error))
    return FALSE;

  if (surface == NULL)
    {
      for (i = 0; i < G_N_ELEMENTS (priv->formats); i++)
        {
          priv->formats[i].vk_format.format = VK_FORMAT_B8G8R8A8_UNORM;
          priv->formats[i].vk_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
          priv->formats[i].gdk_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
        }
      return TRUE;
    }

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
          if (formats[i].colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
              formats[i].colorSpace != VK_COLOR_SPACE_PASS_THROUGH_EXT)
            continue;

          switch ((int) formats[i].format)
            {
              case VK_FORMAT_B8G8R8A8_UNORM:
                if (priv->formats[GDK_MEMORY_U8].vk_format.format == VK_FORMAT_UNDEFINED ||
                    (formats[i].colorSpace == VK_COLOR_SPACE_PASS_THROUGH_EXT &&
                     priv->formats[GDK_MEMORY_U8].vk_format.colorSpace != VK_COLOR_SPACE_PASS_THROUGH_EXT))
                  {
                    priv->formats[GDK_MEMORY_U8].vk_format = formats[i];
                    priv->formats[GDK_MEMORY_U8].gdk_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
                  };
                break;

              case VK_FORMAT_R8G8B8A8_UNORM:
                if (priv->formats[GDK_MEMORY_U8].vk_format.format == VK_FORMAT_UNDEFINED ||
                    (formats[i].colorSpace == VK_COLOR_SPACE_PASS_THROUGH_EXT &&
                     priv->formats[GDK_MEMORY_U8].vk_format.colorSpace != VK_COLOR_SPACE_PASS_THROUGH_EXT))
                  {
                    priv->formats[GDK_MEMORY_U8].vk_format = formats[i];
                    priv->formats[GDK_MEMORY_U8].gdk_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
                  }
                break;

              case VK_FORMAT_B8G8R8A8_SRGB:
                if (priv->formats[GDK_MEMORY_U8_SRGB].vk_format.format == VK_FORMAT_UNDEFINED ||
                    (formats[i].colorSpace == VK_COLOR_SPACE_PASS_THROUGH_EXT &&
                     priv->formats[GDK_MEMORY_U8_SRGB].vk_format.colorSpace != VK_COLOR_SPACE_PASS_THROUGH_EXT))
                  {
                    priv->formats[GDK_MEMORY_U8_SRGB].vk_format = formats[i];
                    priv->formats[GDK_MEMORY_U8_SRGB].gdk_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
                  }
                break;

              case VK_FORMAT_R8G8B8A8_SRGB:
                if (priv->formats[GDK_MEMORY_U8_SRGB].vk_format.format == VK_FORMAT_UNDEFINED ||
                    (formats[i].colorSpace == VK_COLOR_SPACE_PASS_THROUGH_EXT &&
                     priv->formats[GDK_MEMORY_U8_SRGB].vk_format.colorSpace != VK_COLOR_SPACE_PASS_THROUGH_EXT))
                  {
                    priv->formats[GDK_MEMORY_U8_SRGB].vk_format = formats[i];
                    priv->formats[GDK_MEMORY_U8_SRGB].gdk_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
                  }
                break;

              case VK_FORMAT_R16G16B16A16_UNORM:
                if (priv->formats[GDK_MEMORY_U16].vk_format.colorSpace != VK_COLOR_SPACE_PASS_THROUGH_EXT)
                  {
                    priv->formats[GDK_MEMORY_U16].vk_format = formats[i];
                    priv->formats[GDK_MEMORY_U16].gdk_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
                  }
                break;

              case VK_FORMAT_R16G16B16A16_SFLOAT:
                if (priv->formats[GDK_MEMORY_FLOAT16].vk_format.colorSpace != VK_COLOR_SPACE_PASS_THROUGH_EXT)
                  {
                    priv->formats[GDK_MEMORY_FLOAT16].vk_format = formats[i];
                    priv->formats[GDK_MEMORY_FLOAT16].gdk_format = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
                  }
                break;

              case VK_FORMAT_R32G32B32A32_SFLOAT:
                if (priv->formats[GDK_MEMORY_FLOAT32].vk_format.colorSpace != VK_COLOR_SPACE_PASS_THROUGH_EXT)
                  {
                    priv->formats[GDK_MEMORY_FLOAT32].vk_format = formats[i];
                    priv->formats[GDK_MEMORY_FLOAT32].gdk_format = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
                  }
                break;

              default:
                break;
            }
        }
      if (priv->formats[GDK_MEMORY_U8].vk_format.format == VK_FORMAT_UNDEFINED)
        {
          g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                               "No supported image format found.");
          goto out_surface;
        }
      /* Ensure all the formats exist:
       * - If a format was found, keep that one.
       * - FLOAT32 chooses the best format we have.
       * - FLOAT16 and U16 pick the format FLOAT32 uses
       */
      if (priv->formats[GDK_MEMORY_FLOAT32].vk_format.format == VK_FORMAT_UNDEFINED)
        {
          if (priv->formats[GDK_MEMORY_FLOAT16].vk_format.format != VK_FORMAT_UNDEFINED)
            priv->formats[GDK_MEMORY_FLOAT32] = priv->formats[GDK_MEMORY_FLOAT16];
          else if (priv->formats[GDK_MEMORY_U16].vk_format.format != VK_FORMAT_UNDEFINED)
            priv->formats[GDK_MEMORY_FLOAT32] = priv->formats[GDK_MEMORY_U16];
          else
            priv->formats[GDK_MEMORY_FLOAT32] = priv->formats[GDK_MEMORY_U8];
        }
      if (priv->formats[GDK_MEMORY_FLOAT16].vk_format.format == VK_FORMAT_UNDEFINED)
        priv->formats[GDK_MEMORY_FLOAT16] = priv->formats[GDK_MEMORY_FLOAT32];
      if (priv->formats[GDK_MEMORY_U16].vk_format.format == VK_FORMAT_UNDEFINED)
        priv->formats[GDK_MEMORY_U16] = priv->formats[GDK_MEMORY_FLOAT32];
      priv->formats[GDK_MEMORY_NONE] = priv->formats[GDK_MEMORY_U8];

      if (!gdk_vulkan_context_check_swapchain (context, error))
        goto out_surface;

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
 * @context: a `GdkVulkanContext`
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
 * @context: a `GdkVulkanContext`
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
 * @context: a `GdkVulkanContext`
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
 * @context: a `GdkVulkanContext`
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
 * @context: a `GdkVulkanContext`
 *
 * Gets the family index for the queue that this context is using.
 *
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

static char *
gdk_vulkan_get_pipeline_cache_dirname (void)
{
  return g_build_filename (g_get_user_cache_dir (), "gtk-4.0", "vulkan-pipeline-cache", NULL);
}

static GFile *
gdk_vulkan_get_pipeline_cache_file (GdkDisplay *display)
{
  VkPhysicalDeviceProperties props;
  char *dirname, *basename, *path;
  GFile *result;

  vkGetPhysicalDeviceProperties (display->vk_physical_device, &props);

  dirname = gdk_vulkan_get_pipeline_cache_dirname ();
  basename = g_strdup_printf ("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x"
                              "-%02x%02x%02x%02x%02x%02x.%u",
                              props.pipelineCacheUUID[0], props.pipelineCacheUUID[1],
                              props.pipelineCacheUUID[2], props.pipelineCacheUUID[3],
                              props.pipelineCacheUUID[4], props.pipelineCacheUUID[5],
                              props.pipelineCacheUUID[6], props.pipelineCacheUUID[7],
                              props.pipelineCacheUUID[8], props.pipelineCacheUUID[9],
                              props.pipelineCacheUUID[10], props.pipelineCacheUUID[11],
                              props.pipelineCacheUUID[12], props.pipelineCacheUUID[13],
                              props.pipelineCacheUUID[14], props.pipelineCacheUUID[15],
                              props.driverVersion);

  path = g_build_filename (dirname, basename, NULL);
  result = g_file_new_for_path (path);

  g_free (path);
  g_free (basename);
  g_free (dirname);

  return result;
}

static VkPipelineCache
gdk_display_load_pipeline_cache (GdkDisplay *display)
{
  GError *error = NULL;
  VkPipelineCache result;
  GFile *cache_file;
  char *etag, *data;
  gsize size;

  cache_file = gdk_vulkan_get_pipeline_cache_file (display);
  if (!g_file_load_contents (cache_file, NULL, &data, &size, &etag, &error))
    {
      GDK_DEBUG (VULKAN, "failed to load Vulkan pipeline cache file '%s': %s\n",
                 g_file_peek_path (cache_file), error->message);
      g_object_unref (cache_file);
      g_clear_error (&error);
      return VK_NULL_HANDLE;
    }

  if (GDK_VK_CHECK (vkCreatePipelineCache, display->vk_device,
                                           &(VkPipelineCacheCreateInfo) {
                                             .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                                             .initialDataSize = size,
                                             .pInitialData = data,
                                           },
                                           NULL,
                                           &result) != VK_SUCCESS)
    result = VK_NULL_HANDLE;

  g_object_unref (cache_file);
  g_free (data);
  g_free (display->vk_pipeline_cache_etag);
  display->vk_pipeline_cache_etag = etag;
  display->vk_pipeline_cache_size = size;

  return result;
}

static gboolean
gdk_vulkan_save_pipeline_cache (GdkDisplay *display)
{
  G_GNUC_UNUSED gint64 begin_time = GDK_PROFILER_CURRENT_TIME;
  GError *error = NULL;
  VkDevice device;
  VkPipelineCache cache;
  GFile *file;
  char *path;
  size_t size;
  char *data, *etag;

  device = display->vk_device;
  cache = display->vk_pipeline_cache;

  GDK_VK_CHECK (vkGetPipelineCacheData, device, cache, &size, NULL);
  if (size == 0)
    return TRUE;

  if (size == display->vk_pipeline_cache_size)
    {
      GDK_DEBUG (VULKAN, "pipeline cache size (%zu bytes) unchanged, skipping save", size);
      return TRUE;
    }

  data = g_malloc (size);
  if (GDK_VK_CHECK (vkGetPipelineCacheData, device, cache, &size, data) != VK_SUCCESS)
    {
      g_free (data);
      return FALSE;
    }

  path = gdk_vulkan_get_pipeline_cache_dirname ();
  if (g_mkdir_with_parents (path, 0755) != 0)
    {
      g_warning_once ("Failed to create pipeline cache directory");
      g_free (path);
      g_free (data);
      return FALSE;
    }
  g_free (path);

  file = gdk_vulkan_get_pipeline_cache_file (display);

  GDK_DEBUG (VULKAN, "Saving pipeline cache of size %" G_GSIZE_FORMAT " to %s", size, g_file_peek_path (file));

  if (!g_file_replace_contents (file,
                                data,
                                size,
                                display->vk_pipeline_cache_etag,
                                FALSE,
                                0,
                                &etag,
                                NULL,
                                &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WRONG_ETAG))
        {
          VkPipelineCache new_cache;

          GDK_DEBUG (VULKAN, "Pipeline cache file modified, merging into current");
          new_cache = gdk_display_load_pipeline_cache (display);
          if (new_cache)
            {
              GDK_VK_CHECK (vkMergePipelineCaches, device, cache, 1, &new_cache);
              vkDestroyPipelineCache (device, new_cache, NULL);
            }
          else
            {
              g_clear_pointer (&display->vk_pipeline_cache_etag, g_free);
            }
          g_clear_error (&error);
          g_object_unref (file);
          g_free (data);

          /* try again */
          return gdk_vulkan_save_pipeline_cache (display);
        }

      g_warning ("Failed to save pipeline cache: %s", error->message);
      g_clear_error (&error);
      g_object_unref (file);
      g_free (data);
      return FALSE;
    }

  gdk_profiler_end_markf (begin_time,
                          "Save Vulkan pipeline cache", "%s size %lu",
                          g_file_peek_path (file), size);

  g_object_unref (file);
  g_free (data);
  g_free (display->vk_pipeline_cache_etag);
  display->vk_pipeline_cache_etag = etag;
  display->vk_pipeline_cache_size = size;

  return TRUE;
}

static gboolean
gdk_vulkan_save_pipeline_cache_cb (gpointer data)
{
  GdkDisplay *display = data;

  gdk_vulkan_save_pipeline_cache (display);

  display->vk_save_pipeline_cache_source = 0;
  return G_SOURCE_REMOVE;
}

void
gdk_display_vulkan_pipeline_cache_updated (GdkDisplay *display)
{
  g_clear_handle_id (&display->vk_save_pipeline_cache_source, g_source_remove);
  display->vk_save_pipeline_cache_source = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT_IDLE - 10,
                                                                       10, /* random choice that is not now */
                                                                       gdk_vulkan_save_pipeline_cache_cb,
                                                                       display,
                                                                       NULL);
}

static void
gdk_display_create_pipeline_cache (GdkDisplay *display)
{
  display->vk_pipeline_cache = gdk_display_load_pipeline_cache (display);

  if (display->vk_pipeline_cache == VK_NULL_HANDLE)
    {
      GDK_VK_CHECK (vkCreatePipelineCache, display->vk_device,
                                           &(VkPipelineCacheCreateInfo) {
                                             .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                                           },
                                           NULL,
                                           &display->vk_pipeline_cache);
      GDK_DEBUG (VULKAN, "Creating empty pipeline cache");
    }
  else
    {
      GDK_DEBUG (VULKAN, "Loading pipeline cache (%" G_GSIZE_FORMAT " bytes)", display->vk_pipeline_cache_size);
    }
}

/**
 * gdk_vulkan_context_get_image_format:
 * @context: a `GdkVulkanContext`
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

  return priv->formats[priv->current_depth].vk_format.format;
}

GdkMemoryFormat
gdk_vulkan_context_get_memory_format (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), GDK_MEMORY_DEFAULT);

  return priv->formats[priv->current_depth].gdk_format;
}
/**
 * gdk_vulkan_context_get_n_images:
 * @context: a `GdkVulkanContext`
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
 * @context: a `GdkVulkanContext`
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
 * @context: a `GdkVulkanContext`
 *
 * Gets the index of the image that is currently being drawn.
 *
 * This function can only be used between [method@Gdk.DrawContext.begin_frame]
 * and [method@Gdk.DrawContext.end_frame] calls.
 *
 * Returns: the index of the images that is being drawn
 */
uint32_t
gdk_vulkan_context_get_draw_index (GdkVulkanContext *context)
{
  GdkVulkanContextPrivate *priv = gdk_vulkan_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), 0);

  return priv->draw_index;
}

static gboolean
gdk_display_create_vulkan_device (GdkDisplay  *display,
                                  GError     **error)
{
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  uint32_t i, j, k;
  GdkVulkanFeatures skip_features;

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

  skip_features = gdk_parse_debug_var ("GDK_VULKAN_DISABLE",
      "GDK_VULKAN_DISABLE can be set to a list of Vulkan features to disable.\n",
      gdk_vulkan_feature_keys,
      G_N_ELEMENTS (gdk_vulkan_feature_keys));
  if (skip_features & GDK_VULKAN_FEATURE_YCBCR)
    skip_features |= GDK_VULKAN_FEATURE_DMABUF;
  if (skip_features & GDK_VULKAN_FEATURE_TIMELINE_SEMAPHORE)
    skip_features |= GDK_VULKAN_FEATURE_WIN32_SEMAPHORE;

  if (GDK_DISPLAY_DEBUG_CHECK (display, VULKAN))
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

  for (i = 0; i < n_devices; i++)
    {
      GdkVulkanFeatures features, device_features;
      uint32_t n_queue_props;

      device_features = physical_device_check_features (devices[i]);

      features = device_features & ~skip_features;

      vkGetPhysicalDeviceQueueFamilyProperties (devices[i], &n_queue_props, NULL);
      VkQueueFamilyProperties *queue_props = g_newa (VkQueueFamilyProperties, n_queue_props);
      vkGetPhysicalDeviceQueueFamilyProperties (devices[i], &n_queue_props, queue_props);
      for (j = 0; j < n_queue_props; j++)
        {
          if (queue_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
              VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maintenance1_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT,
                .swapchainMaintenance1 = VK_TRUE,
              };
              gpointer create_device_pNext = NULL;
              GPtrArray *device_extensions;

              device_extensions = g_ptr_array_new ();
              g_ptr_array_add (device_extensions, (gpointer) VK_KHR_SWAPCHAIN_EXTENSION_NAME);
              if (features & GDK_VULKAN_FEATURE_YCBCR)
                {
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_MAINTENANCE_1_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
                }
              if (features & GDK_VULKAN_FEATURE_DMABUF)
                {
                  g_assert (features & GDK_VULKAN_FEATURE_YCBCR);

                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);

                  g_ptr_array_add (device_extensions, (gpointer) VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
                }
              if (features & GDK_VULKAN_FEATURE_TIMELINE_SEMAPHORE)
                {
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
                }
#ifdef GDK_WINDOWING_WIN32
              if (features & GDK_VULKAN_FEATURE_WIN32)
                {
                  if (!(features & GDK_VULKAN_FEATURE_DMABUF))
                    g_ptr_array_add (device_extensions, (gpointer) VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
                }
              if (features & GDK_VULKAN_FEATURE_WIN32_SEMAPHORE)
                {
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
                }
#endif
              if (features & (GDK_VULKAN_FEATURE_SEMAPHORE_IMPORT | GDK_VULKAN_FEATURE_SEMAPHORE_EXPORT))
                {
                  if (!(features & GDK_VULKAN_FEATURE_TIMELINE_SEMAPHORE))
                    g_ptr_array_add (device_extensions, (gpointer) VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
                }
              if (features & GDK_VULKAN_FEATURE_INCREMENTAL_PRESENT)
                g_ptr_array_add (device_extensions, (gpointer) VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
              if (features & GDK_VULKAN_FEATURE_SWAPCHAIN_MAINTENANCE)
                {
                  g_ptr_array_add (device_extensions, (gpointer) VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
                  swapchain_maintenance1_features.pNext = create_device_pNext;
                  create_device_pNext = &swapchain_maintenance1_features;
                }
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
              if (features & GDK_VULKAN_FEATURE_PORTABILITY_SUBSET)
                {
                  g_ptr_array_add (device_extensions, (gpointer) VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
                }
#endif

#define ENABLE_IF(flag) ((features & (flag)) ? VK_TRUE : VK_FALSE)
              GDK_DISPLAY_DEBUG (display, VULKAN, "Using Vulkan device %u, queue %u", i, j);
              if (GDK_VK_CHECK (vkCreateDevice, devices[i],
                                                &(VkDeviceCreateInfo) {
                                                    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                                    .queueCreateInfoCount = 1,
                                                    .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
                                                        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                        .queueFamilyIndex = j,
                                                        .queueCount = 1,
                                                        .pQueuePriorities = (float []) { 1.0f },
                                                    },
                                                    .enabledExtensionCount = device_extensions->len,
                                                    .ppEnabledExtensionNames = (const char * const *) device_extensions->pdata,
                                                    .pNext = &(VkPhysicalDeviceVulkan11Features) {
                                                        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                                                        .samplerYcbcrConversion = ENABLE_IF (GDK_VULKAN_FEATURE_YCBCR),
                                                        .pNext = &(VkPhysicalDeviceVulkan12Features) {
                                                          .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                                                          .timelineSemaphore = ENABLE_IF (GDK_VULKAN_FEATURE_TIMELINE_SEMAPHORE),
                                                          .pNext = create_device_pNext,
                                                        }
                                                    }
                                                },
                                                NULL,
                                                &display->vk_device) != VK_SUCCESS)
                {
                  g_ptr_array_unref (device_extensions);
                  continue;
                }
#undef ENABLE_IF

              g_ptr_array_unref (device_extensions);

              display->vk_physical_device = devices[i];
              vkGetDeviceQueue(display->vk_device, j, 0, &display->vk_queue);
              display->vk_queue_family_index = j;
              display->vulkan_features = features;

              GDK_DISPLAY_DEBUG (display, VULKAN, "Enabled features (use GDK_VULKAN_DISABLE env var to disable):");
              for (i = 0; i < G_N_ELEMENTS (gdk_vulkan_feature_keys); i++)
                {
                  GDK_DISPLAY_DEBUG (display, VULKAN, "    %s: %s",
                                     gdk_vulkan_feature_keys[i].key,
                                     (features & gdk_vulkan_feature_keys[i].value) ? "✓" :
                                     ((skip_features & gdk_vulkan_feature_keys[i].value) ? "disabled via env var" :
                                      (((device_features & gdk_vulkan_feature_keys[i].value) == 0) ? "✗" :
                                       "Hum, what? This should not happen.")));
                }

              gdk_profiler_end_mark (start_time, "Create Vulkan device", NULL);

              return TRUE;
            }
        }
    }

  g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                       "Could not find a Vulkan device with the required features.");
  return FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
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
    g_warning ("Vulkan: %s: %s", pLayerPrefix, pMessage);
  else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    g_warning ("Vulkan: %s: %s", pLayerPrefix, pMessage);
  else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    g_warning ("Vulkan: %s: %s", pLayerPrefix, pMessage);
  else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    g_debug ("Vulkan: %s: %s", pLayerPrefix, pMessage);
  else
    g_info ("Vulkan: %s: %s", pLayerPrefix, pMessage);

  return VK_FALSE;
}

gboolean
gdk_display_create_vulkan_instance (GdkDisplay  *display,
                                    GError     **error)
{
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  uint32_t i;
  GPtrArray *used_extensions;
  gboolean have_debug_report = FALSE;
  VkInstanceCreateFlags flags = 0;
  VkResult res;

  g_assert (display->vk_instance == NULL);

  if (!gdk_has_feature (GDK_FEATURE_VULKAN))
    {
      g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                           _("Vulkan support disabled via GDK_DISABLE"));
      return FALSE;
    }

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
  g_ptr_array_add (used_extensions, (gpointer) VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
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
      if (g_str_equal (extensions[i].extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        g_ptr_array_add (used_extensions, (gpointer) VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
      if (g_str_equal (extensions[i].extensionName, VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME))
        g_ptr_array_add (used_extensions, (gpointer) VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
      if (g_str_equal (extensions[i].extensionName, VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME))
        g_ptr_array_add (used_extensions, (gpointer) VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
      if (g_str_equal (extensions[i].extensionName, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
        g_ptr_array_add (used_extensions, (gpointer) VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
      if (g_str_equal (extensions[i].extensionName, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME))
        g_ptr_array_add (used_extensions, (gpointer) VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
      if (g_str_equal (extensions[i].extensionName, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME))
        g_ptr_array_add (used_extensions, (gpointer) VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
      if (g_str_equal (extensions[i].extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
          g_ptr_array_add (used_extensions, (gpointer) VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
          flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
    }

  res = vkCreateInstance (&(VkInstanceCreateInfo) {
                               .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pNext = NULL,
                               .flags = flags,
                               .pApplicationInfo = &(VkApplicationInfo) {
                                   .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                   .pNext = NULL,
                                   .pApplicationName = g_get_application_name (),
                                   .applicationVersion = 0,
                                   .pEngineName = "GTK",
                                   .engineVersion = VK_MAKE_VERSION (GDK_MAJOR_VERSION, GDK_MINOR_VERSION, GDK_MICRO_VERSION),
                                   .apiVersion = VK_API_VERSION_1_3
                               },
                               .enabledLayerCount = 0,
                               .ppEnabledLayerNames = NULL,
                               .enabledExtensionCount = used_extensions->len,
                               .ppEnabledExtensionNames = (const char * const *) used_extensions->pdata,
                           },
                           NULL,
                           &display->vk_instance);
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

  gdk_display_create_pipeline_cache (display);

  display->vk_shader_modules = g_hash_table_new (g_str_hash, g_str_equal);

  gdk_profiler_end_mark (start_time, "Create Vulkan instance", NULL);

  return TRUE;
}

void
gdk_display_destroy_vulkan_instance (GdkDisplay *display)
{
  GHashTableIter iter;
  gpointer key, value;

  g_assert (GDK_IS_DISPLAY (display));
  g_assert (display->vk_instance != NULL);

  GDK_DEBUG (VULKAN, "Destroy Vulkan instance");
  display->vulkan_features = 0;
  g_clear_pointer (&display->vk_dmabuf_formats, gdk_dmabuf_formats_unref);
  g_hash_table_iter_init (&iter, display->vk_shader_modules);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_free (key);
      vkDestroyShaderModule (display->vk_device,
                             *((VkShaderModule *)value),
                             NULL);
      g_free (value);
    }
  g_hash_table_unref (display->vk_shader_modules);

  if (display->vk_save_pipeline_cache_source)
    {
      g_clear_handle_id (&display->vk_save_pipeline_cache_source, g_source_remove);
      gdk_vulkan_save_pipeline_cache_cb (display);
      g_assert (display->vk_save_pipeline_cache_source == 0);
    }
  vkDestroyPipelineCache (display->vk_device, display->vk_pipeline_cache, NULL);
  display->vk_pipeline_cache = VK_NULL_HANDLE;
  g_clear_pointer (&display->vk_pipeline_cache_etag, g_free);
  display->vk_pipeline_cache_size = 0;

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

/* Hack. We don't include gsk/gsk.h here to avoid a build order problem
 * with the generated header gskenumtypes.h, so we need to hack around
 * a bit to access the gsk api we need.
 */

typedef struct _GskRenderer GskRenderer;

extern GskRenderer *   gsk_vulkan_renderer_new                  (void);
extern gboolean        gsk_renderer_realize_for_display         (GskRenderer  *renderer,
                                                                 GdkDisplay   *display,
                                                                 GError      **error);

#ifdef HAVE_DMABUF
static void
gdk_vulkan_context_add_dmabuf_format (GdkDisplay              *display,
                                      VkFormat                 vk_format,
                                      guint32                  fourcc,
                                      GdkDmabufFormatsBuilder *builder)
{
  VkDrmFormatModifierPropertiesEXT modifier_list[100];
  VkDrmFormatModifierPropertiesListEXT modifier_props = {
    .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    .pNext = NULL,
    .pDrmFormatModifierProperties = modifier_list,
  };
  VkFormatProperties2 props = {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    .pNext = &modifier_props,
  };
  gsize i;

  if (vk_format == VK_FORMAT_UNDEFINED || fourcc == 0)
    return;

  modifier_props.drmFormatModifierCount = G_N_ELEMENTS (modifier_list);
  vkGetPhysicalDeviceFormatProperties2 (display->vk_physical_device,
                                        vk_format,
                                        &props);
  g_warn_if_fail (modifier_props.drmFormatModifierCount < G_N_ELEMENTS (modifier_list));
  for (i = 0; i < modifier_props.drmFormatModifierCount; i++)
    {
      gboolean advertise = modifier_list[i].drmFormatModifier != DRM_FORMAT_MOD_LINEAR;

      GDK_DISPLAY_DEBUG (display, DMABUF,
                         "Vulkan %s dmabuf format %.4s::%016"G_GINT64_MODIFIER"x with %u planes and features 0x%x",
                         advertise ? "advertises" : "supports",
                         (char *) &fourcc,
                         modifier_list[i].drmFormatModifier,
                         modifier_list[i].drmFormatModifierPlaneCount,
                         modifier_list[i].drmFormatModifierTilingFeatures);

      if (advertise)
        gdk_dmabuf_formats_builder_add_format (builder,
                                               fourcc,
                                               modifier_list[i].drmFormatModifier);
    }
}
#endif

void
gdk_vulkan_init_dmabuf (GdkDisplay *display)
{
#ifdef HAVE_DMABUF
  GdkDmabufFormatsBuilder *vulkan_builder;
  GskRenderer *renderer;
  guint32 fourcc;
  GError *error = NULL;
  gsize i;
  VkFormat vk_format;

  if (display->vk_dmabuf_formats != NULL)
    return;

  if (!gdk_has_feature (GDK_FEATURE_DMABUF) ||
      !gdk_display_prepare_vulkan (display, NULL) ||
      (display->vulkan_features & GDK_VULKAN_FEATURE_DMABUF) == 0)
    return;

  vulkan_builder = gdk_dmabuf_formats_builder_new ();

  for (i = 0; i < GDK_MEMORY_N_FORMATS; i++)
    {
      vk_format = gdk_memory_format_vk_format (i, NULL, NULL);
      fourcc = gdk_memory_format_get_dmabuf_rgb_fourcc (i);
      gdk_vulkan_context_add_dmabuf_format (display, vk_format, fourcc, vulkan_builder);
      fourcc = gdk_memory_format_get_dmabuf_yuv_fourcc (i);
      gdk_vulkan_context_add_dmabuf_format (display, vk_format, fourcc, vulkan_builder);
    }

  display->vk_dmabuf_formats = gdk_dmabuf_formats_builder_free_to_formats (vulkan_builder);

  renderer = gsk_vulkan_renderer_new ();

  if (!gsk_renderer_realize_for_display (renderer, display, &error))
    {
      g_warning ("Failed to realize Vulkan renderer: %s", error->message);
      g_error_free (error);
      g_object_unref (renderer);
    }
  else
    {
      display->vk_downloader = GDK_DMABUF_DOWNLOADER (renderer);
    }
#endif
}

VkShaderModule
gdk_display_get_vk_shader_module (GdkDisplay *self,
                                  const char *resource_name)
{
  VkShaderModule *shader;
  GError *error = NULL;
  GBytes *bytes;

  shader = g_hash_table_lookup (self->vk_shader_modules, resource_name);
  if (shader)
    return *shader;

  bytes = g_resources_lookup_data (resource_name, 0, &error);
  if (bytes == NULL)
    {
      GDK_DEBUG (VULKAN, "Error loading shader data: %s", error->message);
      g_clear_error (&error);
      return VK_NULL_HANDLE;
    }

  shader = g_new0 (VkShaderModule, 1);
  if (GDK_VK_CHECK (vkCreateShaderModule, self->vk_device,
                                          &(VkShaderModuleCreateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                              .codeSize = g_bytes_get_size (bytes),
                                              .pCode = (uint32_t *) g_bytes_get_data (bytes, NULL),
                                          },
                                          NULL,
                                          shader) == VK_SUCCESS)
    {
      g_hash_table_insert (self->vk_shader_modules, g_strdup (resource_name), shader);
    }
  else
    {
      g_free (shader);

      return VK_NULL_HANDLE;
    }

  g_bytes_unref (bytes);

  return *shader;
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
