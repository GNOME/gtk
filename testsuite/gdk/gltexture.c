#include <gtk/gtk.h>
#include <epoxy/gl.h>

static cairo_surface_t *
make_surface (void)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  guchar *data;

  data = g_malloc (64 * 64 * 4);
  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 64, 64, 64 * 4);
  cr = cairo_create (surface);
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  return surface;
}

enum {
  SAME_CONTEXT,
  NO_CONTEXT,
  SHARED_CONTEXT
};

static void
test_gltexture (int test)
{
  GdkDisplay *display;
  GdkGLContext *context;
  GdkGLContext *context2 = NULL;
  GdkGLTextureBuilder *builder;
  GdkTexture *texture;
  cairo_surface_t *surface;
  GError *error = NULL;
  unsigned int id;
  guchar *data;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_message ("no GL support: %s", error->message);
      g_test_skip ("no GL support");
      g_clear_error (&error);
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context);
  g_assert_no_error (error);

  gdk_gl_context_realize (context, &error);
  g_assert_no_error (error);

  surface = make_surface ();

  gdk_gl_context_make_current (context);

  glGenTextures (1, &id);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, id);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                cairo_image_surface_get_data (surface));

  g_assert_true (glGetError () == GL_NO_ERROR);

  if (test == NO_CONTEXT)
    gdk_gl_context_clear_current ();
  else if (test == SHARED_CONTEXT)
    {
      context2 = gdk_display_create_gl_context (display, &error);
      g_assert_nonnull (context2);
      g_assert_no_error (error);

      gdk_gl_context_realize (context2, &error);
      g_assert_no_error (error);

      gdk_gl_context_make_current (context2);
    }

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_id (builder, id);
  gdk_gl_texture_builder_set_context (builder, context);
  gdk_gl_texture_builder_set_width (builder, 64);
  gdk_gl_texture_builder_set_height (builder, 64);
  texture = gdk_gl_texture_builder_build (builder, NULL, NULL);

  data = g_malloc0 (64 * 64 * 4);
  gdk_texture_download (texture, data, 64 * 4);

  g_assert_true (memcmp (data, cairo_image_surface_get_data (surface), 64 * 64 * 4) == 0);

  g_free (data);
  g_object_unref (texture);
  g_object_unref (builder);

  cairo_surface_destroy (surface);

  g_object_unref (context);

  g_clear_object (&context2);
}

static void
test_gltexture_same_context (void)
{
  test_gltexture (SAME_CONTEXT);
}

static void
test_gltexture_no_context (void)
{
  test_gltexture (NO_CONTEXT);
}

static void
test_gltexture_shared_context (void)
{
  test_gltexture (SHARED_CONTEXT);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gltexture/same-context", test_gltexture_same_context);
  g_test_add_func ("/gltexture/no-context", test_gltexture_no_context);
  g_test_add_func ("/gltexture/shared-context", test_gltexture_shared_context);

  return g_test_run ();
}
