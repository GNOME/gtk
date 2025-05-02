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

#include <windows.h>

G_DEFINE_TYPE (GdkWin32CairoContext, gdk_win32_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static cairo_surface_t *
create_cairo_surface_for_surface (GdkSurface *surface)
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

  return cairo_surface;
}

static void
gdk_win32_cairo_context_begin_frame (GdkDrawContext  *draw_context,
                                     gpointer         context_data,
                                     GdkMemoryDepth   depth,
                                     cairo_region_t  *region,
                                     GdkColorState  **out_color_state,
                                     GdkMemoryDepth  *out_depth)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface;
  cairo_t *cr;

  surface = gdk_draw_context_get_surface (draw_context);

  self->hwnd_surface = create_cairo_surface_for_surface (surface);

  /* Clear the paint region.
   * For non-double-buffered rendering we must clear it, otherwise
   * semi-transparent pixels will "add up" with each repaint.
   */
  cr = cairo_create (self->hwnd_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region (cr, region);
  cairo_fill (cr);
  cairo_destroy (cr);

  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
gdk_win32_cairo_context_end_frame (GdkDrawContext *draw_context,
                                   gpointer        context_data,
                                   cairo_region_t *painted)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);

  cairo_surface_flush (self->hwnd_surface);

  g_clear_pointer (&self->hwnd_surface, cairo_surface_destroy);
}

static void
gdk_win32_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
}

static cairo_t *
gdk_win32_cairo_context_cairo_create (GdkCairoContext *context)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (context);

  return cairo_create (self->hwnd_surface);
}

static void
gdk_win32_cairo_context_class_init (GdkWin32CairoContextClass *klass)
{
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);

  draw_context_class->begin_frame = gdk_win32_cairo_context_begin_frame;
  draw_context_class->end_frame = gdk_win32_cairo_context_end_frame;
  draw_context_class->empty_frame = gdk_win32_cairo_context_empty_frame;

  cairo_context_class->cairo_create = gdk_win32_cairo_context_cairo_create;
}

static void
gdk_win32_cairo_context_init (GdkWin32CairoContext *self)
{
}
