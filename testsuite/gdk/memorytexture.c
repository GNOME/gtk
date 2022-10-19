#include <gtk/gtk.h>

#include "gsk/gl/gskglrenderer.h"

#define N 20

static GskRenderer *gl_renderer = NULL;

typedef struct _TextureBuilder TextureBuilder;

typedef enum {
  TEXTURE_METHOD_LOCAL,
  TEXTURE_METHOD_GL,
  TEXTURE_METHOD_GL_RELEASED,
  TEXTURE_METHOD_PNG,
  TEXTURE_METHOD_PNG_PIXBUF,
  TEXTURE_METHOD_TIFF,
  TEXTURE_METHOD_TIFF_PIXBUF,

  N_TEXTURE_METHODS
} TextureMethod;

struct _TextureBuilder
{
  GdkMemoryFormat format;
  int width;
  int height;

  guchar *pixels;
  gsize stride;
  gsize offset;
};

static gsize
gdk_memory_format_bytes_per_pixel (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
      return 3;

    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
      return 4;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
      return 6;

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
      return 8;

    case GDK_MEMORY_R32G32B32_FLOAT:
      return 12;

    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
      return 16;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return 4;
    }
}

static gboolean
gdk_memory_format_has_alpha (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
      return FALSE;

    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
      return TRUE;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return TRUE;
    }
}

static gpointer
encode (GdkMemoryFormat format,
        TextureMethod   method)
{
  return GSIZE_TO_POINTER (method * GDK_MEMORY_N_FORMATS + format);
}

static gboolean
decode (gconstpointer     data,
        GdkMemoryFormat *format,
        TextureMethod   *method)
{
  gsize value = GPOINTER_TO_SIZE (data);

  *format = value % GDK_MEMORY_N_FORMATS;
  value /= GDK_MEMORY_N_FORMATS;

  *method = value;

  if (*method == TEXTURE_METHOD_TIFF_PIXBUF)
    {
      g_test_skip ("the pixbuf tiff loader is broken (gdk-pixbuf#100)");
      return FALSE;
    }

  return TRUE;
}

static void
texture_builder_init (TextureBuilder  *builder,
                      GdkMemoryFormat  format,
                      int              width,
                      int              height)
{
  gsize extra_stride;

  builder->format = format;
  builder->width = width;
  builder->height = height;

  extra_stride = g_test_rand_bit() ? g_test_rand_int_range (0, 16) : 0;
  builder->offset = g_test_rand_bit() ? g_test_rand_int_range (0, 128) : 0;
  builder->stride = width * gdk_memory_format_bytes_per_pixel (format) + extra_stride;
  builder->pixels = g_malloc0 (builder->offset + builder->stride * height);
}

static GdkTexture *
texture_builder_finish (TextureBuilder *builder)
{
  GBytes *bytes;
  GdkTexture *texture;

  bytes = g_bytes_new_with_free_func (builder->pixels + builder->offset,
                                      builder->height * builder->stride,
                                      g_free,
                                      builder->pixels);
  texture = gdk_memory_texture_new (builder->width,
                                    builder->height,
                                    builder->format,
                                    bytes,
                                    builder->stride);
  g_bytes_unref (bytes);

  return texture;
}

static inline void
set_pixel_u8 (guchar          *data,
              int              r,
              int              g,
              int              b,
              int              a,
              gboolean         premultiply,
              const GdkRGBA   *color)
{
  if (a >= 0)
    data[a] = CLAMP (color->alpha * 256.f, 0.f, 255.f);
  if (premultiply)
    {
      data[r] = CLAMP (color->red * color->alpha * 256.f, 0.f, 255.f);
      data[g] = CLAMP (color->green * color->alpha * 256.f, 0.f, 255.f);
      data[b] = CLAMP (color->blue * color->alpha * 256.f, 0.f, 255.f);
    }
  else
    {
      data[r] = CLAMP (color->red * 256.f, 0.f, 255.f);
      data[g] = CLAMP (color->green * 256.f, 0.f, 255.f);
      data[b] = CLAMP (color->blue * 256.f, 0.f, 255.f);
    }
}

static inline guint16
float_to_half (const float x)
{
  const guint b = *(guint*)&x+0x00001000; // round-to-nearest-even
  const guint e = (b&0x7F800000)>>23; // exponent
  const guint m = b&0x007FFFFF; // mantissa
  return (b&0x80000000)>>16 | (e>112)*((((e-112)<<10)&0x7C00)|m>>13) | ((e<113)&(e>101))*((((0x007FF000+m)>>(125-e))+1)>>1) | (e>143)*0x7FFF; // sign : normalized : denormalized : saturate
}

