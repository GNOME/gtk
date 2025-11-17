#pragma once

#include "gdkcolorstateprivate.h"
#include "gdkcolorprivate.h"

#include "gdkmemoryformatprivate.h"
#include "gdkmemorytexture.h"

#include <cairo.h>
#include <graphene.h>

static inline cairo_format_t
gdk_cairo_format_for_depth (GdkMemoryDepth depth)
{
  switch (depth)
    {
      case GDK_MEMORY_NONE:
      case GDK_MEMORY_U8:
        return CAIRO_FORMAT_ARGB32;

      case GDK_MEMORY_U8_SRGB:
      case GDK_MEMORY_FLOAT16:
      case GDK_MEMORY_FLOAT32:
        return CAIRO_FORMAT_RGBA128F;

      case GDK_N_DEPTHS:
      default:
        g_return_val_if_reached (CAIRO_FORMAT_ARGB32);
    }
}

static inline GdkMemoryDepth
gdk_cairo_depth_for_format (cairo_format_t format)
{
  switch (format)
  {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_A8:
      return GDK_MEMORY_U8;

    case CAIRO_FORMAT_RGB30:
      return GDK_MEMORY_FLOAT16;

    case CAIRO_FORMAT_RGB96F:
    case CAIRO_FORMAT_RGBA128F:
      return GDK_MEMORY_FLOAT32;

    case CAIRO_FORMAT_INVALID:
    default:
      g_assert_not_reached ();
      return GDK_MEMORY_NONE;
  }
}

static GdkMemoryFormat
gdk_cairo_format_to_memory_format (cairo_format_t format)
{
  switch (format)
  {
    case CAIRO_FORMAT_ARGB32:
      return GDK_MEMORY_DEFAULT;

    case CAIRO_FORMAT_RGB24:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      return GDK_MEMORY_B8G8R8X8;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
      return GDK_MEMORY_X8R8G8B8;
#else
#error "Unknown byte order for Cairo format"
#endif
    case CAIRO_FORMAT_A8:
      return GDK_MEMORY_A8;
    case CAIRO_FORMAT_RGB96F:
      return GDK_MEMORY_R32G32B32_FLOAT;
    case CAIRO_FORMAT_RGBA128F:
      return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;

    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB30:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
    default:
      g_assert_not_reached ();
      return GDK_MEMORY_DEFAULT;
  }
}

static inline cairo_format_t
gdk_cairo_format_for_content (cairo_content_t content)
{
  switch (content)
    {
      case CAIRO_CONTENT_COLOR:
        return CAIRO_FORMAT_RGB24;
      case CAIRO_CONTENT_ALPHA:
        return CAIRO_FORMAT_A8;
      case CAIRO_CONTENT_COLOR_ALPHA:
        return CAIRO_FORMAT_ARGB32;
      default:
        g_return_val_if_reached (CAIRO_FORMAT_ARGB32);
    }
}

static inline void
gdk_cairo_set_source_color (cairo_t        *cr,
                            GdkColorState  *ccs,
                            const GdkColor *color)
{
  float c[4];
  gdk_color_to_float (color, ccs, c);
  cairo_set_source_rgba (cr, c[0], c[1], c[2], c[3]);
}

static inline void
gdk_cairo_set_source_rgba_ccs (cairo_t       *cr,
                               GdkColorState *ccs,
                               const GdkRGBA *rgba)
{
  GdkColor c;
  gdk_color_init_from_rgba (&c, rgba);
  gdk_cairo_set_source_color (cr, ccs, &c);
  gdk_color_finish (&c);
}

static inline void
gdk_cairo_pattern_add_color_stop_rgba_ccs (cairo_pattern_t *pattern,
                                           GdkColorState   *ccs,
                                           double           offset,
                                           const GdkRGBA   *rgba)
{
  float color[4];

  gdk_color_state_from_rgba (ccs, rgba, color);
  cairo_pattern_add_color_stop_rgba (pattern, offset, color[0], color[1], color[2], color[3]);
}

static inline void
gdk_cairo_pattern_add_color_stop_color (cairo_pattern_t *pattern,
                                        GdkColorState   *ccs,
                                        double           offset,
                                        const GdkColor  *color)
{
  float values[4];

  gdk_color_to_float (color, ccs, values);
  cairo_pattern_add_color_stop_rgba (pattern, offset, values[0], values[1], values[2], values[3]);
}

