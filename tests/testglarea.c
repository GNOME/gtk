#include <stdlib.h>
#include <gtk/gtk.h>

#include <epoxy/gl.h>

enum {
  X_AXIS,
  Y_AXIS,
  Z_AXIS,

  N_AXIS
};

static float rotation_angles[N_AXIS] = { 0.0 };

static GtkWidget *gl_area;

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
init (GtkWidget *widget)
{
  GdkGLContext *context = gtk_gl_area_get_context (GTK_GL_AREA (widget));
  GdkGLPixelFormat *format = gdk_gl_context_get_pixel_format (context);

  g_print ("GL Pixel format:\n"
           " - double-buffer: %s\n"
           " - multi-sample: %s\n"
           " - stereo: %s\n"
           " - color-size: %d, alpha-size: %d\n"
           " - depth-size: %d\n"
           " - stencil-size: %d\n"
           " - aux-buffers: %d\n"
           " - accum-size: %d\n"
           " - sample-buffers: %d\n"
           " - samples: %d\n\n",
           gdk_gl_pixel_format_get_double_buffer (format) ? "yes" : "no",
           gdk_gl_pixel_format_get_multi_sample (format) ? "yes" : "no",
           gdk_gl_pixel_format_get_stereo (format) ? "yes" : "no",
           gdk_gl_pixel_format_get_color_size (format),
           gdk_gl_pixel_format_get_alpha_size (format),
           gdk_gl_pixel_format_get_depth_size (format),
           gdk_gl_pixel_format_get_stencil_size (format),
           gdk_gl_pixel_format_get_aux_buffers (format),
           gdk_gl_pixel_format_get_accum_size (format),
           gdk_gl_pixel_format_get_sample_buffers (format),
           gdk_gl_pixel_format_get_samples (format));
}

static void
on_axis_value_change (GtkAdjustment *adjustment,
                      gpointer       data)
{
  int axis = GPOINTER_TO_INT (data);

  if (axis < 0 || axis >= N_AXIS)
    return;

  rotation_angles[axis] = gtk_adjustment_get_value (adjustment);

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

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *button, *controls;
  GdkGLPixelFormat *pixel_format;
  int i;

  gtk_init (&argc, &argv);

  /* create a new pixel format; we use this to configure the
   * GL context, and to check for features
   */
  pixel_format = gdk_gl_pixel_format_new ("double-buffer", TRUE,
                                          NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GtkGLArea - Golden Triangle");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show (box);

  gl_area = gtk_gl_area_new (pixel_format);
  gtk_widget_set_hexpand (gl_area, TRUE);
  gtk_widget_set_vexpand (gl_area, TRUE);
  gtk_container_add (GTK_CONTAINER (box), gl_area);
  g_signal_connect (gl_area, "realize", G_CALLBACK (init), NULL);
  g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);
  gtk_widget_show (gl_area);
  g_object_unref (pixel_format);

  controls = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_container_add (GTK_CONTAINER (box), controls);
  gtk_widget_set_hexpand (controls, TRUE);
  gtk_widget_show (controls);

  for (i = 0; i < N_AXIS; i++)
    gtk_container_add (GTK_CONTAINER (controls), create_axis_slider (i));

  button = gtk_button_new_with_label ("Quit");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);
  gtk_widget_show (button);

  gtk_main ();

  return EXIT_SUCCESS;
}
