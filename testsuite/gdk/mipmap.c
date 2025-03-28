#include <gtk/gtk.h>

#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkmemorytextureprivate.h"

#include "testsuite/gdk/gdktestutils.h"

static gpointer
encode (GdkMemoryFormat format,
        gsize           size,
        gsize           lod_level,
        gboolean        linear)
{
  g_assert (lod_level < (1 << 4));
  return GSIZE_TO_POINTER ((((size * (1 << 4) + lod_level) * GDK_MEMORY_N_FORMATS + format) << 1) + linear);
}

static void
decode (gconstpointer    data,
        GdkMemoryFormat *format,
        gsize           *size,
        gsize           *lod_level,
        gboolean        *linear)
{
  gsize s = GPOINTER_TO_SIZE (data);
  *linear = s & 1;
  s >>= 1;
  *format = s % GDK_MEMORY_N_FORMATS;
  s /= GDK_MEMORY_N_FORMATS;
  *lod_level = s % (1 << 4);
  s /= 1 << 4;
  *size = s;
}

static void
test_mipmap_simple (gconstpointer data_)
{
  GdkMemoryFormat format;
  TextureBuilder builder;
  GdkTexture *ref, *large, *mipmap;
  GdkRGBA color;
  GBytes *bytes;
  gsize size, lod_level;
  gboolean linear;
  GdkMemoryLayout layout;
  guchar *data;

  decode (data_, &format, &size, &lod_level, &linear);

  color = (GdkRGBA) {
              g_test_rand_bit () * 1.0,
              g_test_rand_bit () * 1.0,
              g_test_rand_bit () * 1.0,
              0.5 + g_test_rand_bit () * 0.5,
          };

  texture_builder_init (&builder,
                        gdk_memory_format_get_mipmap_format (format),
                        size,
                        size);
  texture_builder_fill (&builder, &color);
  ref = texture_builder_finish (&builder);

  texture_builder_init (&builder,
                        format,
                        size << lod_level,
                        size << lod_level);
  texture_builder_fill (&builder, &color);
  large = texture_builder_finish (&builder);

  gdk_memory_layout_init (&layout,
                          gdk_memory_format_get_mipmap_format (format),
                          size,
                          size,
                          1);
  data = g_malloc (layout.size);
  gdk_memory_mipmap (data,
                     &layout,
                     g_bytes_get_data (gdk_memory_texture_get_bytes (GDK_MEMORY_TEXTURE (large)), NULL),
                     gdk_memory_texture_get_layout (GDK_MEMORY_TEXTURE (large)),
                     lod_level,
                     linear);
  bytes = g_bytes_new_take (data, layout.size);
  mipmap = gdk_memory_texture_new_from_layout (bytes, &layout, gdk_color_state_get_srgb (), NULL, NULL);
  g_bytes_unref (bytes);
  
  compare_textures (ref, mipmap, TRUE);

  g_object_unref (mipmap);
  g_object_unref (large);
  g_object_unref (ref);
}

static void
add_test (const char    *name,
          GTestDataFunc  func)
{
  GdkMemoryFormat format;
  GEnumClass *enum_class;
  unsigned size, lod_level, linear;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  size = 4;
  lod_level = 1;

  for (linear = FALSE; linear <= TRUE; linear++)
    {
      for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
        {
          char *test_name = g_strdup_printf ("/mipmap/%s/%s/%s/%ux%u/%u",
                                             name,
                                             linear ? "linear" : "nearest",
                                             g_enum_get_value (enum_class, format)->value_nick,
                                             size, size,
                                             lod_level);
          g_test_add_data_func_full (test_name, encode (format, size, lod_level, linear), func, NULL);
          g_free (test_name);
        }
    }
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  add_test ("simple", test_mipmap_simple);

  return g_test_run ();
}
