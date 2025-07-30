#include "config.h"

#include "gdktestutils.h"

#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkmemorytextureprivate.h"
#include "gdk/gdktexturedownloaderprivate.h"

#include "gsk/gl/fp16private.h"

ChannelType
gdk_memory_format_get_channel_type (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_X8B8G8R8:
    case GDK_MEMORY_G8:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8:
    case GDK_MEMORY_G8_B8R8_420:
    case GDK_MEMORY_G8_R8B8_420:
    case GDK_MEMORY_G8_B8R8_422:
    case GDK_MEMORY_G8_R8B8_422:
    case GDK_MEMORY_G8_B8R8_444:
    case GDK_MEMORY_G8_R8B8_444:
    case GDK_MEMORY_G8_B8_R8_410:
    case GDK_MEMORY_G8_R8_B8_410:
    case GDK_MEMORY_G8_B8_R8_411:
    case GDK_MEMORY_G8_R8_B8_411:
    case GDK_MEMORY_G8_B8_R8_420:
    case GDK_MEMORY_G8_R8_B8_420:
    case GDK_MEMORY_G8_B8_R8_422:
    case GDK_MEMORY_G8_R8_B8_422:
    case GDK_MEMORY_G8_B8_R8_444:
    case GDK_MEMORY_G8_R8_B8_444:
    case GDK_MEMORY_G8B8G8R8_422:
    case GDK_MEMORY_G8R8G8B8_422:
    case GDK_MEMORY_R8G8B8G8_422:
    case GDK_MEMORY_B8G8R8G8_422:
      return CHANNEL_UINT_8;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_G10X6_B10X6R10X6_420:
    case GDK_MEMORY_G12X4_B12X4R12X4_420:
    case GDK_MEMORY_G16_B16R16_420:
    case GDK_MEMORY_X6G10_X6B10_X6R10_420:
    case GDK_MEMORY_X6G10_X6B10_X6R10_422:
    case GDK_MEMORY_X6G10_X6B10_X6R10_444:
    case GDK_MEMORY_X4G12_X4B12_X4R12_420:
    case GDK_MEMORY_X4G12_X4B12_X4R12_422:
    case GDK_MEMORY_X4G12_X4B12_X4R12_444:
    case GDK_MEMORY_G16_B16_R16_420:
    case GDK_MEMORY_G16_B16_R16_422:
    case GDK_MEMORY_G16_B16_R16_444:
      return CHANNEL_UINT_16;

    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_A16_FLOAT:
      return CHANNEL_FLOAT_16;

    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return CHANNEL_FLOAT_32;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return CHANNEL_UINT_8;
    }
}

/* return the number of color channels, ignoring alpha */
guint
gdk_memory_format_n_colors (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_X8B8G8R8:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_G8_B8R8_420:
    case GDK_MEMORY_G8_R8B8_420:
    case GDK_MEMORY_G8_B8R8_422:
    case GDK_MEMORY_G8_R8B8_422:
    case GDK_MEMORY_G8_B8R8_444:
    case GDK_MEMORY_G8_R8B8_444:
    case GDK_MEMORY_G10X6_B10X6R10X6_420:
    case GDK_MEMORY_G12X4_B12X4R12X4_420:
    case GDK_MEMORY_G16_B16R16_420:
    case GDK_MEMORY_G8_B8_R8_410:
    case GDK_MEMORY_G8_R8_B8_410:
    case GDK_MEMORY_G8_B8_R8_411:
    case GDK_MEMORY_G8_R8_B8_411:
    case GDK_MEMORY_G8_B8_R8_420:
    case GDK_MEMORY_G8_R8_B8_420:
    case GDK_MEMORY_G8_B8_R8_422:
    case GDK_MEMORY_G8_R8_B8_422:
    case GDK_MEMORY_G8_B8_R8_444:
    case GDK_MEMORY_G8_R8_B8_444:
    case GDK_MEMORY_G8B8G8R8_422:
    case GDK_MEMORY_G8R8G8B8_422:
    case GDK_MEMORY_R8G8B8G8_422:
    case GDK_MEMORY_B8G8R8G8_422:
    case GDK_MEMORY_X6G10_X6B10_X6R10_420:
    case GDK_MEMORY_X6G10_X6B10_X6R10_422:
    case GDK_MEMORY_X6G10_X6B10_X6R10_444:
    case GDK_MEMORY_X4G12_X4B12_X4R12_420:
    case GDK_MEMORY_X4G12_X4B12_X4R12_422:
    case GDK_MEMORY_X4G12_X4B12_X4R12_444:
    case GDK_MEMORY_G16_B16_R16_420:
    case GDK_MEMORY_G16_B16_R16_422:
    case GDK_MEMORY_G16_B16_R16_444:
      return 3;

    case GDK_MEMORY_G8:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16A16:
      return 1;

    case GDK_MEMORY_A8:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_A16_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return 0;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return TRUE;
    }
}

