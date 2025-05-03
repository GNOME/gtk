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
#include "gdkd3d12textureprivate.h" /* for GdkD312Error */
#include "gdkdisplay-win32.h"
#include "gdkprivate-win32.h"
#include "gdkwin32misc.h"

/* Vulkan uses this number */
#define SWAP_CHAIN_BUFFER_COUNT 4

struct _GdkD3d12Context
{
  GdkDrawContext parent_instance;

  ID3D12CommandQueue *command_queue;
  IDXGISwapChain3 *swap_chain;

  struct {
    ID3D12Resource *resource;
    cairo_region_t *damage_to_next;
  } damage_tracker[SWAP_CHAIN_BUFFER_COUNT];
};

struct _GdkD3d12ContextClass
{
  GdkDrawContextClass parent_class;
};

G_DEFINE_TYPE (GdkD3d12Context, gdk_d3d12_context, GDK_TYPE_DRAW_CONTEXT)

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
  if (d3d12_device == NULL)
    {
      g_set_error (error, GDK_D3D12_ERROR, GDK_D3D12_ERROR_NOT_AVAILABLE, "No Direct3D 12 device available");
      return FALSE;
    }

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
      IDXGISwapChain1 *swap_chain;
      guint width, height;

      gdk_draw_context_get_buffer_size (draw_context, &width, &height);

      hr = IDXGIFactory4_CreateSwapChainForComposition (gdk_win32_display_get_dxgi_factory (display),
                                                        (IUnknown *) self->command_queue,
                                                        (&(DXGI_SWAP_CHAIN_DESC1) {
                                                            .Width = width,
                                                            .Height = height,
                                                            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                                            .Stereo = false,
                                                            .SampleDesc = (DXGI_SAMPLE_DESC) {
                                                                .Count = 1,
                                                                .Quality = 0,
                                                            },
                                                            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                                                            .BufferCount = SWAP_CHAIN_BUFFER_COUNT,
                                                            .Scaling = DXGI_SCALING_STRETCH,
                                                            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
                                                            .AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED,
                                                            .Flags = 0,
                                                        }),
                                                        NULL,
                                                        &swap_chain);
        if (!gdk_win32_check_hresult (hr, error, "Failed to create swap chain"))
          return FALSE;

        hr = IDXGISwapChain1_QueryInterface (swap_chain, &IID_IDXGISwapChain3, (void **) &self->swap_chain);
        gdk_win32_com_clear (&swap_chain);
        if (!gdk_win32_check_hresult (hr, error, "Swap chain version not new enough"))
          return FALSE;

        gdk_win32_surface_set_dcomp_content (GDK_WIN32_SURFACE (surface), (IUnknown *) self->swap_chain);
      }

  return TRUE;
}

static void
gdk_d3d12_context_reset_damage_tracker (GdkD3d12Context *self)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (self->damage_tracker); i++)
    {
      gdk_win32_com_clear (&self->damage_tracker[i].resource);
      g_clear_pointer (&self->damage_tracker[i].damage_to_next, cairo_region_destroy);
    }
}

