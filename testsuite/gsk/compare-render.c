#include "config.h"
#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include "../reftests/reftest-compare.h"


static char *arg_output_dir = NULL;

extern void
replay_node (GskRenderNode *node, GtkSnapshot *snapshot);

static const char *
get_output_dir (void)
{
  static const char *output_dir = NULL;
  GError *error = NULL;
  GFile *file;

  if (output_dir)
    return output_dir;

  if (arg_output_dir)
    {
      GFile *arg_file = g_file_new_for_commandline_arg (arg_output_dir);
      const char *subdir;

      subdir = g_getenv ("TEST_OUTPUT_SUBDIR");
      if (subdir)
        {
          GFile *child = g_file_get_child (arg_file, subdir);
          g_object_unref (arg_file);
          arg_file = child;
        }

      output_dir = g_file_get_path (arg_file);
      g_object_unref (arg_file);
    }
  else
    {
      output_dir = g_get_tmp_dir ();
    }

  /* Just try to create the output directory.
   * If it already exists, that's exactly what we wanted to check,
   * so we can happily skip that error.
   */
  file = g_file_new_for_path (output_dir);
  if (!g_file_make_directory_with_parents (file, NULL, &error))
    {
      g_object_unref (file);

      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          g_error ("Failed to create output dir: %s", error->message);
          g_error_free (error);
          return NULL;
        }
      g_error_free (error);
    }
  else
    g_object_unref (file);

  return output_dir;
}

static char *
file_replace_extension (const char *old_file,
                        const char *old_ext,
                        const char *new_ext)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (old_file, old_ext))
    g_string_append_len (file, old_file, strlen (old_file) - strlen (old_ext));
  else
    g_string_append (file, old_file);

  g_string_append (file, new_ext);

  return g_string_free (file, FALSE);
}

static char *
get_output_file (const char *file,
                 const char *variant,
                 const char *orig_ext,
                 const char *new_ext)
{
  const char *dir;
  char *result, *base;
  char *name;

  dir = get_output_dir ();
  base = g_path_get_basename (file);
  if (variant)
    {
      char *s = file_replace_extension (base, orig_ext, "");
      name = g_strconcat (s, "-", variant, new_ext, NULL);
      g_free (s);
    }
  else
    {
      name = file_replace_extension (base, orig_ext, new_ext);
    }

  result = g_strconcat (dir, G_DIR_SEPARATOR_S, name, NULL);

  g_free (base);
  g_free (name);

  return result;
}

static void
save_image (GdkTexture *texture,
            const char *test_name,
            const char *variant_name,
            const char *extension)
{
  char *filename = get_output_file (test_name, variant_name, ".node", extension);
  gboolean result;

  g_print ("Storing test result image at %s\n", filename);
  result = gdk_texture_save_to_png (texture, filename);
  g_assert_true (result);
  g_free (filename);
}

static void
save_node (GskRenderNode *node,
           const char    *test_name,
           const char    *variant_name,
           const char    *extension)
{
  char *filename = get_output_file (test_name, variant_name, ".node", extension);
  gboolean result;

  g_print ("Storing modified nodes at %s\n", filename);
  result = gsk_render_node_write_to_file (node, filename, NULL);
  g_assert_true (result);
  g_free (filename);
}

static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *string = g_string_new ("<data>");

  g_string_append_printf (string, ":%zu:%zu",
                          start->lines + 1, start->line_chars + 1);
  if (start->lines != end->lines || start->line_chars != end->line_chars)
    {
      g_string_append (string, "-");
      if (start->lines != end->lines)
        g_string_append_printf (string, "%zu:", end->lines + 1);
      g_string_append_printf (string, "%zu", end->line_chars + 1);
    }

  g_warning ("Error at %s: %s", string->str, error->message);

  g_string_free (string, TRUE);
}

