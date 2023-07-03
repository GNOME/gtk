#pragma once

#include "gskdebugprivate.h"

#include <gdk/gdk.h>
#include <graphene.h>

static inline VkResult
gsk_vulkan_handle_result (VkResult    res,
                          const char *called_function)
{
  if (res != VK_SUCCESS)
    {
      GSK_DEBUG (VULKAN, "%s(): %s (%d)", called_function, gdk_vulkan_strerror (res), res);
    }
  return res;
}

#define GSK_VK_CHECK(func, ...) gsk_vulkan_handle_result (func (__VA_ARGS__), G_STRINGIFY (func))

static inline void
gsk_vulkan_normalize_tex_coords (graphene_rect_t       *tex_coords,
                                 const graphene_rect_t *rect,
                                 const graphene_rect_t *tex)
{
  graphene_rect_init (tex_coords,
                      (rect->origin.x - tex->origin.x) / tex->size.width,
                      (rect->origin.y - tex->origin.y) / tex->size.height,
                      rect->size.width / tex->size.width,
                      rect->size.height / tex->size.height);
}

static inline void
gsk_vulkan_rect_to_float (const graphene_rect_t *rect,
                          float                  values[4])
{
  values[0] = rect->origin.x;
  values[1] = rect->origin.y;
  values[2] = rect->size.width;
  values[3] = rect->size.height;
}

static inline void
gsk_vulkan_rgba_to_float (const GdkRGBA *rgba,
                          float          values[4])
{
  values[0] = rgba->red;
  values[1] = rgba->green;
  values[2] = rgba->blue;
  values[3] = rgba->alpha;
}

static inline void
gsk_vulkan_point_to_float (const graphene_point_t *point,
                           float                   values[2])
{
  values[0] = point->x;
  values[1] = point->y;
}

