/*
 * Copyright © 2024 Benjamin Otte
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

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_MEMORY_TEXTURE_BUILDER (gdk_memory_texture_builder_get_type ())
GDK_AVAILABLE_IN_4_16
GDK_DECLARE_INTERNAL_TYPE (GdkMemoryTextureBuilder, gdk_memory_texture_builder, GDK, MEMORY_TEXTURE_BUILDER, GObject)

GDK_AVAILABLE_IN_4_16
GdkMemoryTextureBuilder *       gdk_memory_texture_builder_new                  (void);

GDK_AVAILABLE_IN_4_16
GBytes *                        gdk_memory_texture_builder_get_bytes            (GdkMemoryTextureBuilder        *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_16
void                            gdk_memory_texture_builder_set_bytes            (GdkMemoryTextureBuilder        *self,
                                                                                 GBytes                         *bytes);

GDK_AVAILABLE_IN_4_16
gsize                           gdk_memory_texture_builder_get_stride           (GdkMemoryTextureBuilder        *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_16
void                            gdk_memory_texture_builder_set_stride           (GdkMemoryTextureBuilder        *self,
                                                                                 gsize                           stride);

GDK_AVAILABLE_IN_4_16
int                             gdk_memory_texture_builder_get_width            (GdkMemoryTextureBuilder        *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_16
void                            gdk_memory_texture_builder_set_width            (GdkMemoryTextureBuilder        *self,
                                                                                 int                             width);

GDK_AVAILABLE_IN_4_16
int                             gdk_memory_texture_builder_get_height           (GdkMemoryTextureBuilder        *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_16
void                            gdk_memory_texture_builder_set_height           (GdkMemoryTextureBuilder        *self,
                                                                                 int                             height);

GDK_AVAILABLE_IN_4_16
GdkMemoryFormat                 gdk_memory_texture_builder_get_format           (GdkMemoryTextureBuilder        *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_16
void                            gdk_memory_texture_builder_set_format           (GdkMemoryTextureBuilder        *self,
                                                                                 GdkMemoryFormat                 format);

GDK_AVAILABLE_IN_4_16
GdkColorState *                 gdk_memory_texture_builder_get_color_state      (GdkMemoryTextureBuilder        *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_16
void                            gdk_memory_texture_builder_set_color_state      (GdkMemoryTextureBuilder        *self,
                                                                                 GdkColorState                  *color_state);

GDK_AVAILABLE_IN_4_16
GdkTexture *                    gdk_memory_texture_builder_get_update_texture   (GdkMemoryTextureBuilder        *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_16
void                            gdk_memory_texture_builder_set_update_texture   (GdkMemoryTextureBuilder        *self,
                                                                                 GdkTexture                     *texture);

GDK_AVAILABLE_IN_4_16
cairo_region_t *                gdk_memory_texture_builder_get_update_region    (GdkMemoryTextureBuilder        *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_16
void                            gdk_memory_texture_builder_set_update_region    (GdkMemoryTextureBuilder        *self,
                                                                                 cairo_region_t                 *region);

GDK_AVAILABLE_IN_4_16
GdkTexture *                    gdk_memory_texture_builder_build                (GdkMemoryTextureBuilder        *self);

G_END_DECLS

