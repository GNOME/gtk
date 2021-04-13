#include <locale.h>
#include <gdk/gdk.h>

/* maximum bytes per pixel */
#define MAX_BPP 4

typedef enum {
  BLUE,
  GREEN,
  RED,
  TRANSPARENT,
  ALMOST_OPAQUE_REBECCAPURPLE,
  N_COLORS
} Color;

const char * color_names[N_COLORS] = {
  "blue",
  "green",
  "red",
  "transparent",
  "almost_opaque_rebeccapurple"
};

typedef struct _MemoryData {
  gsize bytes_per_pixel;
  guint opaque : 1;
  guchar data[N_COLORS][MAX_BPP];
} MemoryData;

typedef struct _TestData {
  GdkMemoryFormat format;
  Color color;
} TestData;

#define RGBA(a, b, c, d) { 0x ## a, 0x ## b, 0x ## c, 0x ## d }

static MemoryData tests[GDK_MEMORY_N_FORMATS] = {
  { 4, FALSE, { RGBA(FF,00,00,FF), RGBA(00,FF,00,FF), RGBA(00,00,FF,FF), RGBA(00,00,00,00), RGBA(66,22,44,AA) } },
  { 4, FALSE, { RGBA(FF,00,00,FF), RGBA(FF,00,FF,00), RGBA(FF,FF,00,00), RGBA(00,00,00,00), RGBA(AA,44,22,66) } },
  { 4, FALSE, { RGBA(00,00,FF,FF), RGBA(00,FF,00,FF), RGBA(FF,00,00,FF), RGBA(00,00,00,00), RGBA(44,22,66,AA) } },
  { 4, FALSE, { RGBA(FF,00,00,FF), RGBA(00,FF,00,FF), RGBA(00,00,FF,FF), RGBA(00,00,00,00), RGBA(99,33,66,AA) } },
  { 4, FALSE, { RGBA(FF,00,00,FF), RGBA(FF,00,FF,00), RGBA(FF,FF,00,00), RGBA(00,00,00,00), RGBA(AA,66,33,99) } },
  { 4, FALSE, { RGBA(00,00,FF,FF), RGBA(00,FF,00,FF), RGBA(FF,00,00,FF), RGBA(00,00,00,00), RGBA(66,33,99,AA) } },
  { 4, FALSE, { RGBA(FF,FF,00,00), RGBA(FF,00,FF,00), RGBA(FF,00,00,FF), RGBA(00,00,00,00), RGBA(AA,99,33,66) } },
  { 3, TRUE,  { RGBA(00,00,FF,00), RGBA(00,FF,00,00), RGBA(FF,00,00,00), RGBA(00,00,00,00), RGBA(44,22,66,00) } },
  { 3, TRUE,  { RGBA(FF,00,00,00), RGBA(00,FF,00,00), RGBA(00,00,FF,00), RGBA(00,00,00,00), RGBA(66,22,44,00) } },
};

static void
compare_textures (GdkTexture *expected,
                  GdkTexture *test,
                  gboolean    ignore_alpha)
{
  guchar *expected_data, *test_data;
  int width, height;
  int x, y;

  g_assert_cmpint (gdk_texture_get_width (expected), ==, gdk_texture_get_width (test));
  g_assert_cmpint (gdk_texture_get_height (expected), ==, gdk_texture_get_height (test));

  width = gdk_texture_get_width (expected);
  height = gdk_texture_get_height (expected);

  expected_data = g_malloc (width * height * 4);
  gdk_texture_download (expected, expected_data, width * 4);

  test_data = g_malloc (width * height * 4);
  gdk_texture_download (test, test_data, width * 4);

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          if (ignore_alpha)
            g_assert_cmphex (*(guint32 *) &expected_data[y * width + x * 4] & 0xFFFFFF, ==, *(guint32 *) &test_data[y * width + x * 4] & 0xFFFFFF);
          else
            g_assert_cmphex (*(guint32 *) &expected_data[y * width + x * 4], ==, *(guint32 *) &test_data[y * width + x * 4]);
        }
    }

  g_free (expected_data);
  g_free (test_data);
}

