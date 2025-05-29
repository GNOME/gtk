#include "config.h"

#include <gdk/gdk.h>
#include <math.h>

#ifdef HAVE_DRM_FOURCC_H
#endif
#include "udmabuf.h"

typedef enum {
  TEXTURE_METHOD_PLAIN,
  TEXTURE_METHOD_DMABUF
} TextureMethod;

static GdkColorState *
get_color_state (guint        id,
                 const char **out_name)
{
  const char *unused;

  if (out_name == NULL)
    out_name = &unused;

  switch (id)
  {
    case 0:
      *out_name = "srgb";
      return gdk_color_state_ref (gdk_color_state_get_srgb ());
    case 1:
      *out_name = "srgb-linear";
      return gdk_color_state_ref (gdk_color_state_get_srgb_linear ());
    case 2:
      *out_name = "rec2100-pq";
      return gdk_color_state_ref (gdk_color_state_get_rec2100_pq ());
    case 3:
      *out_name = "rec2100-linear";
      return gdk_color_state_ref (gdk_color_state_get_rec2100_linear ());
    default:
      return NULL;
  }
}

static void
test_equal (void)
{
  GdkColorState *csi, *csj;
  guint i, j;

  for (i = 0; ; i++)
    {
      csi = get_color_state (i, NULL);
      if (csi == NULL)
        break;

      g_assert_true (gdk_color_state_equal (csi, csi));

      for (j = 0; ; j++)
        {
          csj = get_color_state (j, NULL);
          if (csj == NULL)
            break;

          if (i != j)
            g_assert_false (gdk_color_state_equal (csi, csj));
          else
            g_assert_true (gdk_color_state_equal (csi, csj)); /* might break for non-default? */
        }
    }
}

static float
image_distance (const guchar *data,
                gsize         stride,
                const guchar *data2,
                gsize         stride2,
                gsize         width,
                gsize         height)
{
  float dist = 0;
  gsize x, y, xmax = 0, ymax = 0;
  float r1 = 0, g1 = 0, b1 = 0, a1 = 0;
  float r2 = 0, g2 = 0, b2 = 0, a2 = 0;

  for (y = 0; y < height; y++)
    {
      const float *p = (const float *) (data + y * stride);
      const float *p2 = (const float *) (data2 + y * stride2);

      for (x = 0; x < width; x++)
        {
          float dr, dg, db, da;

          if (p[4 * x + 3] == 0 &&
              p2[4 * x + 3] == 0)
            continue;

          dr = p[4 * x + 0] - p2[4 * x + 0];
          dg = p[4 * x + 1] - p2[4 * x + 1];
          db = p[4 * x + 2] - p2[4 * x + 2];
          da = p[4 * x + 3] - p2[4 * x + 3];

          float d = dr * dr + dg * dg + db * db + da * da;
          if (d > dist)
            {
              dist = d;
              xmax = x;
              ymax = y;
              r1 = p[4*x+0];
              g1 = p[4*x+1];
              b1 = p[4*x+2];
              a1 = p[4*x+3];
              r2 = p2[4*x+0];
              g2 = p2[4*x+1];
              b2 = p2[4*x+2];
              a2 = p2[4*x+3];
            }
        }
    }

  if (g_test_verbose() &&
      dist > 0)
    {
      g_test_message ("worst pixel %" G_GSIZE_FORMAT " %" G_GSIZE_FORMAT ": %f %f %f %f  vs  %f %f %f %f\n",
                      xmax, ymax, r1, g1, b1, a1, r2, g2, b2, a2);
    }

  return sqrt (dist);
}

