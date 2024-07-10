#include <gdk/gdk.h>
#include <gdk/gdkcolorstateprivate.h>
#include <gdk/gdkmemoryformatprivate.h>
#include <math.h>

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

static float
image_distance (const guchar *data,
                const guchar *data2,
                gsize         width,
                gsize         height,
                gsize         stride)
{
  float dist = 0;

  for (gsize i = 0; i < height; i++)
    {
      const float *p = (const float *) (data + i * stride);
      const float *p2 = (const float *) (data2 + i * stride);

      for (gsize j = 0; j < width; j++)
        {
          float dr, dg, db, da;

          dr = p[4 * j + 0] - p2[4 * j + 0];
          dg = p[4 * j + 1] - p2[4 * j + 1];
          db = p[4 * j + 2] - p2[4 * j + 2];
          da = p[4 * j + 3] - p2[4 * j + 3];

          dist = MAX (dist, dr * dr + dg * dg + db * db + da * da);
        }
    }

  return sqrt (dist);
}

static void
test_convert (gconstpointer testdata)
{
  GdkColorState *cs;
  char *path;
  GdkTexture *texture;
  GdkTextureDownloader *downloader;
  GError *error = NULL;
  GBytes *bytes;
  const guchar *data;
  guchar *data2;
  gsize width, height;
  gsize size;
  gsize stride;

  cs = gdk_color_state_get_by_id ((GdkColorStateId) GPOINTER_TO_UINT (testdata));

  path = g_test_build_filename (G_TEST_DIST, "image-data", "image.png", NULL);

  texture = gdk_texture_new_from_filename (path, &error);
  g_assert_no_error (error);

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R32G32B32A32_FLOAT);

  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
  data = g_bytes_get_data (bytes, &size);
  data2 = g_memdup2 (data, size);

  gdk_memory_convert_color_state (data2,
                                  stride,
                                  GDK_MEMORY_R32G32B32A32_FLOAT,
                                  gdk_texture_get_color_state (texture),
                                  cs,
                                  width,
                                  height);

  gdk_memory_convert_color_state (data2,
                                  stride,
                                  GDK_MEMORY_R32G32B32A32_FLOAT,
                                  cs,
                                  gdk_texture_get_color_state (texture),
                                  width,
                                  height);

  g_assert_true (image_distance (data, data2, width, height, stride) < 0.001);

  g_free (data2);
  g_bytes_unref (bytes);
  gdk_texture_downloader_free (downloader);
  g_object_unref (texture);
  g_free (path);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/colorstate/srgb", test_srgb);
  g_test_add_data_func ("/colorstate/convert/srgb<->srgb-linear", GUINT_TO_POINTER (GDK_COLOR_STATE_ID_SRGB_LINEAR), test_convert);
  g_test_add_data_func ("/colorstate/convert/srgb<->xyz", GUINT_TO_POINTER (GDK_COLOR_STATE_ID_XYZ), test_convert);
  g_test_add_data_func ("/colorstate/convert/srgb<->oklab", GUINT_TO_POINTER (GDK_COLOR_STATE_ID_OKLAB), test_convert);
  g_test_add_data_func ("/colorstate/convert/srgb<->oklch", GUINT_TO_POINTER (GDK_COLOR_STATE_ID_OKLCH), test_convert);

  return g_test_run ();
}
