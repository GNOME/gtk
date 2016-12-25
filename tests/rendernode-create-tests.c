#include <gtk/gtk.h>

#include <math.h>

static void
hsv_to_rgb (GdkRGBA *rgba,
            gdouble  h,
            gdouble  s,
            gdouble  v)
{
  gdouble hue, saturation, value;
  gdouble f, p, q, t;

  rgba->alpha = 1.0;

  if ( s == 0.0)
    {
      rgba->red = v;
      rgba->green = v;
      rgba->blue = v; /* heh */
    }
  else
    {
      hue = h * 6.0;
      saturation = s;
      value = v;

      if (hue == 6.0)
        hue = 0.0;

      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - saturation * f);
      t = value * (1.0 - saturation * (1.0 - f));

      switch ((int) hue)
        {
        case 0:
          rgba->red = value;
          rgba->green = t;
          rgba->blue = p;
          break;

        case 1:
          rgba->red = q;
          rgba->green = value;
          rgba->blue = p;
          break;

        case 2:
          rgba->red = p;
          rgba->green = value;
          rgba->blue = t;
          break;

        case 3:
          rgba->red = p;
          rgba->green = q;
          rgba->blue = value;
          break;

        case 4:
          rgba->red = t;
          rgba->green = p;
          rgba->blue = value;
          break;

        case 5:
          rgba->red = value;
          rgba->green = p;
          rgba->blue = q;
          break;

        default:
          g_assert_not_reached ();
        }
    }
}

GskRenderNode *
rounded_borders (guint n)
{
  GskRenderNode *nodes[n];
  GskRenderNode *container;
  GskRoundedRect outline;
  float widths[4];
  GdkRGBA colors[4];
  guint i;

  for (i = 0; i < n; i++)
    {
      outline.bounds.size.width = g_random_int_range (20, 1000);
      outline.bounds.origin.x = g_random_int_range (0, 1000 - outline.bounds.size.width);
      outline.bounds.size.height = g_random_int_range (20, 1000);
      outline.bounds.origin.y = g_random_int_range (0, 1000 - outline.bounds.size.height);
      outline.corner[0].width = outline.corner[0].height = 10 - (int) sqrt (g_random_int_range (0, 100));
      outline.corner[1].width = outline.corner[1].height = 10 - (int) sqrt (g_random_int_range (0, 100));
      outline.corner[2].width = outline.corner[2].height = 10 - (int) sqrt (g_random_int_range (0, 100));
      outline.corner[3].width = outline.corner[3].height = 10 - (int) sqrt (g_random_int_range (0, 100));
      widths[0] = widths[1] = widths[2] = widths[3] = g_random_int_range (0, 5);
      hsv_to_rgb (&colors[0], g_random_double (), 1.0, 1.0);
      colors[3] = colors[2] = colors[1] = colors[0];
      nodes[i] = gsk_border_node_new (&outline, widths, colors);
    }

  container = gsk_container_node_new (nodes, n);

  for (i = 0; i < n; i++)
    gsk_render_node_unref (nodes[i]);

  return container;
}

GskRenderNode *
rounded_backgrounds (guint n)
{
  GskRenderNode *nodes[n];
  GskRenderNode *container, *texture;
  GskRoundedRect outline;
  GdkRGBA color;
  guint i;

  for (i = 0; i < n; i++)
    {
      outline.bounds.size.width = g_random_int_range (20, 100);
      outline.bounds.origin.x = g_random_int_range (0, 1000 - outline.bounds.size.width);
      outline.bounds.size.height = g_random_int_range (20, 100);
      outline.bounds.origin.y = g_random_int_range (0, 1000 - outline.bounds.size.height);
      outline.corner[0].width = outline.corner[0].height = 10 - (int) sqrt (g_random_int_range (0, 100));
      outline.corner[1].width = outline.corner[1].height = 10 - (int) sqrt (g_random_int_range (0, 100));
      outline.corner[2].width = outline.corner[2].height = 10 - (int) sqrt (g_random_int_range (0, 100));
      outline.corner[3].width = outline.corner[3].height = 10 - (int) sqrt (g_random_int_range (0, 100));
      hsv_to_rgb (&color, g_random_double (), g_random_double_range (0.15, 0.4), g_random_double_range (0.6, 0.85));
      color.alpha = g_random_double_range (0.5, 0.75);
      texture = gsk_color_node_new (&color, &outline.bounds);
      nodes[i] = gsk_rounded_clip_node_new (texture, &outline);
    }

  container = gsk_container_node_new (nodes, n);

  for (i = 0; i < n; i++)
    gsk_render_node_unref (nodes[i]);

  return container;
}

