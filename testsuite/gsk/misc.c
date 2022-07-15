#include <gtk/gtk.h>
#include "gsk/gskrendernodeprivate.h"

static void
test_rendernode_gvalue (void)
{
  GValue value = G_VALUE_INIT;
  GskRenderNode *node, *node2;

  g_assert_false (GSK_VALUE_HOLDS_RENDER_NODE (&value));
  g_value_init (&value, GSK_TYPE_RENDER_NODE);
  g_assert_true (GSK_VALUE_HOLDS_RENDER_NODE (&value));

  node = gsk_value_get_render_node (&value);
  g_assert_null (node);

  node = gsk_color_node_new (&(GdkRGBA){0,1,1,1}, &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  gsk_value_set_render_node (&value, node);

  node2 = gsk_value_dup_render_node (&value);
  g_assert_true (node == node2);

  g_value_reset (&value);
  gsk_value_take_render_node (&value, node);

  g_value_unset (&value);
}

static void
test_bordernode_uniform (void)
{
  GskRenderNode *node;
  GskRoundedRect rect;
  GdkRGBA colors[4] = {
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
  };

  gsk_rounded_rect_init (&rect,
                         &GRAPHENE_RECT_INIT (0, 0, 50, 50),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 10));

  node = gsk_border_node_new (&rect, (const float[]){ 1, 1, 1, 1}, colors);

  g_assert_true (gsk_border_node_get_uniform (node));
  g_assert_true (gsk_border_node_get_uniform_color (node));

  gsk_render_node_unref (node);

  node = gsk_border_node_new (&rect, (const float[]){ 1, 2, 3, 4}, colors);

  g_assert_false (gsk_border_node_get_uniform (node));
  g_assert_true (gsk_border_node_get_uniform_color (node));

  gsk_render_node_unref (node);
}

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))

static void
test_conic_gradient_angle (void)
{
  GskRenderNode *node;
  GskColorStop stops[] = {
    { 0.f, (GdkRGBA) { 0, 0, 0, 1} },
    { 1.f, (GdkRGBA) { 1, 0, 1, 1} },
  };

  node = gsk_conic_gradient_node_new (&GRAPHENE_RECT_INIT (0, 0, 50, 50),
                                      &GRAPHENE_POINT_INIT (10, 20),
                                      33.f,
                                      stops,
                                      G_N_ELEMENTS (stops));

  g_assert_cmpfloat_with_epsilon (gsk_conic_gradient_node_get_angle (node), DEG_TO_RAD (90.f - 33.f), 0.001);

  gsk_render_node_unref (node);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  gtk_init ();

  g_test_add_func ("/rendernode/gvalue", test_rendernode_gvalue);
  g_test_add_func ("/rendernode/border/uniform", test_bordernode_uniform);
  g_test_add_func ("/rendernode/conic-gradient/angle", test_conic_gradient_angle);

  return g_test_run ();
}
