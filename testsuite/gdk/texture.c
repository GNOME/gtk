#include <gtk.h>

static gboolean
compare_pixels (int     width,
                int     height,
                guchar *data1,
                gsize   stride1,
                guchar *data2,
                gsize   stride2)
{
  int i;
  for (i = 0; i < height; i++)
    {
      gconstpointer p1 = data1 + i * stride1;
      gconstpointer p2 = data2 + i * stride2;
      if (memcmp (p1, p2, width * 4) != 0)
        return FALSE;
    }
  return TRUE;
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

  g_assert_true (compare_pixels (width, height,
                                 data, stride,
                                 cairo_image_surface_get_data (surface),
                                 cairo_image_surface_get_stride (surface)));

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

int
main (int argc, char *argv[])
{
  /* We want to use resources from libgtk, so we need gtk initialized */
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/texture/from-pixbuf", test_texture_from_pixbuf);
  g_test_add_func ("/texture/from-resource", test_texture_from_resource);
  g_test_add_func ("/texture/save-to-png", test_texture_save_to_png);

  return g_test_run ();
}
