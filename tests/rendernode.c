#include <gtk/gtk.h>

static gboolean benchmark = FALSE;
static gboolean dump_variant = FALSE;
static gboolean fallback = FALSE;
static int runs = 1;

static GOptionEntry options[] = {
  { "benchmark", 'b', 0, G_OPTION_ARG_NONE, &benchmark, "Time operations", NULL },
  { "dump-variant", 'd', 0, G_OPTION_ARG_NONE, &dump_variant, "Dump GVariant structure", NULL },
  { "fallback", '\0', 0, G_OPTION_ARG_NONE, &fallback, "Draw node without a renderer", NULL },
  { "runs", 'r', 0, G_OPTION_ARG_INT, &runs, "Render the test N times", "N" },
  { NULL }
};

int
main(int argc, char **argv)
{
  cairo_surface_t *surface;
  GskRenderNode *node;
  GError *error = NULL;
  GBytes *bytes;
  gint64 start, end;
  char *contents;
  gsize len;
  int run;
  GOptionContext *context;

  context = g_option_context_new ("NODE-FILE PNG-FILE");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  gtk_init ();

  if (runs < 1)
    {
      g_printerr ("Number of runs given with -r/--runs must be at least 1 and not %d.\n", runs);
      return 1;
    }
  if (!(argc == 3 || (argc == 2 && (dump_variant || benchmark))))
    {
      g_printerr ("Usage: %s [OPTIONS] NODE-FILE PNG-FILE\n", argv[0]);
      return 1;
    }

  if (!g_file_get_contents (argv[1], &contents, &len, &error))
    {
      g_printerr ("Could not open node file: %s\n", error->message);
      return 1;
    }

  bytes = g_bytes_new_take (contents, len);
  if (dump_variant)
    {
      GVariant *variant = g_variant_new_from_bytes (G_VARIANT_TYPE ("(suuv)"), bytes, FALSE);
      char *s;

      s = g_variant_print (variant, FALSE);
      g_print ("%s\n", s);
      g_free (s);
      g_variant_unref (variant);
    }

  start = g_get_monotonic_time ();
  node = gsk_render_node_deserialize (bytes, &error);
  end = g_get_monotonic_time ();
  if (benchmark)
    {
      char *bytes_string = g_format_size (g_bytes_get_size (bytes));
      g_print ("Loaded %s in %.4gs\n", bytes_string, (double) (end - start) / G_USEC_PER_SEC);
      g_free (bytes_string);
    }
  g_bytes_unref (bytes);

  if (node == NULL)
    {
      g_printerr ("Invalid node file: %s\n", error->message);
      g_clear_error (&error);
      return 1;
    }

  if (fallback)
    {
      graphene_rect_t bounds;
      cairo_t *cr;

      gsk_render_node_get_bounds (node, &bounds);
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil (bounds.size.width), ceil (bounds.size.height));
      cr = cairo_create (surface);

      cairo_translate (cr, - bounds.origin.x, - bounds.origin.y);
      for (run = 0; run < runs; run++)
        {
          if (run > 0)
            {
              cairo_save (cr);
              cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
              cairo_paint (cr);
              cairo_restore (cr);
            }
          start = g_get_monotonic_time ();
          gsk_render_node_draw (node, cr);
          end = g_get_monotonic_time ();
          if (benchmark)
            g_print ("Run %d: Rendered fallback in %.4gs\n", run, (double) (end - start) / G_USEC_PER_SEC);
        }

      cairo_destroy (cr);
    }
  else
    {
      GskRenderer *renderer;
      GdkWindow *window;
      GskTexture *texture = NULL;

      window = gdk_window_new_toplevel (gdk_display_get_default(), 0, 10 , 10);
      renderer = gsk_renderer_new_for_window (window);

      for (run = 0; run < runs; run++)
        {
          if (run > 0)
            g_object_unref (texture);
          start = g_get_monotonic_time ();
          texture = gsk_renderer_render_texture (renderer, node, NULL);
          end = g_get_monotonic_time ();
          if (benchmark)
            g_print ("Run %u: Rendered using %s in %.4gs\n", run, G_OBJECT_TYPE_NAME (renderer), (double) (end - start) / G_USEC_PER_SEC);
        }

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            gsk_texture_get_width (texture),
                                            gsk_texture_get_height (texture));
      gsk_texture_download (texture,
                            cairo_image_surface_get_data (surface),
                            cairo_image_surface_get_stride (surface));
      cairo_surface_mark_dirty (surface);
      g_object_unref (texture);
      g_object_unref (window);
      g_object_unref (renderer);
    }

  gsk_render_node_unref (node);

  if (argc > 2)
    {
      cairo_status_t status;
      
      status = cairo_surface_write_to_png (surface, argv[2]);

      if (status != CAIRO_STATUS_SUCCESS)
        {
          cairo_surface_destroy (surface);
          g_print ("Failed to save PNG file: %s\n", cairo_status_to_string (status));
          return 1;
        }
    }

  cairo_surface_destroy (surface);

  return 0;
}
