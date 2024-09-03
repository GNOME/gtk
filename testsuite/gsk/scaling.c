#include <gtk/gtk.h>

#define N 10

#include "gsk/gl/fp16private.h"
#include "testsuite/gdk/gdktestutils.h"

struct {
  const char *name;
  GskRenderer * (*create_func) (void);
  GskRenderer *renderer;
} renderers[] = {
#if 0
  /* The GL renderer is broken, no idea why. It's suppsoed to work. */
  {
    "gl",
    gsk_gl_renderer_new,
  },
#endif
  {
    "cairo",
    gsk_cairo_renderer_new,
  },
  {
    "vulkan",
    gsk_vulkan_renderer_new,
  },
  {
    "ngl",
    gsk_ngl_renderer_new,
  },
};

static gpointer
encode_renderer_format (guint           renderer,
                        GdkMemoryFormat format)
{
  return GSIZE_TO_POINTER (format * G_N_ELEMENTS (renderers) + renderer);
}

static void
decode_renderer_format (gconstpointer     data,
                        GskRenderer     **renderer,
                        GdkMemoryFormat  *format)
{
  gsize value = GPOINTER_TO_SIZE (data);

  *renderer = renderers[value % G_N_ELEMENTS (renderers)].renderer;
  value /= G_N_ELEMENTS (renderers);

  *format = value;
}

#if 0
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
#endif

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

static void
create_random_color_for_format (GdkRGBA *color,
                                GdkMemoryFormat format)
{
  /* non-premultiplied can represet GdkRGBA (1, 1, 1, 0)
   * but premultiplied cannot.
   * Premultiplied will always represent this as (0, 0, 0, 0)
   */
  do
    {
      create_random_color (color);
    }
  while (color->alpha == 0 &&
         gdk_memory_format_is_premultiplied (format));

  /* If the format can't handle alpha, make things opaque
   */
  if (!gdk_memory_format_has_alpha (format))
    color_make_opaque (color, color);

  /* If the format has fewer color channels than the
   * target, make sure the colors get adjusted.
   */
  if (gdk_memory_format_n_colors (format) == 1)
    color_make_gray (color, color);
  else if (gdk_memory_format_n_colors (format) == 0)
    color_make_white (color, color);
}

static GdkTexture *
create_solid_color_texture (GdkMemoryFormat  format,
                            int              width,
                            int              height,
                            const GdkRGBA   *color)
{
  TextureBuilder builder;

  texture_builder_init (&builder, format, width, height);
  texture_builder_fill (&builder, color);

  return texture_builder_finish (&builder);
}

/* randomly creates 4 colors with values that are multiples
 * of 16, so that averaging the colors works without rounding
 * errors, and then creates a stipple pattern like this:
 *
 * 1 2 1 2 1 2 ...
 * 3 4 3 4 3 4
 * 1 2 1 2 1 2
 * 3 4 3 4 3 4
 * 1 2 1 2 1 2
 * 3 4 3 4 3 4
 * ⋮
 */
static GdkTexture *
create_stipple_texture (GdkMemoryFormat  format,
                        gsize            width,
                        gsize            height,
                        GdkRGBA          colors[2][2],
                        GdkRGBA         *average)
{
  TextureBuilder builder;
  GdkTexture *texture;
  int x, y;

  *average = (GdkRGBA) { 0, 0, 0, 0 };

  for (y = 0; y < 2; y++)
    {
      for (x = 0; x < 2; x++)
        {
          create_random_color_for_format (&colors[x][y], format);
          if (gdk_memory_format_has_alpha (format))
            colors[x][y].alpha *= 16.f/17.f;
          else
            {
              colors[x][y].red *= 16.f/17.f;
              colors[x][y].green *= 16.f/17.f;
              colors[x][y].blue *= 16.f/17.f;
            }

          average->red += colors[x][y].red * colors[x][y].alpha;
          average->green += colors[x][y].green * colors[x][y].alpha;
          average->blue += colors[x][y].blue * colors[x][y].alpha;
          average->alpha += colors[x][y].alpha;
        }
    }

  if (average->alpha != 0.0f)
    {
      average->red /= average->alpha;
      average->green /= average->alpha;
      average->blue /= average->alpha;
      average->alpha /= 4.0f;
    }
  else
    {
      /* Each component of the average has been multiplied by the alpha
       * already, so if the alpha is zero, all components should also
       * be zero */
      g_assert_cmpfloat (average->red, ==, 0.0f);
      g_assert_cmpfloat (average->green, ==, 0.0f);
      g_assert_cmpfloat (average->blue, ==, 0.0f);
    }

  texture_builder_init (&builder, format, width, height);
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          texture_builder_set_pixel (&builder, x, y, &colors[x % 2][y % 2]);
        }
    }
  texture = texture_builder_finish (&builder);

  return texture;
}

