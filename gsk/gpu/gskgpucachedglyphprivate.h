#pragma once

#include "gskgpucachedprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

typedef enum
{
  GSK_GPU_GLYPH_X_OFFSET_1 = 0x1,
  GSK_GPU_GLYPH_X_OFFSET_2 = 0x2,
  GSK_GPU_GLYPH_X_OFFSET_3 = 0x3,
  GSK_GPU_GLYPH_Y_OFFSET_1 = 0x4,
  GSK_GPU_GLYPH_Y_OFFSET_2 = 0x8,
  GSK_GPU_GLYPH_Y_OFFSET_3 = 0xC
} GskGpuGlyphLookupFlags;

void                    gsk_gpu_cached_glyph_init_cache                 (GskGpuCache            *cache);
void                    gsk_gpu_cached_glyph_finish_cache               (GskGpuCache            *cache);

GskGpuImage *           gsk_gpu_cached_glyph_lookup                     (GskGpuCache            *self,
                                                                         GskGpuFrame            *frame,
                                                                         PangoFont              *font,
                                                                         PangoGlyph              glyph,
                                                                         GskGpuGlyphLookupFlags  flags,
                                                                         float                   scale,
                                                                         graphene_rect_t        *out_bounds,
                                                                         graphene_point_t       *out_origin);


G_END_DECLS
