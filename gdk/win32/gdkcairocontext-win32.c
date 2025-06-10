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
#include "gdkwin32misc.h"

#include <cairo-win32.h>

struct _GdkWin32CairoContext
{
  GdkCairoContext parent_instance;

  IDXGISwapChain3 *swap_chain;
  ID3D11Texture2D *staging_texture;

  /* only set while drawing */
  cairo_surface_t *cairo_surface;
};
struct _GdkWin32CairoContextClass
{
  GdkCairoContextClass parent_class;
};


G_DEFINE_TYPE (GdkWin32CairoContext, gdk_win32_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static void
gdk_win32_cairo_context_ensure_staging_texture (GdkWin32CairoContext *self)
{
  GdkDrawContext *context = GDK_DRAW_CONTEXT (self);
  GdkWin32Display *display;
  guint width, height;

  if (self->staging_texture)
    return;
  
  display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (context));
 
  gdk_draw_context_get_buffer_size (context, &width, &height);
  
  hr_warn (ID3D11Device_CreateTexture2D (gdk_win32_display_get_d3d11_device (display),
                                         (&(D3D11_TEXTURE2D_DESC) {
                                             .Width = width,
                                             .Height = height,
                                             .MipLevels = 1,
                                             .ArraySize = 1,
                                             .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
                                             .SampleDesc = {
                                                 .Count = 1,
                                                 .Quality = 0
                                             },
                                             .Usage = D3D11_USAGE_STAGING,
                                             .BindFlags = 0,
                                             .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
                                             .MiscFlags = 0,
                                         }),
                                         NULL,
                                         &self->staging_texture));
}

static gboolean
gdk_win32_cairo_context_surface_attach (GdkDrawContext  *context,
                                        GError         **error)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (context);
  GdkWin32Surface *surface;
  GdkWin32Display *display;
  ID3D11Device *d3d11_device;
  IDXGISwapChain1 *swap_chain;
  guint width, height;
  HRESULT hr;

  display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (context));
  surface = GDK_WIN32_SURFACE (gdk_draw_context_get_surface (context));
  d3d11_device = gdk_win32_display_get_d3d11_device (display);
  if (d3d11_device == NULL ||
      !gdk_win32_display_get_dcomp_device (display))
    return TRUE; /* We'll fallback */

  gdk_draw_context_get_buffer_size (context, &width, &height);

  hr_warn (IDXGIFactory4_CreateSwapChainForComposition (gdk_win32_display_get_dxgi_factory (display),
                                                        (IUnknown *) d3d11_device,
                                                        (&(DXGI_SWAP_CHAIN_DESC1) {
                                                            .Width = width,
                                                            .Height = height,
                                                            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
                                                            .Stereo = false,
                                                            .SampleDesc = (DXGI_SAMPLE_DESC) {
                                                                .Count = 1,
                                                                .Quality = 0,
                                                            },
                                                            .BufferUsage = 0,
                                                            .BufferCount = 2,
                                                            .Scaling = DXGI_SCALING_STRETCH,
                                                            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
                                                            .AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED,
                                                            .Flags = 0,
                                                        }),
                                                        NULL,
                                                        &swap_chain));

  hr_warn (IDXGISwapChain1_QueryInterface (swap_chain, &IID_IDXGISwapChain3, (void **) &self->swap_chain));
  gdk_win32_com_clear (&swap_chain);

  gdk_win32_surface_set_dcomp_content (GDK_WIN32_SURFACE (surface), (IUnknown *) self->swap_chain);

  return TRUE;
}

static void
gdk_win32_cairo_context_surface_detach (GdkDrawContext *context)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (context);
  GdkSurface *surface = gdk_draw_context_get_surface (context);

  if (!GDK_SURFACE_DESTROYED (surface))
    gdk_win32_surface_set_dcomp_content (GDK_WIN32_SURFACE (surface), NULL);

  gdk_win32_com_clear (&self->staging_texture);
  gdk_win32_com_clear (&self->swap_chain);
}

