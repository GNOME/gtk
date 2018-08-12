

#include <gtk/gtk.h>

static const char *css =
"button {"
"  all: unset; "
"  background-color: white;"
"  border: 20px solid black;"
"  padding: 20px;"
"  margin: 40px;"
"}"
"button:hover {"
"  background-color: blue;"
"  border-color: red;"
"}"
"image {"
"  background-color: teal;"
"}"
;

/* Just so we can avoid a signal */
GtkWidget *transform_tester;
GtkWidget *test_widget;
GtkWidget *test_child;
float scale = 1;
graphene_matrix_t global_transform;

static const GdkRGBA RED   = {1, 0, 0, 0.4};
static const GdkRGBA GREEN = {0, 1, 0, 0.4};
static const GdkRGBA BLUE  = {0, 0, 1, 0.4};
static const GdkRGBA BLACK = {0, 0, 0,   1};



/* ######################################################################### */
/* ############################## MatrixChooser ############################ */
/* ######################################################################### */


#define GTK_TYPE_MATRIX_CHOOSER (gtk_matrix_chooser_get_type ())
G_DECLARE_FINAL_TYPE (GtkMatrixChooser, gtk_matrix_chooser, GTK, MATRIX_CHOOSER, GtkWidget)

struct _GtkMatrixChooser
{
  GtkWidget parent_instance;
};

G_DEFINE_TYPE (GtkMatrixChooser, gtk_matrix_chooser, GTK_TYPE_WIDGET)

static void
gtk_matrix_chooser_init (GtkMatrixChooser *self)
{
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);
}

static void
gtk_matrix_chooser_class_init (GtkMatrixChooserClass *klass)
{

}


/* ######################################################################### */
/* ############################# TransformTester ########################### */
/* ######################################################################### */

#define TEST_WIDGET_MIN_SIZE 100

#define GTK_TYPE_TRANSFORM_TESTER (gtk_transform_tester_get_type ())
G_DECLARE_FINAL_TYPE (GtkTransformTester, gtk_transform_tester, GTK, TRANSFORM_TESTER, GtkWidget);

struct _GtkTransformTester
{
  GtkWidget parent_instance;

  GtkWidget *test_widget;
  int pick_increase;
};

G_DEFINE_TYPE (GtkTransformTester, gtk_transform_tester, GTK_TYPE_WIDGET);

static void
gtk_transform_tester_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkTransformTester *self = (GtkTransformTester *)widget;

  if (self->test_widget)
    {
      gtk_widget_measure (self->test_widget, orientation, for_size,
                          minimum, natural, NULL, NULL);
    }
}

static void
gtk_transform_tester_size_allocate (GtkWidget           *widget,
                                    const GtkAllocation *allocation,
                                    int                  baseline)
{
  GtkTransformTester *self = (GtkTransformTester *)widget;

  gtk_widget_size_allocate (self->test_widget, allocation, -1);
}

static void
gtk_transform_tester_snapshot (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  GtkTransformTester *self = (GtkTransformTester *)widget;
  const int width = gtk_widget_get_width (widget);
  const int height = gtk_widget_get_height (widget);
  const int inc = self->pick_increase;
  graphene_rect_t child_bounds;
  graphene_rect_t self_bounds;
  int x, y;

  GTK_WIDGET_CLASS (gtk_transform_tester_parent_class)->snapshot (widget, snapshot);

  gtk_widget_compute_bounds (self->test_widget, widget, &child_bounds);
  gtk_widget_compute_bounds (self->test_widget, self->test_widget, &self_bounds);

  {
    const struct {
      graphene_point_t coords;
      GdkRGBA color;
    } points[4] = {
      { self_bounds.origin, {1, 0, 0, 1} },
      { GRAPHENE_POINT_INIT (self_bounds.origin.x + self_bounds.size.width, self_bounds.origin.y), {0, 1, 0, 1} },
      { GRAPHENE_POINT_INIT (self_bounds.origin.x + self_bounds.size.width, self_bounds.origin.y + self_bounds.size.height), {0, 0, 1, 1} },
      { GRAPHENE_POINT_INIT (self_bounds.origin.x, self_bounds.origin.y + self_bounds.size.height), {1, 0, 1, 1} }
    };

    for (x = 0; x < G_N_ELEMENTS (points); x ++)
      {
        double px, py;

        gtk_widget_translate_coordinatesf (self->test_widget, widget,
                                          points[x].coords.x, points[x].coords.y,
                                          &px, &py);

        gtk_snapshot_append_color (snapshot, &points[x].color,
                                   &GRAPHENE_RECT_INIT (px, py,
                                                        4,
                                                        4));
      }
  }

  /* Now add custom drawing */
  for (x = 0; x < width; x += inc)
    {
      for (y = 0; y < height; y += inc)
        {
          const float px = x;
          const float py = y;
          GtkWidget *picked;
#if 1
          picked = gtk_widget_pick (widget, px, py);
#else
          {
            double dx, dy;
            gtk_widget_translate_coordinatesf (widget, self->test_widget, px, py, &dx, &dy);
            picked = gtk_widget_pick (self->test_widget, dx, dy);
          }
#endif

          if (picked == self->test_widget)
            gtk_snapshot_append_color (snapshot, &GREEN,
                                       &GRAPHENE_RECT_INIT (px - (inc / 2), py - (inc / 2), inc, inc));
          else if (picked == test_child)
            gtk_snapshot_append_color (snapshot, &BLUE,
                                       &GRAPHENE_RECT_INIT (px - (inc / 2), py - (inc / 2), inc, inc));

          else
            gtk_snapshot_append_color (snapshot, &RED,
                                       &GRAPHENE_RECT_INIT (px - (inc / 2), py - (inc / 2), inc, inc));
        }
    }



  gtk_snapshot_append_color (snapshot, &BLACK,
                             &GRAPHENE_RECT_INIT (child_bounds.origin.x,
                                                  child_bounds.origin.y,
                                                  child_bounds.size.width,
                                                  1));

  gtk_snapshot_append_color (snapshot, &BLACK,
                             &GRAPHENE_RECT_INIT (child_bounds.origin.x + child_bounds.size.width,
                                                  child_bounds.origin.y,
                                                  1,
                                                  child_bounds.size.height));

  gtk_snapshot_append_color (snapshot, &BLACK,
                             &GRAPHENE_RECT_INIT (child_bounds.origin.x,
                                                  child_bounds.origin.y + child_bounds.size.height,
                                                  child_bounds.size.width,
                                                  1));

  gtk_snapshot_append_color (snapshot, &BLACK,
                             &GRAPHENE_RECT_INIT (child_bounds.origin.x,
                                                  child_bounds.origin.y,
                                                  1,
                                                  child_bounds.size.height));
}

