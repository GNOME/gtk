/* OpenGL Area
 *
 * GtkGLArea is a widget that allows custom drawing using OpenGL calls.
 */

#include <gtk/gtk.h>
#include <epoxy/gl.h>

static GtkWidget *demo_window = NULL;

/* the GtkGLArea widget */
static GtkWidget *gl_area = NULL;

enum {
  X_AXIS,
  Y_AXIS,
  Z_AXIS,

  N_AXIS
};

/* Rotation angles on each axis */
static float rotation_angles[N_AXIS] = { 0.0 };

/* The object we are drawing */
static void
draw_triangle (void)
{
  glColor3f (1.0f, 0.85f, 0.35f);
  glBegin (GL_TRIANGLES);
  {
    glVertex3f ( 0.0,  0.6,  0.0);
    glVertex3f (-0.2, -0.3,  0.0);
    glVertex3f ( 0.2, -0.3,  0.0);
  }
  glEnd ();
}

/* The main rendering callback */
static gboolean
render (GtkGLArea    *area,
        GdkGLContext *context)
{
  glClearColor (0.5, 0.5, 0.5, 1.0);
  glClear (GL_COLOR_BUFFER_BIT);

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  glRotatef (rotation_angles[X_AXIS], 1, 0, 0);
  glRotatef (rotation_angles[Y_AXIS], 0, 1, 0);
  glRotatef (rotation_angles[Z_AXIS], 0, 0, 1);

  draw_triangle ();

  glFlush ();

  return TRUE;
}

static void
on_axis_value_change (GtkAdjustment *adjustment,
                      gpointer       data)
{
  int axis = GPOINTER_TO_INT (data);

  g_assert (axis >= 0 && axis < N_AXIS);

  /* Update the rotation angle */
  rotation_angles[axis] = gtk_adjustment_get_value (adjustment);

  /* Update the contents of the GL drawing area */
  gtk_widget_queue_draw (gl_area);
}

static GtkWidget *
create_axis_slider (int axis)
{
  GtkWidget *box, *label, *slider;
  GtkAdjustment *adj;
  const char *text;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);

  switch (axis)
    {
    case X_AXIS:
      text = "X axis";
      break;

    case Y_AXIS:
      text = "Y axis";
      break;

    case Z_AXIS:
      text = "Z axis";
      break;

    default:
      g_assert_not_reached ();
    }

  label = gtk_label_new (text);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_widget_show (label);

  adj = gtk_adjustment_new (0.0, 0.0, 360.0, 1.0, 12.0, 0.0);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (on_axis_value_change),
                    GINT_TO_POINTER (axis));
  slider = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adj);
  gtk_container_add (GTK_CONTAINER (box), slider);
  gtk_widget_set_hexpand (slider, TRUE);
  gtk_widget_show (slider);

  gtk_widget_show (box);

  return box;
}

static void
close_window (GtkWidget *widget)
{
  /* Reset the state */
  demo_window = NULL;
  gl_area = NULL;

  rotation_angles[X_AXIS] = 0.0;
  rotation_angles[Y_AXIS] = 0.0;
  rotation_angles[Z_AXIS] = 0.0;
}

GtkWidget *
create_glarea_window (GtkWidget *do_widget)
{
  GtkWidget *window, *box, *button, *controls;
  int i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_screen (GTK_WINDOW (window), gtk_widget_get_screen (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "GtkGLArea - Golden Triangle");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_container_add (GTK_CONTAINER (window), box);

  gl_area = gtk_gl_area_new ();
  gtk_widget_set_hexpand (gl_area, TRUE);
  gtk_widget_set_vexpand (gl_area, TRUE);
  gtk_container_add (GTK_CONTAINER (box), gl_area);

  /* the main "draw" call for GtkGLArea */
  g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);

  controls = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_container_add (GTK_CONTAINER (box), controls);
  gtk_widget_set_hexpand (controls, TRUE);

  for (i = 0; i < N_AXIS; i++)
    gtk_container_add (GTK_CONTAINER (controls), create_axis_slider (i));

  button = gtk_button_new_with_label ("Quit");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);

  return window;
}

GtkWidget*
do_glarea (GtkWidget *do_widget)
{
  if (demo_window == NULL)
    demo_window = create_glarea_window (do_widget);

  if (!gtk_widget_get_visible (demo_window))
    gtk_widget_show_all (demo_window);
  else
    gtk_widget_destroy (demo_window);

  return demo_window;
}
