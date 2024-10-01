#include <gtk/gtk.h>

#include <epoxy/gl.h>

#include "gsk/gl/fp16private.h"
#include "testsuite/gdk/gdktestutils.h"

#define N 10

static GdkGLContext *gl_context = NULL;
static GskRenderer *gl_renderer = NULL;
static GskRenderer *vulkan_renderer = NULL;

typedef enum {
  TEXTURE_METHOD_LOCAL,
  TEXTURE_METHOD_GL_NATIVE,
  TEXTURE_METHOD_GL,
  TEXTURE_METHOD_GL_RELEASED,
  TEXTURE_METHOD_VULKAN,
  TEXTURE_METHOD_PNG,
  TEXTURE_METHOD_PNG_PIXBUF,
  TEXTURE_METHOD_TIFF,
  TEXTURE_METHOD_TIFF_PIXBUF,

  N_TEXTURE_METHODS
} TextureMethod;

static gpointer
encode (GdkMemoryFormat format,
        TextureMethod   method)
{
  return GSIZE_TO_POINTER (method * GDK_MEMORY_N_FORMATS + format);
}

static void
decode (gconstpointer     data,
        GdkMemoryFormat *format,
        TextureMethod   *method)
{
  gsize value = GPOINTER_TO_SIZE (data);

  *format = value % GDK_MEMORY_N_FORMATS;
  value /= GDK_MEMORY_N_FORMATS;

  *method = value;
}

static gpointer
encode_two_formats (GdkMemoryFormat format1,
                    GdkMemoryFormat format2)
{
  return GSIZE_TO_POINTER (format1 * GDK_MEMORY_N_FORMATS + format2);
}

static void
decode_two_formats (gconstpointer    data,
                    GdkMemoryFormat *format1,
                    GdkMemoryFormat *format2)
{
  gsize value = GPOINTER_TO_SIZE (data);

  *format2 = value % GDK_MEMORY_N_FORMATS;
  value /= GDK_MEMORY_N_FORMATS;

  *format1 = value;
}

static GdkTexture *
upload_to_renderer (GdkTexture  *texture,
                    GskRenderer *renderer)
{
  GskRenderNode *node;
  GdkTexture *result;

  if (renderer == NULL)
    {
      g_test_skip ("renderer not supported");
      return texture;
    }

  node = gsk_texture_node_new (texture,
                               &GRAPHENE_RECT_INIT(
                                 0, 0, 
                                 gdk_texture_get_width (texture),
                                 gdk_texture_get_height (texture)
                               ));
  result = gsk_renderer_render_texture (renderer, node, NULL);
  gsk_render_node_unref (node);
  g_object_unref (texture);

  return result;
}

static gboolean
gl_native_should_skip_format (GdkMemoryFormat format)
{
  int major, minor;

  if (gl_context == NULL)
    {
      g_test_skip ("OpenGL is not supported");
      return TRUE;
    }

  if (!gdk_gl_context_get_use_es (gl_context))
    return FALSE;

  gdk_gl_context_get_version (gl_context, &major, &minor);

  if (major < 3)
    {
      g_test_skip ("GLES < 3.0 is not supported");
      return TRUE;
    }

  if (gdk_memory_format_is_deep (format) &&
      (major < 3 || (major == 3 && minor < 1)))
    {
      g_test_skip ("GLES < 3.1 can't handle 16bit non-RGBA formats");
      return TRUE;
    }

  return FALSE;
}

static void
release_texture (gpointer data)
{
  unsigned int id = GPOINTER_TO_UINT (data);

  gdk_gl_context_make_current (gl_context);

  glDeleteTextures (1, &id);
}

