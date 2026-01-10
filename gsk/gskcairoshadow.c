#include "gskcairoshadowprivate.h"

#include "gskroundedrectprivate.h"

#include "gdk/gdkcairoprivate.h"

void
gsk_cairo_shadow_draw (cairo_t              *cr,
                       GdkColorState        *ccs,
                       gboolean              inset,
                       const GskRoundedRect *box,
                       const GskRoundedRect *clip_box,
                       float                 radius,
                       const GdkColor       *color,
                       GskBlurFlags          blur_flags)
{
  cairo_t *shadow_cr;

  if (gdk_cairo_is_all_clipped (cr))
    return;

  gdk_cairo_set_source_color (cr, ccs, color);
  shadow_cr = gsk_cairo_blur_start_drawing (cr, radius, blur_flags);

  cairo_set_fill_rule (shadow_cr, CAIRO_FILL_RULE_EVEN_ODD);
  gsk_rounded_rect_path (box, shadow_cr);
  if (inset)
    gdk_cairo_rect (shadow_cr, &clip_box->bounds);

  cairo_fill (shadow_cr);

  gsk_cairo_blur_finish_drawing (shadow_cr, ccs, radius, color, blur_flags);
}

typedef struct {
  float radius;
  graphene_size_t corner;
} CornerMask;

static guint
corner_mask_hash (CornerMask *mask)
{
  return ((guint)mask->radius << 24) ^
    ((guint)(mask->corner.width*4)) << 12 ^
    ((guint)(mask->corner.height*4)) << 0;
}

static gboolean
corner_mask_equal (CornerMask *mask1,
                   CornerMask *mask2)
{
  return
    mask1->radius == mask2->radius &&
    mask1->corner.width == mask2->corner.width &&
    mask1->corner.height == mask2->corner.height;
}

void
gsk_cairo_shadow_draw_corner (cairo_t               *cr,
                              GdkColorState         *ccs,
                              gboolean               inset,
                              const GskRoundedRect  *box,
                              const GskRoundedRect  *clip_box,
                              float                  radius,
                              const GdkColor        *color,
                              GskCorner              corner,
                              cairo_rectangle_int_t *drawn_rect)
{
  float clip_radius;
  int x1, x2, x3, y1, y2, y3, x, y;
  GskRoundedRect corner_box;
  cairo_t *mask_cr;
  cairo_surface_t *mask;
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;
  float sx, sy;
  static GHashTable *corner_mask_cache = NULL;
  float max_other;
  CornerMask key;
  gboolean overlapped;

  clip_radius = gsk_cairo_blur_compute_pixels (radius);

  overlapped = FALSE;
  if (corner == GSK_CORNER_TOP_LEFT || corner == GSK_CORNER_BOTTOM_LEFT)
    {
      x1 = floor (box->bounds.origin.x - clip_radius);
      x2 = ceil (box->bounds.origin.x + box->corner[corner].width + clip_radius);
      x = x1;
      sx = 1;
      max_other = MAX(box->corner[GSK_CORNER_TOP_RIGHT].width, box->corner[GSK_CORNER_BOTTOM_RIGHT].width);
      x3 = floor (box->bounds.origin.x + box->bounds.size.width - max_other - clip_radius);
      if (x2 > x3)
        overlapped = TRUE;
    }
  else
    {
      x1 = floor (box->bounds.origin.x + box->bounds.size.width - box->corner[corner].width - clip_radius);
      x2 = ceil (box->bounds.origin.x + box->bounds.size.width + clip_radius);
      x = x2;
      sx = -1;
      max_other = MAX(box->corner[GSK_CORNER_TOP_LEFT].width, box->corner[GSK_CORNER_BOTTOM_LEFT].width);
      x3 = ceil (box->bounds.origin.x + max_other + clip_radius);
      if (x3 > x1)
        overlapped = TRUE;
    }

  if (corner == GSK_CORNER_TOP_LEFT || corner == GSK_CORNER_TOP_RIGHT)
    {
      y1 = floor (box->bounds.origin.y - clip_radius);
      y2 = ceil (box->bounds.origin.y + box->corner[corner].height + clip_radius);
      y = y1;
      sy = 1;
      max_other = MAX(box->corner[GSK_CORNER_BOTTOM_LEFT].height, box->corner[GSK_CORNER_BOTTOM_RIGHT].height);
      y3 = floor (box->bounds.origin.y + box->bounds.size.height - max_other - clip_radius);
      if (y2 > y3)
        overlapped = TRUE;
    }
  else
    {
      y1 = floor (box->bounds.origin.y + box->bounds.size.height - box->corner[corner].height - clip_radius);
      y2 = ceil (box->bounds.origin.y + box->bounds.size.height + clip_radius);
      y = y2;
      sy = -1;
      max_other = MAX(box->corner[GSK_CORNER_TOP_LEFT].height, box->corner[GSK_CORNER_TOP_RIGHT].height);
      y3 = ceil (box->bounds.origin.y + max_other + clip_radius);
      if (y3 > y1)
        overlapped = TRUE;
    }

  drawn_rect->x = x1;
  drawn_rect->y = y1;
  drawn_rect->width = x2 - x1;
  drawn_rect->height = y2 - y1;

  cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
  cairo_clip (cr);

  if (inset || overlapped)
    {
      /* Fall back to generic path if inset or if the corner radius
         runs into each other */
      gsk_cairo_shadow_draw (cr, ccs, inset, box, clip_box, radius, color, GSK_BLUR_X | GSK_BLUR_Y);
      return;
    }

  if (gdk_cairo_is_all_clipped (cr))
    return;

  /* At this point we're drawing a blurred outset corner. The only
   * things that affect the output of the blurred mask in this case
   * is:
   *
   * What corner this is, which defines the orientation (sx,sy)
   * and position (x,y)
   *
   * The blur radius (which also defines the clip_radius)
   *
   * The horizontal and vertical corner radius
   *
   * We apply the first position and orientation when drawing the
   * mask, so we cache rendered masks based on the blur radius and the
   * corner radius.
   */
  if (corner_mask_cache == NULL)
    corner_mask_cache = g_hash_table_new_full ((GHashFunc)corner_mask_hash,
                                               (GEqualFunc)corner_mask_equal,
                                               g_free, (GDestroyNotify)cairo_surface_destroy);

  key.radius = radius;
  key.corner = box->corner[corner];

  mask = g_hash_table_lookup (corner_mask_cache, &key);
  if (mask == NULL)
    {
      mask = cairo_surface_create_similar_image (cairo_get_target (cr), CAIRO_FORMAT_A8,
                                                 drawn_rect->width + clip_radius,
                                                 drawn_rect->height + clip_radius);
      mask_cr = cairo_create (mask);
      gsk_rounded_rect_init_from_rect (&corner_box, &GRAPHENE_RECT_INIT (clip_radius, clip_radius, 2*drawn_rect->width, 2*drawn_rect->height), 0);
      corner_box.corner[0] = box->corner[corner];
      gsk_rounded_rect_path (&corner_box, mask_cr);
      cairo_fill (mask_cr);
      gsk_cairo_blur_surface (mask, radius, GSK_BLUR_X | GSK_BLUR_Y);
      cairo_destroy (mask_cr);
      g_hash_table_insert (corner_mask_cache, g_memdup2 (&key, sizeof (key)), mask);
    }

  gdk_cairo_set_source_color (cr, ccs, color);
  pattern = cairo_pattern_create_for_surface (mask);
  cairo_matrix_init_identity (&matrix);
  cairo_matrix_scale (&matrix, sx, sy);
  cairo_matrix_translate (&matrix, -x, -y);
  cairo_pattern_set_matrix (pattern, &matrix);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
}

