#include <gdk/gdk.h>

static void
test_contentformats_types (void)
{
  GdkContentFormats *formats;
  const char *const *mimetypes;
  gsize n_types;
  const GType *gtypes;

  formats = gdk_content_formats_parse ("text/plain GdkFileList application/x-color GdkRGBA");

  g_assert_nonnull (formats);

  mimetypes = gdk_content_formats_get_mime_types (formats, &n_types);
  g_assert_true (n_types == 2);
  g_assert_cmpstr (mimetypes[0], ==, "text/plain");
  g_assert_cmpstr (mimetypes[1], ==, "application/x-color");

  gtypes = gdk_content_formats_get_gtypes (formats, &n_types);
  g_assert_true (n_types == 2);
  g_assert_true (gtypes[0] == GDK_TYPE_FILE_LIST);
  g_assert_true (gtypes[1] == GDK_TYPE_RGBA);

  gdk_content_formats_unref (formats);
}

static void
test_contentformats_union (void)
{
  GdkContentFormats *formats;
  GdkContentFormats *formats2;
  const char *const *mimetypes;
  gsize n_types;
  const GType *gtypes;

  formats = gdk_content_formats_parse ("text/plain application/x-color");
  formats2 = gdk_content_formats_parse ("GdkFileList GdkRGBA");

  formats = gdk_content_formats_union (formats, formats2);

  g_assert_nonnull (formats);

  mimetypes = gdk_content_formats_get_mime_types (formats, &n_types);
  g_assert_true (n_types == 2);
  g_assert_cmpstr (mimetypes[0], ==, "text/plain");
  g_assert_cmpstr (mimetypes[1], ==, "application/x-color");

  gtypes = gdk_content_formats_get_gtypes (formats, &n_types);
  g_assert_true (n_types == 2);
  g_assert_true (gtypes[0] == GDK_TYPE_FILE_LIST);
  g_assert_true (gtypes[1] == GDK_TYPE_RGBA);

  gdk_content_formats_unref (formats2);
  gdk_content_formats_unref (formats);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_type_ensure (GDK_TYPE_RGBA);
  g_type_ensure (GDK_TYPE_FILE_LIST);

  g_test_add_func ("/contentformats/types", test_contentformats_types);
  g_test_add_func ("/contentformats/union", test_contentformats_union);

  return g_test_run ();
}