GskRenderNode *
colors (guint n)
{
  GskRenderNode *nodes[10 * n];
  GskRenderNode *container;
  graphene_rect_t bounds;
  GdkRGBA color;
  guint i;

  for (i = 0; i < 10 * n; i++)
    {
      bounds.size.width = g_random_int_range (20, 100);
      bounds.origin.x = g_random_int_range (0, 1000 - bounds.size.width);
      bounds.size.height = g_random_int_range (20, 100);
      bounds.origin.y = g_random_int_range (0, 1000 - bounds.size.height);
      hsv_to_rgb (&color, g_random_double (), g_random_double_range (0.15, 0.4), g_random_double_range (0.6, 0.85));
      color.alpha = g_random_double_range (0.5, 0.75);
      nodes[i] = gsk_color_node_new (&color, &bounds);
    }

  container = gsk_container_node_new (nodes, 10 * n);

  for (i = 0; i < 10 * n; i++)
    gsk_render_node_unref (nodes[i]);

  return container;
}

GskRenderNode *
clipped_colors (guint n)
{
  GskRenderNode *nodes[n];
  GskRenderNode *container;
  graphene_rect_t bounds;
  GdkRGBA color;
  guint i;

  for (i = 0; i < n; i++)
    {
      bounds.size.width = g_random_int_range (20, 100);
      bounds.origin.x = g_random_int_range (0, 1000 - bounds.size.width);
      bounds.size.height = g_random_int_range (20, 100);
      bounds.origin.y = g_random_int_range (0, 1000 - bounds.size.height);
      hsv_to_rgb (&color, g_random_double (), g_random_double_range (0.15, 0.4), g_random_double_range (0.6, 0.85));
      color.alpha = g_random_double_range (0.5, 0.75);
      nodes[i] = gsk_color_node_new (&color, &bounds);
    }

  container = gsk_container_node_new (nodes, n);

  for (i = 0; i < n; i++)
    gsk_render_node_unref (nodes[i]);

#define GRID_SIZE 4
  for (i = 0; i < GRID_SIZE * GRID_SIZE; i++)
    {
      guint x = i % GRID_SIZE;
      guint y = i / GRID_SIZE;

      if ((x + y) % 2)
        continue;

      nodes[i / 2] = gsk_clip_node_new (container,
                                        &GRAPHENE_RECT_INIT(
                                            x * 1000 / GRID_SIZE, y * 1000 / GRID_SIZE,
                                            1000 / GRID_SIZE, 1000 / GRID_SIZE
                                        ));
    }

  gsk_render_node_unref (container);

  container = gsk_container_node_new (nodes, GRID_SIZE * GRID_SIZE / 2);

  for (i = 0; i < GRID_SIZE * GRID_SIZE / 2; i++)
    gsk_render_node_unref (nodes[i]);

  return container;
}

int
main (int argc, char **argv)
{
  static const struct {
    const char *name;
    GskRenderNode * (* func) (guint n);
  } functions[] = {
    { "colors.node", colors },
    { "clipped-colors.node", clipped_colors },
    { "rounded-borders.node", rounded_borders },
    { "rounded-backgrounds.node", rounded_backgrounds },
  };
  GError *error = NULL;
  GskRenderNode *node;
  guint i, n;

  gtk_init (&argc, &argv);

  if (argc > 1)
    n = atoi (argv[1]);
  else
    n = 100000;

  for (i = 0; i < G_N_ELEMENTS (functions); i++)
    {
      node = functions[i].func (n);
      if (!gsk_render_node_write_to_file (node, functions[i].name, &error))
        {
          g_print ("Error writing \"%s\": %s\n", functions[i].name, error->message);
          g_clear_error (&error);
          return 1;
        }
      gsk_render_node_unref (node);
      g_print ("Created test file \"%s\".\n", functions[i].name);
    }

  return 0;
}