void
gsk_cairo_shadow_draw_side (cairo_t               *cr,
                            GdkColorState         *ccs,
                            gboolean               inset,
                            const GskRoundedRect  *box,
                            const GskRoundedRect  *clip_box,
                            float                  radius,
                            const GdkColor        *color,
                            GskShadowSide           side,
                            cairo_rectangle_int_t *drawn_rect)
{
  GskBlurFlags blur_flags = GSK_BLUR_REPEAT;
  double clip_radius;
  int x1, x2, y1, y2;

  clip_radius = gsk_cairo_blur_compute_pixels (radius);

  if (side == TOP || side == BOTTOM)
    {
      blur_flags |= GSK_BLUR_Y;
      x1 = floor (box->bounds.origin.x - clip_radius);
      x2 = ceil (box->bounds.origin.x + box->bounds.size.width + clip_radius);
    }
  else if (side == LEFT)
    {
      x1 = floor (box->bounds.origin.x -clip_radius);
      x2 = ceil (box->bounds.origin.x + clip_radius);
    }
  else
    {
      x1 = floor (box->bounds.origin.x + box->bounds.size.width -clip_radius);
      x2 = ceil (box->bounds.origin.x + box->bounds.size.width + clip_radius);
    }

  if (side == LEFT || side == RIGHT)
    {
      blur_flags |= GSK_BLUR_X;
      y1 = floor (box->bounds.origin.y - clip_radius);
      y2 = ceil (box->bounds.origin.y + box->bounds.size.height + clip_radius);
    }
  else if (side == TOP)
    {
      y1 = floor (box->bounds.origin.y -clip_radius);
      y2 = ceil (box->bounds.origin.y + clip_radius);
    }
  else
    {
      y1 = floor (box->bounds.origin.y + box->bounds.size.height -clip_radius);
      y2 = ceil (box->bounds.origin.y + box->bounds.size.height + clip_radius);
    }

  drawn_rect->x = x1;
  drawn_rect->y = y1;
  drawn_rect->width = x2 - x1;
  drawn_rect->height = y2 - y1;

  cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
  cairo_clip (cr);
  gsk_cairo_shadow_draw (cr, ccs, inset, box, clip_box, radius, color, blur_flags);
}

gboolean
gsk_cairo_shadow_needs_blur (double radius)
{
  /* The code doesn't actually do any blurring for radius 1, as it
   * ends up with box filter size 1 */
  if (radius <= 1.0)
    return FALSE;

  return TRUE;
}
