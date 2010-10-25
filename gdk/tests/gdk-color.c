#include <gdk/gdk.h>

static void
test_color_parse (void)
{
  GdkRGBA color;
  GdkRGBA expected;
  gboolean res;

  res = gdk_rgba_parse ("foo", &color);
  g_assert (!res);

  res = gdk_rgba_parse ("", &color);
  g_assert (!res);

  expected.red = 0.4;
  expected.green = 0.3;
  expected.blue = 0.2;
  expected.alpha = 0.1;
  res = gdk_rgba_parse ("rgba(0.4,0.3,0.2,0.1)", &color);
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));

  res = gdk_rgba_parse ("rgba ( 0.4 ,  0.3  ,   0.2 ,  0.1     )", &color);
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));

  expected.red = 0.4;
  expected.green = 0.3;
  expected.blue = 0.2;
  expected.alpha = 1.0;
  res = gdk_rgba_parse ("rgb(0.4,0.3,0.2)", &color);
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));

  expected.red = 1.0;
  expected.green = 0.0;
  expected.blue = 0.0;
  expected.alpha = 1.0;
  res = gdk_rgba_parse ("red", &color);
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));

  expected.red = 0.0;
  expected.green = 0x8080 / 65535.;
  expected.blue = 1.0;
  expected.alpha = 1.0;
  res = gdk_rgba_parse ("#0080ff", &color);
  g_assert (res);
  g_assert (gdk_rgba_equal (&color, &expected));
}

int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);
        gdk_init (&argc, &argv);

        g_test_add_func ("/color/parse", test_color_parse);

        return g_test_run ();
}
