#include <gtk/gtk.h>

static GOptionEntry options[] = {
  { NULL }
};

int
main(int argc, char **argv)
{
  graphene_rect_t bounds;
  cairo_surface_t *surface;
  GskRenderNode *node;
  cairo_t *cr;
  GError *error = NULL;
  GBytes *bytes;
  char *contents;
  gsize len;

  if (!gtk_init_with_args (&argc, &argv, "NODE-FILE PNG-FILE",
                           options, NULL, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  if (argc != 3)
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
  node = gsk_render_node_deserialize (bytes);
  g_bytes_unref (bytes);

  if (node == NULL)
    {
      g_printerr ("Invalid node file.\n");
      return 1;
    }

  gsk_render_node_get_bounds (node, &bounds);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil (bounds.size.width), ceil (bounds.size.height));
  cr = cairo_create (surface);

  cairo_translate (cr, - bounds.origin.x, - bounds.origin.y);
  gsk_render_node_draw (node, cr);

  cairo_destroy (cr);
  gsk_render_node_unref (node);

  if (cairo_surface_write_to_png (surface, argv[2]))
    {
      cairo_surface_destroy (surface);
      g_print ("Failed to save PNG file.\n");
      return 1;
    }

  cairo_surface_destroy (surface);

  return 0;
}
