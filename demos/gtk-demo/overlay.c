/* Overlay
 *
 * Stack widgets in static positions over a main widget
 */

#include <gtk/gtk.h>

GtkWidget *
do_overlay (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *view;
      GtkWidget *sw;
      GtkWidget *overlay;
      GtkWidget *entry;
      GtkWidget *label;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window),
                                   450, 450);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "Overlay");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      view = gtk_text_view_new ();

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (sw), view);

      overlay = gtk_overlay_new ();
      gtk_container_add (GTK_CONTAINER (overlay), sw);
      gtk_container_add (GTK_CONTAINER (window), overlay);

      entry = gtk_entry_new ();
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);
      gtk_widget_set_halign (entry, GTK_ALIGN_END);
      gtk_widget_set_valign (entry, GTK_ALIGN_END);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_END);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
      gtk_widget_set_margin_left (label, 20);
      gtk_widget_set_margin_right (label, 20);
      gtk_widget_set_margin_top (label, 5);
      gtk_widget_set_margin_bottom (label, 5);

      entry = gtk_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_START);
      gtk_widget_set_valign (entry, GTK_ALIGN_END);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_END);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
      gtk_widget_set_margin_left (label, 20);
      gtk_widget_set_margin_right (label, 20);
      gtk_widget_set_margin_top (label, 5);
      gtk_widget_set_margin_bottom (label, 5);

      entry = gtk_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_END);
      gtk_widget_set_valign (entry, GTK_ALIGN_START);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_START);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
      gtk_widget_set_margin_left (label, 20);
      gtk_widget_set_margin_right (label, 20);
      gtk_widget_set_margin_top (label, 5);
      gtk_widget_set_margin_bottom (label, 5);

      entry = gtk_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_START);
      gtk_widget_set_valign (entry, GTK_ALIGN_START);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_START);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
      gtk_widget_set_margin_left (label, 20);
      gtk_widget_set_margin_right (label, 20);
      gtk_widget_set_margin_top (label, 5);
      gtk_widget_set_margin_bottom (label, 5);

      entry = gtk_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_END);
      gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
      gtk_widget_set_margin_left (label, 20);
      gtk_widget_set_margin_right (label, 20);
      gtk_widget_set_margin_top (label, 5);
      gtk_widget_set_margin_bottom (label, 5);

      entry = gtk_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_START);
      gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
      gtk_widget_set_margin_left (label, 20);
      gtk_widget_set_margin_right (label, 20);
      gtk_widget_set_margin_top (label, 5);
      gtk_widget_set_margin_bottom (label, 5);

      entry = gtk_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (entry, GTK_ALIGN_START);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (label, GTK_ALIGN_START);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
      gtk_widget_set_margin_left (label, 20);
      gtk_widget_set_margin_right (label, 20);
      gtk_widget_set_margin_top (label, 5);
      gtk_widget_set_margin_bottom (label, 5);

      entry = gtk_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (entry, GTK_ALIGN_END);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (label, GTK_ALIGN_END);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
      gtk_widget_set_margin_left (label, 10);
      gtk_widget_set_margin_right (label, 10);
      gtk_widget_set_margin_top (label, 5);
      gtk_widget_set_margin_bottom (label, 5);

      entry = gtk_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);

      label = gtk_label_new ("Hello world");
      gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);

      gtk_widget_show_all (overlay);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
