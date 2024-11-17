#include <gtk/gtk.h>

#include "gdk/gdkdataformatprivate.h"
#include "udmabuf.h"
#include "../reftests/reftest-compare.h"

static GskRenderer *gl_renderer = NULL;
static GskRenderer *vulkan_renderer = NULL;

static GBytes *
my_bytes_base64_decode (GBytes  *bytes,
                        GError **error)
{
  guchar *output;
  const char *input;
  gsize out_size, in_size;
  gint state = 0;
  guint save = 0;

  input = g_bytes_get_data (bytes, &in_size);

  /* We can use a smaller limit here, since we know the saved state is 0,
     +1 used to avoid calling g_malloc0(0), and hence returning NULL */
  output = g_malloc0 ((in_size / 4) * 3 + 1);

  out_size = g_base64_decode_step (input, in_size, output, &state, &save);

  return g_bytes_new_take (output, out_size);
}

static guint32
data_format_from_string (const char *string)
{
  GdkDataFormat format;

  for (format = 0; format < GDK_DATA_N_FORMATS; format++)
    {
      if (g_strcasecmp (string, gdk_data_format_get_name (format)) == 0)
        break;
    }

  return format;
}

static gboolean
parse_size (GKeyFile    *keyfile,
            const char  *name,
            gsize       *result,
            GError     **error)
{
  guint64 value;

  value = g_key_file_get_uint64 (keyfile, "GdkTexture", name, error);
  
  if (error && *error)
    return FALSE;

  if (value == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "0 is an invalid value for %s", name);
      return FALSE;
    }

  if (value > G_MAXSIZE)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "%s value %"G_GUINT64_FORMAT" too large", name, value);
      return FALSE;
    }

  *result = value;
  return TRUE;
}

static gboolean
parse_data (GKeyFile      *keyfile,
            const char    *name,
            GBytes        *bytes,
            gsize          stride,
            gsize          height,
            const guchar **result,
            GError       **error)
{
  guint64 offset;
  const guchar *data;
  gsize size;
  
  if (!g_key_file_has_key (keyfile, "GdkTexture", name, NULL))
    {
      offset = 0;
    }
  else
    {
      offset = g_key_file_get_uint64 (keyfile, "GdkTexture", name, error);
      if (error && *error)
        return FALSE;
    }

  data = g_bytes_get_data (bytes, &size);

  if (offset >= size)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "%s %"G_GUINT64_FORMAT" >= data size %zu",
                   name, offset, size);
      return FALSE;
    }
#if 0
  /* wrong for subsampled planes */
  if ((size - offset) / stride < height)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "stride %zu * height %zu does not fit for %s %"G_GUINT64_FORMAT" and data size %zu",
                   stride, height, name, offset, size);
      return FALSE;
    }
#endif

  *result = data + offset;
  return TRUE;
}

static gboolean
parse_header (GKeyFile       *keyfile,
              GBytes         *bytes,
              GdkDataBuffer  *buffer,
              GError        **error)
{
  char *format_string;
  gsize n_planes, i;

  format_string = g_key_file_get_string (keyfile, "GdkTexture", "format", error);
  if (format_string == NULL)
    return FALSE;
  buffer->format = data_format_from_string (format_string);
  if (buffer->format >= GDK_DATA_N_FORMATS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "Unknown format \"%s\"", format_string);
      g_free (format_string);
      return FALSE;
    }
  g_free (format_string);

  if (!parse_size (keyfile, "width", &buffer->width, error) ||
      !parse_size (keyfile, "height", &buffer->height, error))
    return FALSE;

  n_planes = gdk_data_format_n_planes (buffer->format);
  if (n_planes == 1)
    {
      if (!parse_size (keyfile, "stride", &buffer->planes[0].stride, error) ||
          !parse_data (keyfile, "offset", bytes, buffer->planes[0].stride, buffer->height, &buffer->planes[0].data, error))
        return FALSE;
    }
  else
    {
      static const gchar *strides[4] = { "stride0", "stride1", "stride2", "stride3" };
      static const gchar *offsets[4] = { "offset0", "offset1", "offset2", "offset3" };

      for (i = 0; i < n_planes; i++)
        {
          if (!parse_size (keyfile, strides[i], &buffer->planes[i].stride, error) ||
              !parse_data (keyfile, offsets[i], bytes, buffer->planes[i].stride, buffer->height, &buffer->planes[i].data, error))
            return FALSE;
        }
    }

  return TRUE;
}

