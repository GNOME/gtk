#include "heif-loader.h"
#include <gtk/gtk.h>
#include <libheif/heif.h>

static void
describe_nclx_color_profile (const struct heif_color_profile_nclx *nclx,
                             GString                              *s)
{
  if (nclx->color_primaries == heif_color_primaries_unspecified)
    return;

  if (nclx->color_primaries == heif_color_primaries_ITU_R_BT_709_5)
    {
      if (nclx->transfer_characteristics == heif_transfer_characteristic_IEC_61966_2_1 &&
          (nclx->matrix_coefficients == heif_matrix_coefficients_ITU_R_BT_470_6_System_B_G ||
           nclx->matrix_coefficients == heif_matrix_coefficients_ITU_R_BT_601_6))
        {
          g_string_append (s, "sRGB");
          return;
        }

      if (nclx->transfer_characteristics == heif_transfer_characteristic_linear &&
          (nclx->matrix_coefficients == heif_matrix_coefficients_ITU_R_BT_470_6_System_B_G ||
           nclx->matrix_coefficients == heif_matrix_coefficients_ITU_R_BT_601_6))
        {
          g_string_append (s, "sRGB linear");
          return;
        }
    }

 if (nclx->color_primaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0)
    {
      if (nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ &&
          nclx->matrix_coefficients == heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance)
        {
          g_string_append (s, "BT.2020 PQ");
          return;
        }

      if (nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG &&
          nclx->matrix_coefficients == heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance)
        {
          g_string_append (s, "BT.2020 HLG");
          return;
        }
    }

  if (nclx->color_primaries == heif_color_primaries_SMPTE_EG_432_1)
    {
      if (nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ)
        {
          g_string_append (s, "P3 PQ");
          return;
        }
    }

  g_string_append_printf (s, "%d/%d/%d",
                          nclx->color_primaries,
                          nclx->matrix_coefficients,
                          nclx->transfer_characteristics);
}