static void
gdk_win32_cairo_context_begin_frame_dcomp (GdkDrawContext  *draw_context,
                                           gpointer         context_data,
                                           GdkMemoryDepth   depth,
                                           cairo_region_t  *region,
                                           GdkColorState  **out_color_state,
                                           GdkMemoryDepth  *out_depth)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  cairo_t *cr;
  cairo_rectangle_int_t extents;
  POINT offset;
  GdkWin32Display *display;
  ID3D11Device *d3d11_device;
  ID3D11DeviceContext *d3d11_context;
  D3D11_MAPPED_SUBRESOURCE map;
  guint width, height;

  gdk_win32_cairo_context_ensure_staging_texture (self);

  display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (draw_context));
  d3d11_device = gdk_win32_display_get_d3d11_device (display);

  ID3D11Device_GetImmediateContext (d3d11_device, &d3d11_context);
  gdk_draw_context_get_buffer_size (draw_context, &width, &height);
  
  hr_warn (ID3D11DeviceContext_Map (d3d11_context,
                                    (ID3D11Resource *) self->staging_texture,
                                    0,
                                    D3D11_MAP_READ_WRITE,
                                    0,
                                    &map));
  
  self->cairo_surface = cairo_image_surface_create_for_data (map.pData,
                                                             CAIRO_FORMAT_ARGB32,
                                                             width,
                                                             height,
                                                             map.RowPitch);

  cr = cairo_create (self->cairo_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region (cr, region);
  cairo_fill (cr);
  cairo_destroy (cr);

  gdk_win32_com_clear (&d3d11_context);

  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
gdk_win32_cairo_context_end_frame_dcomp (GdkDrawContext *draw_context,
                                         gpointer        context_data,
                                         cairo_region_t *painted)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  GdkWin32Display *display;
  ID3D11Device *d3d11_device;
  ID3D11DeviceContext *d3d11_context;
  ID3D11Texture2D *texture;
  guint width, height;

  display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (draw_context));
  d3d11_device = gdk_win32_display_get_d3d11_device (display);

  ID3D11Device_GetImmediateContext (d3d11_device, &d3d11_context);
  gdk_draw_context_get_buffer_size (draw_context, &width, &height);

  cairo_surface_flush (self->cairo_surface);
  g_clear_pointer (&self->cairo_surface, cairo_surface_destroy);

  ID3D11DeviceContext_Unmap (d3d11_context, (ID3D11Resource *) self->staging_texture, 0);

  hr_warn (IDXGISwapChain3_GetBuffer (self->swap_chain,
                                      IDXGISwapChain3_GetCurrentBackBufferIndex (self->swap_chain),
                                      &IID_ID3D11Texture2D,
                                      (void **) &texture));

  if (cairo_region_contains_rectangle (painted,
                                       &(cairo_rectangle_int_t) {
                                           .x = 0,
                                           .y = 0,
                                           .width = width,
                                           .height = height,
                                       }) == CAIRO_REGION_OVERLAP_IN)
    {
      ID3D11DeviceContext_CopySubresourceRegion (d3d11_context,
                                                 (ID3D11Resource *) texture,
                                                 0,
                                                 0, 0, 0,
                                                 (ID3D11Resource *) self->staging_texture,
                                                 0,
                                                 (&(D3D11_BOX) {
                                                     .left = 0,
                                                     .top = 0,
                                                     .front = 0,
                                                     .right = width,
                                                     .bottom = height,
                                                     .back = 1
                                                 }));
      hr_warn (IDXGISwapChain3_Present1 (self->swap_chain,
                                        0,
                                        DXGI_PRESENT_RESTART,
                                        (&(DXGI_PRESENT_PARAMETERS) {
                                            .DirtyRectsCount = 0,
                                            .pDirtyRects = NULL,
                                        })));
    }
  else
    {
      int i, n_rects;
      RECT *rects;
      cairo_rectangle_int_t crect;

      n_rects = cairo_region_num_rectangles (painted);
      if (n_rects > 0)
        {
          rects = g_new (RECT, n_rects);
          for (i = 0; i < n_rects; i++)
            {
              cairo_region_get_rectangle (painted, i, &crect);
              rects[i].left = crect.x;
              rects[i].top = crect.y;
              rects[i].right = crect.x + crect.width;
              rects[i].bottom = crect.y + crect.height;
              ID3D11DeviceContext_CopySubresourceRegion (d3d11_context,
                                                        (ID3D11Resource *) texture,
                                                        0,
                                                        crect.x, crect.y, 0,
                                                        (ID3D11Resource *) self->staging_texture,
                                                        0,
                                                        (&(D3D11_BOX) {
                                                            .left = crect.x,
                                                            .top = crect.y,
                                                            .front = 0,
                                                            .right = crect.x + crect.width,
                                                            .bottom = crect.y + crect.height,
                                                            .back = 1
                                                        }));
            }

          hr_warn (IDXGISwapChain3_Present1 (self->swap_chain,
                                            0,
                                            DXGI_PRESENT_RESTART,
                                            (&(DXGI_PRESENT_PARAMETERS) {
                                                .DirtyRectsCount = n_rects,
                                                .pDirtyRects = rects,
                                            })));
          g_clear_pointer (&rects, g_free);
        }
    }

  gdk_win32_com_clear (&texture);
  gdk_win32_com_clear (&d3d11_context);
}