static GdkTexture *
udmabuf_texture_new_for_data (GBytes              *bytes,
                              const GdkDataBuffer *buffer,
                              GdkColorState       *color_state,
                              GError             **error)
{
  guint32 fourcc = gdk_data_format_get_dmabuf_fourcc (buffer->format);
  const guchar *data = g_bytes_get_data (bytes, NULL);

  if (fourcc == 0)
    {
      return NULL;
    }

  return udmabuf_texture_new (buffer->width,
                              buffer->height,
                              fourcc,
                              color_state,
                              FALSE,
                              bytes,
                              gdk_data_format_n_planes (buffer->format),
                              (gsize[]) {
                                  buffer->planes[0].stride,
                                  buffer->planes[1].stride,
                                  buffer->planes[2].stride,
                                  buffer->planes[3].stride,
                              },
                              (gsize[]) {
                                  buffer->planes[0].data - data,
                                  buffer->planes[1].data - data,
                                  buffer->planes[2].data - data,
                                  buffer->planes[3].data - data,
                              },
                              error);
}

static char *
file_get_testname (GFile *file)
{
  char *basename = g_file_get_basename (file);
  char *dot = strchr (basename, '.');

  if (dot)
    *dot = '\0';

  return basename;
}

static char *
get_reference_file (GFile *file)
{
  char *path;
  GString *result;

  path = g_file_get_path (file);
  result = g_string_new (NULL);

  if (g_str_has_suffix (path, ".gdktex"))
    g_string_append_len (result, path, strlen (path) - 7);
  else
    g_string_append (result, path);
  
  g_string_append (result, ".png");

  return g_string_free (result, FALSE);
}

static GdkTexture *
load_texture (GFile   *file,
              GError **error)
{
  GBytes *file_bytes, *header_bytes, *data_bytes, *decompressed_bytes;
  const guchar *split, *file_data;
  gsize file_size, header_size;
  GZlibDecompressor *decompressor;
  GKeyFile *keyfile;
  GdkTexture *texture = NULL;
  GdkDataBuffer buffer;

  file_bytes = g_file_load_bytes (file, NULL, NULL, error);
  if (file_bytes == NULL)
    return NULL;

  file_data = g_bytes_get_data (file_bytes, &file_size);
  split = memmem (file_data, file_size, "\n\n", 2);
  if (split == NULL)
    {
      g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE,
                   "Could not find separator between header and data");
      g_bytes_unref (file_bytes);
      return NULL;
    }

  header_size = split - file_data;
  header_bytes = g_bytes_new_from_bytes (file_bytes, 0, header_size);
  data_bytes = g_bytes_new_from_bytes (file_bytes,
                                       header_size + 2,
                                       file_size - header_size - 2);

  decompressed_bytes = my_bytes_base64_decode (data_bytes, error);
  if (decompressed_bytes == NULL)
    goto out;
  g_bytes_unref (data_bytes);
  data_bytes = decompressed_bytes;

  decompressor = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
  decompressed_bytes = g_converter_convert_bytes (G_CONVERTER (decompressor), data_bytes, error);
  if (decompressed_bytes == NULL)
    goto out;
  g_bytes_unref (data_bytes);
  data_bytes = decompressed_bytes;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_bytes (keyfile, header_bytes, 0, error) ||
      !parse_header (keyfile, data_bytes, &buffer, error))
    {
      g_key_file_unref (keyfile);
      goto out;
    }
  g_key_file_unref (keyfile);

  texture = udmabuf_texture_new_for_data (data_bytes,
                                          &buffer,
                                          NULL,
                                          error);

out:
  g_bytes_unref (data_bytes);
  g_bytes_unref (header_bytes);
  g_bytes_unref (file_bytes);

  return texture;
}

static char *
get_output_file (const char *variant,
                 const char *testname,
                 const char *extension)
{
  const char *subdir = g_getenv ("TEST_OUTPUT_SUBDIR");

  return g_strdup_printf ("%s%s%s%s%s.%s.%s",
                          g_test_get_dir (G_TEST_DIST), G_DIR_SEPARATOR_S,
                          subdir ? subdir : "", subdir ? G_DIR_SEPARATOR_S : "",
                          testname, variant, extension);
}

static void
save_image (GdkTexture *texture,
            const char *variant,
            const char *test_name,
            const char *extension)
{
  char *filename = get_output_file (variant, test_name, extension);
  gboolean result;

  g_test_message ("Storing test result image at %s", filename);
  result = gdk_texture_save_to_png (texture, filename);
  g_assert_true (result);
  g_free (filename);
}

