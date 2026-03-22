#pragma once

#include "gskgpucachedprivate.h"

#include <graphene.h>
#include <pango/pango.h>

G_BEGIN_DECLS

typedef struct _GskGpuGlyphyValue GskGpuGlyphyValue;

struct _GskGpuGlyphyValue
{
  float min_x;
  float min_y;
  float max_x;
  float max_y;
  guint atlas_x;
  guint atlas_y;
  guint atlas_width;
  guint atlas_height;
};

void                    gsk_gpu_cached_glyphy_init_cache                 (GskGpuCache            *cache);
void                    gsk_gpu_cached_glyphy_finish_cache               (GskGpuCache            *cache);

GskGpuImage *           gsk_gpu_cached_glyphy_lookup                     (GskGpuCache            *cache,
                                                                         GskGpuFrame            *frame,
                                                                         PangoFont              *font,
                                                                         PangoGlyph              glyph,
                                                                         const GskGpuGlyphyValue **out_value);

float                   gsk_gpu_glyphy_get_font_scale                    (PangoFont              *font);

guint                   gsk_gpu_glyphy_get_font_key                      (PangoFont              *font);

G_END_DECLS
