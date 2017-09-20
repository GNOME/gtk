#ifndef __GSK_VULKAN_GLYPH_CACHE_PRIVATE_H__
#define __GSK_VULKAN_GLYPH_CACHE_PRIVATE_H__

#include <pango/pango.h>
#include "gskvulkanrendererprivate.h"
#include "gskvulkanimageprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_GLYPH_CACHE (gsk_vulkan_glyph_cache_get_type ())

G_DECLARE_FINAL_TYPE(GskVulkanGlyphCache, gsk_vulkan_glyph_cache, GSK, VULKAN_GLYPH_CACHE, GObject)

GskVulkanGlyphCache  *gsk_vulkan_glyph_cache_new            (GdkVulkanContext *vulkan);

GskVulkanImage *     gsk_vulkan_glyph_cache_get_glyph_image (GskVulkanGlyphCache *cache,
                                                             GskVulkanUploader   *uploader,
                                                             guint                index);

GskVulkanCachedGlyph *gsk_vulkan_glyph_cache_lookup         (GskVulkanGlyphCache *cache,
                                                             gboolean             create,
                                                             PangoFont           *font,
                                                             PangoGlyph           glyph);

void                  gsk_vulkan_glyph_cache_begin_frame    (GskVulkanGlyphCache *cache);

#endif /* __GSK_VULKAN_GLYPH_CACHE_PRIVATE_H__ */