static GdkTexture *
create_texture (GdkMemoryFormat  format,
                Color            color,
                int              width,
                int              height,
                gsize            stride)
{
  GdkTexture *texture;
  GBytes *bytes;
  guchar *data;
  int x, y;

  data = g_malloc (height * MAX (stride, tests[format].bytes_per_pixel));
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        memcpy (&data[y * stride + x * tests[format].bytes_per_pixel],
                &tests[format].data[color],
                tests[format].bytes_per_pixel);
      }

  bytes = g_bytes_new_take (data, height * MAX (stride, tests[format].bytes_per_pixel));
  texture = gdk_memory_texture_new (width, height,
                                    format,
                                    bytes,
                                    stride);
  g_bytes_unref (bytes);

  return texture;
}

static void
test_download_1x1 (gconstpointer data)
{
  const TestData *test_data = data;
  GdkTexture *expected, *test;

  expected = create_texture (GDK_MEMORY_DEFAULT, test_data->color, 1, 1, tests[test_data->format].bytes_per_pixel);
  test = create_texture (test_data->format, test_data->color, 1, 1, tests[test_data->format].bytes_per_pixel);

  compare_textures (expected, test, tests[test_data->format].opaque);

  g_object_unref (expected);
  g_object_unref (test);
}

static void
test_download_1x1_with_stride (gconstpointer data)
{
  const TestData *test_data = data;
  GdkTexture *expected, *test;

  expected = create_texture (GDK_MEMORY_DEFAULT, test_data->color, 1, 1, 4);
  test = create_texture (test_data->format, test_data->color, 1, 1, 2 * MAX_BPP);

  compare_textures (expected, test, tests[test_data->format].opaque);

  g_object_unref (expected);
  g_object_unref (test);
}

static void
test_download_4x4 (gconstpointer data)
{
  const TestData *test_data = data;
  GdkTexture *expected, *test;

  expected = create_texture (GDK_MEMORY_DEFAULT, test_data->color, 4, 4, 16);
  test = create_texture (test_data->format, test_data->color, 4, 4, 4 * tests[test_data->format].bytes_per_pixel);

  compare_textures (expected, test, tests[test_data->format].opaque);

  g_object_unref (expected);
  g_object_unref (test);
}

static void
test_download_4x4_with_stride (gconstpointer data)
{
  const TestData *test_data = data;
  GdkTexture *expected, *test;

  expected = create_texture (GDK_MEMORY_DEFAULT, test_data->color, 4, 4, 16);
  test = create_texture (test_data->format, test_data->color, 4, 4, 4 * MAX_BPP);

  compare_textures (expected, test, tests[test_data->format].opaque);

  g_object_unref (expected);
  g_object_unref (test);
}

int
main (int argc, char *argv[])
{
  GdkMemoryFormat format;
  Color color;
  GEnumClass *enum_class;

  (g_test_init) (&argc, &argv, NULL);

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
    {
      for (color = 0; color < N_COLORS; color++)
        {
          TestData *test_data = g_new (TestData, 1);
          char *test_name = g_strdup_printf ("/memorytexture/download_1x1/%s/%s",
                                             g_enum_get_value (enum_class, format)->value_nick,
                                             color_names[color]);
          test_data->format = format;
          test_data->color = color;
          g_test_add_data_func_full (test_name, test_data, test_download_1x1, g_free);
          g_free (test_name);

          test_data = g_new (TestData, 1);
          test_name = g_strdup_printf ("/memorytexture/download_1x1_with_stride/%s/%s",
                                       g_enum_get_value (enum_class, format)->value_nick,
                                       color_names[color]);
          test_data->format = format;
          test_data->color = color;
          g_test_add_data_func_full (test_name, test_data, test_download_1x1_with_stride, g_free);
          g_free (test_name);

          test_data = g_new (TestData, 1);
          test_name = g_strdup_printf ("/memorytexture/download_4x4/%s/%s",
                                       g_enum_get_value (enum_class, format)->value_nick,
                                       color_names[color]);
          test_data->format = format;
          test_data->color = color;
          g_test_add_data_func_full (test_name, test_data, test_download_4x4, g_free);
          g_free (test_name);

          test_data = g_new (TestData, 1);
          test_name = g_strdup_printf ("/memorytexture/download_4x4_with_stride/%s/%s",
                                       g_enum_get_value (enum_class, format)->value_nick,
                                       color_names[color]);
          test_data->format = format;
          test_data->color = color;
          g_test_add_data_func_full (test_name, test_data, test_download_4x4_with_stride, g_free);
          g_free (test_name);
        }
    }

  return g_test_run ();
}
