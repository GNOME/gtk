/* GTK - The GIMP Toolkit
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtkpixelcacheprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkstylecontextprivate.h"

#define BLOW_CACHE_TIMEOUT_SEC 20

/* The extra size of the offscreen surface we allocate
   to make scrolling more efficient */
#define DEFAULT_EXTRA_SIZE 64

/* When resizing viewport we allow this extra
   size to avoid constantly reallocating when resizing */
#define ALLOW_SMALLER_SIZE_FACTOR 0.5
#define ALLOW_LARGER_SIZE_FACTOR  1.25

struct _GtkPixelCache {
  cairo_surface_t *surface;
  cairo_content_t content;

  /* Valid if surface != NULL */
  int surface_x;
  int surface_y;
  int surface_w;
  int surface_h;
  double surface_scale;

  /* may be null if not dirty */
  cairo_region_t *surface_dirty;

  GSource *timeout_source;

  guint extra_width;
  guint extra_height;

  guint always_cache : 1;
  guint is_opaque : 1;
};

GtkPixelCache *
_gtk_pixel_cache_new (void)
{
  GtkPixelCache *cache;

  cache = g_new0 (GtkPixelCache, 1);
  cache->extra_width = DEFAULT_EXTRA_SIZE;
  cache->extra_height = DEFAULT_EXTRA_SIZE;

  return cache;
}

void
_gtk_pixel_cache_free (GtkPixelCache *cache)
{
  if (cache == NULL)
    return;

  if (cache->timeout_source ||
      cache->surface ||
      cache->surface_dirty)
    {
      g_warning ("pixel cache freed that wasn't unmapped: tag %u surface %p dirty %p",
                 g_source_get_id (cache->timeout_source), cache->surface, cache->surface_dirty);
    }

  g_clear_pointer (&cache->timeout_source, g_source_destroy);
  g_clear_pointer (&cache->surface, cairo_surface_destroy);
  g_clear_pointer (&cache->surface_dirty, cairo_region_destroy);

  g_free (cache);
}

void
_gtk_pixel_cache_set_extra_size (GtkPixelCache *cache,
                                 guint          extra_width,
                                 guint          extra_height)
{
  cache->extra_width = extra_width ? extra_width : DEFAULT_EXTRA_SIZE;
  cache->extra_height = extra_height ? extra_height : DEFAULT_EXTRA_SIZE;
}

void
_gtk_pixel_cache_get_extra_size (GtkPixelCache *cache,
                                 guint         *extra_width,
                                 guint         *extra_height)
{
  if (extra_width)
    *extra_width = cache->extra_width;

  if (extra_height)
    *extra_height = cache->extra_height;
}

void
_gtk_pixel_cache_set_content (GtkPixelCache   *cache,
                              cairo_content_t  content)
{
  cache->content = content;
  _gtk_pixel_cache_invalidate (cache, NULL);
}

/* Region is in canvas coordinates */
void
_gtk_pixel_cache_invalidate (GtkPixelCache  *cache,
                             cairo_region_t *region)
{
  cairo_rectangle_int_t r;
  cairo_region_t *free_region = NULL;

  if (cache->surface == NULL ||
      (region != NULL && cairo_region_is_empty (region)))
    return;

  if (region == NULL)
    {
      r.x = cache->surface_x;
      r.y = cache->surface_y;
      r.width = cache->surface_w;
      r.height = cache->surface_h;

      free_region = region =
        cairo_region_create_rectangle (&r);
    }

  if (cache->surface_dirty == NULL)
    {
      cache->surface_dirty = cairo_region_copy (region);
      cairo_region_translate (cache->surface_dirty,
                              -cache->surface_x,
                              -cache->surface_y);
    }
  else
    {
      cairo_region_translate (region,
                              -cache->surface_x,
                              -cache->surface_y);
      cairo_region_union (cache->surface_dirty, region);
      cairo_region_translate (region,
                              cache->surface_x,
                              cache->surface_y);
    }

  if (free_region)
    cairo_region_destroy (free_region);

  r.x = 0;
  r.y = 0;
  r.width = cache->surface_w;
  r.height = cache->surface_h;

  cairo_region_intersect_rectangle (cache->surface_dirty, &r);
}