void
gdk_memory_pixel_print (const guchar          *data,
                        const GdkMemoryLayout *layout,
                        gsize                  x,
                        gsize                  y,
                        GString               *string)
{
  switch (layout->format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
      data += gdk_memory_layout_offset (layout, 0, x, y);
      g_string_append_printf (string, "%02X %02X %02X %02X", data[0], data[1], data[2], data[3]);
      break;

    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
      data += gdk_memory_layout_offset (layout, 0, x, y);
      g_string_append_printf (string, "%02X %02X %02X", data[0], data[1], data[2]);
      break;

    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
      data += gdk_memory_layout_offset (layout, 0, x, y);
      g_string_append_printf (string, "%02X %02X", data[0], data[1]);
      break;

    case GDK_MEMORY_A8:
    case GDK_MEMORY_G8:
      data += gdk_memory_layout_offset (layout, 0, x, y);
      g_string_append_printf (string, "%02X", data[0]);
      break;

    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_X8B8G8R8:
      data += gdk_memory_layout_offset (layout, 0, x, y);
      g_string_append_printf (string, "%02X %02X %02X", data[1], data[2], data[3]);
      break;


    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      {
        const guint16 *data16 = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%04X %04X %04X %04X", data16[0], data16[1], data16[2], data16[3]);
      }
      break;

    case GDK_MEMORY_R16G16B16:
      {
        const guint16 *data16 = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%04X %04X %04X", data16[0], data16[1], data16[2]);
      }
      break;

    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
      {
        const guint16 *data16 = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%04X %04X", data16[0], data16[1]);
      }
      break;

    case GDK_MEMORY_G16:
    case GDK_MEMORY_A16:
      {
        const guint16 *data16 = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%04X", data16[0]);
      }
      break;

    case GDK_MEMORY_R16G16B16_FLOAT:
      {
        const guint16 *data16 = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%f %f %f", half_to_float_one (data16[0]), half_to_float_one (data16[1]), half_to_float_one (data16[2]));
      }
      break;

    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      {
        const guint16 *data16 = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%f %f %f %f", half_to_float_one (data16[0]), half_to_float_one (data16[1]), half_to_float_one (data16[2]), half_to_float_one (data16[3]));
      }
      break;
    case GDK_MEMORY_A16_FLOAT:
      {
        const guint16 *data16 = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%f", half_to_float_one (data16[0]));
      }
      break;

    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      {
        const float *dataf = (const float *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%f %f %f %f", dataf[0], dataf[1], dataf[2], dataf[3]);
      }
      break;

    case GDK_MEMORY_R32G32B32_FLOAT:
      {
        const float *dataf = (const float *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%f %f %f", dataf[0], dataf[1], dataf[2]);
      }
      break;

    case GDK_MEMORY_A32_FLOAT:
      {
        const float *dataf = (const float *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        g_string_append_printf (string, "%f", dataf[0]);
      }
      break;

    /* multiplanar */
    case GDK_MEMORY_G8_B8R8_420:
    case GDK_MEMORY_G8_R8B8_420:
    case GDK_MEMORY_G8_B8R8_422:
    case GDK_MEMORY_G8_R8B8_422:
    case GDK_MEMORY_G8_B8R8_444:
    case GDK_MEMORY_G8_R8B8_444:
      {
        const guchar *y_data = data + gdk_memory_layout_offset (layout, 0, x, y);
        const guchar *uv_data = data + gdk_memory_layout_offset (layout,
                                                                 1,
                                                                 x - x % gdk_memory_format_get_plane_block_width (layout->format, 1),
                                                                 y - y % gdk_memory_format_get_plane_block_height (layout->format, 1));
        g_string_append_printf (string, "%02X %02X %02X", y_data[0], uv_data[0], uv_data[1]);
      }
      break;

    case GDK_MEMORY_G10X6_B10X6R10X6_420:
    case GDK_MEMORY_G12X4_B12X4R12X4_420:
    case GDK_MEMORY_G16_B16R16_420:
      {
        const guint16 *y_data = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        const guint16 *uv_data = (const guint16 *) (data + gdk_memory_layout_offset (layout,
                                                                                     1,
                                                                                     x - x % gdk_memory_format_get_plane_block_width (layout->format, 1),
                                                                                     y - y % gdk_memory_format_get_plane_block_height (layout->format, 1)));
        g_string_append_printf (string, "%04X %04X %04X", y_data[0], uv_data[0], uv_data[1]);
      }
      break;

    case GDK_MEMORY_G8_B8_R8_410:
    case GDK_MEMORY_G8_R8_B8_410:
    case GDK_MEMORY_G8_B8_R8_411:
    case GDK_MEMORY_G8_R8_B8_411:
    case GDK_MEMORY_G8_B8_R8_420:
    case GDK_MEMORY_G8_R8_B8_420:
    case GDK_MEMORY_G8_B8_R8_422:
    case GDK_MEMORY_G8_R8_B8_422:
    case GDK_MEMORY_G8_B8_R8_444:
    case GDK_MEMORY_G8_R8_B8_444:
      {
        const guchar *y_data = data + gdk_memory_layout_offset (layout, 0, x, y);
        const guchar *u_data = data + gdk_memory_layout_offset (layout,
                                                                1,
                                                                x - x % gdk_memory_format_get_plane_block_width (layout->format, 1),
                                                                y - y % gdk_memory_format_get_plane_block_height (layout->format, 1));
        const guchar *v_data = data + gdk_memory_layout_offset (layout,
                                                                2,
                                                                x - x % gdk_memory_format_get_plane_block_width (layout->format, 2),
                                                                y - y % gdk_memory_format_get_plane_block_height (layout->format, 2));
        g_string_append_printf (string, "%02X %02X %02X", y_data[0], u_data[0], v_data[0]);
      }
      break;

    case GDK_MEMORY_X6G10_X6B10_X6R10_420:
    case GDK_MEMORY_X6G10_X6B10_X6R10_422:
    case GDK_MEMORY_X6G10_X6B10_X6R10_444:
    case GDK_MEMORY_X4G12_X4B12_X4R12_420:
    case GDK_MEMORY_X4G12_X4B12_X4R12_422:
    case GDK_MEMORY_X4G12_X4B12_X4R12_444:
    case GDK_MEMORY_G16_B16_R16_420:
    case GDK_MEMORY_G16_B16_R16_422:
    case GDK_MEMORY_G16_B16_R16_444:
      {
        const guint16 *y_data = (const guint16 *) (data + gdk_memory_layout_offset (layout, 0, x, y));
        const guint16 *u_data = (const guint16 *) (data + gdk_memory_layout_offset (layout,
                                                                                    1,
                                                                                    x - x % gdk_memory_format_get_plane_block_width (layout->format, 1),
                                                                                    y - y % gdk_memory_format_get_plane_block_height (layout->format, 1)));
        const guint16 *v_data = (const guint16 *) (data + gdk_memory_layout_offset (layout,
                                                                                    2,
                                                                                    x - x % gdk_memory_format_get_plane_block_width (layout->format, 2),
                                                                                    y - y % gdk_memory_format_get_plane_block_height (layout->format, 2)));
        guint16 mask = gdk_memory_format_get_default_shader_op (layout->format) == GDK_SHADER_3_PLANES_10BIT_LSB ? 0x3FF :
                       gdk_memory_format_get_default_shader_op (layout->format) == GDK_SHADER_3_PLANES_12BIT_LSB ? 0xFFF : 0xFFFF;
        g_string_append_printf (string, "%04X %04X %04X", y_data[0] & mask, u_data[0] & mask, v_data[0] & mask);
      }
      break;

    case GDK_MEMORY_G8B8G8R8_422:
    case GDK_MEMORY_G8R8G8B8_422:
    case GDK_MEMORY_R8G8B8G8_422:
    case GDK_MEMORY_B8G8R8G8_422:
      data += gdk_memory_layout_offset (layout, 0, x & ~1, y);
      g_string_append_printf (string, "%02X %02X %02X %02X", data[0], data[1], data[2], data[3]);
      break;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
    }
}

gboolean
gdk_memory_pixel_equal (const guchar          *data1,
                        const GdkMemoryLayout *layout1,
                        const guchar          *data2,
                        const GdkMemoryLayout *layout2,
                        gsize                  x,
                        gsize                  y,
                        gboolean               accurate)
{
  g_assert (layout1->format == layout2->format);

  switch (layout1->format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_A8:
    case GDK_MEMORY_G8:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
      return memcmp (data1 + gdk_memory_layout_offset (layout1, 0, x, y),
                     data2 + gdk_memory_layout_offset (layout2, 0, x, y),
                     gdk_memory_format_get_plane_block_bytes (layout1->format, 0)) == 0;

    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_R8G8B8X8:
      return memcmp (data1 + gdk_memory_layout_offset (layout1, 0, x, y),
                     data2 + gdk_memory_layout_offset (layout2, 0, x, y),
                     3) == 0;

    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_X8B8G8R8:
      return memcmp (data1 + gdk_memory_layout_offset (layout1, 0, x, y) + 1,
                     data2 + gdk_memory_layout_offset (layout2, 0, x, y) + 1,
                     3) == 0;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_A16:
      {
        const guint16 *u1 = (const guint16 *) (data1 + gdk_memory_layout_offset (layout1, 0, x, y));
        const guint16 *u2 = (const guint16 *) (data2 + gdk_memory_layout_offset (layout2, 0, x, y));
        guint i;
        for (i = 0; i < gdk_memory_format_get_plane_block_bytes (layout1->format, 0) / sizeof (guint16); i++)
          {
            if (!G_APPROX_VALUE (u1[i], u2[i], accurate ? 1 : 256))
              return FALSE;
          }
      }
      return TRUE;

    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_A16_FLOAT:
      {
        const guint16 *pixel1 = (const guint16 *) (data1 + gdk_memory_layout_offset (layout1, 0, x, y));
        const guint16 *pixel2 = (const guint16 *) (data2 + gdk_memory_layout_offset (layout2, 0, x, y));
        guint i;
        for (i = 0; i < gdk_memory_format_get_plane_block_bytes (layout1->format, 0) / sizeof (guint16); i++)
          {
            float f1 = half_to_float_one (pixel1[i]);
            float f2 = half_to_float_one (pixel2[i]);
            if (!G_APPROX_VALUE (f1, f2, accurate ? 1./65535 : 1./255))
              return FALSE;
          }
      }
      return TRUE;

    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_A32_FLOAT:
      {
        const float *f1 = (const float *) (data1 + gdk_memory_layout_offset (layout1, 0, x, y));
        const float *f2 = (const float *) (data2 + gdk_memory_layout_offset (layout2, 0, x, y));
        guint i;
        for (i = 0; i < gdk_memory_format_get_plane_block_bytes (layout1->format, 0) / sizeof (float); i++)
          {
            if (!G_APPROX_VALUE (f1[i], f2[i], accurate ? 1./65535 : 1./255))
              return FALSE;
          }
      }
      return TRUE;

    /* multiplanar */
    case GDK_MEMORY_G8_B8R8_420:
    case GDK_MEMORY_G8_R8B8_420:
    case GDK_MEMORY_G8_B8R8_422:
    case GDK_MEMORY_G8_R8B8_422:
    case GDK_MEMORY_G8_B8R8_444:
    case GDK_MEMORY_G8_R8B8_444:
      return memcmp (data1 + gdk_memory_layout_offset (layout1, 0, x, y),
                     data2 + gdk_memory_layout_offset (layout2, 0, x, y),
                     gdk_memory_format_get_plane_block_bytes (layout1->format, 0)) == 0 &&
             memcmp (data1 + gdk_memory_layout_offset (layout1,
                                                       1,
                                                       x - x % gdk_memory_format_get_plane_block_width (layout1->format, 1),
                                                       y - y % gdk_memory_format_get_plane_block_height (layout1->format, 1)),
                     data2 + gdk_memory_layout_offset (layout2,
                                                       1,
                                                       x - x % gdk_memory_format_get_plane_block_width (layout2->format, 1),
                                                       y - y % gdk_memory_format_get_plane_block_height (layout2->format, 1)),
                     gdk_memory_format_get_plane_block_bytes (layout1->format, 1)) == 0;

    case GDK_MEMORY_G16_B16R16_420:
    case GDK_MEMORY_G10X6_B10X6R10X6_420:
    case GDK_MEMORY_G12X4_B12X4R12X4_420:
      {
        const guint16 *y1 = (const guint16 *) (data1 + gdk_memory_layout_offset (layout1, 0, x, y));
        const guint16 *y2 = (const guint16 *) (data2 + gdk_memory_layout_offset (layout2, 0, x, y));
        const guint16 *uv1 = (const guint16 *) (data1 + gdk_memory_layout_offset (layout1, 1, x & ~1, y & ~1));
        const guint16 *uv2 = (const guint16 *) (data2 + gdk_memory_layout_offset (layout2, 1, x & ~1, y & ~1));
        guint16 mask;

        if (!accurate)
          mask = 0xFF00;
        else if (layout1->format == GDK_MEMORY_G10X6_B10X6R10X6_420)
          mask = 0xFFC0;
        else if (layout1->format == GDK_MEMORY_G12X4_B12X4R12X4_420)
          mask = 0xFFF0;
        else
          mask = 0xFFFF;

        if (((y1[0] & mask) != (y2[0] & mask)) ||
            ((uv1[0] & mask) != (uv2[0] & mask)) ||
            ((uv1[1] & mask) != (uv2[1] & mask)))
          return FALSE;
      }
      return TRUE;

    case GDK_MEMORY_G8_B8_R8_410:
    case GDK_MEMORY_G8_R8_B8_410:
    case GDK_MEMORY_G8_B8_R8_411:
    case GDK_MEMORY_G8_R8_B8_411:
    case GDK_MEMORY_G8_B8_R8_420:
    case GDK_MEMORY_G8_R8_B8_420:
    case GDK_MEMORY_G8_B8_R8_422:
    case GDK_MEMORY_G8_R8_B8_422:
    case GDK_MEMORY_G8_B8_R8_444:
    case GDK_MEMORY_G8_R8_B8_444:
      return *(data1 + gdk_memory_layout_offset (layout1, 0, x, y)) == 
             *(data2 + gdk_memory_layout_offset (layout2, 0, x, y)) &&
             *(data1 + gdk_memory_layout_offset (layout1,
                                                 1,
                                                 x - x % gdk_memory_format_get_plane_block_width (layout1->format, 1),
                                                 y - y % gdk_memory_format_get_plane_block_height (layout1->format, 1))) ==
             *(data2 + gdk_memory_layout_offset (layout2,
                                                 1,
                                                 x - x % gdk_memory_format_get_plane_block_width (layout2->format, 1),
                                                  y - y % gdk_memory_format_get_plane_block_height (layout2->format, 1))) &&
             *(data1 + gdk_memory_layout_offset (layout1,
                                                 2,
                                                 x - x % gdk_memory_format_get_plane_block_width (layout1->format, 2),
                                                 y - y % gdk_memory_format_get_plane_block_height (layout1->format, 2))) ==
             *(data2 + gdk_memory_layout_offset (layout2,
                                                 2,
                                                 x - x % gdk_memory_format_get_plane_block_width (layout2->format, 2),
                                                 y - y % gdk_memory_format_get_plane_block_height (layout2->format, 2)));

    case GDK_MEMORY_X6G10_X6B10_X6R10_420:
    case GDK_MEMORY_X6G10_X6B10_X6R10_422:
    case GDK_MEMORY_X6G10_X6B10_X6R10_444:
    case GDK_MEMORY_X4G12_X4B12_X4R12_420:
    case GDK_MEMORY_X4G12_X4B12_X4R12_422:
    case GDK_MEMORY_X4G12_X4B12_X4R12_444:
    case GDK_MEMORY_G16_B16_R16_420:
    case GDK_MEMORY_G16_B16_R16_422:
    case GDK_MEMORY_G16_B16_R16_444:
      {
        const guint16 *y1 = (const guint16 *) (data1 + gdk_memory_layout_offset (layout1, 0, x, y));
        const guint16 *y2 = (const guint16 *) (data2 + gdk_memory_layout_offset (layout2, 0, x, y));
        const guint16 *u1 = (const guint16 *) (data1 + gdk_memory_layout_offset (layout1,
                                                                                 1,
                                                                                 x - x % gdk_memory_format_get_plane_block_width (layout1->format, 1),
                                                                                 y - y % gdk_memory_format_get_plane_block_height (layout1->format, 1)));
        const guint16 *u2 = (const guint16 *) (data2 + gdk_memory_layout_offset (layout2,
                                                                                 1,
                                                                                 x - x % gdk_memory_format_get_plane_block_width (layout2->format, 1),
                                                                                 y - y % gdk_memory_format_get_plane_block_height (layout2->format, 1)));
        const guint16 *v1 = (const guint16 *) (data1 + gdk_memory_layout_offset (layout1,
                                                                                 2,
                                                                                 x - x % gdk_memory_format_get_plane_block_width (layout1->format, 2),
                                                                                 y - y % gdk_memory_format_get_plane_block_height (layout1->format, 2)));
        const guint16 *v2 = (const guint16 *) (data2 + gdk_memory_layout_offset (layout2,
                                                                                 2,
                                                                                 x - x % gdk_memory_format_get_plane_block_width (layout2->format, 2),
                                                                                 y - y % gdk_memory_format_get_plane_block_height (layout2->format, 2)));

        guint16 mask = gdk_memory_format_get_default_shader_op (layout1->format) == GDK_SHADER_3_PLANES_10BIT_LSB ? 0x3FF :
                       gdk_memory_format_get_default_shader_op (layout1->format) == GDK_SHADER_3_PLANES_12BIT_LSB ? 0xFFF : 0xFFFF;

        if (!G_APPROX_VALUE ((y1[0] & mask), (y2[0] & mask), accurate ? 1 : 256) ||
            !G_APPROX_VALUE ((u1[0] & mask), (u2[0] & mask), accurate ? 1 : 256) ||
            !G_APPROX_VALUE ((v1[0] & mask), (v2[0] & mask), accurate ? 1 : 256))
          return FALSE;
      }
      return TRUE;

    case GDK_MEMORY_G8B8G8R8_422:
    case GDK_MEMORY_G8R8G8B8_422:
      data1 += gdk_memory_layout_offset (layout1, 0, x & ~1, y);
      data2 += gdk_memory_layout_offset (layout2, 0, x & ~1, y);
      return data1[1] == data2[1] &&
             data1[3] == data2[3] &&
             data1[2 * (y & 1)] == data2[2 * (y & 1)];

    case GDK_MEMORY_R8G8B8G8_422:
    case GDK_MEMORY_B8G8R8G8_422:
      data1 += gdk_memory_layout_offset (layout1, 0, x & ~1, y);
      data2 += gdk_memory_layout_offset (layout2, 0, x & ~1, y);
      return data1[0] == data2[0] &&
             data1[2] == data2[2] &&
             data1[1 + 2 * (y & 1)] == data2[1 + 2 * (y & 1)];

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

void
gdk_memory_layout_fudge (GdkMemoryLayout *layout,
                         gsize            align)
{
  gsize p, waste;

  waste = g_test_rand_bit () ? round_up (g_test_rand_int_range (0, 128), align) : 0;
  for (p = 0; p < gdk_memory_format_get_n_planes (layout->format); p++)
    {
      gsize extra_stride = g_test_rand_bit() ? round_up (g_test_rand_int_range (0, 16), align) : 0;

      layout->planes[p].offset += waste;
      layout->planes[p].stride += extra_stride;
      waste += extra_stride * layout->height;

      waste += g_test_rand_bit () ? round_up (g_test_rand_int_range (0, 128), align) : 0;
    }

  layout->size += waste;
}

void
texture_builder_init (TextureBuilder  *builder,
                      GdkMemoryFormat  format,
                      int              width,
                      int              height)
{
  gdk_memory_layout_init (&builder->layout, format, width, height, 1);
  gdk_memory_layout_fudge (&builder->layout, 1);
  builder->pixels = g_malloc0 (builder->layout.size);
}

GdkTexture *
texture_builder_finish (TextureBuilder *builder)
{
  GBytes *bytes;
  GdkTexture *texture;

  bytes = g_bytes_new_take (builder->pixels, builder->layout.size);
  texture = gdk_memory_texture_new_from_layout (bytes,
                                                &builder->layout,
                                                gdk_color_state_get_srgb (),
                                                NULL, NULL);
  g_bytes_unref (bytes);

  return texture;
}

void
texture_builder_draw_color (TextureBuilder              *builder,
                            const cairo_rectangle_int_t *area,
                            const GdkRGBA               *color)
{
  float data[4 * 4 * 4];
  GdkMemoryLayout data_layout;
  gsize xs, ys, block_width, block_height;

  g_assert_cmpint (area->x, >=, 0);
  g_assert_cmpint (area->x + area->width, <=, builder->layout.width);
  g_assert_cmpint (area->y, >=, 0);
  g_assert_cmpint (area->y + area->height, <=, builder->layout.height);
  g_assert_true (gdk_memory_format_is_block_boundary (builder->layout.format, area->x, area->y));
  g_assert_true (gdk_memory_format_is_block_boundary (builder->layout.format, area->width, area->height));

  block_width = gdk_memory_format_get_block_width (builder->layout.format);
  block_height = gdk_memory_format_get_block_height (builder->layout.format);
  g_assert (block_width * block_height * 4 <= G_N_ELEMENTS (data));

  for (ys = 0; ys < block_height; ys++)
    {
      for (xs = 0; xs < block_width; xs++)
        {
          data[4 * (ys * block_width + xs) + 0] = color->red;
          data[4 * (ys * block_width + xs) + 1] = color->green;
          data[4 * (ys * block_width + xs) + 2] = color->blue;
          data[4 * (ys * block_width + xs) + 3] = color->alpha;
        }
    }
  gdk_memory_layout_init (&data_layout, GDK_MEMORY_R32G32B32A32_FLOAT, block_width, block_height, 1);

  for (ys = 0; ys < area->height; ys += block_height)
    {
      for (xs = 0; xs < area->width; xs += block_width)
        {
          GdkMemoryLayout sub;

          gdk_memory_layout_init_sublayout (&sub,
                                            &builder->layout,
                                            &(cairo_rectangle_int_t) {
                                                area->x + xs, area->y + ys,
                                                block_width, block_height
                                            });

          gdk_memory_convert (builder->pixels,
                              &sub,
                              gdk_color_state_get_srgb (),
                              (guchar *) data,
                              &data_layout,
                              gdk_color_state_get_srgb ());
        }
    }
}

void
texture_builder_fill (TextureBuilder  *builder,
                      const GdkRGBA   *color)
{
  texture_builder_draw_color (builder,
                              &(cairo_rectangle_int_t) {
                                  0, 0,
                                  builder->layout.width,
                                  builder->layout.height
                              },
                              color);
}

void
texture_builder_draw_data (TextureBuilder        *builder,
                           gsize                  x,
                           gsize                  y,
                           const guchar          *data,
                           const GdkMemoryLayout *layout)
{
  GdkMemoryLayout sub;

  g_assert_cmpint (x + layout->width, <=, builder->layout.width);
  g_assert_cmpint (y + layout->height, <=, builder->layout.height);
  g_assert_true (gdk_memory_format_is_block_boundary (builder->layout.format, x, y));
  g_assert_true (gdk_memory_format_is_block_boundary (builder->layout.format, layout->width, layout->height));

  gdk_memory_layout_init_sublayout (&sub,
                                    &builder->layout,
                                    &(cairo_rectangle_int_t) {
                                        x, y,
                                        layout->width, layout->height
                                    });

  gdk_memory_convert (builder->pixels,
                      &sub,
                      gdk_color_state_get_srgb (),
                      (guchar *) data,
                      layout,
                      gdk_color_state_get_srgb ());
}

void
compare_textures (GdkTexture *texture1,
                  GdkTexture *texture2,
                  gboolean    accurate_compare)
{
  GdkTextureDownloader *downloader1, *downloader2;
  GBytes *bytes1, *bytes2;
  GdkMemoryLayout layout1, layout2;
  const guchar *data1, *data2;
  int width, height, x, y;
  GdkMemoryFormat format;
  GError *error = NULL;

  g_assert_cmpint (gdk_texture_get_width (texture1), ==, gdk_texture_get_width (texture2));
  g_assert_cmpint (gdk_texture_get_height (texture1), ==, gdk_texture_get_height (texture2));
  g_assert_cmpint (gdk_texture_get_format (texture1), ==, gdk_texture_get_format (texture2));

  format = gdk_texture_get_format (texture1);
  width = gdk_texture_get_width (texture1);
  height = gdk_texture_get_height (texture1);

  downloader1 = gdk_texture_downloader_new (texture1);
  gdk_texture_downloader_set_format (downloader1, format);
  bytes1 = gdk_texture_downloader_download_bytes_layout (downloader1, &layout1);
  g_assert_nonnull (bytes1);
  g_assert_true (gdk_memory_layout_is_valid (&layout1, &error));
  g_assert_no_error (error);
  gdk_texture_downloader_free (downloader1);

  downloader2 = gdk_texture_downloader_new (texture2);
  gdk_texture_downloader_set_format (downloader2, format);
  bytes2 = gdk_texture_downloader_download_bytes_layout (downloader2, &layout2);
  g_assert_nonnull (bytes2);
  g_assert_true (gdk_memory_layout_is_valid (&layout2, &error));
  g_assert_no_error (error);
  gdk_texture_downloader_free (downloader2);

  data1 = g_bytes_get_data (bytes1, NULL);
  data2 = g_bytes_get_data (bytes2, NULL);
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          if (!gdk_memory_pixel_equal (data1, &layout1, data2, &layout2, x, y, accurate_compare))
            {
              GString *msg = g_string_new (NULL);
              GEnumClass *enum_class;
              const char *format_name;

              enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);
              format_name = g_enum_get_value (enum_class, format)->value_nick;

              g_string_append_printf (msg, "%s (%u %u): ", format_name, x, y);
              gdk_memory_pixel_print (data1, &layout1, x, y, msg);
              g_string_append (msg, " != ");
              gdk_memory_pixel_print (data2, &layout2, x, y, msg);
              g_test_message ("%s", msg->str);
              g_string_free (msg, TRUE);
              g_test_fail ();
            }
        }
    }

  g_bytes_unref (bytes2);
  g_bytes_unref (bytes1);
}
