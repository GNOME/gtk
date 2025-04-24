/*
 * Copyright © 2025  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gdk/gdk.h>
#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GSK_TYPE_D3D12_RENDERER (gsk_d3d12_renderer_get_type ())

#define GSK_D3D12_RENDERER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_D3D12_RENDERER, GskD3d12Renderer))
#define GSK_IS_D3D12_RENDERER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_D3D12_RENDERER))
#define GSK_D3D12_RENDERER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_D3D12_RENDERER, GskD3d12RendererClass))
#define GSK_IS_D3D12_RENDERER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_D3D12_RENDERER))
#define GSK_D3D12_RENDERER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_D3D12_RENDERER, GskD3d12RendererClass))

/**
 * GskD3d12Renderer:
 *
 * Renders a GSK rendernode tree with Direct3D12 on Windows.
 *
 * This renderer will fail to realize if D3D12 is not supported.
 *
 * In particular that will happen on non-Windows setups.
 *
 * Since: 4.20
 */
typedef struct _GskD3d12Renderer                GskD3d12Renderer;
typedef struct _GskD3d12RendererClass           GskD3d12RendererClass;

GDK_AVAILABLE_IN_ALL
GType                   gsk_d3d12_renderer_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GskRenderer *           gsk_d3d12_renderer_new                 (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskD3d12Renderer, g_object_unref)

G_END_DECLS