static void
test_convert (gconstpointer testdata,
              TextureMethod method,
              float         max_distance_premultiplied,
              float         max_distance_straight)
{
  GdkColorState *cs = (GdkColorState *) testdata;
  char *path;
  GdkTexture *texture, *texture2;
  GdkTextureDownloader *downloader;
  GdkMemoryTextureBuilder *membuild;
  GError *error = NULL;
  GBytes *bytes, *bytes2;
  const guchar *data, *data2;
  gsize stride, stride2;
  gsize width, height;
  GdkMemoryFormat test_format;
  float max_distance;

  if (g_test_rand_bit ())
    {
      max_distance = max_distance_premultiplied;
      test_format = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
    }
  else
    {
      max_distance = max_distance_straight;
      test_format = GDK_MEMORY_R32G32B32A32_FLOAT;
    }

  path = g_test_build_filename (G_TEST_DIST, "image-data", "image.png", NULL);

  /* Create a texture */
  texture = gdk_texture_new_from_filename (path, &error);
  g_assert_no_error (error);
  g_assert_true (gdk_color_state_equal (gdk_texture_get_color_state (texture),
                                        gdk_color_state_get_srgb ()));
  if (method == TEXTURE_METHOD_DMABUF)
    {
#ifdef HAVE_DRM_FOURCC_H
      texture = udmabuf_texture_from_texture (texture, &error);
#else
      g_assert_not_reached ();
#endif
    }
  if (texture == NULL)
    {
      g_test_skip (error->message);
      g_clear_error (&error);
      return;
    }

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  /* download the texture as float for later comparison */
  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, test_format);
  gdk_texture_downloader_set_color_state (downloader, gdk_texture_get_color_state (texture));
  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
  data = g_bytes_get_data (bytes, NULL);

  /* Download the texture into the test colorstate, this does a conversion */
  gdk_texture_downloader_set_color_state (downloader, cs);
  bytes2 = gdk_texture_downloader_download_bytes (downloader, &stride2);

  /* Create a new texture in the test colorstate with the just downloaded data */
  membuild = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_format (membuild, test_format);
  gdk_memory_texture_builder_set_color_state (membuild, cs);
  gdk_memory_texture_builder_set_width (membuild, width);
  gdk_memory_texture_builder_set_height (membuild, height);
  gdk_memory_texture_builder_set_bytes (membuild, bytes2);
  gdk_memory_texture_builder_set_stride (membuild, stride2);
  texture2 = gdk_memory_texture_builder_build (membuild);
  g_object_unref (membuild);
  g_bytes_unref (bytes2);

  /* Download the data of the new texture in the original texture's colorstate.
   * This does the reverse conversion.
   */
  gdk_texture_downloader_set_texture (downloader, texture2);
  gdk_texture_downloader_set_color_state (downloader, gdk_texture_get_color_state (texture));
  bytes2 = gdk_texture_downloader_download_bytes (downloader, &stride2);
  data2 = g_bytes_get_data (bytes2, NULL);

  /* Check that the conversions produce pixels that are close enough */
  g_assert_cmpfloat (image_distance (data, stride, data2, stride2, width, height), <, max_distance);

  if (g_test_verbose ())
    g_test_message ("%g\n", image_distance (data, stride, data2, stride2, width, height));

  g_bytes_unref (bytes2);
  g_bytes_unref (bytes);
  gdk_texture_downloader_free (downloader);
  g_object_unref (texture2);
  g_object_unref (texture);
  g_free (path);
}

static void
test_convert_plain (gconstpointer testdata)
{
  test_convert (testdata, TEXTURE_METHOD_PLAIN, 0.001, 0.001);
}

#ifdef HAVE_DRM_FOURCC_H
static void
test_convert_dmabuf (gconstpointer testdata)
{
  test_convert (testdata, TEXTURE_METHOD_DMABUF, 0.02, 0.005);
}
#endif

static void
test_png (gconstpointer testdata)
{
  GdkColorState *cs = (GdkColorState *) testdata;
  static const float texture_data[] = { 0,0,0,1,  0,0,1,1,  0,1,0,1,  0,1,1,1,
                                        1,0,0,1,  1,0,1,1,  1,1,0,1,  1,1,1,1 };
  static gsize width = 4, height = 2, stride = 4 * 4 * sizeof (float);
  GdkMemoryTextureBuilder *membuild;
  GdkTexture *saved, *loaded;
  GBytes *bytes;
  GError *error = NULL;

  membuild = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_format (membuild, GDK_MEMORY_R32G32B32A32_FLOAT);
  gdk_memory_texture_builder_set_color_state (membuild, cs);
  gdk_memory_texture_builder_set_width (membuild, width);
  gdk_memory_texture_builder_set_height (membuild, height);
  bytes = g_bytes_new_static (texture_data, sizeof (texture_data));
  gdk_memory_texture_builder_set_bytes (membuild, bytes);
  g_bytes_unref (bytes);
  gdk_memory_texture_builder_set_stride (membuild, stride);
  saved = gdk_memory_texture_builder_build (membuild);
  g_object_unref (membuild);

  bytes = gdk_texture_save_to_png_bytes (saved);
  loaded = gdk_texture_new_from_bytes (bytes, &error);
  g_assert_nonnull (loaded);
  g_assert_no_error (error);
  g_bytes_unref (bytes);

  g_assert_cmpint (gdk_texture_get_width (saved), ==, gdk_texture_get_width (loaded));
  g_assert_cmpint (gdk_texture_get_height (saved), ==, gdk_texture_get_height (loaded));
  g_assert_true (gdk_color_state_equal (gdk_texture_get_color_state (saved), gdk_texture_get_color_state (loaded)));

  g_object_unref (saved);
  g_object_unref (loaded);
}

