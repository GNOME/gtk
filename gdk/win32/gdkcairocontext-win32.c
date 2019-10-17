/* GDK - The GIMP Drawing Kit
 *
 * Copyright © 2018  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkconfig.h"

#include "gdkcairocontext-win32.h"
#include "gdkprivate-win32.h"
#include "gdksurface-win32.h"
#include "gdkwin32misc.h"

#include <cairo-win32.h>

#include <Windows.h>

G_DEFINE_TYPE (GdkWin32CairoContext, gdk_win32_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

void
gdk_win32_surface_get_queued_window_rect (GdkSurface *surface,
                                          gint        scale,
                                          RECT       *return_window_rect)
{
  RECT window_rect;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  _gdk_win32_get_window_client_area_rect (surface, scale, &window_rect);

  /* Turn client area into window area */
  _gdk_win32_adjust_client_rect (surface, &window_rect);

  /* Convert GDK screen coordinates to W32 desktop coordinates */
  window_rect.left -= _gdk_offset_x * impl->surface_scale;
  window_rect.right -= _gdk_offset_x * impl->surface_scale;
  window_rect.top -= _gdk_offset_y * impl->surface_scale;
  window_rect.bottom -= _gdk_offset_y * impl->surface_scale;

  *return_window_rect = window_rect;
}

void
gdk_win32_surface_apply_queued_move_resize (GdkSurface *surface,
                                            RECT        window_rect)
{
  if (!IsIconic (GDK_SURFACE_HWND (surface)))
    {
      GDK_NOTE (EVENTS, g_print ("Setting window position ... "));

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                               SWP_NOZORDER_SPECIFIED,
                               window_rect.left, window_rect.top,
                               window_rect.right - window_rect.left,
                               window_rect.bottom - window_rect.top,
                               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW));

      GDK_NOTE (EVENTS, g_print (" ... set window position\n"));

      return;
    }

  /* Don't move iconic windows */
  /* TODO: use SetWindowPlacement() to change non-minimized window position */
}

static cairo_surface_t *
create_cairo_surface_for_layered_window (GdkWin32Surface  *impl,
                                         gint                  width,
                                         gint                  height,
                                         gint                  scale)
{
  if (width > impl->dib_width ||
      height > impl->dib_height)
    {
      cairo_surface_t *new_cache;

      impl->dib_width = MAX (impl->dib_width, MAX (width, 1));
      impl->dib_height = MAX (impl->dib_height, MAX (height, 1));
      /* Create larger cache surface, copy old cache surface over it */
      new_cache = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32,
                                                       impl->dib_width,
                                                       impl->dib_height);

      if (impl->cache_surface)
        {
          cairo_t *cr = cairo_create (new_cache);
          cairo_set_source_surface (cr, impl->cache_surface, 0, 0);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          cairo_paint (cr);
          cairo_destroy (cr);
          cairo_surface_flush (new_cache);

          cairo_surface_destroy (impl->cache_surface);
        }

      impl->cache_surface = new_cache;

      cairo_surface_set_device_scale (impl->cache_surface,
                                      scale,
                                      scale);
    }

  return cairo_surface_reference (impl->cache_surface);
}

static cairo_surface_t *
create_cairo_surface_for_surface (GdkSurface *surface,
                                  int         scale)
{
  cairo_surface_t *cairo_surface;
  HDC hdc;

  hdc = GetDC (GDK_SURFACE_HWND (surface));
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }

  cairo_surface = cairo_win32_surface_create_with_format (hdc, CAIRO_FORMAT_ARGB32);
  cairo_surface_set_device_scale (cairo_surface, scale, scale);

  return cairo_surface;
}

