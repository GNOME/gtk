#include <gtk.h>

static void
test_srgb (void)
{
  GdkColorState *srgb;
  GdkColorState *srgb_linear;

  srgb = gdk_color_state_get_srgb ();
  srgb_linear = gdk_color_state_get_srgb_linear ();

  g_assert_true (gdk_color_state_equal (srgb, srgb));
  g_assert_true (gdk_color_state_equal (srgb_linear, srgb_linear));
  g_assert_false (gdk_color_state_equal (srgb, srgb_linear));
}

typedef enum
{
  PNG,
  TIFF,
} ImageFormat;

static const char *
image_format_name (ImageFormat image_format)
{
  const char *names[] = { "png", "tiff" };
  return names[image_format];
}

static const char *
color_state_name (GdkColorState *color_state)
{
  if (color_state == gdk_color_state_get_srgb ())
    return "srgb";
  else if (color_state == gdk_color_state_get_srgb_linear ())
    return "srgb-linear";
  else
    return "unknown";
}

static const char *
memory_format_name (GdkMemoryFormat format)
{
  if (format == GDK_MEMORY_R8G8B8A8)
    return "RGBA8";
  else if (format == GDK_MEMORY_R16G16B16A16)
    return "RGBA16";
  else
    return "unknown";
}

typedef struct
{
  GdkMemoryFormat format;
  GdkColorState *color_state;
  ImageFormat image_format;
} ImageTest;

static GBytes *
get_image_data (GdkMemoryFormat format)
{
  guchar *data;
  gsize size;

  switch ((int) format)
    {
    case GDK_MEMORY_R8G8B8A8:
      data = g_new (guchar, 4);
      size = 4;
      data[0] = 128;
      data[1] = 10;
      data[2] = 245;
      data[3] = 255;
      break;
    case GDK_MEMORY_R16G16B16A16:
      {
        guint16 *data16;

        data16 = g_new (guint16, 4);

        data16[0] = (128 << 8) + 10;
        data16[1] = (10 << 8) + 120;
        data16[2] = (245 << 8) + 140;
        data16[3] = (255 << 8) + 245;

        data = (guchar *) data16;
        size = 8;
      }
      break;
    default:
      g_assert_not_reached ();
    }

  return g_bytes_new (data, size);
}

static void
test_image_roundtrip (gconstpointer data)
{
  const ImageTest *test = data;
  gsize bpp;
  GBytes *bytes;
  GdkTexture *texture;
  GBytes *saved;
  GdkTexture *texture2;
  GError *error = NULL;

  bytes = get_image_data (test->format);
  bpp = g_bytes_get_size (bytes);

  texture = gdk_memory_texture_new_with_color_state (1, 1, test->format, test->color_state, bytes, bpp);

  switch (test->image_format)
    {
    case PNG:
      saved = gdk_texture_save_to_png_bytes (texture);
      texture2 = gdk_texture_new_from_bytes (saved, &error);
      g_assert_no_error (error);
      g_bytes_unref (saved);
      break;
    case TIFF:
      saved = gdk_texture_save_to_tiff_bytes (texture);
      texture2 = gdk_texture_new_from_bytes (saved, &error);
      g_assert_no_error (error);
      g_bytes_unref (saved);
      break;
    default:
      g_assert_not_reached ();
    }

  g_assert_true (gdk_color_state_equal (gdk_texture_get_color_state (texture),
                                        gdk_texture_get_color_state (texture2)));
  g_assert_true (gdk_texture_get_format (texture) == gdk_texture_get_format (texture2));

  g_object_unref (texture2);
  g_object_unref (texture);

  g_bytes_unref (bytes);
}

static void
add_image_roundtrip_tests (void)
{
  ImageFormat image_formats[] = { PNG, TIFF };
  GdkColorState *color_states[2];
  GdkMemoryFormat formats[] = { GDK_MEMORY_R8G8B8A8, GDK_MEMORY_R16G16B16A16 };

  color_states[0] = gdk_color_state_get_srgb ();
  color_states[1] = gdk_color_state_get_srgb_linear ();

  for (int i = 0; i < G_N_ELEMENTS (image_formats); i++)
    for (int j = 0; j < G_N_ELEMENTS (color_states); j++)
      for (int k = 0; k < G_N_ELEMENTS (formats); k++)
        {
          char *path = g_strdup_printf ("/colorstate/image/%s/%s/%s",
                                        image_format_name (image_formats[i]),
                                        color_state_name (color_states[j]),
                                        memory_format_name (formats[k]));

          ImageTest *data = g_new (ImageTest, 1);
          data->image_format = image_formats[i];
          data->color_state = color_states[j];
          data->format = formats[k];

          g_test_add_data_func_full (path, data, test_image_roundtrip, g_free);

          g_free (path);
        }

}

int
main (int argc, char *argv[])
{
  gtk_init ();

  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/colorstate/srgb", test_srgb);
  add_image_roundtrip_tests ();

  return g_test_run ();
}
