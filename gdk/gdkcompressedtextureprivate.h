/*
 * Copyright © 2021 Benjamin Otte
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

#ifndef __GDK_COMPRESSED_TEXTURE_PRIVATE_H__
#define __GDK_COMPRESSED_TEXTURE_PRIVATE_H__

#include "gdktextureprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_COMPRESSED_TEXTURE (gdk_compressed_texture_get_type ())

G_DECLARE_FINAL_TYPE (GdkCompressedTexture, gdk_compressed_texture, GDK, COMPRESSED_TEXTURE, GdkTexture)

GdkTexture *            gdk_compressed_texture_new_from_bytes   (GBytes                 *bytes,
                                                                 GError                **error);


G_END_DECLS

#endif /* __GDK_COMPRESSED_TEXTURE_PRIVATE_H__ */