static GskRenderNode *
load_node_file (const char *node_file)
{
  GBytes *bytes;
  gsize len;
  char *contents;
  GError *error = NULL;
  GskRenderNode *node;

  if (!g_file_get_contents (node_file, &contents, &len, &error))
    {
      g_print ("Could not open node file: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }

  bytes = g_bytes_new_take (contents, len);
  node = gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
  g_bytes_unref (bytes);

  g_assert_no_error (error);
  g_assert_nonnull (node);

  return node;
}

static void
make_random_clip (cairo_rectangle_int_t *int_clip,
                  int                    width,
                  int                    height)
{
  int_clip->width = g_test_rand_int_range (1, width);
  int_clip->height = g_test_rand_int_range (1, height);

  int_clip->x = g_test_rand_int_range (0, width - int_clip->width);
  int_clip->y = g_test_rand_int_range (0, height - int_clip->height);
}

static void
gsk_rect_from_cairo (graphene_rect_t              *rect,
                      const cairo_rectangle_int_t *int_rect)
{
  rect->origin.x = int_rect->x;
  rect->origin.y = int_rect->y;
  rect->size.width = int_rect->width;
  rect->size.height = int_rect->height;
}

static GskRenderNode *
flip_create_test (GskRenderNode *node,
                  gconstpointer  unused)
{
  GskRenderNode *result;
  GskTransform *transform;

  transform = gsk_transform_scale (NULL, -1, 1);
  result = gsk_transform_node_new (node, transform);
  gsk_transform_unref (transform);

  return result;
}

static GdkTexture *
flip_create_reference (GskRenderer   *renderer,
                       GdkTexture    *texture,
                       gconstpointer  unused)
{
  GskRenderNode *texture_node, *transform_node;
  GdkTexture *result;
  GskTransform *transform;

  texture_node = gsk_texture_node_new (texture,
                                       &GRAPHENE_RECT_INIT (
                                         0, 0,
                                         gdk_texture_get_width (texture),
                                         gdk_texture_get_height (texture)
                                       ));

  transform = gsk_transform_scale (NULL, -1, 1);
  transform_node = gsk_transform_node_new (texture_node, transform);
  gsk_transform_unref (transform);

  result = gsk_renderer_render_texture (renderer, transform_node, NULL);

  gsk_render_node_unref (transform_node);
  gsk_render_node_unref (texture_node);

  return result;
}

static GskRenderNode *
repeat_create_test (GskRenderNode *node,
                    gconstpointer  unused)
{
  graphene_rect_t bounds, node_bounds;

  gsk_render_node_get_bounds (node, &node_bounds);

  node_bounds.size.width = ceil (node_bounds.size.width);
  node_bounds.size.height = ceil (node_bounds.size.height);

  bounds.size.width = MIN (1000, 3 * node_bounds.size.width);
  bounds.size.height = MIN (1000, 3 * node_bounds.size.height);
  bounds.origin.x = node_bounds.origin.x + floorf (node_bounds.size.width / 2);
  bounds.origin.y = node_bounds.origin.y + floorf (node_bounds.size.height / 2);

  return gsk_repeat_node_new (&bounds, node, &node_bounds);
}

static GdkTexture *
repeat_create_reference (GskRenderer   *renderer,
                         GdkTexture    *texture,
                         gconstpointer  unused)
{
  GskRenderNode *texture_nodes[16], *container_node, *reference_node;
  GdkTexture *result;
  int width, height;
  int i, j;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
        {
          texture_nodes[4 * j + i] = gsk_texture_node_new (texture,
                                                           &GRAPHENE_RECT_INIT (
                                                               i * width,
                                                               j * height,
                                                               width,
                                                               height
                                                           ));
        }
    }
  container_node = gsk_container_node_new (texture_nodes, G_N_ELEMENTS (texture_nodes));
  reference_node = gsk_clip_node_new (container_node,
                                      &GRAPHENE_RECT_INIT (
                                        width / 2,
                                        height / 2,
                                        MIN (1000, 3 * width),
                                        MIN (1000, 3 * height)
                                      ));
  result = gsk_renderer_render_texture (renderer, reference_node, NULL);

  for (i = 0; i < G_N_ELEMENTS (texture_nodes); i++)
    gsk_render_node_unref (texture_nodes[i]);
  gsk_render_node_unref (container_node);
  gsk_render_node_unref (reference_node);

  return result;
}

static GskRenderNode *
rotate_create_test (GskRenderNode *node,
                    gconstpointer  unused)
{
  GskRenderNode *result;
  GskTransform *transform;

  transform = gsk_transform_rotate (NULL, 90);
  result = gsk_transform_node_new (node, transform);
  gsk_transform_unref (transform);

  return result;
}

static GdkTexture *
rotate_create_reference (GskRenderer   *renderer,
                         GdkTexture    *texture,
                         gconstpointer  unused)
{
  GskRenderNode *texture_node, *transform_node;
  GdkTexture *result;
  GskTransform *transform;

  texture_node = gsk_texture_node_new (texture,
                                       &GRAPHENE_RECT_INIT (
                                         0, 0,
                                         gdk_texture_get_width (texture),
                                         gdk_texture_get_height (texture)
                                       ));

  transform = gsk_transform_rotate (NULL, 90);
  transform_node = gsk_transform_node_new (texture_node, transform);
  gsk_transform_unref (transform);

  result = gsk_renderer_render_texture (renderer, transform_node, NULL);

  gsk_render_node_unref (transform_node);
  gsk_render_node_unref (texture_node);

  return result;
}

static GskRenderNode *
mask_create_test (GskRenderNode *node,
                  gconstpointer  unused)
{
  GskRenderNode *result, *nodes[2], *mask_node;
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (node, &bounds);
  
  nodes[0] = gsk_color_node_new (&(GdkRGBA){ 0, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (bounds.origin.x, bounds.origin.y, 25, 25));
  if (bounds.size.width > 25 && bounds.size.height > 25)
    {
      nodes[1] = gsk_color_node_new (&(GdkRGBA){ 0, 0, 0, 1},
                                     &GRAPHENE_RECT_INIT (
                                         bounds.origin.x + 25,
                                         bounds.origin.y + 25,
                                         MIN (1000, bounds.size.width) - 25,
                                         MIN (1000, bounds.size.height) - 25));
      mask_node = gsk_container_node_new (nodes, G_N_ELEMENTS (nodes));
      gsk_render_node_unref (nodes[0]);
      gsk_render_node_unref (nodes[1]);
    }
  else
    {
      mask_node = nodes[0];
    }

  result = gsk_mask_node_new (node, mask_node, GSK_MASK_MODE_ALPHA);
  gsk_render_node_unref (mask_node);

  return result;
}

static GdkTexture *
mask_create_reference (GskRenderer   *renderer,
                       GdkTexture    *texture,
                       gconstpointer  unused)
{
  GskRenderNode *texture_node, *reference_node;
  GdkTexture *result;
  GskRenderNode *nodes[2];
  int width, height;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  texture_node = gsk_texture_node_new (texture,
                                       &GRAPHENE_RECT_INIT (0, 0, width, height));
  nodes[0] = gsk_clip_node_new (texture_node,
                                &GRAPHENE_RECT_INIT (
                                    0, 0,
                                    MIN (width, 25),
                                    MIN (height, 25)
                                ));
  if (width > 25 && height > 25)
    {
      nodes[1] = gsk_clip_node_new (texture_node,
                                    &GRAPHENE_RECT_INIT (
                                        25, 25,
                                        MIN (1000, width) - 25,
                                        MIN (1000, height) - 25
                                    ));
      reference_node = gsk_container_node_new (nodes, G_N_ELEMENTS (nodes));
      gsk_render_node_unref (nodes[0]);
      gsk_render_node_unref (nodes[1]);
    }
  else
    {
      reference_node = nodes[0];
    }
  result = gsk_renderer_render_texture (renderer, reference_node, NULL);

  gsk_render_node_unref (texture_node);
  gsk_render_node_unref (reference_node);

  return result;
}

static GskRenderNode *
replay_create_test (GskRenderNode *node,
                    gconstpointer  unused)
{
  GskRenderNode *result;
  graphene_rect_t node_bounds, result_bounds;
  GtkSnapshot *snapshot = gtk_snapshot_new ();

  replay_node (node, snapshot);
  result = gtk_snapshot_free_to_node (snapshot);
  /* If the whole render node tree got eliminated, make sure we have
     something to work with nevertheless.  */
  if (result == NULL)
    result = gsk_container_node_new (NULL, 0);

  gsk_render_node_get_bounds (node, &node_bounds);
  gsk_render_node_get_bounds (result, &result_bounds);
  /* Check that the node didn't grow.  */
  if (!graphene_rect_contains_rect (&node_bounds, &result_bounds))
    g_test_fail_printf ("Node bounds grew");

  return result;
}

static gpointer
clip_setup (GskRenderNode *node)
{
  cairo_rectangle_int_t *result;
  graphene_rect_t bounds;

  result = g_new (cairo_rectangle_int_t, 1);

  gsk_render_node_get_bounds (node, &bounds);
  if (bounds.size.width <= 1 || bounds.size.height <= 1)
    *result = (cairo_rectangle_int_t) { 0, 0, 1, 1 };
  else
    make_random_clip (result, ceil (bounds.size.width), ceil (bounds.size.height));

  g_print ("Random clip rectangle %d %d %d %d\n",
           result->x, result->y, result->width, result->height);

  return result;
}

static GskRenderNode *
clip_create_test (GskRenderNode *node,
                  gconstpointer  data)
{
  const cairo_rectangle_int_t *int_clip = data;
  graphene_rect_t clip_rect, bounds;

  gsk_rect_from_cairo (&clip_rect, int_clip);
  gsk_render_node_get_bounds (node, &bounds);
  clip_rect.origin.x += bounds.origin.x;
  clip_rect.origin.y += bounds.origin.y;
  
  return gsk_clip_node_new (node, &clip_rect);
}

static GdkTexture *
clip_create_reference (GskRenderer   *renderer,
                       GdkTexture    *texture,
                       gconstpointer  data)
{
  const cairo_rectangle_int_t *int_clip = data;
  GskRenderNode *texture_node, *reference_node;
  graphene_rect_t texture_bounds, clip_rect;
  GdkTexture *result;

  gsk_rect_from_cairo (&clip_rect, int_clip);
  texture_bounds = GRAPHENE_RECT_INIT (0,
                                       0,
                                       gdk_texture_get_width (texture),
                                       gdk_texture_get_height (texture));

  texture_node = gsk_texture_node_new (texture, &texture_bounds);
  reference_node = gsk_clip_node_new (texture_node, &clip_rect);
  result = gsk_renderer_render_texture (renderer, reference_node, &texture_bounds);

  gsk_render_node_unref (reference_node);
  gsk_render_node_unref (texture_node);
  
  return result;
}

static GskRenderNode *
colorflip_create_test (GskRenderNode *node,
                       gconstpointer  unused)
{
  graphene_matrix_t matrix;

  graphene_matrix_init_from_float (&matrix,
                                   (const float []) { 0, 1, 0, 0,
                                                      1, 0, 0, 0,
                                                      0, 0, 1, 0,
                                                      0, 0, 0, 1 });

  return gsk_color_matrix_node_new (node, &matrix, graphene_vec4_zero ());
}

static GdkTexture *
colorflip_create_reference (GskRenderer   *renderer,
                            GdkTexture    *texture,
                            gconstpointer  unused)
{
  GskRenderNode *texture_node, *reference_node;
  GdkTexture *result;
  graphene_matrix_t matrix;

  texture_node = gsk_texture_node_new (texture,
                                       &GRAPHENE_RECT_INIT (
                                         0, 0,
                                         gdk_texture_get_width (texture),
                                         gdk_texture_get_height (texture)
                                       ));

  graphene_matrix_init_from_float (&matrix,
                                   (const float []) { 0, 1, 0, 0,
                                                      1, 0, 0, 0,
                                                      0, 0, 1, 0,
                                                      0, 0, 0, 1 });
  reference_node = gsk_color_matrix_node_new (texture_node, &matrix, graphene_vec4_zero ());
  result = gsk_renderer_render_texture (renderer, reference_node, NULL);

  gsk_render_node_unref (texture_node);
  gsk_render_node_unref (reference_node);

  return result;
}

typedef enum
{
  KEEP_BOUNDS = (1 << 0),
} TestFlags;

typedef struct _TestSetup TestSetup;
struct _TestSetup
{
  const char *name;
  const char *description;
  TestFlags flags;
  gpointer        (* setup)            (GskRenderNode *node);
  void            (* free)             (gpointer       data);
  GskRenderNode * (* create_test)      (GskRenderNode *node,
                                        gconstpointer  data);
  GdkTexture *    (* create_reference) (GskRenderer   *renderer,
                                        GdkTexture    *texture,
                                        gconstpointer  data);
};

static const TestSetup test_setups[] = {
  {
    .name = "plain",
    .description = "Run test as-is",
    .create_test = NULL,
    .create_reference = NULL,
  },
  {
    .name = "flip",
    .description = "Do flipped test",
    .create_test = flip_create_test,
    .create_reference = flip_create_reference,
  },
  {
    .name = "repeat",
    .description = "Do rotated test",
    .create_test = repeat_create_test,
    .create_reference = repeat_create_reference,
  },
  {
    .name = "rotate",
    .description = "Do repeated test",
    .create_test = rotate_create_test,
    .create_reference = rotate_create_reference,
  },
  {
    .name = "mask",
    .description = "Do masked test",
    .create_test = mask_create_test,
    .create_reference = mask_create_reference,
  },
  {
    .name = "replay",
    .description = "Do replay test",
    .flags = KEEP_BOUNDS,
    .create_test = replay_create_test,
    .create_reference = NULL,
  },
  {
    .name = "clip",
    .description = "Do clip test",
    .flags = KEEP_BOUNDS,
    .setup = clip_setup,
    .free = g_free,
    .create_test = clip_create_test,
    .create_reference = clip_create_reference,
  },
  {
    .name = "colorflip",
    .description = "Swap colors",
    .create_test = colorflip_create_test,
    .create_reference = colorflip_create_reference,
  },
};

static void
run_single_test (const TestSetup *setup,
                 const char      *file_name,
                 GskRenderer     *renderer,
                 GskRenderNode   *org_test,
                 GdkTexture      *org_reference)
{
  GskRenderNode *test;
  GdkTexture *reference, *rendered, *diff;
  graphene_rect_t test_bounds, *render_bounds;
  gpointer test_data;

  if (setup->flags & KEEP_BOUNDS)
    {
      gsk_render_node_get_bounds (org_test, &test_bounds);
      render_bounds = &test_bounds;
    }
  else
    render_bounds = NULL;

  if (setup->setup)
    test_data = setup->setup (org_test);
  else
    test_data = NULL;

  if (setup->create_test)
    {
      test = setup->create_test (org_test, test_data);
      save_node (test, file_name, setup->name, ".node");
    }
  else
    test = gsk_render_node_ref (org_test);

  rendered = gsk_renderer_render_texture (renderer, test, render_bounds);
  save_image (rendered, file_name, setup->name, ".out.png");

  if (setup->create_reference)
    {
      reference = setup->create_reference (renderer, org_reference, test_data);
      save_image (reference, file_name, setup->name, ".ref.png");
    }
  else
    reference = g_object_ref (org_reference);

  if (setup->free)
    setup->free (test_data);

  diff = reftest_compare_textures (reference, rendered);
  if (diff)
    {
      save_image (diff, file_name, setup->name, ".diff.png");
      g_test_fail ();
    }

  g_clear_object (&diff);
  g_object_unref (rendered);
  g_object_unref (reference);
  gsk_render_node_unref (test);
}
                 
typedef struct _TestData TestData;

struct _TestData {
  char *node_file;
  char *png_file;
};

static void
test_data_free (TestData *test)
{
  g_free (test->node_file);
  g_free (test->png_file);

  g_free (test);
}

static gboolean test_enabled[G_N_ELEMENTS (test_setups)] = { FALSE, };

/*
 * Non-option arguments:
 *   1) .node file to compare
 *   2) .png file to compare the rendered .node file to
 */
static void
run_node_test (gconstpointer data)
{
  const TestData *test = data;
  GdkTexture *reference_texture = NULL;
  GskRenderer *renderer;
  GdkSurface *window;
  GskRenderNode *node;
  GError *error = NULL;
  gsize i;

  g_print ("Node file: '%s'\n", test->node_file);
  g_print ("PNG file: '%s'\n", test->png_file);

  window = gdk_surface_new_toplevel (gdk_display_get_default());
  renderer = gsk_renderer_new_for_surface (window);

  /* Load the render node from the given .node file */
  node = load_node_file (test->node_file);
  if (!node)
    {
      g_test_fail ();
      return;
    }

  /* Load the given reference png file */
  reference_texture = gdk_texture_new_from_filename (test->png_file, &error);
  if (reference_texture == NULL)
    {
      g_print ("Error loading reference surface: %s\n", error->message);
      g_clear_error (&error);
      g_test_fail ();
      return;
    }

  for (i = 0; i < G_N_ELEMENTS (test_setups); i++)
    {
      if (test_enabled[i])
        run_single_test (&test_setups[i], test->node_file, renderer, node, reference_texture);
    }

  g_object_unref (reference_texture);
  gsk_render_node_unref (node);
  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
  gdk_surface_destroy (window);
}

int
main (int argc, char **argv)
{
  GOptionEntry options[] = {
    { "output", 0, 0, G_OPTION_ARG_FILENAME, &arg_output_dir, "Directory to save image files to", "DIR" },
    { test_setups[0].name, 0, 0, G_OPTION_ARG_NONE, &test_enabled[0], test_setups[0].description, NULL },
    { test_setups[1].name, 0, 0, G_OPTION_ARG_NONE, &test_enabled[1], test_setups[1].description, NULL },
    { test_setups[2].name, 0, 0, G_OPTION_ARG_NONE, &test_enabled[2], test_setups[2].description, NULL },
    { test_setups[3].name, 0, 0, G_OPTION_ARG_NONE, &test_enabled[3], test_setups[3].description, NULL },
    { test_setups[4].name, 0, 0, G_OPTION_ARG_NONE, &test_enabled[4], test_setups[4].description, NULL },
    { test_setups[5].name, 0, 0, G_OPTION_ARG_NONE, &test_enabled[5], test_setups[5].description, NULL },
    { test_setups[6].name, 0, 0, G_OPTION_ARG_NONE, &test_enabled[6], test_setups[6].description, NULL },
    { test_setups[7].name, 0, 0, G_OPTION_ARG_NONE, &test_enabled[7], test_setups[7].description, NULL },
    { NULL }
  };
  GOptionContext *context;
  GError *error = NULL;
  TestData *test;
  int result;
  gsize i;

  (g_test_init) (&argc, &argv, NULL);

  context = g_option_context_new ("NODE [REF] - run GSK node tests");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return 1;
    }
  else if (argc != 3 && argc != 2)
    {
      char *help = g_option_context_get_help (context, TRUE, NULL);
      g_print ("%s", help);
      return 1;
    }

  g_option_context_free (context);

  for (i = 0; i < G_N_ELEMENTS (test_enabled); i++)
    {
      if (test_enabled[i])
        break;
    }
  if (i >= G_N_ELEMENTS (test_enabled))
    test_enabled[0] = TRUE;

  gtk_init ();

  test = g_new0 (TestData, 1);
  test->node_file = g_canonicalize_filename (argv[1], NULL);
  if (argc <= 2)
    test->png_file = file_replace_extension (test->node_file, ".node", ".png");
  else
    test->png_file = g_canonicalize_filename (argv[2], NULL);

  g_test_add_vtable (test->node_file,
                     0,
                     test,
                     NULL,
                     (GTestFixtureFunc) run_node_test,
                     (GTestFixtureFunc) test_data_free);

  result = g_test_run ();

  return result;
}
