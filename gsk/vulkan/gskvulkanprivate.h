#pragma once

#include "gskdebugprivate.h"

#include "gskroundedrectprivate.h"

#include <gdk/gdk.h>
#include <graphene.h>

typedef struct _GskVulkanOp GskVulkanOp;
typedef struct _GskVulkanOpClass GskVulkanOpClass;
typedef struct _GskVulkanRender GskVulkanRender;
typedef struct _GskVulkanRenderPass GskVulkanRenderPass;

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


static inline void
print_indent (GString *string,
              guint    indent)
{
  g_string_append_printf (string, "%*s", 2 * indent, "");
}

static inline void
print_rect (GString               *string,
            const graphene_rect_t *rect)
{
  g_string_append_printf (string, "%g %g %g %g ",
                          rect->origin.x, rect->origin.y,
                          rect->size.width, rect->size.height);
}

static inline void
print_int_rect (GString                     *string,
                const cairo_rectangle_int_t *rect)
{
  g_string_append_printf (string, "%d %d %d %d ",
                          rect->x, rect->y,
                          rect->width, rect->height);
}

static inline void
print_rounded_rect (GString              *string,
                    const GskRoundedRect *rect)
{
  print_rect (string, &rect->bounds);

  if (gsk_rounded_rect_is_rectilinear (rect))
    return;

  g_string_append (string, "/ ");

  if (rect->corner[0].width != rect->corner[0].height ||
      rect->corner[1].width != rect->corner[1].height ||
      rect->corner[2].width != rect->corner[2].height ||
      rect->corner[3].width != rect->corner[3].height)
    {
      g_string_append (string, "variable ");
    }
  else if (rect->corner[0].width != rect->corner[1].width ||
           rect->corner[0].width != rect->corner[2].width ||
           rect->corner[0].width != rect->corner[3].width)
    {
      g_string_append_printf (string, "%g %g %g %g ",
                              rect->corner[0].width, rect->corner[1].width,
                              rect->corner[2].width, rect->corner[3].width);
    }
  else
    {
      g_string_append_printf (string, "%g ", rect->corner[0].width);
    }
}
static inline void
print_rgba (GString       *string,
            const GdkRGBA *rgba)
{
  char *s = gdk_rgba_to_string (rgba);
  g_string_append (string, s);
  g_string_append_c (string, ' ');
  g_free (s);
}

static inline void
print_newline (GString *string)
{
  if (string->len && string->str[string->len - 1] == ' ')
    string->str[string->len - 1] = '\n';
  else
    g_string_append_c (string, '\n');
}
