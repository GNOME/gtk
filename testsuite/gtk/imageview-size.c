#include <gtk/gtk.h>


void
empty ()
{
  int min, nat;
  GtkWidget *iv = gtk_image_view_new ();
  gtk_widget_show (iv);


  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, 0);
  g_assert_cmpint (nat, ==, 0);


  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, 0);
  g_assert_cmpint (nat, ==, 0);
}

void
image_fit_allocation ()
{
  int min, nat;
  GtkWidget *iv;
  GdkPixbuf *pic;

  iv = gtk_image_view_new ();
  pic = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 200);
  gtk_image_view_set_pixbuf (GTK_IMAGE_VIEW (iv), pic, 1);
  gtk_image_view_set_fit_allocation (GTK_IMAGE_VIEW (iv), TRUE);

  gtk_widget_show (iv);

  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, 0);
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_width (pic));

  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, 0);
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_height (pic));
}

void
image_no_fit_allocation ()
{
  int min, nat;
  GtkWidget *iv;
  GdkPixbuf *pic;

  iv = gtk_image_view_new ();
  pic = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 200);
  gtk_image_view_set_pixbuf (GTK_IMAGE_VIEW (iv), pic, 1);

  gtk_widget_show (iv);

  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_width (pic));
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_width (pic));

  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_height (pic));
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_height (pic));
}

void
image_scaled ()
{
  int min, nat;
  GtkWidget *iv;
  GdkPixbuf *pic;

  iv = gtk_image_view_new ();
  pic = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 200);
  gtk_image_view_set_pixbuf (GTK_IMAGE_VIEW (iv), pic, 1);

  gtk_widget_show (iv);

  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_width (pic));
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_width (pic));

  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_height (pic));
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_height (pic));

  gtk_image_view_set_scale (GTK_IMAGE_VIEW (iv), 2.0);

  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_width (pic) * 2.0);
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_width (pic) * 2.0);

  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_height (pic) * 2.0);
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_height (pic) * 2.0);


  gtk_image_view_set_scale (GTK_IMAGE_VIEW (iv), 0.5);

  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_width (pic) * 0.5);
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_width (pic) * 0.5);

  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_height (pic) * 0.5);
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_height (pic) * 0.5);
}

void
image_rotated ()
{
  int min, nat;
  GtkWidget *iv;
  GdkPixbuf *pic;

  iv = gtk_image_view_new ();
  pic = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 200);
  gtk_image_view_set_pixbuf (GTK_IMAGE_VIEW (iv), pic, 1);
  gtk_image_view_set_angle (GTK_IMAGE_VIEW (iv), 90.0);

  gtk_widget_show (iv);

  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_height (pic));
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_height (pic));

  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_width (pic));
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_width (pic));
}

void
image_rotated_scaled ()
{
  int min, nat;
  GtkWidget *iv;
  GdkPixbuf *pic;

  iv = gtk_image_view_new ();
  pic = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 200);
  gtk_image_view_set_pixbuf (GTK_IMAGE_VIEW (iv), pic, 1);
  gtk_image_view_set_angle (GTK_IMAGE_VIEW (iv), 90.0);

  gtk_widget_show (iv);

  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_height (pic));
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_height (pic));

  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_width (pic));
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_width (pic));

  gtk_image_view_set_scale (GTK_IMAGE_VIEW (iv), 0.5);

  gtk_widget_get_preferred_width (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_height (pic) * 0.5);
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_height (pic) * 0.5);

  gtk_widget_get_preferred_height (iv, &min, &nat);
  g_assert_cmpint (min, ==, gdk_pixbuf_get_width (pic) * 0.5);
  g_assert_cmpint (nat, ==, gdk_pixbuf_get_width (pic) * 0.5);
}

int
main (int argc, char **argv)
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/sizing/imageview/empty", empty);
  g_test_add_func ("/sizing/imageview/image-fit-allocation", image_fit_allocation);
  g_test_add_func ("/sizing/imageview/image-no-fit-allocation", image_no_fit_allocation);
  g_test_add_func ("/sizing/imageview/image-scaled", image_scaled);
  g_test_add_func ("/sizing/imageview/image-rotated", image_rotated);
  g_test_add_func ("/sizing/imageview/image-rotated-scaled", image_rotated_scaled);


  return g_test_run ();
}
