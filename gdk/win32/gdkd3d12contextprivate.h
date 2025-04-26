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

#pragma once

#include "gdkconfig.h"

#include "gdkdrawcontextprivate.h"

#include "gdkwin32display.h"

#include <dxgi1_6.h>

G_BEGIN_DECLS

#define GDK_TYPE_D3D12_CONTEXT		(gdk_d3d12_context_get_type ())
#define GDK_D3D12_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_D3D12_CONTEXT, GdkD3d12Context))
#define GDK_IS_D3D12_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_D3D12_CONTEXT))
#define GDK_D3D12_CONTEXT_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_D3D12_CONTEXT, GdkD3d12ContextClass))
#define GDK_IS_D3D12_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_D3D12_CONTEXT))
#define GDK_D3D12_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_D3D12_CONTEXT, GdkD3d12ContextClass))

typedef struct _GdkD3d12Context GdkD3d12Context;
typedef struct _GdkD3d12ContextClass GdkD3d12ContextClass;

GType                   gdk_d3d12_context_get_type                      (void) G_GNUC_CONST;

GdkD3d12Context *       gdk_d3d12_context_new                           (GdkWin32Display       *display,
                                                                         GdkSurface            *surface,
                                                                         GError               **error);

ID3D12CommandQueue *    gdk_d3d12_context_get_command_queue             (GdkD3d12Context       *self);
IDXGISwapChain3 *       gdk_d3d12_context_get_swap_chain                (GdkD3d12Context       *self);

G_END_DECLS