static void
test_renderer (GskRenderer *renderer,
               const char  *renderer_name,
               GFile       *file)
{
  GdkTexture *texture, *reference, *diff;
  GError *error = NULL;
  char *reference_path;

  texture = load_texture (file, &error);
  g_test_message ("Test file is %s", g_file_peek_path (file));
  if (texture == NULL)
    {
      g_test_fail_printf ("Failed to load test \"%s\": %s\n",
                          g_file_peek_path (file), error->message);
      g_clear_error (&error);
      return;
    }
  g_assert_nonnull (texture);

  if (renderer)
    {
      GskRenderNode *node;
      GdkTexture *rendered;

      node = gsk_texture_node_new (texture,
                                   &GRAPHENE_RECT_INIT(
                                     0, 0, 
                                     gdk_texture_get_width (texture),
                                     gdk_texture_get_height (texture)
                                   ));
      rendered = gsk_renderer_render_texture (renderer, node, NULL);
      gsk_render_node_unref (node);
      g_object_unref (texture);
      texture = rendered;
    }

  reference_path = get_reference_file (file);
  g_test_message ("Reference image is %s", reference_path);
  reference = gdk_texture_new_from_filename (reference_path, &error);
  if (reference == NULL)
    {
      g_test_fail_printf ("Failed to load reference \"%s\": %s\n",
                          reference_path, error->message);
      g_object_unref (texture);
      g_free (reference_path);
      g_clear_error (&error);
      return;
    }
  g_free (reference_path);

  diff = reftest_compare_textures (reference, texture);
  if (diff)
    {
      char *test_name = file_get_testname (file);

      save_image (texture, renderer_name, test_name, "out.png");
      save_image (diff, renderer_name, test_name, "diff.png");
      g_test_fail_printf ("Image differs from reference");
      g_object_unref (diff);
      g_free (test_name);
    }

  g_object_unref (reference);
  g_object_unref (texture);
}

static void
test_cpu (gconstpointer data)
{
  test_renderer (NULL, "cpu", (GFile *) data);
}

static void
test_gl (gconstpointer data)
{
  test_renderer (gl_renderer, "gl", (GFile *) data);
}

static void
test_vulkan (gconstpointer data)
{
  test_renderer (vulkan_renderer, "vulkan", (GFile *) data);
}

static char *
get_test_name (const char *test,
               GFile      *file)
{
  char *base, *result;

  base = g_file_get_basename (file);

  result = g_strdup_printf ("/dataformat/%s/%s", test, base);

  g_free (base);

  return result;
}

static void
add_tests_for_file (GFile *file)
{
  char *testname;

  testname = get_test_name ("cpu", file);
  g_test_add_vtable (testname,
                     0,
                     g_object_ref (file),
                     NULL,
                     (GTestFixtureFunc) test_cpu,
                     (GTestFixtureFunc) g_object_unref);
  g_free (testname);

  if (gl_renderer)
    {
      testname = get_test_name ("gl", file);
      g_test_add_vtable (testname,
                         0,
                         g_object_ref (file),
                         NULL,
                         (GTestFixtureFunc) test_gl,
                         (GTestFixtureFunc) g_object_unref);
      g_free (testname);
    }

  if (gl_renderer)
    {
      testname = get_test_name ("vulkan", file);
      g_test_add_vtable (testname,
                         0,
                         g_object_ref (file),
                         NULL,
                         (GTestFixtureFunc) test_vulkan,
                         (GTestFixtureFunc) g_object_unref);
      g_free (testname);
    }
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static void
add_tests_for_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".gdktex"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (dir, filename));

      g_object_unref (info);
    }
  
  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_tests_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

int
main (int argc, char **argv)
{
  GdkDisplay *display;
  int result;

  gtk_test_init (&argc, &argv);

  display = gdk_display_get_default ();

  gl_renderer = gsk_gl_renderer_new ();
  if (!gsk_renderer_realize_for_display (gl_renderer, display, NULL))
    {
      g_clear_object (&gl_renderer);
    }

  vulkan_renderer = gsk_vulkan_renderer_new ();
  if (!gsk_renderer_realize_for_display (vulkan_renderer, display, NULL))
    {
      g_clear_object (&vulkan_renderer);
    }

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      basedir = g_test_get_dir (G_TEST_DIST);
      dir = g_file_new_for_path (basedir);
      add_tests_for_files_in_directory (dir);

      g_object_unref (dir);
    }
  else
    {
      for (guint i = 1; i < argc; i++)
        {
          GFile *file;

          file = g_file_new_for_commandline_arg (argv[i]);
          add_tests_for_file (file);
          g_object_unref (file);
        }
    }

  result = g_test_run ();

  if (vulkan_renderer)
    {
      gsk_renderer_unrealize (vulkan_renderer);
      g_clear_object (&vulkan_renderer);
    }
  if (gl_renderer)
    {
      gsk_renderer_unrealize (gl_renderer);
      g_clear_object (&gl_renderer);
    }

  return result;
}

