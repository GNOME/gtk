#pragma once

#include <pango/pango.h>
#include "gskvulkanimageprivate.h"
#include "gskvulkanprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_GLYPH_CACHE (gsk_vulkan_glyph_cache_get_type ())

G_DECLARE_FINAL_TYPE(GskVulkanGlyphCache, gsk_vulkan_glyph_cache, GSK, VULKAN_GLYPH_CACHE, GObject)

typedef struct
{
  guint texture_index;

  float tx;
  float ty;
  float tw;
  float th;

  int draw_x;
  int draw_y;
  int draw_width;
  int draw_height;

  GskVulkanImage *atlas_image;
  int atlas_x;
  int atlas_y;

  guint64 timestamp;
} GskVulkanCachedGlyph;

GskVulkanGlyphCache  *gsk_vulkan_glyph_cache_new            (GdkVulkanContext    *vulkan);

GskVulkanCachedGlyph *gsk_vulkan_glyph_cache_lookup         (GskVulkanGlyphCache *cache,
                                                             GskVulkanRender     *render,
                                                             PangoFont           *font,
                                                             PangoGlyph           glyph,
                                                             int                  x,
                                                             int                  y,

                                                             float                scale);

void                  gsk_vulkan_glyph_cache_begin_frame    (GskVulkanGlyphCache *cache);

