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

#include "config.h"

#include "gskslimagetypeprivate.h"

#include "gsksltypeprivate.h"
#include "gskspvwriterprivate.h"

gboolean
gsk_sl_image_type_supports_projection (const GskSlImageType *type,
                                       gboolean              extra_dim)
{
  if (type->arrayed)
    return FALSE;

  if (type->multisampled)
    return FALSE;

  if (extra_dim && type->shadow)
    return FALSE;

  switch (type->dim)
  {
    case GSK_SPV_DIM_1_D:
    case GSK_SPV_DIM_2_D:
    case GSK_SPV_DIM_RECT:
      return TRUE;
    case GSK_SPV_DIM_3_D:
      return !extra_dim;
    case GSK_SPV_DIM_CUBE:
    case GSK_SPV_DIM_BUFFER:
    case GSK_SPV_DIM_SUBPASS_DATA:
    default:
      return FALSE;
  }
}

gboolean
gsk_sl_image_type_supports_lod (const GskSlImageType *type)
{
  if (type->multisampled)
    return FALSE;

  switch (type->dim)
  {
    case GSK_SPV_DIM_1_D:
      return TRUE;
    case GSK_SPV_DIM_2_D:
    case GSK_SPV_DIM_3_D:
      return !type->arrayed || !type->shadow;
    case GSK_SPV_DIM_CUBE:
      return !type->shadow;
    case GSK_SPV_DIM_RECT:
    case GSK_SPV_DIM_BUFFER:
    case GSK_SPV_DIM_SUBPASS_DATA:
    default:
      return FALSE;
  }
}

gboolean
gsk_sl_image_type_supports_bias (const GskSlImageType *type)
{
  if (type->multisampled)
    return FALSE;

  switch (type->dim)
  {
    case GSK_SPV_DIM_1_D:
    case GSK_SPV_DIM_3_D:
    case GSK_SPV_DIM_CUBE:
      return TRUE;
    case GSK_SPV_DIM_2_D:
      return !type->arrayed || !type->shadow;
    case GSK_SPV_DIM_RECT:
    case GSK_SPV_DIM_BUFFER:
    case GSK_SPV_DIM_SUBPASS_DATA:
    default:
      return FALSE;
  }
}

gboolean
gsk_sl_image_type_supports_offset (const GskSlImageType *type)
{
  if (type->multisampled)
    return FALSE;

  switch (type->dim)
  {
    case GSK_SPV_DIM_1_D:
    case GSK_SPV_DIM_2_D:
    case GSK_SPV_DIM_3_D:
    case GSK_SPV_DIM_RECT:
      return TRUE;
    case GSK_SPV_DIM_CUBE:
    case GSK_SPV_DIM_BUFFER:
    case GSK_SPV_DIM_SUBPASS_DATA:
    default:
      return FALSE;
  }
}

gboolean
gsk_sl_image_type_supports_gradient (const GskSlImageType *type)
{
  if (type->multisampled)
    return FALSE;

  if (type->dim == GSK_SPV_DIM_BUFFER)
    return FALSE;

  return TRUE;
}

gboolean
gsk_sl_image_type_supports_texel_fetch (const GskSlImageType *type)
{
  if (type->shadow)
    return FALSE;

  if (type->dim == GSK_SPV_DIM_CUBE)
    return FALSE;

  return TRUE;
}

gboolean
gsk_sl_image_type_supports_texture (const GskSlImageType *type)
{
  if (type->multisampled)
    return FALSE;

  if (type->dim == GSK_SPV_DIM_BUFFER)
    return FALSE;

  return TRUE;
}

gboolean
gsk_sl_image_type_needs_lod_argument (const GskSlImageType *type,
                                      gboolean              texel_fetch)
{
  if (type->multisampled)
    return TRUE;

  return type->dim != GSK_SPV_DIM_RECT
      && type->dim != GSK_SPV_DIM_BUFFER;
}

guint
gsk_sl_image_type_get_dimensions (const GskSlImageType *type)
{
  switch (type->dim)
  {
    case GSK_SPV_DIM_1_D:
    case GSK_SPV_DIM_BUFFER:
      return 1;

    case GSK_SPV_DIM_2_D:
    case GSK_SPV_DIM_RECT:
    case GSK_SPV_DIM_SUBPASS_DATA:
      return 2;

    case GSK_SPV_DIM_3_D:
    case GSK_SPV_DIM_CUBE:
      return 3;

    default:
      g_assert_not_reached ();
      return 1;
  }
}

guint
gsk_sl_image_type_get_lookup_dimensions (const GskSlImageType *type,
                                         gboolean              projection)
{
  guint result = gsk_sl_image_type_get_dimensions (type);

  if (type->arrayed)
    result++;

  if (type->shadow)
    {
      /* because GLSL is GLSL */
      result = MAX (result, 2);
      result++;
    }

  if (projection)
    result++;

  return result;
}

GskSlType *
gsk_sl_image_type_get_pixel_type (const GskSlImageType *type)
{
  if (type->shadow)
    return gsk_sl_type_get_scalar (type->sampled_type);
  else
    return gsk_sl_type_get_vector (type->sampled_type, 4);
}

guint32
gsk_sl_image_type_write_spv (const GskSlImageType *type,
                             GskSpvWriter         *writer)
{
  guint32 sampled_type_id;

  sampled_type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (type->sampled_type));
  return gsk_spv_writer_type_image (writer,
                                    sampled_type_id,
                                    type->dim,
                                    type->shadow,
                                    type->arrayed,
                                    type->multisampled,
                                    2 - type->sampler,
                                    GSK_SPV_IMAGE_FORMAT_UNKNOWN,
                                    -1);
}

guint
gsk_sl_image_type_hash (gconstpointer type)
{
  const GskSlImageType *image = type;

  return  image->sampled_type
       | (image->dim << 8)
       | (image->shadow << 16)
       | (image->arrayed << 17)
       | (image->multisampled << 18)
       | (image->sampler << 19);
}

gboolean
gsk_sl_image_type_equal (gconstpointer a,
                         gconstpointer b)
{
  const GskSlImageType *ia = a;
  const GskSlImageType *ib = b;

  return ia->sampled_type == ib->sampled_type
      && ia->dim == ib->dim
      && ia->shadow == ib->shadow
      && ia->arrayed == ib->arrayed
      && ia->multisampled == ib->multisampled
      && ia->sampler == ib->sampler;
}

