#pragma once

#include "gdkdxgiformatprivate.h"
#include "gdktypes.h"

/* epoxy needs this, see https://github.com/anholt/libepoxy/issues/299 */
#ifdef GDK_WINDOWING_WIN32
#include <windows.h>
#endif

#include <epoxy/gl.h>

#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

typedef enum {
  GDK_SWIZZLE_R,
  GDK_SWIZZLE_G,
  GDK_SWIZZLE_B,
  GDK_SWIZZLE_A,
  GDK_SWIZZLE_1,
  GDK_SWIZZLE_0
} GdkSwizzleComponent;

typedef guint32 GdkSwizzle;

#define GDK_SWIZZLE(r,g,b,a) \
  ((GDK_SWIZZLE_ ## r <<  0) | \
  (GDK_SWIZZLE_ ## g <<  8) | \
  (GDK_SWIZZLE_ ## b << 16) | \
  (GDK_SWIZZLE_ ## a << 24))

#define GDK_SWIZZLE_IDENTITY GDK_SWIZZLE (R, G, B, A)
#define GDK_SWIZZLE_OPAQUE GDK_SWIZZLE (R, G, B, 1)

static inline gboolean
gdk_swizzle_is_identity (GdkSwizzle swizzle)
{
  return swizzle == GDK_SWIZZLE_IDENTITY;
}

static inline gboolean
gdk_swizzle_is_framebuffer_compatible (GdkSwizzle swizzle)
{
  return swizzle == GDK_SWIZZLE_IDENTITY ||
         swizzle == GDK_SWIZZLE_OPAQUE;
}
static inline GdkSwizzleComponent
gdk_swizzle_get_component (GdkSwizzle swizzle,
                           gsize      nth)
{
  return (swizzle >> 8 * nth) & 0xFF;
}

static inline D3D12_SHADER_COMPONENT_MAPPING
gdk_swizzle_component_to_d3d12 (GdkSwizzle swizzle)
{
  switch (swizzle)
    {
      case GDK_SWIZZLE_R:
        return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
      case GDK_SWIZZLE_G:
        return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
      case GDK_SWIZZLE_B:
        return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
      case GDK_SWIZZLE_A:
        return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
      case GDK_SWIZZLE_1:
        return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
      case GDK_SWIZZLE_0:
        return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
      default:
        g_assert_not_reached ();
        return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
    }
}

static inline guint
gdk_swizzle_to_d3d12 (GdkSwizzle swizzle)
{
  return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING (
            gdk_swizzle_component_to_d3d12 (gdk_swizzle_get_component (swizzle, 0)),
            gdk_swizzle_component_to_d3d12 (gdk_swizzle_get_component (swizzle, 1)),
            gdk_swizzle_component_to_d3d12 (gdk_swizzle_get_component (swizzle, 2)),
            gdk_swizzle_component_to_d3d12 (gdk_swizzle_get_component (swizzle, 3)));
}

#ifdef GDK_RENDERING_VULKAN
static inline VkComponentSwizzle
gdk_swizzle_component_to_vk_component_swizzle (GdkSwizzleComponent swizzle)
{
  switch (swizzle)
    {
      case GDK_SWIZZLE_R:
        return VK_COMPONENT_SWIZZLE_R;
      case GDK_SWIZZLE_G:
        return VK_COMPONENT_SWIZZLE_G;
      case GDK_SWIZZLE_B:
        return VK_COMPONENT_SWIZZLE_B;
      case GDK_SWIZZLE_A:
        return VK_COMPONENT_SWIZZLE_A;
      case GDK_SWIZZLE_1:
        return VK_COMPONENT_SWIZZLE_ONE;
      case GDK_SWIZZLE_0:
        return VK_COMPONENT_SWIZZLE_ZERO;
      default:
        g_assert_not_reached ();
        return VK_COMPONENT_SWIZZLE_IDENTITY;
    }
}

static inline void
gdk_swizzle_to_vk_component_mapping (GdkSwizzle          swizzle,
                                     VkComponentMapping *out_mapping)
{
  out_mapping->r = gdk_swizzle_component_to_vk_component_swizzle (gdk_swizzle_get_component (swizzle, 0));
  out_mapping->g = gdk_swizzle_component_to_vk_component_swizzle (gdk_swizzle_get_component (swizzle, 1));
  out_mapping->b = gdk_swizzle_component_to_vk_component_swizzle (gdk_swizzle_get_component (swizzle, 2));
  out_mapping->a = gdk_swizzle_component_to_vk_component_swizzle (gdk_swizzle_get_component (swizzle, 3));
}
#endif

static inline GLint
gdk_swizzle_component_to_gl (GdkSwizzleComponent swizzle)
{
  switch (swizzle)
    {
      case GDK_SWIZZLE_R:
        return GL_RED;
      case GDK_SWIZZLE_G:
        return GL_GREEN;
      case GDK_SWIZZLE_B:
        return GL_BLUE;
      case GDK_SWIZZLE_A:
        return GL_ALPHA;
      case GDK_SWIZZLE_1:
        return GL_ONE;
      case GDK_SWIZZLE_0:
        return GL_ZERO;
      default:
        g_assert_not_reached ();
        return GL_ZERO;
    }
}

static inline void
gdk_swizzle_to_gl (GdkSwizzle swizzle,
                   GLint      out_swizzle[4])
{
  out_swizzle[0] = gdk_swizzle_component_to_gl (gdk_swizzle_get_component (swizzle, 0));
  out_swizzle[1] = gdk_swizzle_component_to_gl (gdk_swizzle_get_component (swizzle, 1));
  out_swizzle[2] = gdk_swizzle_component_to_gl (gdk_swizzle_get_component (swizzle, 2));
  out_swizzle[3] = gdk_swizzle_component_to_gl (gdk_swizzle_get_component (swizzle, 3));
}
