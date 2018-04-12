#include <gtk/gtk.h>
#include "benchmark.h"

/* Command line options */
const char *profile_benchmark_name = NULL;
static GOptionEntry options[] = {
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile_benchmark_name, "Benchmark name to profile using callgrind", NULL },
  { NULL }
};


static void
borders_benchmark (Benchmark *b,
                   gsize      size,
                   gpointer   user_data)
{
  GskRenderer *renderer = user_data;
  GskRenderNode *root_node;
  GskRenderNode **child_nodes;
  guint i;

  child_nodes = g_malloc (sizeof (GskRenderNode *) * size);
  for (i = 0; i < size; i ++)
    {
      GskRoundedRect outline;

      gsk_rounded_rect_init (&outline,
                             &GRAPHENE_RECT_INIT (0, 0, 1920, 1080),
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4});

      child_nodes[i] = gsk_border_node_new (&outline,
                                            (float[4]){2, 2, 2 ,2},   /* Widths */
                                            (GdkRGBA[4]) { /* Colors */
                                              {1, 0, 0, 1},
                                              {1, 0, 0, 1},
                                              {1, 0, 0, 1},
                                              {1, 0, 0, 1},
                                            });
    }

  root_node = gsk_container_node_new (child_nodes, size);

  for (i = 0; i < size; i ++)
    gsk_render_node_unref (child_nodes[i]);

  benchmark_start (b);
  gsk_renderer_render_texture (renderer, root_node, NULL);
  benchmark_stop (b);

  g_free (child_nodes);
  gsk_render_node_unref (root_node);
}

static void
outset_shadows_unblurred_benchmark (Benchmark *b,
                                    gsize      size,
                                    gpointer   user_data)
{
  GskRenderer *renderer = user_data;
  GskRenderNode *root_node;
  GskRenderNode **child_nodes;
  GdkTexture *texture;
  guint i;

  child_nodes = g_malloc (sizeof (GskRenderNode *) * size);
  for (i = 0; i < size; i ++)
    {
      GskRoundedRect outline;

      gsk_rounded_rect_init (&outline,
                             &GRAPHENE_RECT_INIT (0, 0, 1920, 1080),
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4});

      child_nodes[i] = gsk_outset_shadow_node_new (&outline,
                                                   &(GdkRGBA){0, 0, 0, 1},
                                                   0, 0, /* Offset */
                                                   10, /* Spread */
                                                   0); /* Blur */
    }


  root_node = gsk_container_node_new (child_nodes, size);

  for (i = 0; i < size; i ++)
    gsk_render_node_unref (child_nodes[i]);

  benchmark_start (b);
  texture = gsk_renderer_render_texture (renderer, root_node, NULL);
  g_object_unref (texture);
  benchmark_stop (b);

  g_free (child_nodes);
  gsk_render_node_unref (root_node);
}

static void
outset_shadows_blurred_benchmark (Benchmark *b,
                                  gsize      size,
                                  gpointer   user_data)
{
  GskRenderer *renderer = user_data;
  GskRenderNode *root_node;
  GskRenderNode **child_nodes;
  GdkTexture *texture;
  guint i;

  child_nodes = g_malloc (sizeof (GskRenderNode *) * size);
  for (i = 0; i < size; i ++)
    {
      GskRoundedRect outline;

      gsk_rounded_rect_init (&outline,
                             &GRAPHENE_RECT_INIT (0, 0, 1920, 1080),
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4},
                             &(graphene_size_t){4, 4});

      child_nodes[i] = gsk_outset_shadow_node_new (&outline,
                                                   &(GdkRGBA){0, 0, 0, 1},
                                                   0, 0, /* Offset */
                                                   10, /* Spread */
                                                   10); /* Blur */
    }

  root_node = gsk_container_node_new (child_nodes, size);
  for (i = 0; i < size; i ++)
    gsk_render_node_unref (child_nodes[i]);


  benchmark_start (b);
  texture = gsk_renderer_render_texture (renderer, root_node, NULL);
  g_object_unref (texture);
  benchmark_stop (b);

  g_free (child_nodes);
  gsk_render_node_unref (root_node);
}