static void
texture_builder_set_pixel (TextureBuilder  *builder,
                           int              x,
                           int              y,
                           const GdkRGBA   *color)
{
  guchar *data;

  g_assert_cmpint (x, >=, 0);
  g_assert_cmpint (x, <, builder->width);
  g_assert_cmpint (y, >=, 0);
  g_assert_cmpint (y, <, builder->height);

  data = builder->pixels
         + builder->offset
         + y * builder->stride
         + x * gdk_memory_format_bytes_per_pixel (builder->format);

  switch (builder->format)
  {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      set_pixel_u8 (data, 2, 1, 0, 3, TRUE, color);
      break;
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      set_pixel_u8 (data, 1, 2, 3, 0, TRUE, color);
      break;
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      set_pixel_u8 (data, 0, 1, 2, 3, TRUE, color);
      break;
    case GDK_MEMORY_B8G8R8A8:
      set_pixel_u8 (data, 2, 1, 0, 3, FALSE, color);
      break;
    case GDK_MEMORY_A8R8G8B8:
      set_pixel_u8 (data, 1, 2, 3, 0, FALSE, color);
      break;
    case GDK_MEMORY_R8G8B8A8:
      set_pixel_u8 (data, 0, 1, 2, 3, FALSE, color);
      break;
    case GDK_MEMORY_A8B8G8R8:
      set_pixel_u8 (data, 3, 2, 1, 0, FALSE, color);
      break;
    case GDK_MEMORY_R8G8B8:
      set_pixel_u8 (data, 0, 1, 2, -1, TRUE, color);
      break;
    case GDK_MEMORY_B8G8R8:
      set_pixel_u8 (data, 2, 1, 0, -1, TRUE, color);
      break;
    case GDK_MEMORY_R16G16B16:
      {
        guint16 pixels[3] = {
          CLAMP (color->red * color->alpha * 65536.f, 0, 65535.f),
          CLAMP (color->green * color->alpha * 65536.f, 0, 65535.f),
          CLAMP (color->blue * color->alpha * 65536.f, 0, 65535.f),
        };
        memcpy (data, pixels, 3 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      {
        guint16 pixels[4] = {
          CLAMP (color->red * color->alpha * 65536.f, 0, 65535.f),
          CLAMP (color->green * color->alpha * 65536.f, 0, 65535.f),
          CLAMP (color->blue * color->alpha * 65536.f, 0, 65535.f),
          CLAMP (color->alpha * 65536.f, 0, 65535.f),
        };
        memcpy (data, pixels, 4 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16A16:
      {
        guint16 pixels[4] = {
          CLAMP (color->red * 65536.f, 0, 65535.f),
          CLAMP (color->green * 65536.f, 0, 65535.f),
          CLAMP (color->blue * 65536.f, 0, 65535.f),
          CLAMP (color->alpha * 65536.f, 0, 65535.f),
        };
        memcpy (data, pixels, 4 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16_FLOAT:
      {
        guint16 pixels[3] = {
          float_to_half (color->red * color->alpha),
          float_to_half (color->green * color->alpha),
          float_to_half (color->blue * color->alpha)
        };
        memcpy (data, pixels, 3 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      {
        guint16 pixels[4] = {
          float_to_half (color->red * color->alpha),
          float_to_half (color->green * color->alpha),
          float_to_half (color->blue * color->alpha),
          float_to_half (color->alpha)
        };
        memcpy (data, pixels, 4 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16A16_FLOAT:
      {
        guint16 pixels[4] = {
          float_to_half (color->red),
          float_to_half (color->green),
          float_to_half (color->blue),
          float_to_half (color->alpha)
        };
        memcpy (data, pixels, 4 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R32G32B32_FLOAT:
      {
        float pixels[3] = {
          color->red * color->alpha,
          color->green * color->alpha,
          color->blue * color->alpha
        };
        memcpy (data, pixels, 3 * sizeof (float));
      }
      break;
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      {
        float pixels[4] = {
          color->red * color->alpha,
          color->green * color->alpha,
          color->blue * color->alpha,
          color->alpha
        };
        memcpy (data, pixels, 4 * sizeof (float));
      }
      break;
    case GDK_MEMORY_R32G32B32A32_FLOAT:
      {
        float pixels[4] = {
          color->red,
          color->green,
          color->blue,
          color->alpha
        };
        memcpy (data, pixels, 4 * sizeof (float));
      }
      break;
    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
texture_builder_fill (TextureBuilder  *builder,
                      const GdkRGBA   *color)
{
  int x, y;

  for (y = 0; y < builder->height; y++)
    for (x = 0; x < builder->width; x++)
      texture_builder_set_pixel (builder, x, y, color);
}

static void
compare_textures (GdkTexture *expected,
                  GdkTexture *test,
                  gboolean    has_alpha)
{
  guint32 *expected_data, *test_data;
  int width, height;
  int x, y;

  g_assert_cmpint (gdk_texture_get_width (expected), ==, gdk_texture_get_width (test));
  g_assert_cmpint (gdk_texture_get_height (expected), ==, gdk_texture_get_height (test));

  width = gdk_texture_get_width (expected);
  height = gdk_texture_get_height (expected);

  expected_data = g_new (guint32, width * height);
  gdk_texture_download (expected, (guchar *) expected_data, width * 4);

  test_data = g_new (guint32, width * height);
  gdk_texture_download (test, (guchar *) test_data, width * 4);

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          if (has_alpha)
            g_assert_cmphex (expected_data[y * width + x], ==, test_data[y * width + x]);
          else
            g_assert_cmphex (expected_data[y * width + x] | 0xFF000000, ==, test_data[y * width + x]);
        }
    }

  g_free (expected_data);
  g_free (test_data);
}

static GdkTexture *
upload_to_gl (GdkTexture *texture)
{
  GskRenderNode *node;
  GdkTexture *result;

  if (gl_renderer == NULL)
    return texture;

  node = gsk_texture_node_new (texture,
                               &GRAPHENE_RECT_INIT(
                                 0, 0, 
                                 gdk_texture_get_width (texture),
                                 gdk_texture_get_height (texture)
                               ));
  result = gsk_renderer_render_texture (gl_renderer, node, NULL);
  gsk_render_node_unref (node);
  g_object_unref (texture);

  return result;
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

    case TEXTURE_METHOD_GL:
      texture = upload_to_gl (texture);
      break;

    case TEXTURE_METHOD_GL_RELEASED:
      texture = upload_to_gl (texture);
      if (GDK_IS_GL_TEXTURE (texture))
        gdk_gl_texture_release (GDK_GL_TEXTURE (texture));
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

static void
create_random_color (GdkRGBA *color)
{
  /* Generate colors so that premultiplying will result in values in steps of 1/15th */
  color->red = g_test_rand_int_range (0, 6) / 5.f;
  color->green = g_test_rand_int_range (0, 6) / 5.f;
  color->blue = g_test_rand_int_range (0, 6) / 5.f;
  color->alpha = g_test_rand_int_range (0, 4) / 3.f;
}

static void
test_download_1x1 (gconstpointer data)
{
  GdkMemoryFormat format;
  TextureMethod method;
  GdkTexture *expected, *test;
  gsize i;

  if (!decode (data, &format, &method))
    return;

  for (i = 0; i < N; i++)
    {
      GdkRGBA color;

      create_random_color (&color);
      expected = create_texture (GDK_MEMORY_DEFAULT, TEXTURE_METHOD_LOCAL, 1, 1, &color);
      test = create_texture (format, method, 1, 1, &color);
      
      compare_textures (expected, test, gdk_memory_format_has_alpha (format));

      g_object_unref (expected);
      g_object_unref (test);
    }
}

static void
test_download_4x4 (gconstpointer data)
{
  GdkMemoryFormat format;
  TextureMethod method;
  GdkTexture *expected, *test;
  gsize i;

  if (!decode (data, &format, &method))
    return;

  for (i = 0; i < N; i++)
    {
      GdkRGBA color;

      create_random_color (&color);
      expected = create_texture (GDK_MEMORY_DEFAULT, TEXTURE_METHOD_LOCAL, 4, 4, &color);
      test = create_texture (format, method, 4, 4, &color);
      
      compare_textures (expected, test, gdk_memory_format_has_alpha (format));

      g_object_unref (expected);
      g_object_unref (test);
    }
}

/* larger than what NGL puts into the icon cache */
static void
test_download_192x192 (gconstpointer data)
{
  GdkMemoryFormat format;
  TextureMethod method;
  GdkTexture *expected, *test;
  GdkRGBA color;

  if (!decode (data, &format, &method))
    return;

  create_random_color (&color);
  expected = create_texture (GDK_MEMORY_DEFAULT, TEXTURE_METHOD_LOCAL, 192, 192, &color);
  test = create_texture (format, method, 192, 192, &color);
  
  compare_textures (expected, test, gdk_memory_format_has_alpha (format));

  g_object_unref (expected);
  g_object_unref (test);
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
          const char *method_names[N_TEXTURE_METHODS] = { "local", "gl", "gl-released", "png", "png-pixbuf", "tiff", "tiff-pixbuf" };
          char *test_name = g_strdup_printf ("%s/%s/%s",
                                             name,
                                             g_enum_get_value (enum_class, format)->value_nick,
                                             method_names[method]);
          g_test_add_data_func_full (test_name, encode (format, method), func, NULL);
          g_free (test_name);
        }
    }
}

int
main (int argc, char *argv[])
{
  int result;

  gtk_test_init (&argc, &argv, NULL);

  add_test ("/memorytexture/download_1x1", test_download_1x1);
  add_test ("/memorytexture/download_4x4", test_download_4x4);
  add_test ("/memorytexture/download_192x192", test_download_192x192);

  gl_renderer = gsk_gl_renderer_new ();
  if (!gsk_renderer_realize (gl_renderer, NULL, NULL))
    {
      g_clear_object (&gl_renderer);
    }

  result = g_test_run ();

  if (gl_renderer)
    {
      gsk_renderer_unrealize (gl_renderer);
      g_clear_object (&gl_renderer);
    }
  gdk_gl_context_clear_current ();

  return result;
}
