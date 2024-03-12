#pragma once

#include "gskgputypesprivate.h"
#include "gskgpuframeprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

#define GSK_TYPE_GPU_DEVICE         (gsk_gpu_device_get_type ())
#define GSK_GPU_DEVICE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_GPU_DEVICE, GskGpuDevice))
#define GSK_GPU_DEVICE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_GPU_DEVICE, GskGpuDeviceClass))
#define GSK_IS_GPU_DEVICE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_GPU_DEVICE))
#define GSK_IS_GPU_DEVICE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_GPU_DEVICE))
#define GSK_GPU_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_GPU_DEVICE, GskGpuDeviceClass))

typedef struct _GskGpuDeviceClass GskGpuDeviceClass;

typedef enum
{
  GSK_GPU_GLYPH_X_OFFSET_1 = 0x1,
  GSK_GPU_GLYPH_X_OFFSET_2 = 0x2,
  GSK_GPU_GLYPH_X_OFFSET_3 = 0x3,
  GSK_GPU_GLYPH_Y_OFFSET_1 = 0x4,
  GSK_GPU_GLYPH_Y_OFFSET_2 = 0x8,
  GSK_GPU_GLYPH_Y_OFFSET_3 = 0xC
} GskGpuGlyphLookupFlags;

typedef struct _GskGpuCached GskGpuCached;
typedef struct _GskGpuCachedGlyph GskGpuCachedGlyph;
typedef struct _GskGpuCachedAtlas GskGpuCachedAtlas;
typedef struct _GskGpuCachedClass GskGpuCachedClass;

#define MAX_SLICES_PER_ATLAS 64

struct _GskGpuCached
{
  const GskGpuCachedClass *class;

  GskGpuCachedAtlas *atlas;
  GskGpuCached *next;
  GskGpuCached *prev;

  gint64 timestamp;
  gboolean stale;
  guint pixels;   /* For glyphs and textures, pixels. For atlases, dead pixels */
};

struct _GskGpuCachedAtlas
{
  GskGpuCached parent;

  GskGpuImage *image;

  gboolean has_colorize;
  gsize colorize_x;
  gsize colorize_y;

  gsize n_slices;
  struct {
    gsize width;
    gsize height;
  } slices[MAX_SLICES_PER_ATLAS];
};

typedef struct _GlyphKey GlyphKey;
struct _GlyphKey
{
  PangoFont *font;
  PangoGlyph glyph;
  GskGpuGlyphLookupFlags flags;
  float scale;
};

struct _GskGpuCachedGlyph
{
  GskGpuCached parent;

  GlyphKey key;

  GskGpuImage *image;
  graphene_rect_t bounds;
  graphene_point_t origin;
};

struct _GskGpuDevice
{
  GObject parent_instance;

  struct {
    GlyphKey key;
    GskGpuCachedGlyph *value;
  } front[256];
};

struct _GskGpuDeviceClass
{
  GObjectClass parent_class;

  GskGpuImage *         (* create_offscreen_image)                      (GskGpuDevice           *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
  GskGpuImage *         (* create_atlas_image)                          (GskGpuDevice           *self,
                                                                         gsize                   width,
                                                                         gsize                   height);
  GskGpuImage *         (* create_upload_image)                         (GskGpuDevice           *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);
  GskGpuImage *         (* create_download_image)                       (GskGpuDevice           *self,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
  void                  (* make_current)                                (GskGpuDevice           *self);

};

GType                   gsk_gpu_device_get_type                         (void) G_GNUC_CONST;

void                    gsk_gpu_device_setup                            (GskGpuDevice           *self,
                                                                         GdkDisplay             *display,
                                                                         gsize                   max_image_size);
void                    gsk_gpu_device_maybe_gc                         (GskGpuDevice           *self);
void                    gsk_gpu_device_queue_gc                         (GskGpuDevice           *self);
GdkDisplay *            gsk_gpu_device_get_display                      (GskGpuDevice           *self);
gsize                   gsk_gpu_device_get_max_image_size               (GskGpuDevice           *self);
GskGpuImage *           gsk_gpu_device_get_atlas_image                  (GskGpuDevice           *self);

GskGpuImage *           gsk_gpu_device_create_offscreen_image           (GskGpuDevice           *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gpu_device_create_upload_image              (GskGpuDevice           *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gpu_device_create_download_image            (GskGpuDevice           *self,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
void                    gsk_gpu_device_make_current                     (GskGpuDevice           *self);
GskGpuImage *           gsk_gpu_device_lookup_texture_image             (GskGpuDevice           *self,
                                                                         GdkTexture             *texture,
                                                                         gint64                  timestamp);
void                    gsk_gpu_device_cache_texture_image              (GskGpuDevice           *self,
                                                                         GdkTexture             *texture,
                                                                         gint64                  timestamp,
                                                                         GskGpuImage            *image);

GskGpuImage *           gsk_gpu_device_lookup_glyph_image               (GskGpuDevice           *self,
                                                                         GskGpuFrame            *frame,
                                                                         PangoFont              *font,
                                                                         PangoGlyph              glyph,
                                                                         GskGpuGlyphLookupFlags  flags,
                                                                         float                   scale,
                                                                         graphene_rect_t        *out_bounds,
                                                                         graphene_point_t       *out_origin);

GskGpuImage *           gsk_gpu_device_get_solid_image                  (GskGpuDevice           *self,
                                                                         GskGpuFrame            *frame,
                                                                         graphene_rect_t        *out_bounds);

static inline void
mark_as_stale (GskGpuCached *cached,
               gboolean      stale)
{
  if (cached->stale != stale)
    {
      cached->stale = stale;

      if (cached->atlas)
        {
          if (stale)
            ((GskGpuCached *) cached->atlas)->pixels += cached->pixels;
          else
            ((GskGpuCached *) cached->atlas)->pixels -= cached->pixels;
        }
    }
}

static inline void
gsk_gpu_cached_use (GskGpuDevice *device,
                    GskGpuCached *cached,
                    gint64        timestamp)
{
  cached->timestamp = timestamp;
  mark_as_stale (cached, FALSE);
}


static inline GskGpuImage *
_gsk_gpu_device_lookup_glyph_image (GskGpuDevice           *self,
                                    GskGpuFrame            *frame,
                                    PangoFont              *font,
                                    PangoGlyph              glyph,
                                    GskGpuGlyphLookupFlags  flags,
                                    float                   scale,
                                    graphene_rect_t        *out_bounds,
                                    graphene_point_t       *out_origin)
{
  GskGpuCachedGlyph lookup = {
    .key.font = font,
    .key.glyph = glyph,
    .key.flags = flags,
    .key.scale = scale
  };

  guint front_index = glyph & 0xFF;

  if (memcmp (&lookup.key, &self->front[front_index], sizeof (GlyphKey)) == 0)
    {
      GskGpuCachedGlyph *cache = self->front[front_index].value;

      gsk_gpu_cached_use (self, (GskGpuCached *) cache,
                          gsk_gpu_frame_get_timestamp (frame));

      *out_bounds = cache->bounds;
      *out_origin = cache->origin;

      return cache->image;
    }

  return gsk_gpu_device_lookup_glyph_image (self, frame, font, glyph, flags, scale, out_bounds, out_origin);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuDevice, g_object_unref)

G_END_DECLS