static void
linear_gradient_benchmark (Benchmark *b,
                           gsize      size,
                           gpointer   user_data)
{
  GskRenderer *renderer = user_data;
  GskRenderNode *root_node;
  GskRenderNode **child_nodes;
  GdkTexture *texture;
  guint i;

  child_nodes = g_malloc (sizeof (GskRenderNode *) * size);
  for (i = 0; i < size; i ++)
    {
      if (i % 2 == 0)
        child_nodes[i] = gsk_linear_gradient_node_new (&GRAPHENE_RECT_INIT (0, 0, 1920, 1080),
                                                       &(graphene_point_t){0, 0},
                                                       &(graphene_point_t){0, 20},
                                                       (GskColorStop[3]) {
                                                         {0.0, (GdkRGBA){1, 0, 0, 1}},
                                                         {0.5, (GdkRGBA){0, 1, 0, 1}},
                                                         {1.0, (GdkRGBA){0, 0, 1, 1}},
                                                       }, 3);
      else
        child_nodes[i] = gsk_linear_gradient_node_new (&GRAPHENE_RECT_INIT (0, 0, 1920, 1080),
                                                       &(graphene_point_t){0, 0},
                                                       &(graphene_point_t){20, 20},
                                                       (GskColorStop[3]) {
                                                         {0.0, (GdkRGBA){1, 0, 0, 1}},
                                                         {0.5, (GdkRGBA){0, 1, 1, 1}},
                                                         {1.0, (GdkRGBA){1, 0, 1, 1}},
                                                       }, 3);

    }

  root_node = gsk_container_node_new (child_nodes, size);
  for (i = 0; i < size; i ++)
    gsk_render_node_unref (child_nodes[i]);


  benchmark_start (b);
  texture = gsk_renderer_render_texture (renderer, root_node, NULL);
  g_object_unref (texture);
  benchmark_stop (b);

  g_free (child_nodes);
  gsk_render_node_unref (root_node);
}

int
main (int argc, char **argv)
{
  BenchmarkSuite suite;
  GOptionContext *option_context;
  GError *error = NULL;
  static const struct {
    const char *type_name;
    const char *renderer_name;
  } renderers[] = {
    {"GskCairoRenderer",  "cairo"},
    {"GskGLRenderer",     "opengl"},
#ifdef GDK_RENDERING_VULKAN
    {"GskVulkanRenderer", "vulkan"},
#endif
  };
  guint i;

  option_context = g_option_context_new ("");
  g_option_context_add_main_entries (option_context, options, NULL);
  if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  benchmark_suite_init (&suite, profile_benchmark_name);
  gtk_init ();

  for (i = 0; i < G_N_ELEMENTS (renderers); i ++)
    {
      GskRenderer *renderer;
      GdkSurface *surface;
      gsize s = 0;

      g_setenv ("GSK_RENDERER", renderers[i].renderer_name, TRUE);

      surface = gdk_surface_new_toplevel (gdk_display_get_default (), 10, 10);
      renderer = gsk_renderer_new_for_surface (surface);

      if (strcmp (g_type_name (G_OBJECT_TYPE (renderer)), renderers[i].type_name) != 0)
        {
          g_message ("%s != %s, skipping...",
                     g_type_name (G_OBJECT_TYPE (renderer)),
                     renderers[i].type_name);
          continue;
        }

      for (s = 2; s < 256; s *= 2)
        {
          /* All the benchmarks for this renderer type */
          benchmark_suite_add (&suite,
                               g_strdup_printf ("%s borders", renderers[i].renderer_name),
                               s,
                               borders_benchmark,
                               renderer);
          benchmark_suite_add (&suite,
                               g_strdup_printf ("%s outset shadows unblurred", renderers[i].renderer_name),
                               s,
                               outset_shadows_unblurred_benchmark,
                               renderer);
          benchmark_suite_add (&suite,
                               g_strdup_printf ("%s outset shadows blurred", renderers[i].renderer_name),
                               s,
                               outset_shadows_blurred_benchmark,
                               renderer);
          benchmark_suite_add (&suite,
                               g_strdup_printf ("%s linear gradient", renderers[i].renderer_name),
                               s,
                               linear_gradient_benchmark,
                               renderer);
        }

      /* XXX We can't do this here of course since the renderers have to live until
       * benchmark_suite_run is done. */
      /*g_object_unref (renderer);*/
      /*g_object_unref (surface);*/
    }

  return benchmark_suite_run (&suite);
}