static void
gdk_d3d12_context_record_damage (GdkD3d12Context *self,
                                 cairo_region_t  *damage)
{
  ID3D12Resource *resource;
  gsize n;
  cairo_region_t *damage_copy;

  hr_warn (IDXGISwapChain3_GetBuffer (self->swap_chain,
                                      IDXGISwapChain3_GetCurrentBackBufferIndex (self->swap_chain),
                                      &IID_ID3D12Resource,
                                      (void **) &resource));

  for (n = 0; n < G_N_ELEMENTS (self->damage_tracker); n++)
    {
      if (self->damage_tracker[n].resource == resource ||
          self->damage_tracker[n].resource == NULL)
        break;
    }

  damage_copy = cairo_region_copy (damage);

  if (n == G_N_ELEMENTS (self->damage_tracker))
    {
      guint width, height;

      gdk_draw_context_get_buffer_size (GDK_DRAW_CONTEXT (self), &width, &height);

      gdk_win32_com_clear (&self->damage_tracker[n - 1].resource);
      g_clear_pointer (&self->damage_tracker[n - 1].damage_to_next, cairo_region_destroy);
      n--;
      cairo_region_union_rectangle (damage, &(cairo_rectangle_int_t) { 0, 0, width, height});
    }
  else
    {
      if (self->damage_tracker[n].damage_to_next != NULL && n > 0)
        {
          cairo_region_union (self->damage_tracker[n - 1].damage_to_next,
                              self->damage_tracker[n].damage_to_next);
        }
      if (self->damage_tracker[n].resource != resource)
        {
          guint width, height;

          gdk_draw_context_get_buffer_size (GDK_DRAW_CONTEXT (self), &width, &height);
          cairo_region_union_rectangle (damage, &(cairo_rectangle_int_t) { 0, 0, width, height});
        }
      gdk_win32_com_clear (&self->damage_tracker[n].resource);
  }

  for (; n > 1; n--)
    {
      self->damage_tracker[n].resource = self->damage_tracker[n - 1].resource;
      self->damage_tracker[n].damage_to_next = self->damage_tracker[n - 1].damage_to_next;
      cairo_region_union (damage, self->damage_tracker[n].damage_to_next);
    }

  self->damage_tracker[0].resource = resource;
  self->damage_tracker[0].damage_to_next = damage_copy;
}

static void
gdk_d3d12_context_begin_frame (GdkDrawContext  *draw_context,
                               gpointer         context_data,
                               GdkMemoryDepth   depth,
                               cairo_region_t  *region,
                               GdkColorState  **out_color_state,
                               GdkMemoryDepth  *out_depth)
{
  GdkD3d12Context *self = GDK_D3D12_CONTEXT (draw_context);

  gdk_win32_surface_handle_queued_move_resize (draw_context);

  gdk_d3d12_context_record_damage (self,
                                   region);

  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
gdk_d3d12_context_end_frame (GdkDrawContext *draw_context,
                             gpointer        context_data,
                             cairo_region_t *painted)
{
  GdkD3d12Context *self = GDK_D3D12_CONTEXT (draw_context);
  int i, n_rects;
  RECT *rects;
  guint width, height;

  gdk_draw_context_get_buffer_size (draw_context, &width, &height);
  if (cairo_region_contains_rectangle (painted,
                                       &(cairo_rectangle_int_t) {
                                           .x = 0,
                                           .y = 0,
                                           .width = width,
                                           .height = height,
                                       }) == CAIRO_REGION_OVERLAP_IN)
    {
      n_rects = 0;
      rects = NULL;
    }
  else
    {
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
    }

  hr_warn (IDXGISwapChain3_Present1 (self->swap_chain,
                                     0,
                                     DXGI_PRESENT_RESTART,
                                     (&(DXGI_PRESENT_PARAMETERS) {
                                         .DirtyRectsCount = n_rects,
                                         .pDirtyRects = rects,
                                     })));
}

static void
gdk_d3d12_context_empty_frame (GdkDrawContext *draw_context)
{
}

static void
gdk_d3d12_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkD3d12Context *self = GDK_D3D12_CONTEXT (draw_context);
  guint width, height;

  gdk_d3d12_context_reset_damage_tracker (self);

  gdk_draw_context_get_buffer_size (draw_context, &width, &height);

  hr_warn (IDXGISwapChain3_ResizeBuffers (self->swap_chain,
                                          SWAP_CHAIN_BUFFER_COUNT,
                                          width,
                                          height,
                                          DXGI_FORMAT_R8G8B8A8_UNORM,
                                          0));
}

static void
gdk_d3d12_context_dispose (GObject *object)
{
  GdkD3d12Context *self = GDK_D3D12_CONTEXT (object);
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self));

  gdk_d3d12_context_reset_damage_tracker (self);

  IDCompositionVisual_SetContent (GDK_WIN32_SURFACE (surface)->dcomp_visual, NULL);

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
  draw_context_class->surface_resized = gdk_d3d12_context_surface_resized;
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

IDXGISwapChain3 *
gdk_d3d12_context_get_swap_chain (GdkD3d12Context *self)
{
  return self->swap_chain;
}