static GdkTexture *
upload_to_gl_native (GdkTexture *texture)
{
  struct {
    GdkMemoryFormat format;
    unsigned int bpp;
    int gl_internalformat;
    int gl_format;
    int gl_type;
    int swizzle[4];
  } formats[] = {
    { GDK_MEMORY_R8G8B8, 3, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, { GL_RED, GL_GREEN, GL_BLUE, GL_ONE } },
    { GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, 8, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    { GDK_MEMORY_G8, 1, GL_R8, GL_RED, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_ONE } },
    { GDK_MEMORY_G16, 2, GL_R16, GL_RED, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_ONE } },
    { GDK_MEMORY_G8A8_PREMULTIPLIED, 2, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    { GDK_MEMORY_G16A16_PREMULTIPLIED, 4, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    { GDK_MEMORY_G8A8, 2, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    { GDK_MEMORY_G16A16, 4, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    { GDK_MEMORY_A8, 1, GL_R8, GL_RED, GL_UNSIGNED_BYTE, { GL_ONE, GL_ONE, GL_ONE, GL_RED } },
    { GDK_MEMORY_A16, 2, GL_R16, GL_RED, GL_UNSIGNED_SHORT, { GL_ONE, GL_ONE, GL_ONE, GL_RED } },
  };

  if (gl_native_should_skip_format (gdk_texture_get_format (texture)))
    return texture;

  gdk_gl_context_make_current (gl_context);

  for (unsigned int i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      unsigned int id;
      guchar *data;
      int width, height;
      gsize stride;
      GdkTextureDownloader *d;
      GdkGLTextureBuilder *b;
      GdkTexture *result;

      if (formats[i].format != gdk_texture_get_format (texture))
        continue;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);
      data = g_malloc (width * height * formats[i].bpp);
      stride = width * formats[i].bpp;

      d = gdk_texture_downloader_new (texture);
      gdk_texture_downloader_set_format (d, formats[i].format);
      gdk_texture_downloader_download_into (d, data, stride);
      gdk_texture_downloader_free (d);

      glGenTextures (1, &id);
      glActiveTexture (GL_TEXTURE0);
      glBindTexture (GL_TEXTURE_2D, id);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D (GL_TEXTURE_2D, 0, formats[i].gl_internalformat, width, height, 0, formats[i].gl_format, formats[i].gl_type, data);
      glTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, formats[i].swizzle);

      g_free (data);

      b = gdk_gl_texture_builder_new  ();
      gdk_gl_texture_builder_set_context (b, gl_context);
      gdk_gl_texture_builder_set_id (b, id);
      gdk_gl_texture_builder_set_width (b, width);
      gdk_gl_texture_builder_set_height (b, height);
      gdk_gl_texture_builder_set_format (b, formats[i].format);
      result = gdk_gl_texture_builder_build (b, release_texture, GUINT_TO_POINTER (id));
      g_object_unref (b);

      g_object_unref (texture);

      return result;
    }

  return upload_to_renderer (texture, gl_renderer);
}

static GdkTexture *
create_texture (GdkMemoryFormat  format,
                TextureMethod    method,
                int              width,
                int              height,
                const GdkRGBA   *color)
{
  TextureBuilder builder;
  GdkTexture *texture;

  texture_builder_init (&builder, format, width, height);
  texture_builder_fill (&builder, color);

  texture = texture_builder_finish (&builder);

  switch (method)
  {
    case TEXTURE_METHOD_LOCAL:
      break;

    case TEXTURE_METHOD_GL_NATIVE:
      texture = upload_to_gl_native (texture);
      break;

    case TEXTURE_METHOD_GL:
      texture = upload_to_renderer (texture, gl_renderer);
      break;

    case TEXTURE_METHOD_GL_RELEASED:
      texture = upload_to_renderer (texture, gl_renderer);
      if (GDK_IS_GL_TEXTURE (texture))
        gdk_gl_texture_release (GDK_GL_TEXTURE (texture));
      break;

    case TEXTURE_METHOD_VULKAN:
      texture = upload_to_renderer (texture, vulkan_renderer);
      break;

    case TEXTURE_METHOD_PNG:
      {
        GBytes *bytes = gdk_texture_save_to_png_bytes (texture);
        g_assert (bytes);
        g_object_unref (texture);
        texture = gdk_texture_new_from_bytes (bytes, NULL);
        g_assert (texture);
        g_bytes_unref (bytes);
      }
      break;

    case TEXTURE_METHOD_PNG_PIXBUF:
      {
        GInputStream *stream;
        GdkPixbuf *pixbuf;
        GBytes *bytes;

        bytes = gdk_texture_save_to_png_bytes (texture);
        g_assert (bytes);
        g_object_unref (texture);
        stream = g_memory_input_stream_new_from_bytes (bytes);
        pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
        g_object_unref (stream);
        g_assert (pixbuf);
        texture = gdk_texture_new_for_pixbuf (pixbuf);
        g_assert (texture);
        g_object_unref (pixbuf);
        g_bytes_unref (bytes);
      }
      break;

    case TEXTURE_METHOD_TIFF:
      {
        GBytes *bytes = gdk_texture_save_to_tiff_bytes (texture);
        g_assert (bytes);
        g_object_unref (texture);
        texture = gdk_texture_new_from_bytes (bytes, NULL);
        g_assert (texture);
        g_bytes_unref (bytes);
      }
      break;

    case TEXTURE_METHOD_TIFF_PIXBUF:
      {
        GInputStream *stream;
        GdkPixbuf *pixbuf;
        GBytes *bytes;

        bytes = gdk_texture_save_to_tiff_bytes (texture);
        g_assert (bytes);
        g_object_unref (texture);
        stream = g_memory_input_stream_new_from_bytes (bytes);
        pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
        g_object_unref (stream);
        g_assert (pixbuf);
        texture = gdk_texture_new_for_pixbuf (pixbuf);
        g_assert (texture);
        g_object_unref (pixbuf);
        g_bytes_unref (bytes);
      }
      break;

    case N_TEXTURE_METHODS:
    default:
      g_assert_not_reached ();
      break;
  }

  return texture;
}

static gboolean
texture_method_is_accurate (TextureMethod method)
{
  switch (method)
  {
    case TEXTURE_METHOD_LOCAL:
    case TEXTURE_METHOD_TIFF:
      return TRUE;

    case TEXTURE_METHOD_GL_NATIVE:
    case TEXTURE_METHOD_GL:
    case TEXTURE_METHOD_GL_RELEASED:
    case TEXTURE_METHOD_VULKAN:
    case TEXTURE_METHOD_PNG:
    case TEXTURE_METHOD_PNG_PIXBUF:
    case TEXTURE_METHOD_TIFF_PIXBUF:
      return FALSE;

    case N_TEXTURE_METHODS:
    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

static GdkTexture *
ensure_texture_format (GdkTexture      *texture,
                       GdkMemoryFormat  format)
{
  GdkTextureDownloader *downloader;
  GdkTexture *result;
  GBytes *bytes;
  gsize stride;

  if (gdk_texture_get_format (texture) == format)
    return texture;

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, format);
  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
  gdk_texture_downloader_free (downloader);

  result = gdk_memory_texture_new (gdk_texture_get_width (texture),
                                   gdk_texture_get_height (texture),
                                   format,
                                   bytes,
                                   stride);
  g_bytes_unref (bytes);
  g_object_unref (texture);

  return result;
}

static void
color_make_opaque (GdkRGBA       *result,
                   const GdkRGBA *color)
{
  result->red *= color->alpha;
  result->green *= color->alpha;
  result->blue *= color->alpha;
  result->alpha = 1.0f;
}

static void
color_make_gray (GdkRGBA       *result,
                 const GdkRGBA *color)
{
  result->red = (color->red + color->green + color->blue) / 3.0f;
  result->green = result->red;
  result->blue = result->red;
  result->alpha = color->alpha;
}

static void
color_make_white (GdkRGBA       *result,
                  const GdkRGBA *color)
{
  result->red = 1.0f;
  result->green = 1.0f;
  result->blue = 1.0f;
  result->alpha = color->alpha;
}

/* Generate colors so that premultiplying will result in values in steps of 1/15th.
 * Also make sure that an averaged gray value fits in that range. */
static void
create_random_color (GdkRGBA *color)
{
  int r, g, b;
  do
    {
      r = g_test_rand_int_range (0, 6);
      g = g_test_rand_int_range (0, 6);
      b = g_test_rand_int_range (0, 6);
    }
  while ((r + g + b) % 3 != 0);
  color->red = r / 5.f;
  color->green = g / 5.f;
  color->blue = b / 5.f;
  color->alpha = g_test_rand_int_range (0, 4) / 3.f;
}

static gboolean
should_skip_download_test (GdkMemoryFormat format,
                           TextureMethod   method)
{
  switch (method)
  {
    case TEXTURE_METHOD_LOCAL:
    case TEXTURE_METHOD_PNG:
    case TEXTURE_METHOD_PNG_PIXBUF:
    case TEXTURE_METHOD_TIFF:
      return FALSE;

    case TEXTURE_METHOD_GL:
    case TEXTURE_METHOD_GL_RELEASED:
      if (gl_renderer == NULL)
        {
          g_test_skip ("NGL renderer is not supported");
          return TRUE;
        }
      return FALSE;

    case TEXTURE_METHOD_GL_NATIVE:
      return gl_native_should_skip_format (format);

    case TEXTURE_METHOD_VULKAN:
      if (vulkan_renderer == NULL)
        {
          g_test_skip ("Vulkan is not supported");
          return TRUE;
        }
      return FALSE;

    case TEXTURE_METHOD_TIFF_PIXBUF:
      g_test_skip ("the pixbuf tiff loader is broken (gdk-pixbuf#100)");
      return TRUE;

    case N_TEXTURE_METHODS:
    default:
      g_assert_not_reached ();
      return TRUE;
  }
}

static void
test_download (gconstpointer data,
               unsigned int  width,
               unsigned int  height,
               gsize         n_runs)
{
  GdkMemoryFormat format;
  TextureMethod method;
  GdkTexture *expected, *test;
  gsize i;

  decode (data, &format, &method);

  if (should_skip_download_test (format, method))
    return;

  for (i = 0; i < n_runs; i++)
    {
      GdkRGBA color;

      create_random_color (&color);

      /* these methods may premultiply during operation */
      if (color.alpha == 0.f &&
          !gdk_memory_format_is_premultiplied (format) &&
          gdk_memory_format_has_alpha (format) &&
          (method == TEXTURE_METHOD_GL_NATIVE || method == TEXTURE_METHOD_VULKAN ||
           method == TEXTURE_METHOD_GL || method == TEXTURE_METHOD_GL_RELEASED))
        color = (GdkRGBA) { 0, 0, 0, 0 };

      expected = create_texture (format, TEXTURE_METHOD_LOCAL, width, height, &color);
      test = create_texture (format, method, width, height, &color);
      test = ensure_texture_format (test, format);
      
      compare_textures (expected, test, texture_method_is_accurate (method));

      g_object_unref (expected);
      g_object_unref (test);
    }
}

static void
test_download_1x1 (gconstpointer data)
{
  test_download (data, 1, 1, N);
}

static void
test_download_random (gconstpointer data)
{
  int width, height;

  /* Make sure the maximum is:
   * - larger than what GSK puts into the icon cache
   * - larger than the small max-texture-size we test against
   */
  do
    {
      width = g_test_rand_int_range (1, 40) * g_test_rand_int_range (1, 40);
      height = g_test_rand_int_range (1, 40) * g_test_rand_int_range (1, 40);
    }
  while (width * height >= 32 * 1024);

  test_download (data, width, height, 1);
}

static void
test_conversion (gconstpointer data,
                 int           size)
{
  GdkMemoryFormat format1, format2;
  GdkTexture *test1, *test2;
  GdkRGBA color1, color2;
  gboolean accurate;
  gsize i;

  decode_two_formats (data, &format1, &format2);

  if (gdk_memory_format_get_channel_type (format1) == CHANNEL_FLOAT_16)
    accurate = FALSE;
  else
    accurate = TRUE;

  for (i = 0; i < N; i++)
    {
      /* non-premultiplied can represet GdkRGBA (1, 1, 1, 0)
       * but premultiplied cannot.
       * Premultiplied will always represent this as (0, 0, 0, 0)
       */
      do
        {
          create_random_color (&color1);
        }
      while (color1.alpha == 0 &&
             gdk_memory_format_is_premultiplied (format1) !=
             gdk_memory_format_is_premultiplied (format2));

      /* If the source can't handle alpha, make sure
       * the target uses with the opaque version of the color.
       */
      color2 = color1;
      if (!gdk_memory_format_has_alpha (format1) &&
          gdk_memory_format_has_alpha (format2))
        color_make_opaque (&color2, &color2);

      /* If the source has fewer color channels than the
       * target, make sure the colors get adjusted.
       */
      if (gdk_memory_format_n_colors (format1) <
          gdk_memory_format_n_colors (format2))
        {
          if (gdk_memory_format_n_colors (format1) == 1)
            color_make_gray (&color2, &color2);
          else
            color_make_white (&color2, &color2);
        }

      test1 = create_texture (format1, TEXTURE_METHOD_LOCAL, 1, 1, &color1);
      test2 = create_texture (format2, TEXTURE_METHOD_LOCAL, 1, 1, &color2);

      /* Convert the first one to the format of the 2nd */
      test1 = ensure_texture_format (test1, format2);
      
      compare_textures (test1, test2, accurate);

      g_object_unref (test2);
      g_object_unref (test1);
    }
}

static void
test_conversion_1x1 (gconstpointer data)
{
  test_conversion (data, 1);
}

static void
test_conversion_random (gconstpointer data)
{
  test_conversion (data, g_test_rand_int_range (2, 18));
}

static void
add_test (const char    *name,
          GTestDataFunc  func)
{
  GdkMemoryFormat format;
  TextureMethod method;
  GEnumClass *enum_class;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
    {
      for (method = 0; method < N_TEXTURE_METHODS; method++)
        {
          const char *method_names[N_TEXTURE_METHODS] = {
            [TEXTURE_METHOD_LOCAL] = "local",
            [TEXTURE_METHOD_GL_NATIVE] = "gl-native",
            [TEXTURE_METHOD_GL] = "gl",
            [TEXTURE_METHOD_GL_RELEASED] = "gl-released",
            [TEXTURE_METHOD_VULKAN] = "vulkan",
            [TEXTURE_METHOD_PNG] = "png",
            [TEXTURE_METHOD_PNG_PIXBUF] = "png-pixbuf",
            [TEXTURE_METHOD_TIFF] = "tiff",
            [TEXTURE_METHOD_TIFF_PIXBUF] = "tiff-pixbuf"
          };
          char *test_name = g_strdup_printf ("%s/%s/%s",
                                             name,
                                             g_enum_get_value (enum_class, format)->value_nick,
                                             method_names[method]);
          g_test_add_data_func_full (test_name, encode (format, method), func, NULL);
          g_free (test_name);
        }
    }
}

static void
add_conversion_test (const char    *name,
                     GTestDataFunc  func)
{
  GdkMemoryFormat format1, format2;
  GEnumClass *enum_class;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  for (format1 = 0; format1 < GDK_MEMORY_N_FORMATS; format1++)
    {
      for (format2 = 0; format2 < GDK_MEMORY_N_FORMATS; format2++)
        {
          char *test_name = g_strdup_printf ("%s/%s/%s",
                                             name,
                                             g_enum_get_value (enum_class, format1)->value_nick,
                                             g_enum_get_value (enum_class, format2)->value_nick);
          g_test_add_data_func_full (test_name, encode_two_formats (format1, format2), func, NULL);
          g_free (test_name);
        }
    }
}

int
main (int argc, char *argv[])
{
  GdkDisplay *display;
  int result;

  gtk_test_init (&argc, &argv, NULL);

  add_test ("/memorytexture/download_1x1", test_download_1x1);
  add_test ("/memorytexture/download_random", test_download_random);
  add_conversion_test ("/memorytexture/conversion_1x1", test_conversion_1x1);
  add_conversion_test ("/memorytexture/conversion_random", test_conversion_random);

  display = gdk_display_get_default ();

  gl_context = gdk_display_create_gl_context (display, NULL);
  if (gl_context == NULL || !gdk_gl_context_realize (gl_context, NULL))
    {
      g_clear_object (&gl_context);
    }

  gl_renderer = gsk_ngl_renderer_new ();
  if (!gsk_renderer_realize_for_display (gl_renderer, display, NULL))
    {
      g_clear_object (&gl_renderer);
    }

  vulkan_renderer = gsk_vulkan_renderer_new ();
  if (!gsk_renderer_realize_for_display (vulkan_renderer, display, NULL))
    {
      g_clear_object (&vulkan_renderer);
    }

  result = g_test_run ();

#ifdef GDK_RENDERING_VULKAN
  if (vulkan_renderer)
    {
      gsk_renderer_unrealize (vulkan_renderer);
      g_clear_object (&vulkan_renderer);
    }
#endif
  if (gl_renderer)
    {
      gsk_renderer_unrealize (gl_renderer);
      g_clear_object (&gl_renderer);
    }
  gdk_gl_context_clear_current ();

  g_clear_object (&gl_context);

  return result;
}
