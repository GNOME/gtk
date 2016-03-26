/* Theming/CSS Accordion
 *
 * A simple accordion demo written using CSS transitions and multiple backgrounds
 *
 */

#include <gtk/gtk.h>

static void
apply_css (GtkWidget *widget, GtkStyleProvider *provider)
{
  gtk_style_context_add_provider (gtk_widget_get_style_context (widget), provider, G_MAXUINT);
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), (GtkCallback) apply_css, provider);
}

GtkWidget *
do_css_accordion (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *container, *child;
      GtkStyleProvider *provider;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "CSS Accordion");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 300);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_halign (container, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (container, GTK_ALIGN_CENTER);
      gtk_container_add (GTK_CONTAINER (window), container);

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

      provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
      gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (provider), "/css_accordion/css_accordion.css");

      apply_css (window, provider);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
