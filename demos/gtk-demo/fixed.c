/* Fixed layout
 *
 * GtkFixed is a container that allows placing and transforming
 * widgets manually.
 */

#include <gtk/gtk.h>

/* This enumeration determines the paint order */
enum {
  FACE_BACK,
  FACE_LEFT,
  FACE_BOTTOM,
  FACE_RIGHT,
  FACE_TOP,
  FACE_FRONT,

  N_FACES
};

/* Map face widgets to CSS classes */
static struct {
  GtkWidget *face;
  const char *css_class;
} faces[N_FACES] = {
  [FACE_BACK] = { NULL, "back", },
  [FACE_LEFT] = { NULL, "left", },
  [FACE_RIGHT] = { NULL, "right", },
  [FACE_TOP] = { NULL, "top", },
  [FACE_BOTTOM] = { NULL, "bottom", },
  [FACE_FRONT] = { NULL, "front", },
};

static GtkWidget *
create_faces (void)
{
  GtkWidget *fixed = gtk_fixed_new ();
  int face_size = 200;
  float w, h, d, p;

  gtk_widget_set_overflow (fixed, GTK_OVERFLOW_VISIBLE);

  w = (float) face_size / 2.f;
  h = (float) face_size / 2.f;
  d = (float) face_size / 2.f;
  p = face_size * 3.f;

  for (int i = 0; i < N_FACES; i++)
    {
      GskTransform *transform = NULL;

      /* Add a face */
      faces[i].face = gtk_frame_new (NULL);
      gtk_widget_set_size_request (faces[i].face, face_size, face_size);
      gtk_style_context_add_class (gtk_widget_get_style_context (faces[i].face), faces[i].css_class);
      gtk_container_add (GTK_CONTAINER (fixed), faces[i].face);

      /* Set up the transformation for each face */
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (w, h));
      transform = gsk_transform_perspective (transform, p);
      transform = gsk_transform_rotate_3d (transform, -30.f, graphene_vec3_x_axis ());
      transform = gsk_transform_rotate_3d (transform, 135.f, graphene_vec3_y_axis ());
      transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (0, 0, -face_size / 6.f));

      switch (i)
        {
        case FACE_FRONT:
          transform = gsk_transform_rotate_3d (transform, 0.f, graphene_vec3_y_axis ());
          break;

        case FACE_BACK:
          transform = gsk_transform_rotate_3d (transform, -180.f, graphene_vec3_y_axis ());
          break;

        case FACE_RIGHT:
          transform = gsk_transform_rotate_3d (transform, 90.f, graphene_vec3_y_axis ());
          break;

        case FACE_LEFT:
          transform = gsk_transform_rotate_3d (transform, -90.f, graphene_vec3_y_axis ());
          break;

        case FACE_TOP:
          transform = gsk_transform_rotate_3d (transform, 90.f, graphene_vec3_x_axis ());
          break;

        case FACE_BOTTOM:
          transform = gsk_transform_rotate_3d (transform, -90.f, graphene_vec3_x_axis ());
          break;

        default:
          break;
        }

      transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (0, 0, d));
      transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (-w, -h, 0));

      gtk_fixed_set_child_transform (GTK_FIXED (fixed), faces[i].face, transform);
      gsk_transform_unref (transform);
    }

  return fixed;
}

static GtkWidget *demo_window = NULL;
static GtkCssStyleSheet *stylesheet = NULL;

static void
close_window (GtkWidget *widget)
{
  /* Reset the state */
  for (int i = 0; i < N_FACES; i++)
    faces[i].face = NULL;

  gtk_style_context_remove_style_sheet_for_display (gdk_display_get_default (), stylesheet);
  stylesheet = NULL;

  demo_window = NULL;
}

static GtkWidget *
create_demo_window (GtkWidget *do_widget)
{
  GtkWidget *window, *sw, *fixed, *cube;

  window = gtk_window_new ();
  gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "Fixed layout");
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (window), sw);

  fixed = gtk_fixed_new ();
  gtk_container_add (GTK_CONTAINER (sw), fixed);
  gtk_widget_set_halign (GTK_WIDGET (fixed), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (fixed), GTK_ALIGN_CENTER);

  cube = create_faces ();
  gtk_container_add (GTK_CONTAINER (fixed), cube);
  gtk_widget_set_overflow (fixed, GTK_OVERFLOW_VISIBLE);

  stylesheet = gtk_css_style_sheet_new ();
  gtk_css_style_sheet_set_priority (stylesheet, 800);
  gtk_css_style_sheet_load_from_resource (stylesheet, "/fixed/fixed.css");
  gtk_style_context_add_style_sheet_for_display (gdk_display_get_default (), stylesheet);
  g_object_unref (stylesheet);

  return window;
}

GtkWidget*
do_fixed (GtkWidget *do_widget)
{
  if (demo_window == NULL)
    demo_window = create_demo_window (do_widget);

  if (!gtk_widget_get_visible (demo_window))
    gtk_widget_show (demo_window);
  else
    gtk_widget_destroy (demo_window);

  return demo_window;
}
