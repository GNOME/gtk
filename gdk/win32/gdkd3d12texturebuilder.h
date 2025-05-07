/*
 * Copyright © 2024 Red Hat, Inc.
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
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#ifndef __ID3D12Fence_FWD_DEFINED__
#define __ID3D12Fence_FWD_DEFINED__
typedef struct ID3D12Fence ID3D12Fence;
#endif 	/* __ID3D12Fence_FWD_DEFINED__ */

#ifndef __ID3D12Resource_FWD_DEFINED__
#define __ID3D12Resource_FWD_DEFINED__
typedef struct ID3D12Resource ID3D12Resource;
#endif 	/* __ID3D12Resource_FWD_DEFINED__ */


#define GDK_TYPE_D3D12_TEXTURE_BUILDER (gdk_d3d12_texture_builder_get_type ())
GDK_AVAILABLE_IN_4_20
GDK_DECLARE_INTERNAL_TYPE (GdkD3D12TextureBuilder, gdk_d3d12_texture_builder, GDK, D3D12_TEXTURE_BUILDER, GObject)

typedef struct _GdkD3D12TextureBuilder     GdkD3D12TextureBuilder;

GDK_AVAILABLE_IN_4_20
GdkD3D12TextureBuilder * gdk_d3d12_texture_builder_new                  (void);

GDK_AVAILABLE_IN_4_20
ID3D12Resource *         gdk_d3d12_texture_builder_get_resource         (GdkD3D12TextureBuilder *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_20
void                     gdk_d3d12_texture_builder_set_resource         (GdkD3D12TextureBuilder *self,
                                                                         ID3D12Resource         *resource);

GDK_AVAILABLE_IN_4_20
ID3D12Fence *            gdk_d3d12_texture_builder_get_fence            (GdkD3D12TextureBuilder *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_20
void                     gdk_d3d12_texture_builder_set_fence            (GdkD3D12TextureBuilder *self,
                                                                         ID3D12Fence            *fence);

GDK_AVAILABLE_IN_4_20
guint64                  gdk_d3d12_texture_builder_get_fence_wait       (GdkD3D12TextureBuilder *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_20
void                     gdk_d3d12_texture_builder_set_fence_wait       (GdkD3D12TextureBuilder *self,
                                                                         guint64                 fence_wait);

GDK_AVAILABLE_IN_4_20
gboolean                 gdk_d3d12_texture_builder_get_premultiplied    (GdkD3D12TextureBuilder *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_20
void                     gdk_d3d12_texture_builder_set_premultiplied    (GdkD3D12TextureBuilder *self,
                                                                         gboolean                premultiplied);

GDK_AVAILABLE_IN_4_20
GdkColorState *          gdk_d3d12_texture_builder_get_color_state      (GdkD3D12TextureBuilder *self);
GDK_AVAILABLE_IN_4_20
void                     gdk_d3d12_texture_builder_set_color_state      (GdkD3D12TextureBuilder *self,
                                                                         GdkColorState          *color_state);

GDK_AVAILABLE_IN_4_20
GdkTexture *             gdk_d3d12_texture_builder_get_update_texture   (GdkD3D12TextureBuilder *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_20
void                     gdk_d3d12_texture_builder_set_update_texture   (GdkD3D12TextureBuilder *self,
                                                                         GdkTexture             *texture);

GDK_AVAILABLE_IN_4_20
cairo_region_t *         gdk_d3d12_texture_builder_get_update_region    (GdkD3D12TextureBuilder *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_20
void                     gdk_d3d12_texture_builder_set_update_region    (GdkD3D12TextureBuilder *self,
                                                                         cairo_region_t         *region);

GDK_AVAILABLE_IN_4_20
GdkTexture *             gdk_d3d12_texture_builder_build                (GdkD3D12TextureBuilder *self,
                                                                         GDestroyNotify          destroy,
                                                                         gpointer                data,
                                                                         GError                **error);

G_END_DECLS