static void
test_cicp (void)
{
  GdkCicpParams *params;
  GdkColorState *cs;
  GError *error = NULL;
  GdkCicpParams *params2;

  params = gdk_cicp_params_new ();

  g_assert_true (gdk_cicp_params_get_color_primaries (params) == 2);
  g_assert_true (gdk_cicp_params_get_transfer_function (params) == 2);
  g_assert_true (gdk_cicp_params_get_matrix_coefficients (params) == 2);
  g_assert_true (gdk_cicp_params_get_range (params) == GDK_CICP_RANGE_NARROW);

  cs = gdk_cicp_params_build_color_state (params, &error);

  g_assert_null (cs);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);

  g_clear_error (&error);

  gdk_cicp_params_set_color_primaries (params, 5);
  gdk_cicp_params_set_transfer_function (params, 1);
  gdk_cicp_params_set_matrix_coefficients (params, 0);
  gdk_cicp_params_set_range (params, GDK_CICP_RANGE_FULL);

  cs = gdk_cicp_params_build_color_state (params, &error);

  g_assert_nonnull (cs);
  g_assert_no_error (error);

  params2 = gdk_color_state_create_cicp_params (cs);

  g_assert_true (gdk_cicp_params_get_color_primaries (params) == gdk_cicp_params_get_color_primaries (params2));
  g_object_unref (params2);

  gdk_color_state_unref (cs);
  g_object_unref (params);
}

int
main (int argc, char *argv[])
{
  guint i;

  (gtk_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/colorstate/equal", test_equal);
  g_test_add_func ("/colorstate/cicp", test_cicp);

  for (i = 0; ; i++)
    {
      GdkColorState *csi;
      const char *cs_name;
      char *test_name;

      csi = get_color_state (i, &cs_name);
      if (csi == NULL)
        break;

      test_name = g_strdup_printf ("/colorstate/convert-plain/srgb/%s", cs_name);
      g_test_add_data_func_full (test_name, gdk_color_state_ref (csi), test_convert_plain, (GDestroyNotify) gdk_color_state_unref);
      g_free (test_name);
#ifdef HAVE_DRM_FOURCC_H
      test_name = g_strdup_printf ("/colorstate/convert-dmabuf/srgb/%s", cs_name);
      g_test_add_data_func_full (test_name, gdk_color_state_ref (csi), test_convert_dmabuf, (GDestroyNotify) gdk_color_state_unref);
      g_free (test_name);
#endif
      test_name = g_strdup_printf ("/colorstate/png/%s", cs_name);
      g_test_add_data_func_full (test_name, gdk_color_state_ref (csi), test_png, (GDestroyNotify) gdk_color_state_unref);
      g_free (test_name);

      gdk_color_state_unref (csi);
    }

  GdkCicpParams *params = gdk_cicp_params_new ();
  for (guint primaries = 0; primaries < 32; primaries++)
    {
      gdk_cicp_params_set_color_primaries (params, primaries);
      for (guint tf = 0; tf < 32; tf++)
        {
          gdk_cicp_params_set_transfer_function (params, tf);
          for (guint matrix = 0; matrix < 32; matrix++)
            {
              GdkColorState *color_state;

              gdk_cicp_params_set_matrix_coefficients (params, matrix);
              gdk_cicp_params_set_range (params, GDK_CICP_RANGE_NARROW);
              color_state = gdk_cicp_params_build_color_state (params, NULL);

              if (color_state)
                {
                  char *test_name = g_strdup_printf ("/colorstate/convert/plain/srgb/cicp/%u/%u/%u/0", primaries, tf, matrix);
                  g_test_add_data_func_full (test_name, gdk_color_state_ref (color_state), test_convert_plain, (GDestroyNotify) gdk_color_state_unref);
                  g_free (test_name);
#ifdef HAVE_DRM_FOURCC_H
                  test_name = g_strdup_printf ("/colorstate/convert/dmabuf/srgb/cicp/%u/%u/%u/0", primaries, tf, matrix);
                  g_test_add_data_func_full (test_name, gdk_color_state_ref (color_state), test_convert_dmabuf, (GDestroyNotify) gdk_color_state_unref);
                  g_free (test_name);
#endif

                  gdk_color_state_unref (color_state);
                }

              gdk_cicp_params_set_range (params, GDK_CICP_RANGE_FULL);
              color_state = gdk_cicp_params_build_color_state (params, NULL);
              if (color_state)
                {
                  char *test_name = g_strdup_printf ("/colorstate/convert/plain/srgb/cicp/%u/%u/%u/1", primaries, tf, matrix);
                  g_test_add_data_func_full (test_name, gdk_color_state_ref (color_state), test_convert_plain, (GDestroyNotify) gdk_color_state_unref);
                  g_free (test_name);
#ifdef HAVE_DRM_FOURCC_H
                  test_name = g_strdup_printf ("/colorstate/convert/dmabuf/srgb/cicp/%u/%u/%u/1", primaries, tf, matrix);
                  g_test_add_data_func_full (test_name, gdk_color_state_ref (color_state), test_convert_dmabuf, (GDestroyNotify) gdk_color_state_unref);
                  g_free (test_name);
#endif

                  gdk_color_state_unref (color_state);
                }
            }
        }
    }

  return g_test_run ();
}
