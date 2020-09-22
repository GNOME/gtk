/* OpenGL/Transitions
 * #Keywords: OpenGL, shader
 *
 * Create transitions between pages using a custom fragment shader.
 * The examples here are taken from gl-transitions.com.
 *
 * Click to start a transition.
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
  gsk_glshader_set_n_required_sources (shader, 2);
  gsk_glshader_add_uniform (shader, "progress", GSK_GLUNIFORM_TYPE_FLOAT);
  g_bytes_unref (shader_b);

  return shader;
}

static void
text_changed (GtkTextBuffer *buffer,
              GtkWidget     *button)
{
  gtk_widget_show (button);
}

static void
apply_text (GtkWidget     *button,
            GtkTextBuffer *buffer)
{
  GtkWidget *stack;
  GskGLShader *shader;
  GtkTextIter start, end;
  char *text;

  stack = gtk_widget_get_ancestor (button, GTK_TYPE_SHADER_STACK);

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);

  shader = gsk_glshader_new (text);
  gsk_glshader_set_n_required_sources (shader, 2);
  gsk_glshader_add_uniform (shader, "progress", GSK_GLUNIFORM_TYPE_FLOAT);

  gtk_shader_stack_set_shader (GTK_SHADER_STACK (stack), shader);

  g_object_unref (shader);
  g_free (text);

  gtk_widget_hide (button);
}

static void
clicked_cb (GtkGestureClick *gesture,
            guint            n_pressed,
            double           x,
            double           y,
            gpointer         data)
{
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static GtkWidget *
make_shader_stack (const char *name,
                   const char *resource_path,
                   GtkWidget  *scale)
{
  GtkWidget *stack, *child, *widget;
  GtkWidget *label, *button, *tv;
  GskGLShader *shader;
  GObjectClass *class;
  GParamSpecFloat *pspec;
  GtkAdjustment *adjustment;
  GtkTextBuffer *buffer;
  GBytes *bytes;
  GtkEventController *controller;
  GtkCssProvider *provider;

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

  child = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  widget = gtk_center_box_new ();
  label = gtk_label_new (name);
  gtk_widget_add_css_class (label, "title-4");
  gtk_widget_set_size_request (label, -1, 26);
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (widget), label);

  button = gtk_button_new_from_icon_name ("view-refresh-symbolic");
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, "button.small { padding: 0; }", -1);
  gtk_style_context_add_provider (gtk_widget_get_style_context (button),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (button, "small");
  gtk_widget_hide (button);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (widget), button);

  gtk_box_append (GTK_BOX (child), widget);

  class = g_type_class_ref (GTK_TYPE_SHADER_STACK);
  pspec = G_PARAM_SPEC_FLOAT (g_object_class_find_property (class, "duration"));

  adjustment = gtk_range_get_adjustment (GTK_RANGE (scale));
  if (gtk_adjustment_get_lower (adjustment) == 0.0 &&
      gtk_adjustment_get_upper (adjustment) == 0.0)
    {
      gtk_adjustment_configure (adjustment,
                                pspec->default_value,
                                pspec->minimum,
                                pspec->maximum,
                                0.1, 0.5, 0);
    }

  g_type_class_unref (class);

  g_object_bind_property (adjustment, "value",
                          stack, "duration",
                          G_BINDING_DEFAULT);

  widget = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (widget), TRUE);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
  g_signal_connect (controller, "released", G_CALLBACK (clicked_cb), NULL);
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
  gtk_widget_add_controller (GTK_WIDGET (widget), controller);

  tv = gtk_text_view_new ();
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (tv), 4);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (tv), 4);
  gtk_text_view_set_top_margin (GTK_TEXT_VIEW (tv), 4);
  gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (tv), 4);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
  bytes = g_resources_lookup_data (resource_path, 0, NULL);
  gtk_text_buffer_set_text (buffer,
                            (const char *)g_bytes_get_data (bytes, NULL),
                            g_bytes_get_size (bytes));
  g_bytes_unref (bytes);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (widget), tv);

  g_signal_connect (buffer, "changed", G_CALLBACK (text_changed), button);
  g_signal_connect (button, "clicked", G_CALLBACK (apply_text), buffer);

  gtk_box_append (GTK_BOX (child), widget);

  gtk_shader_stack_add_child (GTK_SHADER_STACK (stack), child);

  return stack;
}

static GtkWidget *
create_gltransition_window (GtkWidget *do_widget)
{
  GtkWidget *window, *headerbar, *scale, *grid;

  window = gtk_window_new ();
  gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "Transitions");
  headerbar = gtk_header_bar_new ();
  scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_widget_set_size_request (scale, 100, -1);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (headerbar), scale);
  gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  grid = gtk_grid_new ();
  gtk_window_set_child (GTK_WINDOW (window), grid);

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

  gtk_grid_attach (GTK_GRID (grid),
                   make_shader_stack ("Wind", "/gltransition/transition1.glsl", scale),
                   0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   make_shader_stack ("Radial", "/gltransition/transition2.glsl", scale),
                   1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   make_shader_stack ("Crosswarp", "/gltransition/transition3.glsl", scale),
                   0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   make_shader_stack ("Kaleidoscope", "/gltransition/transition4.glsl", scale),
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
