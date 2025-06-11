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

#include "gdkdisplay-win32.h"
#include "gdkprivate-win32.h"
#include "gdkwin32misc.h"

#include <dwmapi.h>

struct _GdkWin32VulkanContext
{
  GdkVulkanContext parent_instance;

  HWND handle;
};

struct _GdkWin32VulkanContextClass
{
  GdkVulkanContextClass parent_class;
};

G_DEFINE_TYPE (GdkWin32VulkanContext, gdk_win32_vulkan_context, GDK_TYPE_VULKAN_CONTEXT)

static ATOM
gdk_win32_vulkan_context_get_class (void)
{
  static ATOM class_atom = 0;

  if (class_atom)
    return class_atom;

  class_atom = RegisterClassExW (&(WNDCLASSEX) {
                                     .cbSize = sizeof (WNDCLASSEX),
                                     .style = CS_HREDRAW | CS_VREDRAW,
                                     .lpfnWndProc = DefWindowProc,
                                     .hInstance = this_module (),
                                     .lpszClassName  = L"GdkWin32Vulkan",
                                 });
  if (class_atom == 0)
    WIN32_API_FAILED ("RegisterClassExW");

  return class_atom;
}

static VkResult
gdk_win32_vulkan_context_create_surface (GdkVulkanContext *context,
                                         VkSurfaceKHR     *vk_surface)
{
  GdkWin32VulkanContext *self = GDK_WIN32_VULKAN_CONTEXT (context);

  return GDK_VK_CHECK (vkCreateWin32SurfaceKHR,
                       gdk_vulkan_context_get_instance (context),
                       (&(VkWin32SurfaceCreateInfoKHR) {
                           .pNext = NULL,
                           .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                           .hinstance = this_module (),
                           .hwnd = self->handle
                       }),
                       NULL,
                       vk_surface);
}

static void
gdk_win32_vulkan_context_empty_frame (GdkDrawContext *draw_context)
{
}

static gboolean
gdk_win32_vulkan_context_surface_attach (GdkDrawContext  *context,
                                         GError         **error)
{
  GdkWin32VulkanContext *self = GDK_WIN32_VULKAN_CONTEXT (context);
  GdkDrawContext *draw_context = GDK_DRAW_CONTEXT (context);
  GdkSurface *surface;
  GdkWin32Display *display;
  VkResult result;
  IUnknown *dcomp_surface;
  guint width, height;

  surface = gdk_draw_context_get_surface (draw_context);
  display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (draw_context));

  if (!gdk_win32_display_get_dcomp_device (display))
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE, "Vulkan requires Direct Composition");
      return FALSE;
    }
  
  gdk_draw_context_get_buffer_size (draw_context, &width, &height);
  
  self->handle = CreateWindowExW (0,
                                  MAKEINTRESOURCEW (gdk_win32_vulkan_context_get_class ()),
                                  NULL,
                                  WS_POPUP,
                                  0, 0,
                                  width, height,
                                  GDK_SURFACE_HWND (surface),
                                  NULL,
                                  this_module (),
                                  self);
  if (self->handle == NULL)
    {
      DWORD err = GetLastError ();
      gdk_win32_check_hresult (HRESULT_FROM_WIN32 (err), error, "Failed to create rendering window");
      return FALSE;
    }
  
  hr_warn (DwmSetWindowAttribute (self->handle, DWMWA_CLOAK, (BOOL[1]) { true }, sizeof (BOOL)));

  if (!GDK_DRAW_CONTEXT_CLASS (gdk_win32_vulkan_context_parent_class)->surface_attach (context, error))
    {
      if (!DestroyWindow (self->handle))
        WIN32_API_FAILED ("DestroyWindow");
      self->handle = NULL;
      return FALSE;
    }

  ShowWindow (self->handle, SW_SHOWNOACTIVATE);

  hr_warn (IDCompositionDevice_CreateSurfaceFromHwnd (gdk_win32_display_get_dcomp_device (display),
                                                      self->handle,
                                                      &dcomp_surface));
  gdk_win32_surface_set_dcomp_content (GDK_WIN32_SURFACE (surface), dcomp_surface);

  return TRUE;
}

static void
gdk_win32_vulkan_context_surface_detach (GdkDrawContext *context)
{
  GdkWin32VulkanContext *self = GDK_WIN32_VULKAN_CONTEXT (context);
  GdkSurface *surface = gdk_draw_context_get_surface (context);

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_vulkan_context_parent_class)->surface_detach (context);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      gdk_win32_surface_set_dcomp_content (GDK_WIN32_SURFACE (surface), NULL);

      if (!DestroyWindow (self->handle))
        WIN32_API_FAILED ("DestroyWindow");
      self->handle = NULL;
    }
}

static void
gdk_win32_vulkan_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkWin32VulkanContext *self = GDK_WIN32_VULKAN_CONTEXT (draw_context);
  guint width, height;

  if (self->handle)
    {
      gdk_draw_context_get_buffer_size (draw_context, &width, &height);

      API_CALL (SetWindowPos, (self->handle,
                              SWP_NOZORDER_SPECIFIED,
                              0,
                              0,
                              width, height,
                              SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOCOPYBITS | SWP_NOZORDER));
    }

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_vulkan_context_parent_class)->surface_resized (draw_context);
}

static void
gdk_win32_vulkan_context_class_init (GdkWin32VulkanContextClass *klass)
{
  GdkVulkanContextClass *context_class = GDK_VULKAN_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  context_class->create_surface = gdk_win32_vulkan_context_create_surface;

  draw_context_class->empty_frame = gdk_win32_vulkan_context_empty_frame;
  draw_context_class->surface_attach = gdk_win32_vulkan_context_surface_attach;
  draw_context_class->surface_detach = gdk_win32_vulkan_context_surface_detach;
  draw_context_class->surface_resized = gdk_win32_vulkan_context_surface_resized;
}

static void
gdk_win32_vulkan_context_init (GdkWin32VulkanContext *self)
{
}

#endif /* GDK_RENDERING_VULKAN */