static void
gdk_win32_cairo_context_begin_frame_gdi (GdkDrawContext  *draw_context,
                                         gpointer         context_data,
                                         GdkMemoryDepth   depth,
                                         cairo_region_t  *region,
                                         GdkColorState  **out_color_state,
                                         GdkMemoryDepth  *out_depth)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  cairo_t *cr;
  HDC hdc;

  hdc = GetDC (gdk_win32_surface_get_handle (surface));
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return;
    }

  self->cairo_surface = cairo_win32_surface_create_with_format (hdc, CAIRO_FORMAT_ARGB32);

  cr = cairo_create (self->cairo_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region (cr, region);
  cairo_fill (cr);
  cairo_destroy (cr);

  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
gdk_win32_cairo_context_end_frame_gdi (GdkDrawContext *draw_context,
                                       gpointer        context_data,
                                       cairo_region_t *painted)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  HDC hdc;

  hdc = cairo_win32_surface_get_dc (self->cairo_surface);
  cairo_surface_flush (self->cairo_surface);

  ReleaseDC (gdk_win32_surface_get_handle (surface), hdc);
  g_clear_pointer (&self->cairo_surface, cairo_surface_destroy);
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

  if (self->swap_chain)
    gdk_win32_cairo_context_begin_frame_dcomp (draw_context, context_data, depth, region, out_color_state, out_depth);
  else
    gdk_win32_cairo_context_begin_frame_gdi (draw_context, context_data, depth, region, out_color_state, out_depth);
}

static void
gdk_win32_cairo_context_end_frame (GdkDrawContext *draw_context,
                                   gpointer        context_data,
                                   cairo_region_t *painted)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);

  if (self->swap_chain)
    gdk_win32_cairo_context_end_frame_dcomp (draw_context, context_data, painted);
  else
    gdk_win32_cairo_context_end_frame_gdi (draw_context, context_data, painted);
}

static void
gdk_win32_cairo_context_empty_frame (GdkDrawContext *draw_context)
{
}

static void
gdk_win32_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkWin32CairoContext *self = GDK_WIN32_CAIRO_CONTEXT (draw_context);
  guint width, height;

  gdk_win32_com_clear (&self->staging_texture);

  if (self->swap_chain)
    {
      GdkWin32Display *display;
      ID3D11Device *d3d11_device;
      ID3D11DeviceContext *d3d11_context;

      display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (draw_context));
      d3d11_device = gdk_win32_display_get_d3d11_device (display);

      ID3D11Device_GetImmediateContext (d3d11_device, &d3d11_context);
      ID3D11DeviceContext_ClearState (d3d11_context);
      gdk_win32_com_clear (&d3d11_context);
 
      gdk_draw_context_get_buffer_size (draw_context, &width, &height);

      hr_warn (IDXGISwapChain3_ResizeBuffers (self->swap_chain,
                                              4,
                                              width,
                                              height,
                                              DXGI_FORMAT_B8G8R8A8_UNORM,
                                              0));
    }
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
  draw_context_class->surface_attach = gdk_win32_cairo_context_surface_attach;
  draw_context_class->surface_detach = gdk_win32_cairo_context_surface_detach;
  draw_context_class->surface_resized = gdk_win32_cairo_context_surface_resized;

  cairo_context_class->cairo_create = gdk_win32_cairo_context_cairo_create;
}

static void
gdk_win32_cairo_context_init (GdkWin32CairoContext *self)
{
}
