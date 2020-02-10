/* Theming/CSS Accordion
 *
 * A simple accordion demo written using CSS transitions and multiple backgrounds
 *
 */

#include <gtk/gtk.h>

static void
destroy_provider (GtkWidget      *window,
                  GtkCssStyleSheet *stylesheet)
{
  gtk_style_context_remove_provider_for_display (gtk_widget_get_display (window), GTK_STYLE_PROVIDER (stylesheet));
}

GtkWidget *
do_css_accordion (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *container, *styled_box, *child;
      GtkStyleProvider *stylesheet;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "CSS Accordion");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 300);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      
      styled_box = gtk_frame_new (NULL);
      gtk_container_add (GTK_CONTAINER (window), styled_box);
      gtk_widget_add_css_class (styled_box, "accordion");
      container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_halign (container, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (container, GTK_ALIGN_CENTER);
      gtk_container_add (GTK_CONTAINER (styled_box), container);

      child = gtk_button_new_with_label ("This");
      gtk_container_add (GTK_CONTAINER (container), child);

      child = gtk_button_new_with_label ("Is");
      gtk_container_add (GTK_CONTAINER (container), child);

      child = gtk_button_new_with_label ("A");
      gtk_container_add (GTK_CONTAINER (container), child);

      child = gtk_button_new_with_label ("CSS");
      gtk_container_add (GTK_CONTAINER (container), child);

      child = gtk_button_new_with_label ("Accordion");
      gtk_container_add (GTK_CONTAINER (container), child);

      child = gtk_button_new_with_label (":-)");
      gtk_container_add (GTK_CONTAINER (container), child);

      stylesheet = GTK_STYLE_PROVIDER (gtk_css_style_sheet_new ());
      gtk_css_style_sheet_load_from_resource (GTK_CSS_STYLE_SHEET (stylesheet), "/css_accordion/css_accordion.css");

      gtk_style_context_add_provider_for_display (gtk_widget_get_display (window),
                                                  GTK_STYLE_PROVIDER (stylesheet),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (destroy_provider), stylesheet);
      g_object_unref (stylesheet);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
