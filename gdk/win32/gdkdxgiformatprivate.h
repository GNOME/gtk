#pragma once

#include <gdk/gdkenums.h>
#include <gdk/gdktypes.h>

#include <directx/d3d12.h>
#include <epoxy/gl.h>
#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

gboolean                gdk_dxgi_format_is_supported            (DXGI_FORMAT             dxgi_format);
GdkMemoryFormat         gdk_dxgi_format_get_memory_format       (DXGI_FORMAT             dxgi_format,
                                                                 gboolean                premultiplied);
gboolean                gdk_dxgi_format_is_yuv                  (DXGI_FORMAT             dxgi_format);

GLenum                  gdk_dxgi_format_get_gl_format           (DXGI_FORMAT             dxgi_format);
#ifdef GDK_RENDERING_VULKAN
VkFormat                gdk_dxgi_format_get_vk_format           (DXGI_FORMAT             dxgi_format,
                                                                 VkComponentMapping     *out_swizzle);
#endif
void                    gdk_dxgi_format_convert                 (DXGI_FORMAT             src_format,
                                                                 gboolean                src_premultiplied,
                                                                 const guchar           *src_data,
                                                                 gsize                   src_stride,
                                                                 GdkColorState          *src_color_state,
                                                                 GdkMemoryFormat         dest_format,
                                                                 guchar                 *dest_data,
                                                                 gsize                   dest_stride,
                                                                 GdkColorState          *dest_color_state,
                                                                 gsize                   width,
                                                                 gsize                   height);
