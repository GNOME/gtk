#include <gtk.h>

#include "gdk/gdkmemorytextureprivate.h"

#include "gdkinternaltests.h"

static void
compare_pixels (int     width,
                int     height,
                guchar *data1,
                gsize   stride1,
                guchar *data2,
                gsize   stride2)
{
  int x, y;
  for (y = 0; y < height; y++)
    {
      const guint32 *p1 = (const guint32*) (data1 + y * stride1);
      const guint32 *p2 = (const guint32*) (data2 + y * stride2);

      for (x = 0; x < width; x++)
        {
          g_assert_cmphex (p1[x], ==, p2[x]);
        }
    }
}

static void
test_texture_from_pixbuf (void)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GError *error = NULL;
  guchar *data;
  int width, height;
  gsize stride;
  cairo_surface_t *surface;
  cairo_t *cr;

  pixbuf = gdk_pixbuf_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png", &error);
  g_assert_no_error (error);
  g_assert_nonnull (pixbuf);
  g_assert_true (gdk_pixbuf_get_has_alpha (pixbuf));

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  texture = gdk_texture_new_for_pixbuf (pixbuf);

  g_assert_nonnull (texture);
  g_assert_cmpint (gdk_texture_get_width (texture), ==, width);
  g_assert_cmpint (gdk_texture_get_height (texture), ==, height);

  stride = 4 * width;
  data = g_new0 (guchar, stride * height);
  gdk_texture_download (texture, data, stride);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  compare_pixels (width, height,
                  data, stride,
                  cairo_image_surface_get_data (surface),
                  cairo_image_surface_get_stride (surface));

  g_free (data);

  g_object_unref (pixbuf);
  g_object_unref (texture);
  cairo_surface_destroy (surface);
}

static void
test_texture_from_resource (void)
{
  GdkTexture *texture;
  int width, height;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  g_assert_nonnull (texture);
  g_object_get (texture,
                "width", &width,
                "height", &height,
                NULL);
  g_assert_cmpint (width, ==, 16);
  g_assert_cmpint (height, ==, 16);

  g_object_unref (texture);
}

static void
test_texture_save_to_png (void)
{
  GdkTexture *texture;
  GError *error = NULL;
  GFile *file;
  GdkTexture *texture2;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  gdk_texture_save_to_png (texture, "test.png");
  file = g_file_new_for_path ("test.png");
  texture2 = gdk_texture_new_from_file (file, &error);
  g_object_unref (file);
  g_assert_no_error (error);

  g_object_unref (texture);
  g_object_unref (texture2);
}

static void
test_texture_subtexture (void)
{
  cairo_t *cr;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  GdkTexture *texture, *subtexture;
  guchar *data, *subdata;
  gsize stride, substride;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 64, 64);
  cr = cairo_create (surface);

  pattern = cairo_pattern_create_linear (0, 0, 64, 64);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 1, 0, 0, 0.2);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 1, 0, 1, 1);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  texture = gdk_texture_new_for_surface (surface);

  cairo_pattern_destroy (pattern);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  subtexture = gdk_memory_texture_new_subtexture (GDK_MEMORY_TEXTURE (texture), 0, 0, 32, 32);

  g_assert_cmpint (gdk_texture_get_width (subtexture), ==, 32);
  g_assert_cmpint (gdk_texture_get_height (subtexture), ==, 32);

  data = g_new0 (guchar, 64 * 64 * 4);
  stride = 64 * 4;
  gdk_texture_download (texture, data, stride);

  subdata = g_new0 (guchar, 32 * 32 * 4);
  substride = 32 * 4;
  gdk_texture_download (subtexture, subdata, substride);

  compare_pixels (32, 32,
                  data, stride,
                  subdata, substride);

  g_free (data);
  g_free (subdata);

  g_object_unref (subtexture);
  g_object_unref (texture);
}

void
add_texture_tests (void)
{
  g_test_add_func ("/texture/from-pixbuf", test_texture_from_pixbuf);
  g_test_add_func ("/texture/from-resource", test_texture_from_resource);
  g_test_add_func ("/texture/save-to-png", test_texture_save_to_png);
  g_test_add_func ("/texture/subtexture", test_texture_subtexture);
}
