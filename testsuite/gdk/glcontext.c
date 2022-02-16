#include <gtk/gtk.h>

#define ALL_APIS (GDK_GL_API_GL | GDK_GL_API_GLES)

static GdkGLAPI
is_unique (GdkGLAPI api)
{
  return (api & (api - 1)) == 0;
}

static void
test_allowed_backends (gconstpointer data)
{
  GdkGLAPI allowed = GPOINTER_TO_SIZE (data);
  GdkGLAPI not_allowed = (~allowed) & ALL_APIS;
  GdkGLAPI api, random_apis;
  GdkDisplay *display = gdk_display_get_default ();
  GdkGLContext *context;
  GError *error = NULL;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_message ("no GL support: %s", error->message);
      g_test_skip ("no GL support");
      g_clear_error (&error);
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert (context);
  g_assert_no_error (error);
  g_assert_cmpint (gdk_gl_context_get_api (context), ==, 0);
  g_assert_cmpint (gdk_gl_context_get_allowed_apis (context), ==, ALL_APIS);

  gdk_gl_context_set_allowed_apis (context, allowed);
  g_assert_cmpint (gdk_gl_context_get_allowed_apis (context), ==, allowed);
  g_assert_cmpint (gdk_gl_context_get_api (context), ==, 0);

  if (!gdk_gl_context_realize (context, &error))
    {
      g_assert_cmpint (gdk_gl_context_get_api (context), ==, 0);

      if (not_allowed && g_error_matches (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE))
        {
          g_clear_error (&error);
          g_object_unref (context);
          return;
        }
      g_assert_no_error (error);
    }

  g_assert_no_error (error);

  g_assert_cmpint (gdk_gl_context_get_allowed_apis (context), ==, allowed);

  api = gdk_gl_context_get_api (context);
  g_assert_cmpint (api, !=, 0);
  g_assert_true (is_unique (api));
  g_assert_cmpint (api & allowed, ==, api);
  g_assert_cmpint (api & not_allowed, ==, 0);

  random_apis = g_random_int_range (0, ALL_APIS + 1);
  gdk_gl_context_set_allowed_apis (context, random_apis);
  g_assert_cmpint (gdk_gl_context_get_allowed_apis (context), ==, random_apis);
  g_assert_cmpint (gdk_gl_context_get_api (context), ==, api);

  g_object_unref (context);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_data_func ("/allowed-apis/none", GSIZE_TO_POINTER (0), test_allowed_backends);
  g_test_add_data_func ("/allowed-apis/gl", GSIZE_TO_POINTER (GDK_GL_API_GL), test_allowed_backends);
  g_test_add_data_func ("/allowed-apis/gles", GSIZE_TO_POINTER (GDK_GL_API_GLES), test_allowed_backends);
  g_test_add_data_func ("/allowed-apis/all", GSIZE_TO_POINTER (GDK_GL_API_GL | GDK_GL_API_GLES), test_allowed_backends);

  return g_test_run ();
}
