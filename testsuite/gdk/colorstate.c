#include <gdk/gdk.h>

static GdkColorState *
get_rec709 (void)
{
  const char *file;
  char *data;
  gsize size;
  GError *error = NULL;
  GBytes *bytes;
  GdkColorState *cs;

  file = g_test_get_filename (G_TEST_DIST, "Rec709.icc", NULL);

  if (!g_file_get_contents (file, &data, &size, &error))
    {
      g_error ("%s", error->message);
      return NULL;
    }

  bytes = g_bytes_new_take (data, size);

  cs = gdk_color_state_new_from_icc_profile (bytes, &error);
  if (!cs)
    g_error ("%s", error->message);

  g_bytes_unref (bytes);

  return cs;
}

static void
test_srgb (void)
{
  GdkColorState *cs;
  GdkColorState *cs2;

  cs = gdk_color_state_get_srgb ();

  g_assert_false (gdk_color_state_is_linear (cs));

  cs2 = get_rec709 ();
  g_assert_false (gdk_color_state_equal (cs, cs2));
  g_object_unref (cs2);
}

static void
test_srgb_linear (void)
{
  GdkColorState *cs;
  GdkColorState *cs2;

  cs = gdk_color_state_get_srgb_linear ();

  g_assert_true (gdk_color_state_is_linear (cs));

  cs2 = get_rec709 ();
  g_assert_false (gdk_color_state_equal (cs, cs2));
  g_object_unref (cs2);
}

static void
test_icc_roundtrip (GdkColorState *cs)
{
  GdkColorState *cs2;
  GBytes *icc_data;
  GError *error = NULL;


  icc_data = gdk_color_state_save_to_icc_profile (cs, &error);
  g_assert_no_error (error);

  cs2 = gdk_color_state_new_from_icc_profile (icc_data, &error);
  g_assert_no_error (error);

  g_assert_true (gdk_color_state_equal (cs, cs2));

  g_bytes_unref (icc_data);

  g_object_unref (cs2);
}

static void
test_icc_roundtrip_srgb (void)
{
  test_icc_roundtrip (gdk_color_state_get_srgb ());
}

static void
test_icc_roundtrip_srgb_linear (void)
{
  test_icc_roundtrip (gdk_color_state_get_srgb_linear ());
}

static void
test_icc_roundtrip_rec709 (void)
{
  GdkColorState *cs;

  cs = get_rec709 ();
  test_icc_roundtrip (cs);
  g_object_unref (cs);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/colorstate/srgb", test_srgb);
  g_test_add_func ("/colorstate/srgb-linear", test_srgb_linear);
  g_test_add_func ("/colorstate/icc-roundtrip-srgb", test_icc_roundtrip_srgb);
  g_test_add_func ("/colorstate/icc-roundtrip-srgb-linear", test_icc_roundtrip_srgb_linear);
  g_test_add_func ("/colorstate/icc-roundtrip-rec709", test_icc_roundtrip_rec709);

  return g_test_run ();
}
