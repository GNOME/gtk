#include <gdk/gdk.h>
#include <gdk/gdkcolorprivate.h>
#include <gdk/gdkcolorspaceprivate.h>

static void
test_roundtrip_srgb (void)
{
  GdkColorSpace *srgb = gdk_color_space_get_srgb ();
  GdkColorSpace *srgb_linear = gdk_color_space_get_srgb_linear ();
  GdkColor orig;
  GdkColor linear;
  GdkColor back;
  GdkRGBA rgba;

  for (int i = 0; i < 1000; i++)
    {
      rgba.red = g_test_rand_double_range (0., 1.);
      rgba.green = g_test_rand_double_range (0., 1.);
      rgba.blue = g_test_rand_double_range (0., 1.);
      rgba.alpha = g_test_rand_double_range (0., 1.);

      gdk_color_init_from_rgba (&orig, &rgba);

      gdk_color_convert (&linear, srgb_linear, &orig);
      gdk_color_convert (&back, srgb, &linear);

      g_assert_true (gdk_color_space_equal (gdk_color_get_color_space (&orig), gdk_color_get_color_space (&back)));
      g_assert_cmpfloat_with_epsilon (gdk_color_get_alpha (&orig), gdk_color_get_alpha (&back), 0.0001);
      g_assert_cmpfloat_with_epsilon (gdk_color_get_components (&orig)[0], gdk_color_get_components (&back)[0], 0.0001);
      g_assert_cmpfloat_with_epsilon (gdk_color_get_components (&orig)[1], gdk_color_get_components (&back)[1], 0.0001);
      g_assert_cmpfloat_with_epsilon (gdk_color_get_components (&orig)[2], gdk_color_get_components (&back)[2], 0.0001);
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/color/roundtrip-srgb", test_roundtrip_srgb);

  return g_test_run ();
}
