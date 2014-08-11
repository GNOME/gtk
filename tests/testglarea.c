#include <stdlib.h>
#include <gtk/gtk.h>

#include <epoxy/gl.h>

static gboolean
render (GtkGLArea    *area,
        GdkGLContext *context)
{
  glClearColor (0, 0, 0, 0);
  glClear (GL_COLOR_BUFFER_BIT);

  glColor3f (1.0f, 0.85f, 0.35f);
  glBegin (GL_TRIANGLES);
  {
    glVertex3f ( 0.0,  0.6,  0.0);
    glVertex3f (-0.2, -0.3,  0.0);
    glVertex3f ( 0.2, -0.3,  0.0);
  }
  glEnd ();

  glFlush ();

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *gl_area;
  GdkGLPixelFormat *pixel_format;

  gtk_init (&argc, &argv);

  /* create a new pixel format; we use this to configure the
   * GL context, and to check for features
   */
  pixel_format = gdk_gl_pixel_format_new ("double-buffer", TRUE, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GtkGLArea - Golden Triangle");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);

  gl_area = gtk_gl_area_new (pixel_format);
  gtk_container_add (GTK_CONTAINER (window), gl_area);
  gtk_widget_show (gl_area);
  g_object_unref (pixel_format);

  g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);

  gtk_main ();

  return EXIT_SUCCESS;
}
