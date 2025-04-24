/* GDK - The GIMP Drawing Kit
 *
 * Copyright ?? 2025  Benjamin Otte
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

#include "gdkd3d12contextprivate.h"
#include "gdkd3d12utilsprivate.h"
#include "gdkdisplay-win32.h"
#include "gdkwin32misc.h"

#include <dxgi1_2.h>

G_DEFINE_TYPE (GdkD3d12Context, gdk_d3d12_context, GDK_TYPE_DRAW_CONTEXT)

static IDXGIFactory2 *
gdk_d3d12_get_dxgi_factory (ID3D12Device *d3d12_device)
{
  IDXGIDevice *dxgi_device;
  IDXGIAdapter *dxgi_adapter;
  IDXGIFactory2 *dxgi_factory;

  hr_return_val_if_fail (ID3D12Device_QueryInterface (d3d12_device, &IID_IDXGIDevice, (void **) &dxgi_device), NULL);
  hr_return_val_if_fail (IDXGIDevice_GetAdapter (dxgi_device, &dxgi_adapter), NULL);
  hr_return_val_if_fail (IDXGIAdapter_GetParent (dxgi_adapter, &IID_IDXGIFactory2, (void **) &dxgi_factory), NULL);

  IDXGIDevice_Release (dxgi_device);
  IDXGIAdapter_Release (dxgi_adapter);

  return dxgi_factory;
}

static gboolean
gdk_d3d12_context_setup (GdkD3d12Context  *self,
                         GError          **error)
{
  GdkDrawContext *draw_context = GDK_DRAW_CONTEXT (self);
  ID3D12Device *d3d12_device;
  GdkWin32Display *display;
  GdkSurface *surface;
  HRESULT hr;

  display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (draw_context));
  surface = gdk_draw_context_get_surface (draw_context);
  d3d12_device = gdk_win32_display_get_d3d12_device (display);

  hr = ID3D12Device_CreateCommandQueue (d3d12_device,
                                        (&(D3D12_COMMAND_QUEUE_DESC) {
                                            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                                            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                            .NodeMask = 0,
                                        }),
                                        &IID_ID3D12CommandQueue,
                                        (void **) &self->command_queue);
  if (!gdk_win32_check_hresult (hr, error, "Failed to create command queue"))
    return FALSE;

  if (surface)
    {
      IDXGIFactory2 *dxgi_factory;
      guint width, height;

      dxgi_factory = gdk_d3d12_get_dxgi_factory (d3d12_device);

      gdk_draw_context_get_buffer_size (draw_context, &width, &height);

      hr = IDXGIFactory2_CreateSwapChainForHwnd (dxgi_factory,
                                                 (IUnknown *) self->command_queue,
                                                 gdk_win32_surface_get_handle (surface),
                                                 (&(DXGI_SWAP_CHAIN_DESC1) {
                                                     .Width = width,
                                                     .Height = height,
                                                     .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                                     .Stereo = false,
                                                     .SampleDesc = (DXGI_SAMPLE_DESC) {
                                                         .Count = 1,
                                                         .Quality = 0,
                                                     },
                                                     .BufferUsage = DXGI_USAGE_BACK_BUFFER,
                                                     .BufferCount = 4, /* Vulkan uses this */
                                                     .Scaling = DXGI_SCALING_STRETCH,
                                                     .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
                                                     .AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED,
                                                     .Flags = 0,
                                                 }),
                                                 NULL,
                                                 NULL,
                                                 &self->swap_chain);
        if (!gdk_win32_check_hresult (hr, error, "Failed to create swap chain"))
          {
            gdk_win32_com_clear (&dxgi_factory);
            return FALSE;
          }

        gdk_win32_com_clear (&dxgi_factory);
      }

  return TRUE;
}

static void
gdk_d3d12_context_begin_frame (GdkDrawContext  *draw_context,
                               GdkMemoryDepth   depth,
                               cairo_region_t  *region,
                               GdkColorState  **out_color_state,
                               GdkMemoryDepth  *out_depth)
{
  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
gdk_d3d12_context_end_frame (GdkDrawContext *draw_context,
                             cairo_region_t *painted)
{
  GdkD3d12Context *self = GDK_D3D12_CONTEXT (draw_context);
  int i, n_rects;
  RECT *rects;

  n_rects = cairo_region_num_rectangles (painted);
  rects = g_newa(RECT, n_rects);
  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t crect;

      cairo_region_get_rectangle (painted, i, &crect);
      rects[i].left = crect.x;
      rects[i].top = crect.y;
      rects[i].right = crect.x + crect.width;
      rects[i].bottom = crect.y + crect.height;
    }

  IDXGISwapChain1_Present1 (self->swap_chain,
                            0,
                            DXGI_PRESENT_RESTART,
                            (&(DXGI_PRESENT_PARAMETERS) {
                                .DirtyRectsCount = n_rects,
                                .pDirtyRects = rects,
                            }));
}

static void
gdk_d3d12_context_empty_frame (GdkDrawContext *draw_context)
{
}

static void
gdk_d3d12_context_dispose (GObject *object)
{
  GdkD3d12Context *self = GDK_D3D12_CONTEXT (object);

  gdk_win32_com_clear (&self->swap_chain);
  gdk_win32_com_clear (&self->command_queue);

  G_OBJECT_CLASS (gdk_d3d12_context_parent_class)->dispose (object);
}

static void
gdk_d3d12_context_class_init (GdkD3d12ContextClass *klass)
{
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gdk_d3d12_context_dispose;

  draw_context_class->begin_frame = gdk_d3d12_context_begin_frame;
  draw_context_class->end_frame = gdk_d3d12_context_end_frame;
  draw_context_class->empty_frame = gdk_d3d12_context_empty_frame;
}

static void
gdk_d3d12_context_init (GdkD3d12Context *self)
{
}

/*<private>
 * gdk_d3d12_context_new
 * @display: a `GdkWin32Display`
 * @surface: (nullable): the `GdkSurface` to use or %NULL for a surfaceless
 *   context
 * @error: return location for an error
 *
 * Creates a new `GdkD3d12Context` for use with @display.
 *
 * If @surface is NULL, the context can not be used to draw to surfaces,
 * it can only be used for custom rendering or compute.
 *
 * If the creation of the `GdkD3d12Context` failed, @error will be set.
 *
 * Returns: (transfer full): the newly created `GdkD3d12Context`, or
 *   %NULL on error
 */
GdkD3d12Context *
gdk_d3d12_context_new (GdkWin32Display  *display,
                       GdkSurface       *surface,
                       GError          **error)
{
  GdkD3d12Context *result;

  g_return_val_if_fail (GDK_IS_WIN32_DISPLAY (display), NULL);
  g_return_val_if_fail (surface == NULL || GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (surface)
    {
      result = g_object_new (GDK_TYPE_D3D12_CONTEXT,
                             "surface", surface,
                             NULL);
    }
  else
    {
      result = g_object_new (GDK_TYPE_D3D12_CONTEXT,
                             "display", display,
                             NULL);
    }

  if (!gdk_d3d12_context_setup (result, error))
    {
      g_object_unref (result);
      return NULL;
    }

  return result;
}

ID3D12CommandQueue *
gdk_d3d12_context_get_command_queue (GdkD3d12Context *self)
{
  return self->command_queue;
}

IDXGISwapChain1 *
gdk_d3d12_context_get_swap_chain (GdkD3d12Context *self)
{
  return self->swap_chain;
}
