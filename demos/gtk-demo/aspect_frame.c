/* Aspect Frame
 *
 * Demonstrates a sample multi-step assistant with GtkAssistant. Assistants
 * are used to divide an operation into several simpler sequential steps,
 * and to guide the user through these steps.
 */

#include <gtk/gtk.h>

static void
setup_ui (GtkWidget *window)
{
  GtkWidget *box, *vbox;
  GtkWidget *aspect_frame;
  GtkWidget *label;
  GtkWidget *scale;

  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.2, 5.0, 0.1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_digits (GTK_SCALE (scale), 2);
  gtk_range_set_value (GTK_RANGE (scale), 1.5);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_vexpand (box, TRUE);

  label = gtk_label_new ("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi ultricies vulputate lobortis. Ut placerat porta quam, eget facilisis felis accumsan a. Cras ut nunc odio.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 50);
  aspect_frame = gtk_aspect_frame_new (0.5, 0.5, 1.5, FALSE);
  g_object_bind_property (gtk_range_get_adjustment (GTK_RANGE (scale)), "value",
                          aspect_frame, "ratio",
                          0);
  gtk_aspect_frame_set_child (GTK_ASPECT_FRAME (aspect_frame), label);
  gtk_box_append (GTK_BOX (box), aspect_frame);

  aspect_frame = gtk_aspect_frame_new (0.5, 0.5, 0, TRUE);
  gtk_aspect_frame_set_child (GTK_ASPECT_FRAME (aspect_frame),
                              gtk_picture_new_for_resource ("/aspect_frame/gtk-logo.svg"));
  gtk_box_append (GTK_BOX (box), aspect_frame);

  aspect_frame = gtk_aspect_frame_new (0.5, 0.5, 1.5, FALSE);
  g_object_bind_property (gtk_range_get_adjustment (GTK_RANGE (scale)), "value",
                          aspect_frame, "ratio",
                          0);
  gtk_aspect_frame_set_child (GTK_ASPECT_FRAME (aspect_frame),
                              gtk_label_new ("Center"));
  gtk_widget_set_halign (aspect_frame, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (aspect_frame, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), aspect_frame);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_append (GTK_BOX (vbox), scale);
  gtk_box_append (GTK_BOX (vbox), box);

  gtk_window_set_child (GTK_WINDOW (window), vbox);
}

GtkWidget*
do_aspect_frame (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkCssProvider *provider;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));

      gtk_window_set_title (GTK_WINDOW (window), "Aspect Frame");
      gtk_widget_add_css_class (window, "aspect-frame-demo");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/aspect_frame/aspect_frame.css");
      gtk_style_context_add_provider_for_display (gtk_widget_get_display (do_widget),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_unref (provider);

      setup_ui (window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
