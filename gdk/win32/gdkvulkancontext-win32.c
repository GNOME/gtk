/* GDK - The GIMP Drawing Kit
 *
 * gdkvulkancontext-win32.c: Win32 specific Vulkan wrappers
 *
 * Copyright © 2016  Benjamin Otte
 * Copyright © 2016  Chun-wei Fan
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

#include "gdkvulkancontext-win32.h"

#include "gdkprivate-win32.h"
#include "gdkwin32misc.h"

extern HINSTANCE	 _gdk_dll_hinstance;

G_DEFINE_TYPE (GdkWin32VulkanContext, gdk_win32_vulkan_context, GDK_TYPE_VULKAN_CONTEXT)

static VkResult
gdk_win32_vulkan_context_create_surface (GdkVulkanContext *context,
                                         VkSurfaceKHR     *surface)
{
  GdkWindow *window = gdk_draw_context_get_window (GDK_DRAW_CONTEXT (context));
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  VkWin32SurfaceCreateInfoKHR info;

  info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  info.pNext = NULL;
  info.flags = 0;
  info.hinstance = _gdk_dll_hinstance;
  info.hwnd = GDK_WINDOW_HWND (window);

  /* This is necessary so that Vulkan sees the Window.
   * Usually, vkCreateWin32SurfaceKHR() will not cause a problem to happen as
   * it just creates resources, but futher calls with the resulting surface
   * do cause issues.
   */
  gdk_display_sync (display);

  return GDK_VK_CHECK (vkCreateWin32SurfaceKHR,
                       gdk_vulkan_context_get_instance (context),
                       &info,
                       NULL,
                       surface);
}

static void
gdk_win32_vulkan_context_class_init (GdkWin32VulkanContextClass *klass)
{
  GdkVulkanContextClass *context_class = GDK_VULKAN_CONTEXT_CLASS (klass);

  context_class->create_surface = gdk_win32_vulkan_context_create_surface;
}

static void
gdk_win32_vulkan_context_init (GdkWin32VulkanContext *self)
{
}

#endif /* GDK_RENDERING_VULKAN */

