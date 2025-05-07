/* gdkd3d12texture.h
 *
 * Copyright 2024 Red Hat, Inc.
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

#pragma once

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>
#include <gdk/gdktexture.h>

G_BEGIN_DECLS

#define GDK_TYPE_D3D12_TEXTURE (gdk_d3d12_texture_get_type ())

#define GDK_D3D12_TEXTURE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_D3D12_TEXTURE, GdkD3D12Texture))
#define GDK_IS_D3D12_TEXTURE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_D3D12_TEXTURE))

#define GDK_D3D12_ERROR       (gdk_d3d12_error_quark ())

typedef struct _GdkD3D12Texture            GdkD3D12Texture;
typedef struct _GdkD3D12TextureClass       GdkD3D12TextureClass;

/**
 * GdkD3D12Error:
 * @GDK_D3D12_ERROR_NOT_AVAILABLE: D3D12 support is not available, because the OS
 *   is not Windows, the Windows version is not recent enough, or it was explicitly
 *   disabled at compile- or runtime
 * @GDK_D3D12_ERROR_UNSUPPORTED_FORMAT: The requested format is not supported
 * @GDK_D3D12_ERROR_CREATION_FAILED: GTK failed to create the resource for other
 *   reasons
 *
 * Error enumeration for GTK's Direct3D 12 integration.
 *
 * Since: 4.20
 */
typedef enum {
  GDK_D3D12_ERROR_NOT_AVAILABLE,
  GDK_D3D12_ERROR_UNSUPPORTED_FORMAT,
  GDK_D3D12_ERROR_CREATION_FAILED,
} GdkD3D12Error;

GDK_AVAILABLE_IN_4_20
GType                   gdk_d3d12_texture_get_type                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_20
GQuark                  gdk_d3d12_error_quark                     (void) G_GNUC_CONST;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkD3D12Texture, g_object_unref)

G_END_DECLS
