#include <locale.h>
#include <gdk/gdk.h>

static void
test_color_parse (void)
{
  GdkRGBA color;
  GdkRGBA expected;
  gboolean res;

  res = gdk_rgba_parse (&color, "foo");
  g_assert (!res);

  res = gdk_rgba_parse (&color, "");
  g_assert (!res);

  expected.red = 100/255.;
  expected.green = 90/255.;
  expected.blue = 80/255.;
  expected.alpha = 0.1;
  res = gdk_rgba_parse (&color, "rgba(100,90,80,0.1)");
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));

  expected.red = 0.4;
  expected.green = 0.3;
  expected.blue = 0.2;
  expected.alpha = 0.1;
  res = gdk_rgba_parse (&color, "rgba(40%,30%,20%,0.1)");
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));

  res = gdk_rgba_parse (&color, "rgba(  40 % ,  30 %  ,   20 % ,  0.1    )");
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));

  expected.red = 1.0;
  expected.green = 0.0;
  expected.blue = 0.0;
  expected.alpha = 1.0;
  res = gdk_rgba_parse (&color, "red");
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));

  expected.red = 0.0;
  expected.green = 0x8080 / 65535.;
  expected.blue = 1.0;
  expected.alpha = 1.0;
  res = gdk_rgba_parse (&color, "#0080ff");
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));
}

static void
test_color_to_string (void)
{
  GdkRGBA rgba;
  GdkRGBA out;
  gchar *res;
  gchar *res_de;
  gchar *res_en;
  gchar *orig;

  /* Using /255. values for the r, g, b components should
   * make sure they round-trip exactly without rounding
   * from the double => integer => double conversions */
  rgba.red = 1.0;
  rgba.green = 128/255.;
  rgba.blue = 64/255.;
  rgba.alpha = 0.5;

  orig = g_strdup (setlocale (LC_ALL, NULL));
  res = gdk_rgba_to_string (&rgba);
  gdk_rgba_parse (&out, res);
  g_assert (gdk_rgba_equal (&rgba, &out));

  setlocale (LC_ALL, "de_DE.utf-8");
  res_de = gdk_rgba_to_string (&rgba);
  g_assert_cmpstr (res, ==, res_de);

  setlocale (LC_ALL, "en_US.utf-8");
  res_en = gdk_rgba_to_string (&rgba);
  g_assert_cmpstr (res, ==, res_en);

  g_free (res);
  g_free (res_de);
  g_free (res_en);

  setlocale (LC_ALL, orig);
  g_free (orig);
}

int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/color/parse", test_color_parse);
        g_test_add_func ("/color/to-string", test_color_to_string);

        return g_test_run ();
}
