/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Benjamin Otte
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

#ifndef __GSK_TEXTURE_H__
#define __GSK_TEXTURE_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GSK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_TEXTURE (gsk_texture_get_type ())

#define GSK_TEXTURE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_TEXTURE, GskTexture))
#define GSK_IS_TEXTURE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_TEXTURE))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskTexture, g_object_unref)

typedef struct _GskTextureClass        GskTextureClass;


GDK_AVAILABLE_IN_3_90
GType gsk_texture_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_90
GskTexture *            gsk_texture_new_for_data               (const guchar    *data,
                                                                int              width,
                                                                int              height,
                                                                int              stride);
GDK_AVAILABLE_IN_3_90
GskTexture *            gsk_texture_new_for_pixbuf             (GdkPixbuf       *pixbuf);

GDK_AVAILABLE_IN_3_90
int                     gsk_texture_get_width                  (GskTexture      *texture);
GDK_AVAILABLE_IN_3_90
int                     gsk_texture_get_height                 (GskTexture      *texture);

GDK_AVAILABLE_IN_3_90
void                    gsk_texture_download                   (GskTexture      *texture,
                                                                guchar          *data,
                                                                gsize            stride);

G_END_DECLS

#endif /* __GSK_TEXTURE_H__ */
