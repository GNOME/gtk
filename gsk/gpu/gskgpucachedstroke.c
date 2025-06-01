#include "config.h"

#include "gskgpucachedstrokeprivate.h"

#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuuploadopprivate.h"
#include "gskgpuutilsprivate.h"
#include "gskgpuprintprivate.h"
#include "gskgpuimageprivate.h"
#include "gskenumtypes.h"
#include "gskrectprivate.h"
#include "gskstrokeprivate.h"

#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gdk/gdkcairoprivate.h"

#include "gsk/gskprivate.h"

#define SUBPIXEL_SCALE_X 32
#define SUBPIXEL_SCALE_Y 32

typedef struct _GskGpuCachedStroke GskGpuCachedStroke;

struct _GskGpuCachedStroke
{
  GskGpuCached parent;

  GskPath *path;
  GskStroke stroke;
  float sx, sy;

  /* Subpixel position of the stroke bounds wrt to device grid */
  guint fx, fy;

  GskGpuImage *image;
  graphene_point_t image_offset;
};

static void
gsk_gpu_cached_stroke_free (GskGpuCached *cached)
{
  GskGpuCachedStroke *self = (GskGpuCachedStroke *) cached;
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);

  g_hash_table_remove (priv->stroke_cache, self);

  gsk_path_unref (self->path);
  gsk_stroke_clear (&self->stroke);
  g_object_unref (self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_stroke_should_collect (GskGpuCached *cached,
                                      gint64        cache_timeout,
                                      gint64        timestamp)
{
  if (gsk_gpu_cached_is_old (cached, cache_timeout, timestamp))
    {
      if (cached->atlas)
        gsk_gpu_cached_set_stale (cached, TRUE);
      else
        return TRUE;
    }

  /* Strokes are only collected when their atlas is freed */
  return FALSE;
}

static guint
gsk_gpu_cached_stroke_hash (gconstpointer data)
{
  const GskGpuCachedStroke *self = data;

  /* We ignore dashes here */
  return GPOINTER_TO_UINT (self->path) ^
         ((guint) (self->stroke.line_width * 10)) ^
         (((guint) (self->stroke.miter_limit * 10)) << 4) ^
         (self->stroke.line_cap << 6) ^
         (self->stroke.line_join << 8) ^
         (((guint) (self->sx * 16)) << 16) ^
         ((guint) (self->sy * 16) << 8) ^
         (self->fx << 4) ^
         self->fy;
}

static gboolean
gsk_gpu_cached_stroke_equal (gconstpointer v1,
                             gconstpointer v2)
{
  const GskGpuCachedStroke *stroke1 = v1;
  const GskGpuCachedStroke *stroke2 = v2;

  return stroke1->fx == stroke2->fx &&
         stroke1->fy == stroke2->fy &&
         stroke1->path == stroke2->path &&
         stroke1->sx == stroke2->sx &&
         stroke1->sy == stroke2->sy &&
         gsk_stroke_equal (&stroke1->stroke, &stroke2->stroke);
}

static const GskGpuCachedClass GSK_GPU_CACHED_STROKE_CLASS =
{
  sizeof (GskGpuCachedStroke),
  "Stroke",
  gsk_gpu_cached_stroke_free,
  gsk_gpu_cached_stroke_should_collect
};

typedef struct _StrokeData StrokeData;
struct _StrokeData
{
  GskPath *path;
  GskStroke stroke;
};

static void
stroke_data_free (gpointer data)
{
  StrokeData *stroke = data;

  gsk_path_unref (stroke->path);
  gsk_stroke_clear (&stroke->stroke);
  g_free (stroke);
}

static void
stroke_path (gpointer  data,
             cairo_t  *cr)
{
  StrokeData *stroke = data;

  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  gsk_stroke_to_cairo (&stroke->stroke, cr);
  gsk_path_to_cairo (stroke->path, cr);
  cairo_stroke (cr);
}

static void
stroke_path_print (gpointer  data,
                   GString  *string)
{
  StrokeData *stroke = data;
  char *str;

  g_string_append_printf (string, "stroke %g ",
                          gsk_stroke_get_line_width (&stroke->stroke));
  gsk_gpu_print_enum (string, GSK_TYPE_LINE_CAP, gsk_stroke_get_line_cap (&stroke->stroke));
  gsk_gpu_print_enum (string, GSK_TYPE_LINE_JOIN, gsk_stroke_get_line_join (&stroke->stroke));
  g_string_append_printf (string, "%g ", gsk_stroke_get_miter_limit (&stroke->stroke));

  str = gsk_path_to_string (stroke->path);
  g_string_append_printf (string, "%.*s%s",
                          20, str, strlen (str) > 20 ? "…" : "");
  g_free (str);
}

static gsize
mod_subpixel (float  pos,
              float  scale,
              gsize  subpixel_scale,
              float *delta)
{
  scale *= subpixel_scale;
  pos = fmod (scale * pos, subpixel_scale);
  *delta = (ceil (pos) - pos) / scale;
  if (pos > 0.0)
    return subpixel_scale - (gsize) ceil (pos);
  else
    return (gsize) - ceil (pos);
}

GskGpuImage *
gsk_gpu_cached_stroke_lookup (GskGpuCache           *self,
                              GskGpuFrame           *frame,
                              const graphene_vec2_t *scale,
                              const graphene_rect_t *bounds,
                              GskPath               *path,
                              const GskStroke       *stroke,
                              graphene_rect_t       *out_rect)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (self);
  float sx = graphene_vec2_get_x (scale);
  float sy = graphene_vec2_get_y (scale);
  float dx, dy;
  GskGpuCachedStroke *cache;
  gsize fx, fy, atlas_x, atlas_y, padding, image_width, image_height;
  GskGpuImage *image = NULL;
  graphene_rect_t viewport;

  fx = mod_subpixel (bounds->origin.x, sx, SUBPIXEL_SCALE_X, &dx);
  fy = mod_subpixel (bounds->origin.y, sy, SUBPIXEL_SCALE_Y, &dy);

  cache = g_hash_table_lookup (priv->stroke_cache,
                               &(GskGpuCachedStroke) {
                                 .path = path,
                                 .stroke = *stroke,
                                 .sx = sx,
                                 .sy = sy,
                                 .fx = fx,
                                 .fy = fy,
                               });
  if (cache)
    {
      gsk_gpu_cached_use ((GskGpuCached *) cache);

      graphene_rect_init (out_rect,
                          cache->image_offset.x - dx,
                          cache->image_offset.y - dy,
                          gsk_gpu_image_get_width (cache->image) / sx,
                          gsk_gpu_image_get_height (cache->image) / sy);

      return g_object_ref (cache->image);
    }

  if (!gsk_path_get_stroke_bounds (path, stroke, &viewport) ||
      !gsk_rect_snap_to_grid (&viewport,
                              scale,
                              &GRAPHENE_POINT_INIT ((float) fx / (sx * SUBPIXEL_SCALE_X), 
                                                    (float) fy / (sy * SUBPIXEL_SCALE_Y)),
                              &viewport))
    return NULL;

  padding = 1;
  /* Should already be integers because of snap_to_grid() above, but round just to be sure */
  image_width = round (sx * viewport.size.width);
  image_height = round (sy * viewport.size.height);

  image = gsk_gpu_cache_add_atlas_image (self,
                                         image_width + 2 * padding,
                                         image_height + 2 * padding,
                                         &atlas_x,
                                         &atlas_y);

  if (image)
    {
      g_object_ref (image);
      cache = gsk_gpu_cached_new_from_current_atlas (self, &GSK_GPU_CACHED_STROKE_CLASS);
      cache->path = gsk_path_ref (path);
      cache->stroke = GSK_STROKE_INIT_COPY (stroke);
      cache->sx = sx;
      cache->sy = sy;
      cache->fx = fx;
      cache->fy = fy;
      cache->image = g_object_ref (image);
      graphene_rect_inset (&viewport, padding / -sx, padding / -sy);
      cache->image_offset = GRAPHENE_POINT_INIT (viewport.origin.x - atlas_x / sx,
                                                 viewport.origin.y - atlas_y / sy);

      ((GskGpuCached *) cache)->pixels = (image_width + 2 * padding) * (image_height + 2 * padding);

      g_hash_table_insert (priv->stroke_cache, cache, cache);
      gsk_gpu_cached_use ((GskGpuCached *) cache);

    }
  else
    {
      /* If the unclipped path is too large to fit in the atlas,
       * we give up on caching. We still need to return a mask,
       * but we'll use the clipped bounds that we were given.
       * We'll also assume those are grid aligned.
       */
      viewport = *bounds;
      padding = 0;
      atlas_x = 0;
      atlas_y = 0;
      image_width = ceil (sx * viewport.size.width);
      image_height = ceil (sy * viewport.size.height);

      image = gsk_gpu_device_create_upload_image (gsk_gpu_cache_get_device (self),
                                                  FALSE,
                                                  GDK_MEMORY_DEFAULT,
                                                  gsk_gpu_color_state_get_conversion (GDK_COLOR_STATE_SRGB),
                                                  image_width,
                                                  image_height);
      if (image == NULL)
        return NULL;
    }

  gsk_gpu_upload_cairo_into_op (frame,
                                image,
                                &(cairo_rectangle_int_t) {
                                  .x = atlas_x,
                                  .y = atlas_y,
                                  .width = image_width + 2 * padding,
                                  .height = image_height + 2 * padding
                                },
                                &viewport,
                                stroke_path,
                                stroke_path_print,
                                g_memdup2 (&(StrokeData) {
                                  .path = gsk_path_ref (path),
                                  .stroke = GSK_STROKE_INIT_COPY (stroke),
                                }, sizeof (StrokeData)),
                                (GDestroyNotify) stroke_data_free);

  graphene_rect_init (out_rect,
                      viewport.origin.x - atlas_x / sx - dx,
                      viewport.origin.y - atlas_y / sy - dy,
                      gsk_gpu_image_get_width (image) / sx,
                      gsk_gpu_image_get_height (image) / sy);

  return image;
}

void
gsk_gpu_cached_stroke_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  priv->stroke_cache = g_hash_table_new (gsk_gpu_cached_stroke_hash,
                                         gsk_gpu_cached_stroke_equal);
}

void
gsk_gpu_cached_stroke_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_hash_table_unref (priv->stroke_cache);
}
