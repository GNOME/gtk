/* OpenGL/Transitions
 * #Keywords: OpenGL, shader
 *
 * Create transitions between pages using a custom fragment shader.
 *
 * The examples here are taken from gl-transitions.com.
 */
#include <math.h>
#include <gtk/gtk.h>
#include "gtkshaderstack.h"

static GtkWidget *demo_window = NULL;

static void
close_window (GtkWidget *widget)
{
  /* Reset the state */
  demo_window = NULL;
}

static GskGLShader *
gsk_shader_new_from_resource (const char *resource_path)
{
  GBytes *shader_b;
  GskGLShader *shader;

  shader_b = g_resources_lookup_data (resource_path, 0, NULL);
  shader = gsk_glshader_new ((const char *)g_bytes_get_data (shader_b, NULL));
  g_bytes_unref (shader_b);

  return shader;
}

static GtkWidget *
make_shader_stack (const char *resource_path)
{
  GtkWidget *stack, *child;
  GskGLShader *shader;

  stack = gtk_shader_stack_new ();
  shader = gsk_shader_new_from_resource (resource_path);
  gtk_shader_stack_set_shader (GTK_SHADER_STACK (stack), shader);
  g_object_unref (shader);

  child = gtk_picture_new_for_resource ("/css_pixbufs/background.jpg");
  gtk_picture_set_can_shrink (GTK_PICTURE (child), TRUE);
  gtk_shader_stack_add_child (GTK_SHADER_STACK (stack), child);

  child = gtk_picture_new_for_resource ("/transparent/portland-rose.jpg");
  gtk_picture_set_can_shrink (GTK_PICTURE (child), TRUE);
  gtk_shader_stack_add_child (GTK_SHADER_STACK (stack), child);

  child = gtk_picture_new_for_resource ("/css_blendmodes/ducky.png");
  gtk_picture_set_can_shrink (GTK_PICTURE (child), TRUE);
  gtk_shader_stack_add_child (GTK_SHADER_STACK (stack), child);

  return stack;
}

static GtkWidget *
create_gltransition_window (GtkWidget *do_widget)
{
  GtkWidget *window, *grid;

  window = gtk_window_new ();
  gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "Transitions");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  grid = gtk_grid_new ();
  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (grid, 12);
  gtk_widget_set_margin_end (grid, 12);
  gtk_widget_set_margin_top (grid, 12);
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

  gtk_window_set_child (GTK_WINDOW (window), grid);

  gtk_grid_attach (GTK_GRID (grid),
                   make_shader_stack ("/gltransition/transition1.glsl"),
                   0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   make_shader_stack ("/gltransition/transition2.glsl"),
                   1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   make_shader_stack ("/gltransition/transition3.glsl"),
                   0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   make_shader_stack ("/gltransition/transition4.glsl"),
                   1, 1, 1, 1);

  return window;
}

GtkWidget *
do_gltransition (GtkWidget *do_widget)
{
  if (!demo_window)
    demo_window = create_gltransition_window (do_widget);

  if (!gtk_widget_get_visible (demo_window))
    gtk_widget_show (demo_window);
  else
    gtk_window_destroy (GTK_WINDOW (demo_window));

  return demo_window;
}
