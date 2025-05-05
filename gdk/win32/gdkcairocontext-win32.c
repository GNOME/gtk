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
#include "gdkdisplay-win32.h"
#include "gdkprivate-win32.h"
#include "gdksurface-win32.h"

struct _GdkWin32CairoContext
{
  GdkCairoContext parent_instance;

  IDCompositionSurface *dcomp_surface;

  /* only set while drawing */
  IDXGISurface *dxgi_surface;
  cairo_surface_t *cairo_surface;
};
struct _GdkWin32CairoContextClass
{
  GdkCairoContextClass parent_class;
};


G_DEFINE_TYPE (GdkWin32CairoContext, gdk_win32_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static void
gdk_win32_cairo_context_ensure_dcomp_surface (GdkWin32CairoContext *self)
{
  GdkDrawContext *context = GDK_DRAW_CONTEXT (self);
  GdkWin32Display *display;
  guint width, height;

  if (self->dcomp_surface)
    return;

  display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (context));
  gdk_draw_context_get_buffer_size (context, &width, &height);

  hr_warn (IDCompositionDevice_CreateSurface (gdk_win32_display_get_dcomp_device (display),
                                              width, height,
                                              DXGI_FORMAT_B8G8R8A8_UNORM,
                                              DXGI_ALPHA_MODE_PREMULTIPLIED,
                                              &self->dcomp_surface));
  gdk_win32_surface_set_dcomp_content (GDK_WIN32_SURFACE (gdk_draw_context_get_surface (context)),
                                       (IUnknown *) self->dcomp_surface);
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
  cairo_rectangle_int_t extents;
  DXGI_MAPPED_RECT mapped_rect;
  POINT offset;

  gdk_win32_cairo_context_ensure_dcomp_surface (self);

  cairo_region_get_extents (region, &extents);

  hr_warn (IDCompositionSurface_BeginDraw (self->dcomp_surface,
                                           (&(RECT) {
                                               .left = extents.x,
                                               .top = extents.y,
                                               .right = extents.x + extents.width,
                                               .bottom = extents.y + extents.height
                                           }),
                                           &IID_IDXGISurface,
                                           (void **) &self->dxgi_surface,
                                           &offset));

  hr_warn (IDXGISurface_Map (self->dxgi_surface, &mapped_rect, DXGI_MAP_READ | DXGI_MAP_WRITE));
  
  self->cairo_surface = cairo_image_surface_create_for_data (mapped_rect.pBits,
                                                             CAIRO_FORMAT_ARGB32,
                                                             extents.width,
                                                             extents.height,
                                                             mapped_rect.Pitch);

  cr = cairo_create (self->cairo_surface);
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

  cairo_surface_flush (self->cairo_surface);
  g_clear_pointer (&self->cairo_surface, cairo_surface_destroy);

  hr_warn (IDXGISurface_Unmap (self->dxgi_surface));
  gdk_win32_com_clear (&self->dxgi_surface);

  hr_warn (IDCompositionSurface_EndDraw (self->dcomp_surface));
}

static void
gdk_win32_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
}

static cairo_t *
gdk_win32_cairo_context_cairo_create (GdkCairoContext *context)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (context);

  return cairo_create (self->cairo_surface);
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
