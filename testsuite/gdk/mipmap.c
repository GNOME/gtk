#include <gtk/gtk.h>

#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkmemorytextureprivate.h"

#include "testsuite/gdk/gdktestutils.h"

#if 0
static guchar *
memory_repeat (const guchar          *src,
               const GdkMemoryLayout *src_layout,
               gsize                  lod_level,
               GdkMemoryLayout       *out_layout)
{
  guchar *data;
  gsize x, y, n;

  n = 1 << lod_level;
  gdk_memory_layout_init (out_layout,
                          src_layout->format,
                          src_layout->width << lod_level,
                          src_layout->height << lod_level,
                          1);
  data = g_malloc (out_layout->size);

  for (y = 0; y < n; y++)
    {
      for (x = 0; x < n; x++)
        {
          gdk_memory_layout_sublayout (&sub,
                                       out_layout,
                                       &(cairo_rectangle_int_t) {
                                           x * src_layout->width,
                                           y * src_layout->height,
                                           src_layout->width,
                                           src_layout->height
                                       });
          gdk_memory_copy (data,
                           sub,
                           src,
                           src_layout);
        }
    }

  return data;
}

static guchar *
memory_stretch (const guchar          *src,
                const GdkMemoryLayout *src_layout,
                gsize                  scale_x,
                gsize                  scale_y,
                GdkMemoryLayout       *out_layout)
{
  guchar *data;
  gsize p, x, y, sx, sy;

  gdk_memory_layout_init (out_layout,
                          src_layout->format,
                          src_layout->width * scale_x,
                          src_layout->height * scale_y,
                          1);
  data = g_malloc (out_layout->size);

  for (p = 0; p < gdk_memory_format_get_n_planes (src_layout->format); p++)
    {
      gsize block_width = gdk_memory_format_get_block_width (src_layout->format, p);
      gsize block_height = gdk_memory_format_get_block_height (src_layout->format, p);
      gsize block_bytes = gdk_memory_format_get_block_bytes (src_layout->format, p);

      for (y = 0; y < src_layout->height; y++)
        {
          for (ys = 0; ys < scale_y; ys)
            {
              for (x = 0; x < src_layout->width; x++)
                {
                  for (xs = 0; xs < scale_x; xs)
                    {
                      memcpy (data + out_layout->planes[p].offset
                                   + (y * scale_y + ys) * out_layout->planes[p].stride
                                   + (x * scale_x + xs) * block_bytes,
                              src + src_layout->planes[p].offset
                                   + y * src_layout->planes[p].stride
                                   + x * block_bytes,
                              block_bytes);
                    }
                }
            }
        }
    }

  return data;
}
#endif

static gpointer
encode (GdkMemoryFormat format,
        gsize           size,
        gsize           lod_level)
{
  g_assert (lod_level < (1 << 4));
  return GSIZE_TO_POINTER ((size * (1 << 4) + lod_level) * GDK_MEMORY_N_FORMATS + format);
}

static void
decode (gconstpointer    data,
        GdkMemoryFormat *format,
        gsize           *size,
        gsize           *lod_level)
{
  gsize s = GPOINTER_TO_SIZE (data);
  *format = s % GDK_MEMORY_N_FORMATS;
  s /= GDK_MEMORY_N_FORMATS;
  *lod_level = s % (1 << 4);
  s /= 1 << 4;
  *size = s;
}

static void
test_mipmap (gconstpointer data_)
{
  GdkMemoryFormat format;
  TextureBuilder builder;
  GdkTexture *ref, *large, *mipmap;
  GdkRGBA color;
  GBytes *bytes;
  gsize size, lod_level;
  GdkMemoryLayout layout;
  guchar *data;

  decode (data_, &format, &size, &lod_level);

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
                     FALSE);
  bytes = g_bytes_new_take (data, layout.size);
  mipmap = gdk_memory_texture_new_from_layout (bytes, &layout, gdk_color_state_get_srgb (), NULL, NULL);
  g_bytes_unref (bytes);
  
  compare_textures (ref, mipmap, TRUE);

  g_object_unref (mipmap);
  g_object_unref (large);
  g_object_unref (ref);
}
/*

void        texture_builder_init      (TextureBuilder  *builder,
                                       GdkMemoryFormat  format,
                                       int              width,
                                       int              height);
GdkTexture *texture_builder_finish    (TextureBuilder  *builder);
void        texture_builder_fill      (TextureBuilder  *builder,
                                       const GdkRGBA   *color);
  
}


void                    gdk_memory_convert                  (guchar                     *dest_data,
                                                             const GdkMemoryLayout      *dest_layout,
                                                             GdkColorState              *dest_cs,
                                                             const guchar               *src_data,
                                                             const GdkMemoryLayout      *src_layout,
                                                             GdkColorState              *src_cs);
void                    gdk_memory_convert_color_state      (guchar                     *data,
                                                             const GdkMemoryLayout      *layout,
                                                             GdkColorState              *src_color_state,
                                                             GdkColorState              *dest_color_state);
void                    gdk_memory_mipmap                   (guchar                     *dest,
                                                             const GdkMemoryLayout      *dest_layout,
                                                             const guchar               *src,
                                                             const GdkMemoryLayout      *src_layout,
                                                             guint                       lod_level,
                                                             gboolean                    linear);
}
*/
static void
add_test (const char    *name,
          GTestDataFunc  func)
{
  GdkMemoryFormat format;
  GEnumClass *enum_class;
  unsigned size, lod_level;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  size = 4;
  lod_level = 1;

  for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
    {
      char *test_name = g_strdup_printf ("/mipmap/%s/%s/%ux%u/%u",
                                         name,
                                         g_enum_get_value (enum_class, format)->value_nick,
                                         size, size,
                                         lod_level);
      g_test_add_data_func_full (test_name, encode (format, size, lod_level), func, NULL);
      g_free (test_name);
    }
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  add_test ("mipmap", test_mipmap);

  return g_test_run ();
}