static void
_gtk_pixel_cache_create_surface_if_needed (GtkPixelCache         *cache,
                                           GdkWindow             *window,
                                           cairo_rectangle_int_t *view_rect,
                                           cairo_rectangle_int_t *canvas_rect)
{
  cairo_rectangle_int_t rect;
  int surface_w, surface_h;
  cairo_content_t content;

#ifdef G_ENABLE_DEBUG
  if (GTK_DISPLAY_DEBUG_CHECK (gdk_window_get_display (window), NO_PIXEL_CACHE))
    return;
#endif

  content = cache->content;
  if (!content)
    {
      if (cache->is_opaque)
        content = CAIRO_CONTENT_COLOR;
      else
        content = CAIRO_CONTENT_COLOR_ALPHA;
    }

  surface_w = view_rect->width;
  if (canvas_rect->width > surface_w)
    surface_w = MIN (surface_w + cache->extra_width, canvas_rect->width);

  surface_h = view_rect->height;
  if (canvas_rect->height > surface_h)
    surface_h = MIN (surface_h + cache->extra_height, canvas_rect->height);

  /* If current surface can't fit view_rect or is too large, kill it */
  if (cache->surface != NULL &&
      (cairo_surface_get_content (cache->surface) != content ||
       cache->surface_w < MAX(view_rect->width, surface_w * ALLOW_SMALLER_SIZE_FACTOR) ||
       cache->surface_w > surface_w * ALLOW_LARGER_SIZE_FACTOR ||
       cache->surface_h < MAX(view_rect->height, surface_h * ALLOW_SMALLER_SIZE_FACTOR) ||
       cache->surface_h > surface_h * ALLOW_LARGER_SIZE_FACTOR ||
       cache->surface_scale != gdk_window_get_scale_factor (window)))
    {
      cairo_surface_destroy (cache->surface);
      cache->surface = NULL;
      if (cache->surface_dirty)
        cairo_region_destroy (cache->surface_dirty);
      cache->surface_dirty = NULL;
    }

  /* Don't allocate a surface if view >= canvas, as we won't
   * be scrolling then anyway, unless the widget requested it.
   */
  if (cache->surface == NULL &&
      (cache->always_cache ||
       (view_rect->width < canvas_rect->width ||
        view_rect->height < canvas_rect->height)))
    {
      cache->surface_x = -canvas_rect->x;
      cache->surface_y = -canvas_rect->y;
      cache->surface_w = MAX (surface_w, 1);
      cache->surface_h = MAX (surface_h, 1);
      cache->surface_scale = gdk_window_get_scale_factor (window);

      cache->surface =
        gdk_window_create_similar_surface (window, content,
                                           cache->surface_w,
                                           cache->surface_h);
      rect.x = 0;
      rect.y = 0;
      rect.width = cache->surface_w;
      rect.height = cache->surface_h;
      cache->surface_dirty =
        cairo_region_create_rectangle (&rect);
    }
}

