#include "config.h"

#include "gskgpucachedfillprivate.h"

#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuuploadopprivate.h"
#include "gskgpuutilsprivate.h"
#include "gskgpuimageprivate.h"
#include "gskpathprivate.h"
#include "gskrectprivate.h"

#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gdk/gdkcairoprivate.h"

#include "gsk/gskprivate.h"

#define SUBPIXEL_SCALE_X 16
#define SUBPIXEL_SCALE_Y 16

typedef struct _GskGpuCachedFill GskGpuCachedFill;

struct _GskGpuCachedFill
{
  GskGpuCached parent;

  GskPath *path;
  GskFillRule fill_rule;
  float sx, sy;

  /* Subpixel position of the path bounds wrt to device grid */
  guint fx, fy;

  GskGpuImage *image;
  graphene_point_t image_offset;
};

static void
gsk_gpu_cached_fill_free (GskGpuCached *cached)
{
  GskGpuCachedFill *self = (GskGpuCachedFill *) cached;
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);

  g_hash_table_remove (priv->fill_cache, self);

  gsk_path_unref (self->path);
  g_object_unref (self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_fill_should_collect (GskGpuCached *cached,
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

  /* Fills are only collected when their atlas is freed */
  return FALSE;
}

static guint
gsk_gpu_cached_fill_hash (gconstpointer data)
{
  const GskGpuCachedFill *self = data;

  return GPOINTER_TO_UINT (self->path) ^
         (self->fill_rule) << 28 ^
         (((guint) (self->sx * 16)) << 16) ^
         ((guint) (self->sy * 16) << 8) ^
         (self->fx << 4) ^
         self->fy;
}

static gboolean
gsk_gpu_cached_fill_equal (gconstpointer v1,
                           gconstpointer v2)
{
  const GskGpuCachedFill *fill1 = v1;
  const GskGpuCachedFill *fill2 = v2;

  return fill1->fx == fill2->fx &&
         fill1->fy == fill2->fy &&
         fill1->path == fill2->path &&
         fill1->fill_rule == fill2->fill_rule &&
         fill1->sx == fill2->sx &&
         fill1->sy == fill2->sy;
}

static const GskGpuCachedClass GSK_GPU_CACHED_FILL_CLASS =
{
  sizeof (GskGpuCachedFill),
  "Fill",
  gsk_gpu_cached_fill_free,
  gsk_gpu_cached_fill_should_collect
};

typedef struct _FillData FillData;
struct _FillData
{
  GskPath *path;
  GskFillRule fill_rule;
  float dx, dy;
};

static void
fill_data_free (gpointer data)
{
  FillData *fill = data;

  gsk_path_unref (fill->path);
  g_free (fill);
}

static void
fill_path (gpointer  data,
           cairo_t  *cr)
{
  FillData *fill = data;

  cairo_translate (cr, fill->dx, fill->dy);

  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  gsk_cairo_set_fill_rule (cr, fill->fill_rule);
  gsk_path_to_cairo (fill->path, cr);
  cairo_fill (cr);
}

static void
fill_path_print (gpointer  data,
                 GString  *string)
{
  FillData *fill = data;
  char *str;

  str = gsk_path_to_string (fill->path);
  g_string_append_printf (string, "fill %s %.*s%s",
                          fill->fill_rule == GSK_FILL_RULE_WINDING ? "★" : "✫",
                          20, str, strlen (str) > 20 ? "…" : "");
  g_free (str);
}

static inline void
point_snap_to_grid_floor (const graphene_point_t *src,
                          const graphene_vec2_t  *scale,
                          const graphene_point_t *offset,
                          graphene_point_t       *dest)
{
  float x, y, sx, sy;

  sx = graphene_vec2_get_x (scale);
  sy = graphene_vec2_get_y (scale);

  x = floorf ((src->x + offset->x) * sx);
  y = floorf ((src->y + offset->y) * sy);

  graphene_point_init (dest,
                       x / sx - offset->x,
                       y / sy - offset->y);
}

#define quick_round(f) ((int) ((f) + 0.5f))

static inline void
compute_subpixel_offset (const graphene_point_t *orig,
                         const graphene_point_t *snapped,
                         const graphene_vec2_t  *scale,
                         guint                  *fx,
                         guint                  *fy,
                         float                  *dx,
                         float                  *dy)
{
  float sx, sy;
  float delta_x, delta_y;

  sx = graphene_vec2_get_x (scale);
  sy = graphene_vec2_get_y (scale);

  delta_x = orig->x - snapped->x;
  delta_y = orig->y - snapped->y;

  *fx = quick_round (delta_x * sx * SUBPIXEL_SCALE_X);
  *fy = quick_round (delta_y * sy * SUBPIXEL_SCALE_Y);

  *dx = *fx / (sx * SUBPIXEL_SCALE_X) - delta_x;
  *dy = *fy / (sy * SUBPIXEL_SCALE_Y) - delta_y;
}

GskGpuImage *
gsk_gpu_cached_fill_lookup (GskGpuCache           *self,
                            GskGpuFrame           *frame,
                            const graphene_vec2_t *scale,
                            const graphene_rect_t *bounds,
                            GskPath               *path,
                            GskFillRule            fill_rule,
                            graphene_rect_t       *out_rect)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (self);
  float sx = graphene_vec2_get_x (scale);
  float sy = graphene_vec2_get_y (scale);
  guint fx, fy;
  float dx, dy;
  GskGpuCachedFill *cache;
  graphene_rect_t rect;
  gsize atlas_x, atlas_y, padding;
  GskGpuImage *image = NULL;
  graphene_rect_t path_bounds;
  graphene_rect_t viewport;

  /* The bounds we are given are the area of the mask that is
   * needed, which may be clipped. For caching, we always want
   * the full path bounds.
   */
  gsk_path_get_bounds (path, &path_bounds);

  /* We still need to work out the grid alignment, which we
   * can do because we assume that the bounds are snapped to
   * the grid.
   */
  point_snap_to_grid_floor (&path_bounds.origin,
                            scale,
                            &GRAPHENE_POINT_INIT (- bounds->origin.x,
                                                  - bounds->origin.y),
                            &viewport.origin);

  /* The subpixel position of the path wrt. to the device grid
   * influences the rendering of the mask, so we need to
   * include it in the cache key, but we quantize it to a
   * subpixel grid to avoid blowing up the cache.
   *
   * This function computes both the shift dx,dy we need to
   * apply to place the origin of the path bounds on the
   * subpixel grid and the subpixel position itself as fx,fy.
   */
  compute_subpixel_offset (&path_bounds.origin,
                           &viewport.origin,
                           scale,
                           &fx, &fy,
                           &dx, &dy);

  /* The viewport is obtained by snapping the shifted path
   * bounds to the device grid.
   *
   * Note that we only shift the bounds here. We still need
   * to apply dx,dy to the path itself when rendering later.
   */
  graphene_rect_offset (&path_bounds, dx, dy);
  if (!gsk_rect_snap_to_grid (&path_bounds,
                              scale,
                              &GRAPHENE_POINT_INIT (- bounds->origin.x,
                                                    - bounds->origin.y),
                              &viewport))
    return NULL;

  cache = g_hash_table_lookup (priv->fill_cache,
                               &(GskGpuCachedFill) {
                                 .path = path,
                                 .fill_rule = fill_rule,
                                 .sx = sx,
                                 .sy = sy,
                                 .fx = fx,
                                 .fy = fy,
                               });
  if (cache)
    {
      gsk_gpu_cached_use ((GskGpuCached *) cache);

      graphene_rect_init (out_rect,
                          viewport.origin.x - cache->image_offset.x / sx,
                          viewport.origin.y - cache->image_offset.y / sy,
                          gsk_gpu_image_get_width (cache->image) / sx,
                          gsk_gpu_image_get_height (cache->image) / sy);

      return g_object_ref (cache->image);
    }

  padding = 1;
  rect.size.width = ceil (sx * viewport.size.width);
  rect.size.height = ceil (sy * viewport.size.height);

  image = gsk_gpu_cache_add_atlas_image (self,
                                         rect.size.width + 2 * padding,
                                         rect.size.height + 2 * padding,
                                         &atlas_x,
                                         &atlas_y);

  if (image)
    {
      g_object_ref (image);
      rect.origin.x = atlas_x + padding;
      rect.origin.y = atlas_y + padding;
      cache = gsk_gpu_cached_new_from_current_atlas (self, &GSK_GPU_CACHED_FILL_CLASS);
      cache->path = gsk_path_ref (path);
      cache->fill_rule = fill_rule;
      cache->sx = sx;
      cache->sy = sy;
      cache->fx = fx;
      cache->fy = fy;
      cache->image = g_object_ref (image);
      cache->image_offset = rect.origin;
      ((GskGpuCached *) cache)->pixels = (rect.size.width + 2 * padding) * (rect.size.height + 2 * padding);

      g_hash_table_insert (priv->fill_cache, cache, cache);
      gsk_gpu_cached_use ((GskGpuCached *) cache);
    }
  else
    {
      /* If the unclipped path is too large to fit in the atlas,
       * we give up on caching. We still need to return a mask,
       * but we'll use the clipped bounds that we were given.
       */
      viewport = *bounds;
      padding = 0;
      dx = dy = 0;
      rect.origin.x = 0;
      rect.origin.y = 0;
      rect.size.width = ceil (sx * viewport.size.width);
      rect.size.height = ceil (sy * viewport.size.height);

      image = gsk_gpu_device_create_upload_image (gsk_gpu_cache_get_device (self),
                                                  FALSE,
                                                  GDK_MEMORY_DEFAULT,
                                                  gsk_gpu_color_state_get_conversion (GDK_COLOR_STATE_SRGB),
                                                  rect.size.width,
                                                  rect.size.height);
    }

  g_assert (image != NULL);

  /* Render the viewport into the rect area, with padding.
   * Shift the path by dx,dy when rendering.
   *
   * Note that dx,dy can in fact be computed from the other
   * variables we are passing: path, viewport and fx,fy. But
   * doing so requires getting the path bounds, which is
   * expensive, so we pass the dx,dy that we already have.
   */
  gsk_gpu_upload_cairo_into_op (frame,
                                image,
                                &(cairo_rectangle_int_t) {
                                  .x = rect.origin.x - padding,
                                  .y = rect.origin.y - padding,
                                  .width = rect.size.width + 2 * padding,
                                  .height = rect.size.height + 2 * padding
                                },
                                &GRAPHENE_RECT_INIT (viewport.origin.x - padding / sx,
                                                     viewport.origin.y - padding / sy,
                                                     viewport.size.width + 2 * padding / sx,
                                                     viewport.size.height + 2 * padding / sy),
                                fill_path,
                                fill_path_print,
                                g_memdup2 (&(FillData) {
                                  .path = gsk_path_ref (path),
                                  .fill_rule = fill_rule,
                                  .dx = dx,
                                  .dy = dy,
                                }, sizeof (FillData)),
                                (GDestroyNotify) fill_data_free);

  graphene_rect_init (out_rect,
                      viewport.origin.x - rect.origin.x / sx,
                      viewport.origin.y - rect.origin.y / sy,
                      gsk_gpu_image_get_width (image) / sx,
                      gsk_gpu_image_get_height (image) / sy);

  return image;
}

void
gsk_gpu_cached_fill_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  priv->fill_cache = g_hash_table_new (gsk_gpu_cached_fill_hash,
                                       gsk_gpu_cached_fill_equal);
}

void
gsk_gpu_cached_fill_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_hash_table_unref (priv->fill_cache);
}