GdkTexture *
load_heif_image (const char  *resource_path,
                 GString     *details,
                 GError     **error)
{
  struct heif_context *ctx;
  struct heif_error err;
  struct heif_image_handle *handle = NULL;
  struct heif_image *image = NULL;
  enum heif_chroma chroma;
  GdkMemoryFormat format;
  const char *format_name;
  uint8_t *data = NULL;
  gsize size;
  int width, height, bpp, stride, bits;
  GBytes *bytes;
  GdkTexture *texture = NULL;
  GdkColorSpace *color_space = NULL;
  char *profile_type = NULL;

  ctx = heif_context_alloc ();

  bytes = g_resources_lookup_data (resource_path, 0, NULL);
  data = (uint8_t *) g_bytes_get_data (bytes, &size);

  err = heif_context_read_from_memory (ctx, data, size, NULL);

  g_bytes_unref (bytes);

  if (err.code != heif_error_Ok)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, err.message);
      goto out;
    }

  err = heif_context_get_primary_image_handle (ctx, &handle);
  if (err.code != heif_error_Ok)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, err.message);
      goto out;
    }

  switch (heif_image_handle_get_color_profile_type (handle))
    {
    case heif_color_profile_type_not_present:
      break;
    case heif_color_profile_type_rICC:
    case heif_color_profile_type_prof:
      {
        guchar *icc_data;
        gsize icc_size;

        profile_type = g_strdup ("icc");
        icc_size = heif_image_handle_get_raw_color_profile_size (handle);
        icc_data = g_new0 (guchar, icc_size);
        err = heif_image_handle_get_raw_color_profile (handle, icc_data);
        if (err.code == heif_error_Ok)
          {
            bytes = g_bytes_new (icc_data, icc_size);
            color_space = gdk_color_space_new_from_icc_profile (bytes, NULL);
            g_bytes_unref (bytes);
          }
        g_free (icc_data);
      }
      break;
    case heif_color_profile_type_nclx:
      {
        struct heif_color_profile_nclx *nclx = NULL;

        err = heif_image_handle_get_nclx_color_profile (handle, &nclx);
        if (err.code == heif_error_Ok)
          {
            GString *s;

            s = g_string_new ("");
            describe_nclx_color_profile (nclx, s);
            profile_type = g_string_free (s, FALSE);
            color_space = gdk_color_space_new_from_cicp (nclx->color_primaries,
                                                         nclx->transfer_characteristics,
                                                         0,
                                                         TRUE);
            heif_nclx_color_profile_free (nclx);
          }
      }
      break;
    default:
      g_assert_not_reached ();
    }

  g_string_append_printf (details, "%d × %d pixels\n%d bits of luma, %d bits of chroma%s\n",
                          heif_image_handle_get_width (handle),
                          heif_image_handle_get_height (handle),
                          heif_image_handle_get_luma_bits_per_pixel (handle),
                          heif_image_handle_get_chroma_bits_per_pixel (handle),
                          heif_image_handle_has_alpha_channel (handle) ? ", with alpha" : "");

  if (profile_type)
    {
      g_string_append_printf (details, "color profile: %s\n", profile_type);
      g_free (profile_type);
    }

  if (color_space == NULL)
    color_space = g_object_ref (gdk_color_space_get_srgb ());

  if (heif_image_handle_has_alpha_channel (handle))
    {
      if (heif_image_handle_get_luma_bits_per_pixel (handle) > 8 ||
          heif_image_handle_get_chroma_bits_per_pixel (handle) > 8)
        {
          chroma = heif_chroma_interleaved_RRGGBBAA_BE;
          format = GDK_MEMORY_R16G16B16A16;
          format_name = "R<sub>16</sub>G<sub>16</sub>B<sub>16</sub>A<sub>16</sub>";
        }
      else
        {
          chroma = heif_chroma_interleaved_RGBA;
          format = GDK_MEMORY_R8G8B8A8;
          format_name = "R<sub>8</sub>G<sub>8</sub>B<sub>8</sub>A<sub>8</sub>";
        }
    }
  else
    {
      if (heif_image_handle_get_luma_bits_per_pixel (handle) > 8 ||
          heif_image_handle_get_chroma_bits_per_pixel (handle) > 8)
        {
          chroma = heif_chroma_interleaved_RRGGBB_BE;
          format = GDK_MEMORY_R16G16B16;
          format_name = "R<sub>16</sub>G<sub>16</sub>B<sub>16</sub>";
        }
      else
        {
          chroma = heif_chroma_interleaved_RGB;
          format = GDK_MEMORY_R8G8B8;
          format_name = "R<sub>8</sub>G<sub>8</sub>B<sub>8</sub>";
        }
    }

  err = heif_decode_image (handle, &image, heif_colorspace_RGB, chroma, NULL);
  if (err.code != heif_error_Ok)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, err.message);
      goto out;
    }

  width = heif_image_get_width (image, heif_channel_interleaved);
  height = heif_image_get_height (image, heif_channel_interleaved);
  bpp = heif_image_get_bits_per_pixel (image, heif_channel_interleaved);
  data = (uint8_t*)heif_image_get_plane_readonly (image, heif_channel_interleaved, &stride);
  bits = heif_image_get_bits_per_pixel_range (image, heif_channel_interleaved);

  g_string_append_printf (details, "texture format %s", format_name);

  if (format == GDK_MEMORY_R16G16B16A16 || format == GDK_MEMORY_R16G16B16)
    {
      /* Shift image data to full 16bit range */
      int shift = 16 - bits;
      if (shift > 0)
        {
          for (int y = 0; y < height; ++y)
            {
              uint8_t *row = data + y * stride;

              for (int x = 0; x < stride; x += 2)
                {
                  uint8_t* p = &row[x];
                  int v = (p[0] << 8) | p[1];
                  v = (v << shift) | (v >> (16 - shift));
                  p[1] = (uint8_t) (v >> 8);
                  p[0] = (uint8_t) (v & 0xFF);
                }
            }
        }
    }

  bytes = g_bytes_new_with_free_func (data, height * stride * bpp, (GDestroyNotify)heif_image_release, image);

  image = NULL;

  texture = gdk_memory_texture_new_with_color_space (width, height,
                                                     format, color_space,
                                                     bytes, stride);
  g_bytes_unref (bytes);
  g_object_unref (color_space);

out:
  heif_context_free (ctx);
  if (handle)
    heif_image_handle_release (handle);
  if (image)
    heif_image_release (image);

  return texture;
}
