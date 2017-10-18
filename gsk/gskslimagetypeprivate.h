/* GTK - The GIMP Toolkit
 *
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#ifndef __GSK_SL_IMAGE_TYPE_PRIVATE_H__
#define __GSK_SL_IMAGE_TYPE_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"
#include "gskspvenumsprivate.h"

G_BEGIN_DECLS

struct _GskSlImageType 
{
  GskSlScalarType sampled_type;
  GskSpvDim dim;
  guint shadow :1;
  guint arrayed :1;
  guint multisampled :1;
  guint sampler :1;
};

gboolean                gsk_sl_image_type_supports_projection           (const GskSlImageType   *type,
                                                                         gboolean                extra_dim);
gboolean                gsk_sl_image_type_supports_lod                  (const GskSlImageType   *type);
gboolean                gsk_sl_image_type_supports_bias                 (const GskSlImageType   *type);
gboolean                gsk_sl_image_type_supports_offset               (const GskSlImageType   *type);
gboolean                gsk_sl_image_type_supports_gradient             (const GskSlImageType   *type);
gboolean                gsk_sl_image_type_supports_texture              (const GskSlImageType   *type);
gboolean                gsk_sl_image_type_supports_texel_fetch          (const GskSlImageType   *type);
gboolean                gsk_sl_image_type_needs_lod_argument            (const GskSlImageType   *type,
                                                                         gboolean                texel_fetch);

guint                   gsk_sl_image_type_get_dimensions                (const GskSlImageType   *type);
guint                   gsk_sl_image_type_get_lookup_dimensions         (const GskSlImageType   *type,
                                                                         gboolean                projection);

GskSlType *             gsk_sl_image_type_get_pixel_type                (const GskSlImageType   *type);

guint32                 gsk_sl_image_type_write_spv                     (const GskSlImageType   *type,
                                                                         GskSpvWriter           *writer);

guint                   gsk_sl_image_type_hash                          (gconstpointer           type);
gboolean                gsk_sl_image_type_equal                         (gconstpointer           a,
                                                                         gconstpointer           b);

G_END_DECLS

#endif /* __GSK_SL_IMAGE_TYPE_PRIVATE_H__ */
