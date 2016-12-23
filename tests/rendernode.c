#include <gtk/gtk.h>

static gboolean benchmark = FALSE;
static gboolean dump_variant = FALSE;
static gboolean fallback = FALSE;

static GOptionEntry options[] = {
  { "benchmark", 'b', 0, G_OPTION_ARG_NONE, &benchmark, "Time operations", NULL },
  { "dump-variant", 'd', 0, G_OPTION_ARG_NONE, &dump_variant, "Dump GVariant structure", NULL },
  { "fallback", '\0', 0, G_OPTION_ARG_NONE, &fallback, "Draw node without a renderer", NULL },
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

  if (!gtk_init_with_args (&argc, &argv, "NODE-FILE PNG-FILE",
                           options, NULL, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  if (argc != 3 && (argc != 2 && dump_variant))
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

      g_variant_print (variant, FALSE);
      g_variant_unref (variant);
    }

  start = g_get_monotonic_time ();
  node = gsk_render_node_deserialize (bytes);
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
      g_printerr ("Invalid node file.\n");
      return 1;
    }

  if (argc > 2)
    {
      if (fallback)
        {
          graphene_rect_t bounds;
          cairo_t *cr;

          gsk_render_node_get_bounds (node, &bounds);
          surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil (bounds.size.width), ceil (bounds.size.height));
          cr = cairo_create (surface);

          cairo_translate (cr, - bounds.origin.x, - bounds.origin.y);
          gsk_render_node_draw (node, cr);

          cairo_destroy (cr);
        }
      else
        {
          GskRenderer *renderer;
          GdkWindow *window;
          GskTexture *texture;

          window = gdk_window_new_toplevel (gdk_display_get_default(), 0, 10 , 10);
          renderer = gsk_renderer_new_for_window (window);

          start = g_get_monotonic_time ();
          texture = gsk_renderer_render_texture (renderer, node, NULL);
          end = g_get_monotonic_time ();
          if (benchmark)
            g_print ("Rendered using %s in %.4gs\n", G_OBJECT_TYPE_NAME (renderer), (double) (end - start) / G_USEC_PER_SEC);
          surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                gsk_texture_get_width (texture),
                                                gsk_texture_get_height (texture));
          gsk_texture_download (texture,
                                cairo_image_surface_get_data (surface),
                                cairo_image_surface_get_stride (surface));
          cairo_surface_mark_dirty (surface);
          gsk_texture_unref (texture);
          g_object_unref (window);
          g_object_unref (renderer);
        }

      gsk_render_node_unref (node);

      if (cairo_surface_write_to_png (surface, argv[2]))
        {
          cairo_surface_destroy (surface);
          g_print ("Failed to save PNG file.\n");
          return 1;
        }

      cairo_surface_destroy (surface);
    }

  return 0;
}