static void
gtk_transform_tester_init (GtkTransformTester *self)
{
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);

  self->pick_increase = 4;
}

static void
gtk_transform_tester_class_init (GtkTransformTesterClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  widget_class->measure = gtk_transform_tester_measure;
  widget_class->size_allocate = gtk_transform_tester_size_allocate;
  widget_class->snapshot = gtk_transform_tester_snapshot;
}

static void
gtk_transform_tester_set_test_widget (GtkTransformTester *self,
                                      GtkWidget          *test_widget)
{
  g_assert (!self->test_widget);

  self->test_widget = test_widget;
  gtk_widget_set_parent (test_widget, (GtkWidget *)self);
}


static gboolean
transform_func (gpointer user_data)
{
  GtkAllocation alloc;

  scale += 2.5f;
  gtk_widget_get_allocation (test_widget, &alloc);

  graphene_matrix_init_identity (&global_transform);
  graphene_matrix_translate (&global_transform,
                             &(graphene_point3d_t){
                              /*- alloc.width,*/
                              /*0,*/
                                 - alloc.width / 2,
                                 - alloc.height / 2,
                                 0}
                            );

  graphene_matrix_rotate (&global_transform, scale,
                          graphene_vec3_z_axis ());

  graphene_matrix_translate (&global_transform,
                             &(graphene_point3d_t){
                                 alloc.width / 2,
                                 alloc.height  /2,
                                 0}
                            );

  /*graphene_matrix_init_scale (&global_transform, 2, 2, 1);*/


  gtk_widget_set_transform (test_widget, &global_transform);





  /*graphene_matrix_init_scale (&global_transform, 0.5, 1, 1);*/
  /*graphene_matrix_translate (&global_transform,*/
                             /*&(graphene_point3d_t){*/
                               /*alloc.width / 2, 0, 0*/
                             /*});*/



  /*gtk_widget_set_transform (test_child, &global_transform);*/


  gtk_widget_queue_draw (test_widget);

  return G_SOURCE_CONTINUE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *matrix_chooser;
  GtkWidget *box;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  matrix_chooser = g_object_new (GTK_TYPE_MATRIX_CHOOSER, NULL);
  transform_tester = g_object_new (GTK_TYPE_TRANSFORM_TESTER, NULL);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

  test_widget = gtk_button_new ();
  gtk_widget_set_size_request (test_widget, TEST_WIDGET_MIN_SIZE, TEST_WIDGET_MIN_SIZE);
  gtk_widget_set_halign (test_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (test_widget, GTK_ALIGN_CENTER);


  test_child = gtk_image_new_from_icon_name ("corebird");
  gtk_widget_set_halign (test_child, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (test_child, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request (test_child, TEST_WIDGET_MIN_SIZE / 2, TEST_WIDGET_MIN_SIZE / 2);
  gtk_container_add (GTK_CONTAINER (test_widget), test_child);


  gtk_transform_tester_set_test_widget (GTK_TRANSFORM_TESTER (transform_tester), test_widget);

  g_timeout_add (16, transform_func, NULL);

  gtk_widget_set_vexpand (transform_tester, TRUE);
  gtk_container_add (GTK_CONTAINER (box), transform_tester);
  gtk_container_add (GTK_CONTAINER (box), matrix_chooser);
  gtk_container_add (GTK_CONTAINER (window), box);

  gtk_window_set_default_size ((GtkWindow *)window, 200, 200);
  g_signal_connect (window, "close-request", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
