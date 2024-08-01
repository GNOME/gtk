#include "config.h"

#include <gtk/gtk.h>
#include "gdk/loaders/gdkavifprivate.h"

#include "../reftests/reftest-compare.h"


static float
image_distance (const guchar *data,
                gsize         stride,
                const guchar *data2,
                gsize         stride2,
                gsize         width,
                gsize         height)
{
  float dist = 0;
  gsize imax = 0, jmax = 0;
  float r1 = 0, g1 = 0, b1 = 0, a1 = 0;
  float r2 = 0, g2 = 0, b2 = 0, a2 = 0;

  for (gsize i = 0; i < height; i++)
    {
      const float *p = (const float *) (data + i * stride);
      const float *p2 = (const float *) (data2 + i * stride2);

      for (gsize j = 0; j < width; j++)
        {
          float dr, dg, db, da;
  
          dr = p[4 * j + 0] - p2[4 * j + 0];
          dg = p[4 * j + 1] - p2[4 * j + 1];
          db = p[4 * j + 2] - p2[4 * j + 2];
          da = p[4 * j + 3] - p2[4 * j + 3];

          float d = dr * dr + dg * dg + db * db + da * da;
          if (d > dist)
            {
              dist = d;
              imax = i;
              jmax = j;
              r1 = p[4 * j + 0];
              g1 = p[4 * j + 1];
              b1 = p[4 * j + 2];
              a1 = p[4 * j + 3];
              r2 = p2[4 * j + 0];
              g2 = p2[4 * j + 1];
              b2 = p2[4 * j + 2];
              a2 = p2[4 * j + 3];
            }
        }
    }

  if (g_test_verbose() &&
      dist > 0)
    {
      g_print ("worst pixel %" G_GSIZE_FORMAT " %" G_GSIZE_FORMAT ": %f %f %f %f  vs  %f %f %f %f   %f\n",
               imax, jmax, r1, g1, b1, a1, r2, g2, b2, a2, sqrt (dist));
    }

  return sqrt (dist);
}

static void
assert_texture_equal (GdkTexture *t1,
                      GdkTexture *t2)
{
  int width;
  int height;
  int stride;
  guchar *d1;
  guchar *d2;
  GdkTextureDownloader *downloader;

  width = gdk_texture_get_width (t1);
  height = gdk_texture_get_height (t1);

  g_assert_cmpint (width, ==, gdk_texture_get_width (t2));
  g_assert_cmpint (height, ==, gdk_texture_get_height (t2));

  stride = 4 * width * sizeof (float);
  d1 = g_malloc (stride * height);
  d2 = g_malloc (stride * height);

  downloader = gdk_texture_downloader_new (t1);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R32G32B32A32_FLOAT);
  gdk_texture_downloader_set_color_state (downloader, gdk_color_state_get_srgb ());

  gdk_texture_downloader_download_into (downloader, d1, stride);

  gdk_texture_downloader_free (downloader);

  downloader = gdk_texture_downloader_new (t2);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R32G32B32A32_FLOAT);
  gdk_texture_downloader_set_color_state (downloader, gdk_color_state_get_srgb ());

  gdk_texture_downloader_download_into (downloader, d2, stride);

  gdk_texture_downloader_free (downloader);

#if 0
  GdkTexture *diff;
  diff = reftest_compare_textures (t1, t2);
  if (diff)
    {
      gdk_texture_save_to_png (t1, "pattern1.png");
      gdk_texture_save_to_png (t2, "pattern2.png");
      gdk_texture_save_to_png (diff, "pattern.diff.png");
      g_object_unref (diff);
      g_test_fail ();
    }
#endif

  if (image_distance (d1, stride, d2, stride, width, height) > 0.01)
    {
      g_test_fail ();
      return;
    }

  //g_assert_cmpmem (d1, stride * height, d2, stride * height);

  g_free (d1);
  g_free (d2);
}

static char *
get_reference (const char *filename)
{
  char *p;
  char *basename;
  char *reference;

  p = strchr (filename, '-');
  if (!p)
    return NULL;

  basename = g_strndup (filename, p - filename);
  reference = g_strconcat (basename, ".png", NULL);
  g_free (basename);

  return reference;
}

static void
test_load_image (gconstpointer testdata)
{
  const char *filename = testdata;
  GdkTexture *texture = NULL;
  GdkTexture *texture2;
  char *path;
  char *reference;
  char *path2;
  char *data;
  gsize size;
  GBytes *bytes;
  GError *error = NULL;

#ifndef HAVE_AVIF
  g_test_skip ("built without avif support");
  return;
#endif

  reference = get_reference (filename);
  if (!reference)
    {
      g_test_skip ("no reference image");
      return;
    }

  path = g_test_build_filename (G_TEST_DIST, "yuv-images", filename, NULL);

  g_file_get_contents (path, &data, &size, &error);
  g_assert_no_error (error);
  bytes = g_bytes_new_take (data, size);

#ifdef HAVE_AVIF
  texture = gdk_load_avif (bytes, &error);
#endif

  g_bytes_unref (bytes);

  if (texture == NULL)
    {
      g_test_skip_printf ("unsupported format: %s", error->message);
      g_error_free (error);
      return;
    }

  if (!GDK_IS_DMABUF_TEXTURE (texture))
    {
      g_test_message ("No dmabuf texture. /dev/udmabuf not available?");
    }

  path2 = g_test_build_filename (G_TEST_DIST, "yuv-images", reference, NULL);
  texture2 = gdk_texture_new_from_filename (path2, &error);
  g_assert_no_error (error);
  g_assert_true (GDK_IS_TEXTURE (texture2));

  assert_texture_equal (texture, texture2);

  g_object_unref (texture);
  g_object_unref (texture2);
  g_free (path);
  g_free (path2);
  g_free (reference);
}

int
main (int argc, char *argv[])
{
  char *path;
  GDir *dir;
  const char *name;
  GError *error = NULL;

  gtk_init ();

  (g_test_init) (&argc, &argv, NULL);

  path = g_test_build_filename (G_TEST_DIST, "yuv-images", NULL);
  dir = g_dir_open (path, 0, &error);
  g_assert_no_error (error);
  g_free (path);

  while ((name = g_dir_read_name (dir)) != NULL)
   {
     if (!g_str_has_suffix (name, ".png"))
       {
         char *test = g_strconcat ("/yuv/load/", name, NULL);
         g_test_add_data_func (test, name, test_load_image);
         g_free (test);
       }
   }

  return g_test_run ();
}