static void
gdk_win32_cairo_context_begin_frame (GdkDrawContext *draw_context,
                                     cairo_region_t *region)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface;
  GdkWin32Surface *impl;
  int scale;
  cairo_t *cr;
  gint width, height;
  RECT queued_window_rect;

  surface = gdk_draw_context_get_surface (draw_context);
  impl = GDK_WIN32_SURFACE (surface);
  scale = gdk_surface_get_scale_factor (surface);

  self->layered = impl->layered;

  gdk_win32_surface_get_queued_window_rect (surface, scale, &queued_window_rect);

  /* Apply queued resizes for non-double-buffered and non-layered windows
   * before painting them (we paint on the window DC directly,
   * it must have the right size).
   * Due to some poorly-undetstood issue delayed
   * resizing of double-buffered windows can produce weird
   * artefacts, so these are also resized before we paint.
   */
  if (impl->drag_move_resize_context.native_move_resize_pending &&
      !self->layered)
    {
      impl->drag_move_resize_context.native_move_resize_pending = FALSE;
      gdk_win32_surface_apply_queued_move_resize (surface, queued_window_rect);
    }

  width = queued_window_rect.right - queued_window_rect.left;
  height = queued_window_rect.bottom - queued_window_rect.top;
  width = MAX (width, 1);
  height = MAX (height, 1);

  if (self->layered)
    self->window_surface = create_cairo_surface_for_layered_window (impl, width, height, scale);
  else
    self->window_surface = create_cairo_surface_for_surface (surface, scale);

  if (self->layered ||
      !self->double_buffered)
    {
      /* Layered windows paint on the window_surface (which is itself
       * an in-memory cache that the window maintains, since layered windows
       * do not support incremental redraws.
       * Non-double-buffered windows paint on the window surface directly
       * as well.
       */
      self->paint_surface = cairo_surface_reference (self->window_surface);
    }
  else
    {
      if (width > self->db_width ||
          height > self->db_height)
        {
          self->db_width = MAX (width, self->db_width);
          self->db_height = MAX (height, self->db_height);

          g_clear_pointer (&self->db_surface, cairo_surface_destroy);

          self->db_surface = gdk_surface_create_similar_surface (surface,
                                                                 cairo_surface_get_content (self->window_surface),
                                                                 self->db_width,
                                                                 self->db_height);
        }

      /* Double-buffered windows paint on a DB surface.
       * Due to performance concerns we don't recreate it unless forced to.
       */
      self->paint_surface = cairo_surface_reference (self->db_surface);
    }

  /* Clear the paint region.
   * For non-double-buffered and for layered rendering we must
   * clear it, otherwise semi-transparent pixels will "add up"
   * with each repaint.
   * For double-buffered rendering we must clear the old pixels
   * from the DB cache surface that we're going to use as a buffer.
   */
  cr = cairo_create (self->paint_surface);
  cairo_set_source_rgba (cr, 0, 0, 0, 00);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_region (cr, region);
  cairo_clip (cr);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static void
gdk_win32_cairo_context_end_frame (GdkDrawContext *draw_context,
                                   cairo_region_t *painted)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface;
  gint scale;

  surface = gdk_draw_context_get_surface (draw_context);
  scale = gdk_surface_get_scale_factor (surface);

  /* The code to resize double-buffered windows immediately
   * before blitting the buffer contents onto them used
   * to be here.
   */

  /* Layered windows have their own, special copying section
   * further down. For double-buffered windows we need to blit
   * the DB buffer contents into the window itself.
   */
  if (!self->layered &&
      self->double_buffered)
    {
      cairo_t *cr;

      cr = cairo_create (self->window_surface);

      cairo_set_source_surface (cr, self->paint_surface, 0, 0);
      gdk_cairo_region (cr, painted);
      cairo_clip (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);

      cairo_destroy (cr);
    }

  cairo_surface_flush (self->window_surface);

  /* Update layered window, updating its contents, size and position
   * in one call.
   */
  if (self->layered)
    {
      RECT client_rect;

      /* Get the position/size of the window that GDK wants. */
      _gdk_win32_get_window_client_area_rect (surface, scale, &client_rect);
      _gdk_win32_update_layered_window_from_cache (surface, &client_rect, TRUE, TRUE, TRUE);
    }

  g_clear_pointer (&self->paint_surface, cairo_surface_destroy);
  g_clear_pointer (&self->window_surface, cairo_surface_destroy);
}

static cairo_t *
gdk_win32_cairo_context_cairo_create (GdkCairoContext *context)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (context);

  return cairo_create (self->paint_surface);
}

static void
gdk_win32_cairo_context_finalize (GObject *object)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (object);

  g_clear_pointer (&self->db_surface, cairo_surface_destroy);

  G_OBJECT_CLASS (gdk_win32_cairo_context_parent_class)->finalize (object);
}

static void
gdk_win32_cairo_context_class_init (GdkWin32CairoContextClass *klass)
{
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_win32_cairo_context_finalize;

  draw_context_class->begin_frame = gdk_win32_cairo_context_begin_frame;
  draw_context_class->end_frame = gdk_win32_cairo_context_end_frame;

  cairo_context_class->cairo_create = gdk_win32_cairo_context_cairo_create;
}

static void
gdk_win32_cairo_context_init (GdkWin32CairoContext *self)
{
  self->double_buffered = g_strcmp0 (g_getenv ("GDK_WIN32_CAIRO_DB"), "1") == 0;
  self->db_width = -1;
  self->db_height = -1;
}

