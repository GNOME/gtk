/* Theming/CSS Accordion
 *
 * A simple accordion demo written using CSS transitions and multiple backgrounds
 */

#include <gtk/gtk.h>

static void
apply_css (GtkWidget *widget, GtkStyleProvider *provider)
{
  GtkWidget *child;

  gtk_style_context_add_provider (gtk_widget_get_style_context (widget), provider, G_MAXUINT);
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    apply_css (child, provider);
}

GtkWidget *
do_css_accordion (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *container, *child;
      GtkStyleProvider *provider;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "CSS Accordion");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 300);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_halign (container, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (container, GTK_ALIGN_CENTER);
      gtk_window_set_child (GTK_WINDOW (window), container);

      child = gtk_button_new_with_label ("This");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label ("Is");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label ("A");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label ("CSS");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label ("Accordion");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label (":-)");
      gtk_box_append (GTK_BOX (container), child);

      provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
      gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (provider), "/css_accordion/css_accordion.css");

      apply_css (window, provider);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
