#include <gtk/gtk.h>

#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkmemorytextureprivate.h"

#include "testsuite/gdk/gdktestutils.h"

static gpointer
encode (GdkMemoryFormat format,
        gsize           size,
        gsize           lod_level,
        gboolean        linear)
{
  g_assert (lod_level < (1 << 4));
  return GSIZE_TO_POINTER ((((size * (1 << 4) + lod_level) * GDK_MEMORY_N_FORMATS + format) << 1) + linear);
}

static void
decode (gconstpointer    data,
        GdkMemoryFormat *format,
        gsize           *size,
        gsize           *lod_level,
        gboolean        *linear)
{
  gsize s = GPOINTER_TO_SIZE (data);
  *linear = s & 1;
  s >>= 1;
  *format = s % GDK_MEMORY_N_FORMATS;
  s /= GDK_MEMORY_N_FORMATS;
  *lod_level = s % (1 << 4);
  s /= 1 << 4;
  *size = s;
}

static void
create_random_color (GdkMemoryFormat  format,
                     GdkRGBA         *color)
{
  switch (gdk_memory_format_get_channel_type (format))
    {
    case CHANNEL_UINT_8:
    case CHANNEL_UINT_16:
      color->red = g_test_rand_int_range (0, 4) / 3.0;
      if (gdk_memory_format_n_colors (format) > 1)
        {
          color->green = g_test_rand_int_range (0, 4) / 3.0;
          color->blue = g_test_rand_int_range (0, 4) / 3.0;
        }
      else
        {
          color->green = color->blue = color->red;
        }
      if (gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_OPAQUE)
        color->alpha = 1.0;
      else
        color->alpha = g_test_rand_int_range (0, 6) / 5.0;
      break;

    case CHANNEL_FLOAT_16:
    case CHANNEL_FLOAT_32:
      color->red = g_test_rand_int_range (0, 5) / 4.0;
      if (gdk_memory_format_n_colors (format) > 1)
        {
          color->green = g_test_rand_int_range (0, 5) / 4.0;
          color->blue = g_test_rand_int_range (0, 5) / 4.0;
        }
      else
        {
          color->green = color->blue = color->red;
        }
      if (gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_OPAQUE)
        color->alpha = 1.0;
      else
        color->alpha = g_test_rand_int_range (0, 5) / 4.0;
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
test_mipmap_pixels (gconstpointer data_)
{
  GdkMemoryFormat format;
  TextureBuilder builder;
  GdkTexture *ref, *large, *mipmap;
  GdkRGBA color;
  GBytes *bytes;
  gsize size, lod_level;
  gboolean linear;
  GdkMemoryLayout layout;
  guchar *data;
  gboolean accurate;

  decode (data_, &format, &size, &lod_level, &linear);

  texture_builder_init (&builder,
                        gdk_memory_format_get_mipmap_format (format),
                        size,
                        size);

  if (linear)
    {
      float pixels[4 * 8 * 8];
      GdkMemoryLayout pixels_layout = GDK_MEMORY_LAYOUT_SIMPLE (GDK_MEMORY_R32G32B32A32_FLOAT,
                                                                8, 8,
                                                                sizeof (float) * 4 * 8);
      gsize x, y, block_size;

      create_random_color (format, &color);

      texture_builder_fill (&builder, &color);
      ref = texture_builder_finish (&builder);

      block_size = MAX (gdk_memory_format_get_block_width (format),
                        gdk_memory_format_get_block_height (format));

      for (y = 0; y < 8; y++)
        {
          for (x = 0; x < 8; x++)
            {
              gboolean light = (x % (2 * block_size) < block_size) ^ (y % (2 * block_size) < block_size);
              gsize i = (8 * y + x) * 4;

              if (color.red < 0.5)
                pixels[i + 0] = light ? color.red * 2 : 0;
              else
                pixels[i + 0] = light ? 1.0 : 2 * color.red - 1.0;
              if (color.green < 0.5)
                pixels[i + 1] = light ? color.green * 2 : 0;
              else
                pixels[i + 1] = light ? 1.0 : 2 * color.green - 1.0;
              if (color.blue < 0.5)
                pixels[i + 2] = light ? color.blue * 2 : 0;
              else
                pixels[i + 2] = light ? 1.0 : 2 * color.blue - 1.0;
              pixels[i + 3] = color.alpha;
            }
        }

      texture_builder_init (&builder,
                            format,
                            size << lod_level,
                            size << lod_level);

      for (y = 0; y < size << lod_level; y += 8)
        {
          for (x = 0; x < size << lod_level; x += 8)
            {
              texture_builder_draw_data (&builder,
                                         x, y,
                                         (const guchar *) pixels,
                                         &pixels_layout);
            }
        }

      large = texture_builder_finish (&builder);
    }
  else
    {
      gsize x, y, n = (1 << lod_level);
      /* red with a yellow pixel in the top left */
      float pick_me[3 * 4 * 4] = { 1, 1, 0,  1, 0, 0,  1, 0, 0,  1, 0, 0, 
                                   1, 0, 0,  1, 0, 0,  1, 0, 0,  1, 0, 0,
                                   1, 0, 0,  1, 0, 0,  1, 0, 0,  1, 0, 0,
                                   1, 0, 0,  1, 0, 0,  1, 0, 0,  1, 0, 0 };
      GdkMemoryLayout pick_layout = GDK_MEMORY_LAYOUT_SIMPLE (GDK_MEMORY_R32G32B32_FLOAT,
                                                              gdk_memory_format_get_block_width (format),
                                                              gdk_memory_format_get_block_height (format),
                                                              sizeof (float) * 3 * 4);

      texture_builder_fill (&builder, &(GdkRGBA) { 1, 1, 0, 1 });
      ref = texture_builder_finish (&builder);

      texture_builder_init (&builder,
                            format,
                            size << lod_level,
                            size << lod_level);
      create_random_color (format, &color);
      texture_builder_fill (&builder, &color);

      for (y = n / 2; y < size * n; y += n)
        {
          for (x = n / 2; x < size * n; x += n)
            {
              texture_builder_draw_data (&builder,
                                         x, y,
                                         (const guchar *) pick_me,
                                         &pick_layout);
            }
        }

      large = texture_builder_finish (&builder);
    }

#if 0
  /* If you want to understand what images this test generates: Save the images! */
  gdk_texture_save_to_png (large, "large.png");
  gdk_texture_save_to_png (ref, "ref.png");
#endif

  gdk_memory_layout_init (&layout,
                          gdk_memory_format_get_mipmap_format (format),
                          size,
                          size,
                          1);
  data = g_malloc (layout.size);
  gdk_memory_mipmap (data,
                     &layout,
                     g_bytes_get_data (gdk_memory_texture_get_bytes (GDK_MEMORY_TEXTURE (large)), NULL),
                     gdk_memory_texture_get_layout (GDK_MEMORY_TEXTURE (large)),
                     lod_level,
                     linear);
  bytes = g_bytes_new_take (data, layout.size);
  mipmap = gdk_memory_texture_new_from_layout (bytes, &layout, gdk_color_state_get_srgb (), NULL, NULL);
  g_bytes_unref (bytes);
  
  if (linear ||
      gdk_memory_format_get_default_shader_op (format) == GDK_SHADER_3_PLANES_10BIT_LSB ||
      gdk_memory_format_get_default_shader_op (format) == GDK_SHADER_3_PLANES_12BIT_LSB)
    accurate = FALSE;
  else
    accurate = TRUE;

  compare_textures (ref, mipmap, accurate);

  g_object_unref (mipmap);
  g_object_unref (large);
  g_object_unref (ref);
}

static void
test_mipmap_simple (gconstpointer data_)
{
  GdkMemoryFormat format;
  TextureBuilder builder;
  GdkTexture *ref, *large, *mipmap;
  GdkRGBA color;
  GBytes *bytes;
  gsize size, lod_level;
  gboolean linear;
  GdkMemoryLayout layout;
  guchar *data;
  gboolean accurate;

  decode (data_, &format, &size, &lod_level, &linear);

  create_random_color (format, &color);

  texture_builder_init (&builder,
                        gdk_memory_format_get_mipmap_format (format),
                        size,
                        size);
  texture_builder_fill (&builder, &color);
  ref = texture_builder_finish (&builder);

  texture_builder_init (&builder,
                        format,
                        size << lod_level,
                        size << lod_level);
  texture_builder_fill (&builder, &color);
  large = texture_builder_finish (&builder);

  gdk_memory_layout_init (&layout,
                          gdk_memory_format_get_mipmap_format (format),
                          size,
                          size,
                          1);
  data = g_malloc (layout.size);
  gdk_memory_mipmap (data,
                     &layout,
                     g_bytes_get_data (gdk_memory_texture_get_bytes (GDK_MEMORY_TEXTURE (large)), NULL),
                     gdk_memory_texture_get_layout (GDK_MEMORY_TEXTURE (large)),
                     lod_level,
                     linear);
  bytes = g_bytes_new_take (data, layout.size);
  mipmap = gdk_memory_texture_new_from_layout (bytes, &layout, gdk_color_state_get_srgb (), NULL, NULL);
  g_bytes_unref (bytes);
  
  if (linear ||
      gdk_memory_format_get_default_shader_op (format) == GDK_SHADER_3_PLANES_10BIT_LSB ||
      gdk_memory_format_get_default_shader_op (format) == GDK_SHADER_3_PLANES_12BIT_LSB)
    accurate = FALSE;
  else
    accurate = TRUE;

  compare_textures (ref, mipmap, accurate);

  g_object_unref (mipmap);
  g_object_unref (large);
  g_object_unref (ref);
}

static void
add_test (const char    *name,
          GTestDataFunc  func,
          unsigned       min_lod)
{
  GdkMemoryFormat format;
  GEnumClass *enum_class;
  unsigned size, more_lod, linear;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  size = 4;

  for (more_lod = 0; more_lod < 6; more_lod += 3)
    {
      for (linear = FALSE; linear <= TRUE; linear++)
        {
          for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
            {
              char *test_name = g_strdup_printf ("/mipmap/%s/%s/%s/%ux%u/lod-%u",
                                                 name,
                                                 linear ? "linear" : "nearest",
                                                 g_enum_get_value (enum_class, format)->value_nick,
                                                 size, size,
                                                 min_lod + more_lod);
              g_test_add_data_func_full (test_name, encode (format, size, min_lod + more_lod, linear), func, NULL);
              g_free (test_name);
            }
        }
    }
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  add_test ("simple", test_mipmap_simple, 1);
  add_test ("pixels", test_mipmap_pixels, 3);

  return g_test_run ();
}
