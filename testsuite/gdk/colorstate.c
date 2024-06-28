#include <gtk/gtk.h>
#include <gdk/gdkcolorstateprivate.h>

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

static void
test_srgb_tf (void)
{
  GdkColorState *srgb;
  GdkColorState *srgb_linear;
  GdkColorState *no_srgb;
  gboolean ret;
  guint saved_debug_flags = _gdk_debug_flags;

  _gdk_debug_flags |= GDK_DEBUG_LINEAR;

  srgb = gdk_color_state_get_srgb ();
  srgb_linear = gdk_color_state_get_srgb_linear ();

  ret = gdk_color_state_has_srgb_tf (srgb, &no_srgb);
  g_assert_true (ret);
  g_assert_true (gdk_color_state_equal (no_srgb, srgb_linear));

  ret = gdk_color_state_has_srgb_tf (srgb_linear, &no_srgb);
  g_assert_false (ret);

  _gdk_debug_flags &= ~GDK_DEBUG_LINEAR;

  ret = gdk_color_state_has_srgb_tf (srgb, &no_srgb);
  g_assert_false (ret);

  ret = gdk_color_state_has_srgb_tf (srgb_linear, &no_srgb);
  g_assert_false (ret);

  _gdk_debug_flags = saved_debug_flags;
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/colorstate/srgb", test_srgb);
  g_test_add_func ("/colorstate/srgb-tf", test_srgb_tf);

  return g_test_run ();
}