static void
dump_scaling_input (const GdkRGBA  colors[2][2],
                    const GdkRGBA *average)
{
  int x, y;

  for (y = 0; y < 2; y++)
    {
      for (x = 0; x < 2; x++)
        g_test_message ("input stipple texture (%d,%d) r=%f g=%f b=%f a=%f",
                        x, y,
                        colors[x][y].red,
                        colors[x][y].green,
                        colors[x][y].blue,
                        colors[x][y].alpha);
    }

  g_test_message ("expected average r=%f g=%f b=%f a=%f",
                  average->red, average->green, average->blue, average->alpha);
}

static void
test_linear_filtering (gconstpointer data,
                       gsize         width,
                       gsize         height)
{
  GdkMemoryFormat format;
  GskRenderer *renderer;
  GdkTexture *input, *output, *expected;
  GskRenderNode *node;
  GdkRGBA colors[2][2];
  GdkRGBA average_color;

  decode_renderer_format (data, &renderer, &format);

  input = create_stipple_texture (format, width, height, colors, &average_color);
  node = gsk_texture_scale_node_new (input, &GRAPHENE_RECT_INIT (0, 0, width / 2, height / 2), GSK_SCALING_FILTER_LINEAR);
  output = gsk_renderer_render_texture (renderer, node, NULL);
  expected = create_solid_color_texture (gdk_texture_get_format (output), width / 2, height / 2, &average_color);

  compare_textures (expected, output, FALSE);

  if (g_test_failed ())
    dump_scaling_input (colors, &average_color);

  g_object_unref (expected);
  g_object_unref (output);
  gsk_render_node_unref (node);
  g_object_unref (input);
}

static void
test_mipmaps (gconstpointer data)
{
  GdkMemoryFormat format;
  GskRenderer *renderer;
  GdkTexture *input, *output, *expected;
  GskRenderNode *node;
  GdkRGBA colors[2][2];
  GdkRGBA average_color;

  decode_renderer_format (data, &renderer, &format);

  input = create_stipple_texture (format, 2, 2, colors, &average_color);
  node = gsk_texture_scale_node_new (input, &GRAPHENE_RECT_INIT (0, 0, 1, 1), GSK_SCALING_FILTER_TRILINEAR);
  output = gsk_renderer_render_texture (renderer, node, NULL);
  expected = create_solid_color_texture (gdk_texture_get_format (output), 1, 1, &average_color);

  compare_textures (expected, output, FALSE);

  if (g_test_failed ())
    dump_scaling_input (colors, &average_color);

  g_object_unref (expected);
  g_object_unref (output);
  gsk_render_node_unref (node);
  g_object_unref (input);
}

static void
test_linear_filtering_2x2 (gconstpointer data)
{
  test_linear_filtering (data, 2, 2);
}

static void
test_linear_filtering_512x512 (gconstpointer data)
{
  test_linear_filtering (data, 512, 512);
}

static void
add_format_test (const char    *name,
                 GTestDataFunc  func)
{
  GdkMemoryFormat format;
  gsize renderer;
  GEnumClass *enum_class;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  for (renderer = 0; renderer < G_N_ELEMENTS (renderers); renderer++)
    {
      if (renderers[renderer].renderer == NULL)
        continue;

      for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
        {
          char *test_name = g_strdup_printf ("%s/%s/%s",
                                             name,
                                             renderers[renderer].name,
                                             g_enum_get_value (enum_class, format)->value_nick);
          g_test_add_data_func_full (test_name, encode_renderer_format (renderer, format), func, NULL);
          g_free (test_name);
        }
    }
}

static void
create_renderers (void)
{
  GError *error = NULL;
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (renderers); i++)
    {
      renderers[i].renderer = renderers[i].create_func ();
      if (!gsk_renderer_realize_for_display (renderers[i].renderer, gdk_display_get_default (), &error))
        {
          g_test_message ("Could not realize %s renderer: %s", renderers[i].name, error->message);
          g_clear_error (&error);
          g_clear_object (&renderers[i].renderer);
        }
    }
}

static void
destroy_renderers (void)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (renderers); i++)
    {
      if (renderers[i].renderer == NULL)
        continue;

      gsk_renderer_unrealize (renderers[i].renderer);
      g_clear_object (&renderers[i].renderer);
    }
}

int
main (int argc, char *argv[])
{
  int result;

  gtk_test_init (&argc, &argv, NULL);
  create_renderers ();

  add_format_test ("/scaling/linear-filtering", test_linear_filtering_2x2);
  add_format_test ("/scaling/linear-filtering-large", test_linear_filtering_512x512);
  add_format_test ("/scaling/mipmap", test_mipmaps);

  result = g_test_run ();

  /* So the context gets actually destroyed */
  gdk_gl_context_clear_current ();

  destroy_renderers ();

  return result;
}
