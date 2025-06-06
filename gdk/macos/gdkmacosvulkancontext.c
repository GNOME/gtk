/* gdkmacosvulkancontext.c
 *
 * gdkmacosvulkancontext.c: macOS specific Vulkan wrappers
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 * Copyright (C) 2025 Arjan Molenaar <gaphor@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkconfig.h"

#ifdef GDK_RENDERING_VULKAN

#import <QuartzCore/CAMetalLayer.h>

#include "gdkmacosvulkancontext-private.h"

#include "gdkmacosdisplay.h"
#include "gdkmacossurface-private.h"

struct _GdkMacosVulkanContext
{
  GdkVulkanContext parent_instance;
};

struct _GdkMacosVulkanContextClass
{
  GdkVulkanContextClass parent_class;
};

G_DEFINE_TYPE (GdkMacosVulkanContext, gdk_macos_vulkan_context, GDK_TYPE_VULKAN_CONTEXT)

static VkResult
gdk_macos_vulkan_context_create_surface (GdkVulkanContext *context,
                                         VkSurfaceKHR     *surface)
{
  GdkSurface *gdk_surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
  NSView *view = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (gdk_surface));
  CALayer *layer = [view layer];
  VkResult result;

  g_assert ([layer isKindOfClass:[CAMetalLayer class]]);

  result = GDK_VK_CHECK (vkCreateMetalSurfaceEXT,
                         gdk_vulkan_context_get_instance (context),
                         &(VkMetalSurfaceCreateInfoEXT) {
                             .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
                             .pNext = NULL,
                             .flags = 0,
                             .pLayer = (CAMetalLayer*) layer,
                         },
                         NULL,
                         surface);

  return result;
}

static void
gdk_macos_vulkan_context_empty_frame (GdkDrawContext *context)
{
}

static void
gdk_macos_vulkan_context_surface_resized (GdkDrawContext *context)
{
  GdkSurface *surface = gdk_draw_context_get_surface (context);
  NSView *view = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface));
  CALayer *layer = [view layer];
  double newScale = gdk_surface_get_scale (surface);

  if (newScale != [layer contentsScale])
    [layer setContentsScale:newScale];

  GDK_DRAW_CONTEXT_CLASS (gdk_macos_vulkan_context_parent_class)->surface_resized(context);
}


static gboolean
gdk_macos_vulkan_context_surface_attach (GdkDrawContext  *context,
                                         GError         **error)
{
  GdkSurface *surface;
  NSView *view;
  NSWindow *window;
  double scale = gdk_surface_get_scale_factor (surface);
  
  surface = gdk_draw_context_get_surface (context);
  view = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface));

  CAMetalLayer *layer = [CAMetalLayer layer];
  [layer setOpaque:NO];
  [layer setContentsScale: scale];
  [view setLayer:layer];

  /* This is a workaround to make sure a window is visible when
   * gtk_widget_set_visible() is called on a window.
   * Windows are presented normally with gtk_window_present().
   */
  if ((window = GDK_MACOS_SURFACE (surface)->window))
    [window orderFront:window];

  return GDK_DRAW_CONTEXT_CLASS (gdk_macos_vulkan_context_parent_class)->surface_attach(context, error);
}

static void
gdk_macos_vulkan_context_class_init (GdkMacosVulkanContextClass *klass)
{
  GdkVulkanContextClass *vulkan_context_class = GDK_VULKAN_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  vulkan_context_class->create_surface = gdk_macos_vulkan_context_create_surface;

  draw_context_class->empty_frame = gdk_macos_vulkan_context_empty_frame;
  draw_context_class->surface_resized = gdk_macos_vulkan_context_surface_resized;
  draw_context_class->surface_attach = gdk_macos_vulkan_context_surface_attach;
}

static void
gdk_macos_vulkan_context_init (GdkMacosVulkanContext *self)
{
}

#endif /* GDK_RENDERING_VULKAN */

