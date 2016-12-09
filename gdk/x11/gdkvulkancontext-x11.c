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

#include "gdkconfig.h"

#ifdef GDK_RENDERING_VULKAN

#include "gdkvulkancontext-x11.h"

#include "gdkinternals.h"
#include "gdkdisplay-x11.h"
#include "gdkwindow-x11.h"

G_DEFINE_TYPE (GdkX11VulkanContext, gdk_x11_vulkan_context, GDK_TYPE_VULKAN_CONTEXT)

static VkResult
gdk_x11_vulkan_context_create_surface (GdkVulkanContext *context,
                                       VkSurfaceKHR     *surface)
{
  GdkWindow *window = gdk_draw_context_get_window (GDK_DRAW_CONTEXT (context));
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));

  /* This is necessary so that Vulkan sees the Window.
   * Usually, vkCreateXlibSurfaceKHR() will not cause a problem to happen as
   * it just creates resources, but futher calls with the resulting surface
   * do cause issues.
   */
  gdk_display_sync (display);

  return GDK_VK_CHECK (vkCreateXlibSurfaceKHR, gdk_vulkan_context_get_instance (context),
                                               &(VkXlibSurfaceCreateInfoKHR) {
                                                   VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                                                   NULL,
                                                   0,
                                                   gdk_x11_display_get_xdisplay (display),
                                                   gdk_x11_window_get_xid (window)
                                               },
                                               NULL,
                                               surface);
}

static void
gdk_x11_vulkan_context_class_init (GdkX11VulkanContextClass *klass)
{
  GdkVulkanContextClass *context_class = GDK_VULKAN_CONTEXT_CLASS (klass);

  context_class->create_surface = gdk_x11_vulkan_context_create_surface;
}

static void
gdk_x11_vulkan_context_init (GdkX11VulkanContext *self)
{
}

#endif /* GDK_RENDERING_VULKAN */

