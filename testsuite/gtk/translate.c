
#include <gtk/gtk.h>

#define BORDER_WIDTH 30

static const char *css =
"button, box {"
"  all: unset; "
"}"
".with-border {"
"  border: 30px solid white;"
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
  g_assert_cmpfloat_with_epsilon (bounds.size.width,  WIDTH * 2, 1);
  g_assert_cmpfloat_with_epsilon (bounds.size.height, HEIGHT,    1);
  /*g_message ("RESULT: %f, %f, %f, %f",*/
             /*bounds.origin.x, bounds.origin.y,*/
             /*bounds.size.width, bounds.size.height);*/
}

static void
translate_with_parent (void)
{
  const int WIDTH = 200;
  const int HEIGHT = 100;
  const float x_scale = 2.0f;
  const int x_margin = 25;
  GtkWidget *parent = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *child = gtk_button_new ();
  graphene_matrix_t transform;
  int i;

  gtk_widget_set_hexpand (child, FALSE);
  gtk_widget_set_vexpand (child, FALSE);
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_widget_set_size_request (child, WIDTH, HEIGHT);
  gtk_widget_set_margin_start (child, x_margin);

  gtk_container_add (GTK_CONTAINER (parent), child);
  gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_measure (parent, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (parent, &(GtkAllocation){0, 0, WIDTH * 10, HEIGHT * 10}, -1);

  /* First we have no transformation. We take a coordinate and translate it from parent
   * to child, then back from child to parent and check if we get our original coordinate. */
  for (i = 0; i < 100; i ++)
    {
      double cx, cy;
      double px, py;

      gtk_widget_translate_coordinatesf (parent, child, i, i, &cx, &cy);

      /* Back up */
      gtk_widget_translate_coordinatesf (child, parent, cx, cy, &px, &py);

      g_assert_cmpfloat_with_epsilon (px, i, 0.1f);
      g_assert_cmpfloat_with_epsilon (py, i, 0.1f);
    }


  graphene_matrix_init_scale (&transform, x_scale, 1, 1);
  gtk_widget_set_transform (child, &transform);

  /* Same thing... */
  for (i = 1; i < 100; i ++)
    {
      double cx, cy;
      double px, py;

      gtk_widget_translate_coordinatesf (parent, child, i, i, &cx, &cy);
      /*g_message ("### %d/%d in child coords: %f/%f", i, i, cx, cy);*/
      /*g_assert_cmpfloat_with_epsilon (cx, (-x_margin+i) / x_scale, 0.1f);*/
      /*g_assert_cmpfloat_with_epsilon (cy, i, 0.1f);*/

      /* Back up */
      gtk_widget_translate_coordinatesf (child, parent, cx, cy, &px, &py);
      /*g_message ("%f, %f", px, py);*/
      /*g_message ("%f/%f in parent coords: %f/%f", cx, cy, px, py);*/

      g_assert_cmpfloat_with_epsilon (px, i, 0.1f);
      g_assert_cmpfloat_with_epsilon (py, i, 0.1f);
    }


  /* Now try a translation... */
  gtk_widget_set_margin_start (child, 0);
  gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_measure (parent, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (parent, &(GtkAllocation){0, 0, WIDTH * 10, HEIGHT * 10}, -1);
  graphene_matrix_init_translate (&transform,
                                  &(graphene_point3d_t){20, 0, 0});
  gtk_widget_set_transform (child, &transform);

  {
    double dx, dy;

    gtk_widget_translate_coordinatesf (parent, child, 0, 0, &dx, &dy);

    g_assert_cmpfloat_with_epsilon (dx, -20, 0.1);
    g_assert_cmpfloat_with_epsilon (dy, 0, 0.1);
  }


}

static void
translate_with_css (void)
{
  const int WIDTH = 200;
  const int HEIGHT = 100;
  GtkWidget *parent = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *child = gtk_button_new ();
  graphene_matrix_t transform;

  gtk_style_context_add_class (gtk_widget_get_style_context (child), "with-border");

  gtk_widget_set_hexpand (child, FALSE);
  gtk_widget_set_vexpand (child, FALSE);
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_widget_set_size_request (child, WIDTH, HEIGHT);
  /*gtk_widget_set_margin_start (child, x_margin);*/

  gtk_container_add (GTK_CONTAINER (parent), child);
  gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_measure (parent, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (parent, &(GtkAllocation){0, 0, WIDTH * 10, HEIGHT * 10}, -1);

  /* Basic checks without a transformation */
  {
    double dx, dy;

    gtk_widget_translate_coordinatesf (child, parent, 0, 0, &dx, &dy);
    g_assert_cmpfloat_with_epsilon (dx, BORDER_WIDTH, 0.1);
    g_assert_cmpfloat_with_epsilon (dy, BORDER_WIDTH, 0.1);

    gtk_widget_translate_coordinatesf (parent, child, 0, 0, &dx, &dy);
    g_assert_cmpfloat_with_epsilon (dx, - BORDER_WIDTH, 0.1);
    g_assert_cmpfloat_with_epsilon (dy, - BORDER_WIDTH, 0.1);
  }

  graphene_matrix_init_scale (&transform, 2, 2, 1);
  gtk_widget_set_transform (child, &transform);

  /* Since the border is also scaled, the values should be double from above. */
  {
    double px, py;
    double cx, cy;

    /*g_message (">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");*/
    gtk_widget_translate_coordinatesf (child, parent, 0, 0, &px, &py);
    /*g_message ("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");*/
    g_assert_cmpfloat_with_epsilon (px, BORDER_WIDTH * 2, 0.1);
    g_assert_cmpfloat_with_epsilon (py, BORDER_WIDTH * 2, 0.1);

    gtk_widget_translate_coordinatesf (parent, child, px, py, &cx, &cy);
    g_assert_cmpfloat_with_epsilon (cx, 0, 0.1);
    g_assert_cmpfloat_with_epsilon (cy, 0, 0.1);
  }

}

static void
pick (void)
{
  const int WIDTH = 200;
  const int HEIGHT = 100;
  GtkWidget *parent = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *child = gtk_button_new ();
  graphene_matrix_t transform;

  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);
  gtk_widget_set_halign (child, GTK_ALIGN_FILL);
  gtk_widget_set_valign (child, GTK_ALIGN_FILL);

  gtk_container_add (GTK_CONTAINER (parent), child);
  gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_measure (parent, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
  gtk_widget_size_allocate (parent, &(GtkAllocation){0, 0, WIDTH, HEIGHT}, -1);

  g_assert (gtk_widget_get_width  (child) == WIDTH);
  g_assert (gtk_widget_get_height (child) == HEIGHT);

  /* We scale the child widget to only half its size on the x axis,
   * which means doing a pick on the left half of the parent should
   * return the child but a pick on the right half should return the
   * parent. */
  graphene_matrix_init_scale (&transform, 0.5, 1, 1);
  gtk_widget_set_transform (child, &transform);

  /*g_assert (gtk_widget_pick (parent, WIDTH * 0.25, HEIGHT / 2) == child);*/
  /*g_assert (gtk_widget_pick (parent, WIDTH * 0.75, HEIGHT / 2) == parent);*/

  /* Now we test translations by simply offsetting the child widget by its own size,
   * which will move it to the left and entirely out of the parent's allocation. */
  graphene_matrix_init_translate (&transform,
                                  &(graphene_point3d_t){ - WIDTH, 0, 0 });
  gtk_widget_set_transform (child, &transform);

  /* ... which means that picking on the parent with any positive x coordinate will
   * yield the parent widget, while negative x coordinates (up until -WIDTH) will
   * yield the child */
  g_assert (gtk_widget_pick (parent, WIDTH * 0.1, 0) == parent);
  g_assert (gtk_widget_pick (parent, WIDTH * 0.9, 0) == parent);

  /*double dx, dy;*/
  /*gtk_widget_translate_coordinatesf (parent, child, - WIDTH * 0.1, 0, &dx, &dy);*/
  /*g_message ("translate: %f, %f", dx, dy);*/


  g_assert (gtk_widget_pick (parent, -WIDTH * 0.1, 0) == child);
  g_assert (gtk_widget_pick (parent, -WIDTH * 0.9, 0) == child);
}


#if 0
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
#endif

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
  g_test_add_func ("/translate/translate-with-parent", translate_with_parent);
  g_test_add_func ("/translate/translate-with-css", translate_with_css);
  g_test_add_func ("/translate/pick", pick);

  return g_test_run ();
}
