#include <gdk/gdk.h>
#include <gdk/gdkcolorprivate.h>

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

  g_assert_true (gdk_color_state_equal (cs, cs));

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

  g_assert_true (gdk_color_state_equal (cs, cs));

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
test_icc_roundtrip_rec709 (void)
{
  GdkColorState *cs;

  cs = get_rec709 ();
  test_icc_roundtrip (cs);
  g_object_unref (cs);
}

static gboolean
gdk_color_near (const GdkColor *color1,
                const GdkColor *color2,
                float           epsilon)
{
  return gdk_color_state_equal (color1->color_state, color2->color_state) &&
         G_APPROX_VALUE (color1->values[0], color2->values[0], epsilon) &&
         G_APPROX_VALUE (color1->values[1], color2->values[1], epsilon) &&
         G_APPROX_VALUE (color1->values[2], color2->values[2], epsilon) &&
         G_APPROX_VALUE (color1->values[3], color2->values[3], epsilon);
}

static void
test_conversions (void)
{
  GdkColorState *cs[] = {
    GDK_COLOR_STATE_SRGB,
    GDK_COLOR_STATE_SRGB_LINEAR,
    GDK_COLOR_STATE_HSL,
    GDK_COLOR_STATE_HWB,
    GDK_COLOR_STATE_OKLAB,
    GDK_COLOR_STATE_OKLCH,
    GDK_COLOR_STATE_XYZ,
    GDK_COLOR_STATE_DISPLAY_P3,
    GDK_COLOR_STATE_REC2020,
    GDK_COLOR_STATE_REC2100_PQ,
    GDK_COLOR_STATE_REC2100_LINEAR,
  };

  for (int k = 0; k < 100; k++)
    {
      int c;
      float values[4];
      GdkColor color;

      values[0] = g_test_rand_double_range (0, 1);
      values[1] = g_test_rand_double_range (0, 1);
      values[2] = g_test_rand_double_range (0, 1);
      values[3] = g_test_rand_double_range (0, 1);

      gdk_color_init (&color, cs[0], values);

      for (int i = 0; i < 100; i++)
       {
         GdkColor color2;
         GdkColor color3;

         c = g_test_rand_int_range (0, G_N_ELEMENTS (cs));

         gdk_color_convert (&color2, cs[c], &color);
         gdk_color_convert (&color3, cs[0], &color2);

         g_assert_true (gdk_color_near (&color, &color3, 0.001));
       }
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/colorstate/srgb", test_srgb);
  g_test_add_func ("/colorstate/srgb-linear", test_srgb_linear);
  g_test_add_func ("/colorstate/icc-roundtrip-rec709", test_icc_roundtrip_rec709);
  g_test_add_func ("/colorstate/conversions", test_conversions);

  return g_test_run ();
}
