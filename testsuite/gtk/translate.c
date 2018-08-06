
#include <gtk/gtk.h>

static const char *css =
"button {"
"  all: unset; "
"}"
".with-border {"
"  border: 10px solid white;"
"}"
;

static void
same_widget (void)
{
  GtkWidget *a = gtk_button_new ();
  int i;

  for (i = -1000; i < 1000; i ++)
    {
      int rx, ry;

      gtk_widget_translate_coordinates (a, a, i, i, &rx, &ry);

      g_assert_cmpint (rx, ==, i);
      g_assert_cmpint (ry, ==, i);
    }
}


static void
compute_bounds (void)
{
  const int WIDTH = 200;
  const int HEIGHT = 100;
  GtkWidget *a = gtk_button_new ();
  graphene_matrix_t transform;
  graphene_rect_t bounds;

  graphene_matrix_init_scale (&transform, 2, 1, 1);

  gtk_widget_measure (a, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (a, &(GtkAllocation){0, 0, WIDTH, HEIGHT}, -1);

  gtk_widget_compute_bounds (a, a, &bounds);
  g_assert_cmpfloat (bounds.origin.x, ==, 0);
  g_assert_cmpfloat (bounds.origin.y, ==, 0);
  g_assert_cmpfloat_with_epsilon (bounds.size.width, WIDTH, 1);
  g_assert_cmpfloat_with_epsilon (bounds.size.height, HEIGHT, 1);

  gtk_widget_set_transform (a, &transform);
  gtk_widget_compute_bounds (a, a, &bounds);

  g_assert_cmpfloat_with_epsilon (bounds.size.width, WIDTH, 1);
  g_assert_cmpfloat_with_epsilon (bounds.size.height, HEIGHT, 1);

}

static void
compute_bounds_with_parent (void)
{
  const int WIDTH = 200;
  const int HEIGHT = 100;
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *a = gtk_button_new ();
  graphene_matrix_t transform;
  graphene_rect_t bounds;

  gtk_widget_set_hexpand (a, FALSE);
  gtk_widget_set_vexpand (a, FALSE);
  gtk_widget_set_halign (a, GTK_ALIGN_START);
  gtk_widget_set_valign (a, GTK_ALIGN_START);
  gtk_widget_set_size_request (a, WIDTH, HEIGHT);
  gtk_widget_set_margin_start (a, 25);


  gtk_container_add (GTK_CONTAINER (box), a);
  gtk_widget_measure (a, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (box, &(GtkAllocation){0, 0, WIDTH * 10, HEIGHT * 10}, -1);

  gtk_widget_compute_bounds (a, box, &bounds);
  g_assert_cmpfloat_with_epsilon (bounds.origin.x, 25, 1);
  g_assert_cmpfloat_with_epsilon (bounds.origin.y, 0, 1);
  g_assert_cmpfloat_with_epsilon (bounds.size.width, WIDTH, 1);
  g_assert_cmpfloat_with_epsilon (bounds.size.height, HEIGHT, 1);


  /* Now set a transform and check that the bounds returned by compute_bounds
   * have the proper values */

  graphene_matrix_init_scale (&transform, 2, 1, 1);
  gtk_widget_set_transform (a, &transform);


  gtk_widget_compute_bounds (a, box, &bounds);
  /* FIXME: Positions here are borked */
  /*g_assert_cmpfloat_with_epsilon (bounds.origin.x, 25, 1);*/
  /*g_assert_cmpfloat_with_epsilon (bounds.origin.y, 0, 1);*/
  g_assert_cmpfloat_with_epsilon (bounds.size.width, WIDTH * 2, 1);
  g_assert_cmpfloat_with_epsilon (bounds.size.height, HEIGHT, 1);
  /*g_message ("RESULT: %f, %f, %f, %f",*/
             /*bounds.origin.x, bounds.origin.y,*/
             /*bounds.size.width, bounds.size.height);*/
}

static void
compute_bounds_css (void)
{
}


static void
single_widget_scale (void)
{
  GtkWidget *p = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *w = gtk_button_new ();
  graphene_matrix_t transform;
  GtkWidget *picked;
  int x, y;

  gtk_container_add (GTK_CONTAINER (p), w);

  gtk_widget_set_hexpand (w, TRUE);
  gtk_widget_set_vexpand (w, TRUE);

  graphene_matrix_init_scale (&transform, 0.5f, 0.5f, 1);
  gtk_widget_set_transform (w, &transform);

  /* Just to shut up the GtkWidget warning... */
  gtk_widget_measure (p, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (p, &(GtkAllocation) {0, 0, 100, 100}, -1);

  gtk_widget_translate_coordinates (p, w, 0, 0, &x, &y);
  g_assert_cmpint (x, ==, 0);
  g_assert_cmpint (y, ==, 0);

  gtk_widget_translate_coordinates (p, w, 10, 10, &x, &y);
  g_assert_cmpint (x, ==, 10 / 2);
  g_assert_cmpint (y, ==, 10 / 2);


  gtk_widget_translate_coordinates (p, w, 100, 100, &x, &y);
  g_assert_cmpint (x, ==, 100 / 2);
  g_assert_cmpint (y, ==, 100 / 2);

  picked = gtk_widget_pick (p, 0, 0);
  g_assert (picked == w);

  picked = gtk_widget_pick (p, 51, 51);
  g_assert (picked == p);
}

static void
single_widget_rotate (void)
{
  GtkWidget *p = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *w = gtk_button_new ();
  graphene_matrix_t transform;
  GtkWidget *picked;
  int x, y;

  gtk_container_add (GTK_CONTAINER (p), w);

  gtk_widget_set_hexpand (w, TRUE);
  gtk_widget_set_vexpand (w, TRUE);

  graphene_matrix_init_rotate (&transform,
                               45.0, /* Deg */
                               graphene_vec3_z_axis ());
  gtk_widget_set_transform (w, &transform);

  /* Just to shut up the GtkWidget warning... */
  gtk_widget_measure (p, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (p, &(GtkAllocation) {0, 0, 100, 100}, -1);

  gtk_widget_translate_coordinates (p, w, 0, 0, &x, &y);
  g_assert_cmpint (x, ==, 0);
  g_assert_cmpint (y, ==, 0);


  picked = gtk_widget_pick (p, 0, 0);
  g_assert (picked == w);

  picked = gtk_widget_pick (p, 0, 100);
  g_assert (picked == w);

  /* Now it gets interesting... */

  /* This should return the button parent since the button is rotated away from the
   * y axis on top */
  picked = gtk_widget_pick (p, 20, 0);
  g_assert (picked == p);

  picked = gtk_widget_pick (p, 50, 10);
  g_assert (picked == p);

  picked = gtk_widget_pick (p, 100, 100);
  g_assert (picked == p);
}


static void
single_widget_scale_css (void)
{
  GtkWidget *p = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *w = gtk_button_new ();
  graphene_matrix_t transform;
  GtkWidget *picked;
  int x, y;

  gtk_style_context_add_class (gtk_widget_get_style_context (w), "with-border");

  gtk_container_add (GTK_CONTAINER (p), w);

  gtk_widget_set_hexpand (w, TRUE);
  gtk_widget_set_vexpand (w, TRUE);

  graphene_matrix_init_scale (&transform, 2, 2, 1);
  gtk_widget_set_transform (w, &transform);

  /* Just to shut up the GtkWidget warning... */
  gtk_widget_measure (p, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (p, &(GtkAllocation) {0, 0, 100, 100}, -1);

  /* This is the interesting part. Scaling by a factor of 2 should also
   * incrase the border size by that factor, and since the border is
   * part of the input region... */
  /*picked = gtk_widget_pick (p, 200, 20);*/
  picked = gtk_widget_pick (p, 199, 20);
  g_message ("%p", picked);
  g_assert (picked == w);
}

int
main (int argc, char **argv)
{
  GtkCssProvider *provider;

  gtk_init ();

  // TODO: Do this only conditionally and/or per-testcase.
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/translate/same-widget", same_widget);
  g_test_add_func ("/translate/compute-bounds", compute_bounds);
  g_test_add_func ("/translate/compute-bounds-with-parent", compute_bounds_with_parent);
  /*g_test_add_func ("/translate/single-widget-scale", single_widget_scale);*/
  /*g_test_add_func ("/translate/single-widget-scale-css", single_widget_scale_css);*/
  /*g_test_add_func ("/translate/single-widget-rotate", single_widget_rotate);*/

  return g_test_run ();
}