static void
_gtk_pixel_cache_set_position (GtkPixelCache         *cache,
                               cairo_rectangle_int_t *view_rect,
                               cairo_rectangle_int_t *canvas_rect)
{
  cairo_rectangle_int_t r, view_pos;
  cairo_region_t *copy_region;
  int new_surf_x, new_surf_y;
  cairo_t *backing_cr;

  if (cache->surface == NULL)
    return;

  /* Position of view inside canvas */
  view_pos.x = -canvas_rect->x;
  view_pos.y = -canvas_rect->y;
  view_pos.width = view_rect->width;
  view_pos.height = view_rect->height;

  /* Reposition so all is visible */
  if (view_pos.x < cache->surface_x ||
      view_pos.x + view_pos.width > cache->surface_x + cache->surface_w ||
      view_pos.y < cache->surface_y ||
      view_pos.y + view_pos.height > cache->surface_y + cache->surface_h)
    {
      new_surf_x = cache->surface_x;
      if (view_pos.x < cache->surface_x)
        new_surf_x = MAX (view_pos.x + view_pos.width - cache->surface_w, 0);
      else if (view_pos.x + view_pos.width >
               cache->surface_x + cache->surface_w)
        new_surf_x = MIN (view_pos.x, canvas_rect->width - cache->surface_w);

      new_surf_y = cache->surface_y;
      if (view_pos.y < cache->surface_y)
        new_surf_y = MAX (view_pos.y + view_pos.height - cache->surface_h, 0);
      else if (view_pos.y + view_pos.height >
               cache->surface_y + cache->surface_h)
        new_surf_y = MIN (view_pos.y, canvas_rect->height - cache->surface_h);

      r.x = 0;
      r.y = 0;
      r.width = cache->surface_w;
      r.height = cache->surface_h;
      copy_region = cairo_region_create_rectangle (&r);

      if (cache->surface_dirty)
        {
          cairo_region_subtract (copy_region, cache->surface_dirty);
          cairo_region_destroy (cache->surface_dirty);
          cache->surface_dirty = NULL;
        }

      cairo_region_translate (copy_region,
                              cache->surface_x - new_surf_x,
                              cache->surface_y - new_surf_y);
      cairo_region_intersect_rectangle (copy_region, &r);

      backing_cr = cairo_create (cache->surface);
      gdk_cairo_region (backing_cr, copy_region);
      cairo_set_operator (backing_cr, CAIRO_OPERATOR_SOURCE);
      cairo_clip (backing_cr);
      cairo_push_group (backing_cr);
      cairo_set_source_surface (backing_cr, cache->surface,
                                cache->surface_x - new_surf_x,
                                cache->surface_y - new_surf_y);
      cairo_paint (backing_cr);
      cairo_pop_group_to_source (backing_cr);
      cairo_paint (backing_cr);
      cairo_destroy (backing_cr);

      cache->surface_x = new_surf_x;
      cache->surface_y = new_surf_y;

      cairo_region_xor_rectangle (copy_region, &r);
      cache->surface_dirty = copy_region;
    }
}

