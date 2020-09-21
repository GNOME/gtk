/* OpenGL/glshader
 * #Keywords: OpenGL, shader
 *
 * Generate pixels using a custom fragment shader.
 *
 * The names of the uniforms are compatible with the shaders on shadertoy.com, so
 * many of the shaders there work here too.
 */
#include <math.h>
#include <gtk/gtk.h>
#include "gtkshaderbin.h"

static GtkWidget *demo_window = NULL;

static void
close_window (GtkWidget *widget)
{
  /* Reset the state */
  demo_window = NULL;
}

static GtkWidget *
fire_bin_new (void)
{
  GtkWidget *bin = gtk_shader_bin_new ();
  GBytes *shader_b;
  GskGLShader *shader;

  shader_b = g_resources_lookup_data ("/glshader/fire.glsl", 0, NULL);
  shader = gsk_glshader_new ((const char *)g_bytes_get_data (shader_b, NULL));
  gtk_shader_bin_add_shader (GTK_SHADER_BIN (bin), shader, GTK_STATE_FLAG_PRELIGHT, GTK_STATE_FLAG_PRELIGHT);

  return bin;
}


static GtkWidget *
create_glshader_window (GtkWidget *do_widget)
{
  GtkWidget *window, *box, *button, *bin;

  window = gtk_window_new ();
  gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "glshader");
  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_window_set_child (GTK_WINDOW (window), box);

  bin = fire_bin_new ();
  gtk_box_append (GTK_BOX (box), bin);

  button = gtk_button_new_with_label ("Click me");
  gtk_widget_set_receives_default (button, TRUE);
  gtk_shader_bin_set_child (GTK_SHADER_BIN (bin), button);

  bin = fire_bin_new ();
  gtk_box_append (GTK_BOX (box), bin);

  button = gtk_button_new_with_label ("Or me!");
  gtk_widget_set_receives_default (button, TRUE);
  gtk_shader_bin_set_child (GTK_SHADER_BIN (bin), button);

  return window;
}

GtkWidget *
do_glshader (GtkWidget *do_widget)
{
  if (!demo_window)
    demo_window = create_glshader_window (do_widget);

  if (!gtk_widget_get_visible (demo_window))
    gtk_widget_show (demo_window);
  else
    gtk_window_destroy (GTK_WINDOW (demo_window));

  return demo_window;
}
