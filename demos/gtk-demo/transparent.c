/* Overlay/Transparency
 *
 * Blur the background behind an overlay.
 */

#include <gtk/gtk.h>

GtkWidget *
do_transparent (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *sw;
      GtkWidget *overlay;
      GtkWidget *button;
      GtkWidget *label;
      GtkWidget *box;
      GtkWidget *image;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 450, 450);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "Transparency");

      overlay = gtk_overlay_new ();
      gtk_container_add (GTK_CONTAINER (window), overlay);

      button = gtk_button_new_with_label ("Don't click this button!");
      label = gtk_bin_get_child (GTK_BIN (button));
      g_object_set (label, "margin", 50, NULL);

      gtk_widget_set_opacity (button, 0.7);
      gtk_widget_set_halign (button, GTK_ALIGN_FILL);
      gtk_widget_set_valign (button, GTK_ALIGN_START);

      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), button);
      gtk_container_child_set (GTK_CONTAINER (overlay), button, "blur", 5.0, NULL);

      button = gtk_button_new_with_label ("Maybe this one?");
      label = gtk_bin_get_child (GTK_BIN (button));
      g_object_set (label, "margin", 50, NULL);

      gtk_widget_set_opacity (button, 0.7);
      gtk_widget_set_halign (button, GTK_ALIGN_FILL);
      gtk_widget_set_valign (button, GTK_ALIGN_END);

      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), button);
      gtk_container_child_set (GTK_CONTAINER (overlay), button, "blur", 5.0, NULL);

      sw = gtk_scrolled_window_new (NULL, NULL);
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (overlay), sw);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (sw), box);
      image = gtk_image_new_from_resource ("/transparent/portland-rose.jpg");

      gtk_container_add (GTK_CONTAINER (box), image);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
