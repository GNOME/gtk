#include <gtk/gtk.h>

static void
test_rectangle_equal (void)
{
  GdkRectangle a = { 0, 0, 1, 1 };
  GdkRectangle b = { 1, 1, 2, 2 };
  GdkRectangle c = { 0, 0, 2, 2 };
  GdkRectangle d = { 0, 0, 1, 1 };
  GdkRectangle e = { 0, 0, 0, 0 };
  GdkRectangle f = { 1, 1, 0, 0 };

  g_assert_true (!gdk_rectangle_equal (&a, &b));
  g_assert_true (!gdk_rectangle_equal (&a, &c));
  g_assert_true (!gdk_rectangle_equal (&b, &c));
  g_assert_true ( gdk_rectangle_equal (&a, &d));
  g_assert_true (!gdk_rectangle_equal (&e, &f));
}

static void
test_rectangle_intersect (void)
{
  GdkRectangle a = { 0, 0, 10, 10 };
  GdkRectangle b = { 5, 5, 10, 10 };
  GdkRectangle c = { 0, 0, 0, 0 };
  GdkRectangle d = { 5, 5, 5, 5 };
  GdkRectangle e = { 0, 0, 10, 10 };
  GdkRectangle f = { 20, 20, 10, 10 };
  GdkRectangle g = { 0, 0, 0, 0 };
  GdkRectangle h = { 10, 10, 0, 0 };
  gboolean res;

  res = gdk_rectangle_intersect (&a, &b, &c);
  g_assert_true (res);
  g_assert_true (gdk_rectangle_equal (&c, &d));

  /* non-empty, non-intersecting rectangles */
  res = gdk_rectangle_intersect (&e, &f, &f);
  g_assert_cmpint (f.width, ==, 0);
  g_assert_cmpint (f.height, ==, 0);

  /* empty rectangles */
  res = gdk_rectangle_intersect (&g, &h, NULL);
  g_assert_true (!res);
}

static void
test_rectangle_union (void)
{
  GdkRectangle a = { 0, 0, 10, 10 };
  GdkRectangle b = { 5, 5, 10, 10 };
  GdkRectangle c = { 0, 0, 0, 0 };
  GdkRectangle d = { 0, 0, 15, 15 };
  GdkRectangle e = { 0, 0, 0, 0 };
  GdkRectangle f = { 50, 50, 0, 0 };
  GdkRectangle g = { 0, 0, 50, 50 };

  gdk_rectangle_union (&a, &b, &c);
  g_assert_true (gdk_rectangle_equal (&c, &d));

  gdk_rectangle_union (&a, &b, &b);
  g_assert_true (gdk_rectangle_equal (&b, &d));

  gdk_rectangle_union (&e, &f, &f);
  g_assert_true (gdk_rectangle_equal (&f, &g));
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/rectangle/equal", test_rectangle_equal);
  g_test_add_func ("/rectangle/intersect", test_rectangle_intersect);
  g_test_add_func ("/rectangle/union", test_rectangle_union);

  return g_test_run ();
}
