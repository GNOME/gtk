/* Overlay/Transparency
 * #Keywords: GtkOverlay, GtkSnapshot, blur, backdrop-filter
 *
 * Blur the background behind an overlay.
 */

#include <gtk/gtk.h>

GtkWidget *
do_transparent (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static GtkCssProvider *css_provider = NULL;

  if (!css_provider)
    {
      css_provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (css_provider, "/transparent/transparent.css");

      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (css_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (!window)
    {
      GtkWidget *overlay;
      GtkWidget *button;
      GtkWidget *label;
      GtkWidget *picture;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 450, 450);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "Transparency");

      overlay = gtk_overlay_new ();
      gtk_window_set_child (GTK_WINDOW (window), overlay);

      button = gtk_button_new_with_label ("Don't click this button!");
      label = gtk_button_get_child (GTK_BUTTON (button));
      gtk_widget_set_margin_start (label, 50);
      gtk_widget_set_margin_end (label, 50);
      gtk_widget_set_margin_top (label, 50);
      gtk_widget_set_margin_bottom (label, 50);
      gtk_widget_add_css_class (button, "blur-overlay");

      gtk_widget_set_opacity (button, 0.7);
      gtk_widget_set_halign (button, GTK_ALIGN_FILL);
      gtk_widget_set_valign (button, GTK_ALIGN_START);

      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), button);

      button = gtk_button_new_with_label ("Maybe this one?");
      label = gtk_button_get_child (GTK_BUTTON (button));
      gtk_widget_set_margin_start (label, 50);
      gtk_widget_set_margin_end (label, 50);
      gtk_widget_set_margin_top (label, 50);
      gtk_widget_set_margin_bottom (label, 50);
      gtk_widget_add_css_class (button, "blur-overlay");

      gtk_widget_set_opacity (button, 0.7);
      gtk_widget_set_halign (button, GTK_ALIGN_FILL);
      gtk_widget_set_valign (button, GTK_ALIGN_END);

      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), button);

      picture = gtk_picture_new_for_resource ("/transparent/portland-rose.jpg");
      gtk_overlay_set_child (GTK_OVERLAY (overlay), picture);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
