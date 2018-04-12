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
                             &GRAPHENE_RECT_INIT (20 * size, 0, 20, 20),
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

  benchmark_start (b);
  gsk_renderer_render_texture (renderer, root_node, NULL);
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

      /* All the benchmarks for this renderer type */
      benchmark_suite_add (&suite,
                           g_strdup_printf ("%s borders", renderers[i].renderer_name),
                           10000,
                           borders_benchmark,
                           renderer);

      /* XXX We can't do this here of course since the renderers have to live until
       * benchmark_suite_run is done. */
      /*g_object_unref (renderer);*/
      /*g_object_unref (surface);*/
    }

  return benchmark_suite_run (&suite);
}