static void
_gtk_pixel_cache_repaint (GtkPixelCache         *cache,
                          GdkWindow             *window,
                          GtkPixelCacheDrawFunc  draw,
                          cairo_rectangle_int_t *view_rect,
                          cairo_rectangle_int_t *canvas_rect,
                          gpointer               user_data)
{
  cairo_t *backing_cr;
  cairo_region_t *region_dirty = cache->surface_dirty;
  cache->surface_dirty = NULL;

  if (cache->surface &&
      region_dirty &&
      !cairo_region_is_empty (region_dirty))
    {
      backing_cr = cairo_create (cache->surface);
      gdk_cairo_region (backing_cr, region_dirty);
      cairo_clip (backing_cr);
      cairo_translate (backing_cr,
                       -cache->surface_x - canvas_rect->x - view_rect->x,
                       -cache->surface_y - canvas_rect->y - view_rect->y);

      cairo_save (backing_cr);
      cairo_set_source_rgba (backing_cr,
                             0.0, 0, 0, 0.0);
      cairo_set_operator (backing_cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (backing_cr);
      cairo_restore (backing_cr);

      cairo_save (backing_cr);
      draw (backing_cr, user_data);
      cairo_restore (backing_cr);

#ifdef G_ENABLE_DEBUG
      if (GTK_DISPLAY_DEBUG_CHECK (gdk_window_get_display (window), PIXEL_CACHE))
        {
          GdkRGBA colors[] = {
            { 1, 0, 0, 0.08},
            { 0, 1, 0, 0.08},
            { 0, 0, 1, 0.08},
            { 1, 0, 1, 0.08},
            { 1, 1, 0, 0.08},
            { 0, 1, 1, 0.08},
          };
          static int current_color = 0;

          gdk_cairo_set_source_rgba (backing_cr, &colors[(current_color++) % G_N_ELEMENTS (colors)]);
          cairo_paint (backing_cr);
        }
#endif

      cairo_destroy (backing_cr);
    }

  if (region_dirty)
    cairo_region_destroy (region_dirty);
}

static void
gtk_pixel_cache_blow_cache (GtkPixelCache *cache)
{
  g_clear_pointer (&cache->timeout_source, g_source_destroy);
  g_clear_pointer (&cache->surface, cairo_surface_destroy);
  g_clear_pointer (&cache->surface_dirty, cairo_region_destroy);
}

static gboolean
blow_cache_cb  (gpointer user_data)
{
  GtkPixelCache *cache = user_data;

  cache->timeout_source = NULL;

  gtk_pixel_cache_blow_cache (cache);

  return G_SOURCE_REMOVE;
}

static gboolean
context_is_unscaled (cairo_t *cr)
{
  cairo_matrix_t matrix;
  gdouble x, y;

  x = y = 1;
  cairo_get_matrix (cr, &matrix);
  cairo_matrix_transform_distance (&matrix, &x, &y);

  return x == 1 && y == 1;
}


void
_gtk_pixel_cache_draw (GtkPixelCache         *cache,
                       cairo_t               *cr,
                       GdkWindow             *window,
                       cairo_rectangle_int_t *view_rect,   /* View position in widget coords */
                       cairo_rectangle_int_t *canvas_rect, /* Size and position of canvas in view coords */
                       GtkPixelCacheDrawFunc  draw,
                       gpointer               user_data)
{
  if (cache->timeout_source)
    {
      gint64 deadline;

      deadline = g_get_monotonic_time () + (BLOW_CACHE_TIMEOUT_SEC * G_USEC_PER_SEC);
      g_source_set_ready_time (cache->timeout_source, deadline);
    }
  else
    {
      guint tag;

      tag = g_timeout_add_seconds (BLOW_CACHE_TIMEOUT_SEC, blow_cache_cb, cache);
      cache->timeout_source = g_main_context_find_source_by_id (NULL, tag);
      g_source_set_name (cache->timeout_source, "[gtk+] blow_cache_cb");
    }

  _gtk_pixel_cache_create_surface_if_needed (cache, window,
                                             view_rect, canvas_rect);
  _gtk_pixel_cache_set_position (cache, view_rect, canvas_rect);
  _gtk_pixel_cache_repaint (cache, window, draw, view_rect, canvas_rect, user_data);

  if (cache->surface && context_is_unscaled (cr) &&
      /* Don't use backing surface if rendering elsewhere */
      cairo_surface_get_type (cache->surface) == cairo_surface_get_type (cairo_get_target (cr)))
    {
      cairo_save (cr);
      cairo_set_source_surface (cr, cache->surface,
                                cache->surface_x + view_rect->x + canvas_rect->x,
                                cache->surface_y + view_rect->y + canvas_rect->y);
      cairo_rectangle (cr, view_rect->x, view_rect->y,
                       view_rect->width, view_rect->height);
      cairo_fill (cr);
      cairo_restore (cr);
    }
  else
    {
      cairo_rectangle (cr,
                       view_rect->x, view_rect->y,
                       view_rect->width, view_rect->height);
      cairo_clip (cr);
      draw (cr, user_data);
    }
}

void
_gtk_pixel_cache_map (GtkPixelCache *cache)
{
  _gtk_pixel_cache_invalidate (cache, NULL);
}

void
_gtk_pixel_cache_unmap (GtkPixelCache *cache)
{
  gtk_pixel_cache_blow_cache (cache);
}

gboolean
_gtk_pixel_cache_get_always_cache (GtkPixelCache *cache)
{
  return cache->always_cache;
}

void
_gtk_pixel_cache_set_always_cache (GtkPixelCache *cache,
                                   gboolean       always_cache)
{
  cache->always_cache = !!always_cache;
}

void
gtk_pixel_cache_set_is_opaque (GtkPixelCache *cache,
                               gboolean       is_opaque)
{
  if (cache->is_opaque == is_opaque)
    return;

  cache->is_opaque = is_opaque;
  _gtk_pixel_cache_invalidate (cache, NULL);
}
