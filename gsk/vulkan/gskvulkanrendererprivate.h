#pragma once

#include "gskvulkanrenderer.h"
#include "gskvulkanglyphcacheprivate.h"
#include "gskvulkanimageprivate.h"

G_BEGIN_DECLS

GskVulkanImage *        gsk_vulkan_renderer_get_texture_image           (GskVulkanRenderer      *self,
                                                                         GdkTexture             *texture);
void                    gsk_vulkan_renderer_add_texture_image           (GskVulkanRenderer      *self,
                                                                         GdkTexture             *texture,
                                                                         GskVulkanImage         *image);

GskVulkanCachedGlyph  *gsk_vulkan_renderer_cache_glyph      (GskVulkanRenderer *renderer,
                                                             PangoFont         *font,
                                                             PangoGlyph         glyph,
                                                             int                x,
                                                             int                y,
                                                             float              scale);

GskVulkanGlyphCache *   gsk_vulkan_renderer_get_glyph_cache             (GskVulkanRenderer      *self);


G_END_DECLS

