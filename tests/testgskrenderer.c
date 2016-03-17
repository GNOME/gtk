#include "config.h"

#include <graphene.h>
#include <cairo.h>
#include <gsk/gsk.h>
#include <gtk/gtk.h>

#define BOX_SIZE        50.f
#define PADDING         10.f
#define ROOT_SIZE       BOX_SIZE * 2 + PADDING * 2

static void
create_color_surface (cairo_t *cr, GdkRGBA *color, int w, int h)
{
  cairo_set_source_rgba (cr, color->red, color->green, color->blue, color->alpha);
  cairo_rectangle (cr, 0, 0, w, h);
  cairo_fill (cr);
}

static GskRenderer *
get_renderer (GtkWidget *widget)
{
  GskRenderer *res;

  res = g_object_get_data (G_OBJECT (widget), "-gsk-renderer");
  if (res == NULL)
    {
      res = gsk_renderer_get_for_display (gtk_widget_get_display (widget));

      g_object_set_data_full (G_OBJECT (widget), "-gsk-renderer",
                              res,
                              (GDestroyNotify) g_object_unref);
    }

  return res;
}

static void
create_scene (GskRenderer *renderer)
{
  GskRenderNode *root, *node;
  graphene_matrix_t ctm;
  cairo_t *cr;

  root = gsk_render_node_new ();
  gsk_render_node_set_name (root, "Root node");
  gsk_render_node_set_bounds (root, &(graphene_rect_t) {
                                .origin.x = 0.f,
                                .origin.y = 0.f,
                                .size.width = ROOT_SIZE,
                                .size.height = ROOT_SIZE
                              });
  cr = gsk_render_node_get_draw_context (root);
  create_color_surface (cr, &(GdkRGBA) { .red = 1, .green = 0, .blue = 0, .alpha = 1 }, ROOT_SIZE, ROOT_SIZE);
  cairo_destroy (cr);
  gsk_renderer_set_root_node (renderer, root);
  g_object_set_data (G_OBJECT (renderer), "-gsk-renderer-root-node", root);

  g_object_unref (root);

  node = gsk_render_node_new ();
  gsk_render_node_set_name (node, "Green node");
  gsk_render_node_set_bounds (node, &(graphene_rect_t) {
                                .origin.x = 0.f,
                                .origin.y = 0.f,
                                .size.width = BOX_SIZE,
                                .size.height = BOX_SIZE
                              });
  cr = gsk_render_node_get_draw_context (node);
  create_color_surface (cr, &(GdkRGBA) { .red = 0, .green = 1, .blue = 0, .alpha = 1 }, BOX_SIZE, BOX_SIZE);
  cairo_destroy (cr);
  graphene_matrix_init_translate (&ctm, &(graphene_point3d_t) { .x = -0.5, .y = -0.5, .z = 0.f });
  gsk_render_node_set_transform (node, &ctm);
  gsk_render_node_insert_child_at_pos (root, node, 0);
  g_object_unref (node);

  node = gsk_render_node_new ();
  gsk_render_node_set_name (node, "Blue node");
  gsk_render_node_set_bounds (node, &(graphene_rect_t) {
                                .origin.x = 0.f,
                                .origin.y = 0.f,
                                .size.width = BOX_SIZE,
                                .size.height = BOX_SIZE
                              });
  cr = gsk_render_node_get_draw_context (node);
  create_color_surface (cr, &(GdkRGBA) { .red = 0, .green = 0, .blue = 1, .alpha = 1 }, BOX_SIZE, BOX_SIZE);
  cairo_destroy (cr);
  graphene_matrix_init_translate (&ctm, &(graphene_point3d_t) { .x = 0.5, .y = 0.5, .z = 0.f });
  gsk_render_node_set_transform (node, &ctm);
  gsk_render_node_insert_child_at_pos (root, node, 1);
  g_object_unref (node);
}

static void
realize (GtkWidget *widget)
{
  GskRenderer *renderer = get_renderer (widget);

  gsk_renderer_set_window (renderer, gtk_widget_get_window (widget));
  gsk_renderer_set_use_alpha (renderer, TRUE);
  gsk_renderer_realize (renderer);

  create_scene (renderer);
}

static void
unrealize (GtkWidget *widget)
{
  g_object_set_data (G_OBJECT (widget), "-gsk-renderer", NULL);
}

static void
size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GskRenderer *renderer = get_renderer (widget);
  GskRenderNode *root;
  graphene_matrix_t ctm;

  gsk_renderer_set_viewport (renderer, &(graphene_rect_t) {
                               .origin.x = 0,
                               .origin.y = 0,
                               .size.width = allocation->width,
                               .size.height = allocation->height
                             });

  graphene_matrix_init_translate (&ctm, &(graphene_point3d_t) {
                                    allocation->x,
                                    allocation->y,
                                    0.f
                                  });
  gsk_renderer_set_modelview (renderer, &ctm);

  root = g_object_get_data (G_OBJECT (renderer), "-gsk-renderer-root-node");
  if (root == NULL)
    {
      create_scene (renderer);
      root = g_object_get_data (G_OBJECT (renderer), "-gsk-renderer-root-node");
    }

  graphene_matrix_init_translate (&ctm, &(graphene_point3d_t) {
                                    .x = 0,
                                    .y = 0,
                                    .z = 0
                                  });
  gsk_render_node_set_transform (root, &ctm);
}

static gboolean
draw (GtkWidget *widget, cairo_t *cr)
{
  GskRenderer *renderer = get_renderer (widget);

  gsk_renderer_set_draw_context (renderer, cr);
  gsk_renderer_render (renderer);

  return TRUE;
}

static gboolean
fade_out (GtkWidget *widget,
          GdkFrameClock *frame_clock,
          gpointer data)
{
  static gint64 first_frame_time;
  static gboolean flip = FALSE;
  gint64 now = gdk_frame_clock_get_frame_time (frame_clock);

  if (first_frame_time == 0)
    {
      first_frame_time = now;

      return G_SOURCE_CONTINUE;
    }

  double start = first_frame_time;
  double end = first_frame_time + (double) 1000000;
  double progress = (now - first_frame_time) / (end - start);

  if (flip)
    progress = 1 - progress;

  if (progress < 0 || progress >= 1)
    {
      first_frame_time = now;
      flip = !flip;
      return G_SOURCE_CONTINUE;
    }

  GskRenderer *renderer = get_renderer (widget);
  GskRenderNode *root = gsk_renderer_get_root_node (renderer);

  gsk_render_node_set_opacity (root, 1.0 - progress);

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}


int
main (int argc, char *argv[])
{
  GtkWidget *window, *area;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_window_set_title (GTK_WINDOW (window), "GSK Renderer");
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  area = gtk_drawing_area_new ();
  gtk_widget_set_hexpand (area, TRUE);
  gtk_widget_set_vexpand (area, TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (area), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (area), TRUE);
  gtk_container_add (GTK_CONTAINER (window), area);

  g_signal_connect (area, "realize", G_CALLBACK (realize), NULL);
  g_signal_connect (area, "unrealize", G_CALLBACK (unrealize), NULL);
  g_signal_connect (area, "size-allocate", G_CALLBACK (size_allocate), NULL);
  g_signal_connect (area, "draw", G_CALLBACK (draw), NULL);

  gtk_widget_add_tick_callback (area, fade_out, NULL, NULL);

  gtk_widget_show_all (window);

  gtk_main ();
}
