/* GDK - The GIMP Drawing Kit
 *
 * gdksurface-vk_c: Vulkan wrappers
 *
 * Copyright © 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gdksurfaceprivate.h"

#ifdef GDK_RENDERING_VULKAN

#include "gdkdisplayprivate.h"
#include "gdkvulkancontextprivate.h"

#include <math.h>

VkResult
gdk_surface_ensure_vk_surface (GdkSurface  *surface,
                               GError     **error)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), VK_ERROR_UNKNOWN);

  if (surface->vk_surface != VK_NULL_HANDLE)
    return VK_SUCCESS;

  return GDK_SURFACE_GET_CLASS (surface)->create_vk_surface (surface,
                                                             &surface->vk_surface,
                                                             error);
}

void
gdk_surface_clear_vk_surface (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->vk_surface != VK_NULL_HANDLE)
    {
      GdkDisplay *display = gdk_surface_get_display (surface);

      vkDestroySurfaceKHR (display->vk_instance,
                           surface->vk_surface,
                           NULL);
      surface->vk_surface = VK_NULL_HANDLE;
    }
}

#endif
