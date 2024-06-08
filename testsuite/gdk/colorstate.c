#include <gdk/gdk.h>

static void
test_srgb (void)
{
  GdkColorState *srgb;
  GdkColorState *srgb_linear;

  srgb = gdk_color_state_get_srgb ();
  srgb_linear = gdk_color_state_get_srgb_linear ();

  g_assert_true (gdk_color_state_equal (srgb, srgb));
  g_assert_true (gdk_color_state_equal (srgb_linear, srgb_linear));
  g_assert_false (gdk_color_state_equal (srgb, srgb_linear));
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/colorstate/srgb", test_srgb);

  return g_test_run ();
}