static inline void
gdk_cairo_rect (cairo_t               *cr,
                const graphene_rect_t *rect)
{
  cairo_rectangle (cr,
                   rect->origin.x, rect->origin.y,
                   rect->size.width, rect->size.height);
}

static inline void
gdk_cairo_surface_convert_color_state (cairo_surface_t *surface,
                                       GdkColorState   *source,
                                       GdkColorState   *target)
{
  cairo_surface_t *image_surface;
  int status, width, height;

  if (gdk_color_state_equal (source, target))
    return;

  image_surface = cairo_surface_map_to_image (surface, NULL);

  status = cairo_surface_status (image_surface);
  width = cairo_image_surface_get_width (image_surface);
  height = cairo_image_surface_get_height (image_surface);

  if (status == CAIRO_STATUS_SUCCESS && width > 0 && height > 0)
    {
      gdk_memory_convert_color_state (cairo_image_surface_get_data (image_surface),
                                      &GDK_MEMORY_LAYOUT_SIMPLE (
                                          gdk_cairo_format_to_memory_format (cairo_image_surface_get_format (image_surface)),
                                          width,
                                          height,
                                          cairo_image_surface_get_stride (image_surface)),
                                      source,
                                      target);
    }

  cairo_surface_mark_dirty (image_surface);
  cairo_surface_unmap_image (surface, image_surface);
  /* https://gitlab.freedesktop.org/cairo/cairo/-/merge_requests/487 */
  cairo_surface_mark_dirty (surface);
}

static inline cairo_region_t *
gdk_cairo_region_scale_grow (const cairo_region_t *region,
                             double                scale_x,
                             double                scale_y)
{
  cairo_region_t *result;

  result = cairo_region_create ();
  for (int i = 0; i < cairo_region_num_rectangles (region); i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      cairo_region_union_rectangle (result, &(cairo_rectangle_int_t) {
                                      .x = (int) floor (rect.x * scale_x),
                                      .y = (int) floor (rect.y * scale_y),
                                      .width = (int) ceil ((rect.x + rect.width) * scale_x) - floor (rect.x * scale_x),
                                      .height = (int) ceil ((rect.y + rect.height) * scale_y) - floor (rect.y * scale_y),
                                    });
    }

  return result;
}

static inline char *
gdk_cairo_region_to_debug_string (const cairo_region_t *region)
{
  GString *string;
  cairo_rectangle_int_t extents;

  cairo_region_get_extents (region, &extents);

  string = g_string_new (NULL);
  g_string_append_printf (string, "{ %d, %d, %d, %d } (%d rects)",
                          extents.x, extents.y, extents.width, extents.height,
                          cairo_region_num_rectangles (region));

  return g_string_free (string, FALSE);
}

static inline gboolean
gdk_cairo_is_all_clipped (cairo_t *cr)
{
  double x1, y1, x2, y2;

  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
  return x1 >= x2 || y1 >= y2;
}

/* apply a rectangle that bounds @rect in
 * pixel-aligned device coordinates.
 *
 * This is useful for clipping to minimize the rectangle
 * in push_group() or when blurring.
 */
static inline void
gdk_cairo_rectangle_snap_to_grid (cairo_t               *cr,
                                  const graphene_rect_t *rect)
{
  double x0, x1, x2, x3;
  double y0, y1, y2, y3;
  double xmin, xmax, ymin, ymax;

  x0 = rect->origin.x;
  y0 = rect->origin.y;
  cairo_user_to_device (cr, &x0, &y0);
  x1 = rect->origin.x + rect->size.width;
  y1 = rect->origin.y;
  cairo_user_to_device (cr, &x1, &y1);
  x2 = rect->origin.x;
  y2 = rect->origin.y + rect->size.height;
  cairo_user_to_device (cr, &x2, &y2);
  x3 = rect->origin.x + rect->size.width;
  y3 = rect->origin.y + rect->size.height;
  cairo_user_to_device (cr, &x3, &y3);

  xmin = MIN (MIN (x0, x1), MIN (x2, x3));
  ymin = MIN (MIN (y0, y1), MIN (y2, y3));
  xmax = MAX (MAX (x0, x1), MAX (x2, x3));
  ymax = MAX (MAX (y0, y1), MAX (y2, y3));

  xmin = floor (xmin);
  ymin = floor (ymin);
  xmax = ceil (xmax);
  ymax = ceil (ymax);

  cairo_save (cr);
  cairo_identity_matrix (cr);
  cairo_rectangle (cr, xmin, ymin, xmax - xmin, ymax - ymin);
  cairo_restore (cr);
}

