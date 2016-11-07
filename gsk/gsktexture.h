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

#define GSK_IS_TEXTURE(texture) ((texture) != NULL)

GDK_AVAILABLE_IN_3_90
GType gsk_texture_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_90
GskTexture *            gsk_texture_ref                        (GskTexture      *texture);
GDK_AVAILABLE_IN_3_90
void                    gsk_texture_unref                      (GskTexture      *texture);

GDK_AVAILABLE_IN_3_90
GskTexture *            gsk_texture_new_for_data               (GskRenderer     *renderer,
                                                                const guchar    *data,
                                                                int              width,
                                                                int              height,
                                                                int              stride);
GDK_AVAILABLE_IN_3_90
GskTexture *            gsk_texture_new_for_pixbuf             (GskRenderer     *renderer,
                                                                GdkPixbuf       *pixbuf);

GDK_AVAILABLE_IN_3_90
GskRenderer *           gsk_texture_get_renderer               (GskTexture      *texture);
GDK_AVAILABLE_IN_3_90
int                     gsk_texture_get_width                  (GskTexture      *texture);
GDK_AVAILABLE_IN_3_90
int                     gsk_texture_get_height                 (GskTexture      *texture);

G_END_DECLS

#endif /* __GSK_TEXTURE_H__ */
