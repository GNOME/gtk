/* Overlay/Interactive Overlay
 *
 * Shows widgets in static positions over a main widget.
 *
 * The overlayed widgets can be interactive controls such
 * as the entry in this example, or just decorative, like
 * the big blue label.
 */

#include <gtk/gtk.h>

static void
do_number (GtkButton *button, GtkEntry *entry)
{
  gtk_entry_set_text (entry, gtk_button_get_label (button));
}

GtkWidget *
do_overlay (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *overlay;
      GtkWidget *grid;
      GtkWidget *button;
      GtkWidget *vbox;
      GtkWidget *label;
      GtkWidget *entry;
      int i, j;
      char *text;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_default_size (GTK_WINDOW (window), 500, 510);
      gtk_window_set_title (GTK_WINDOW (window), "Interactive Overlay");

      overlay = gtk_overlay_new ();
      grid = gtk_grid_new ();
      gtk_container_add (GTK_CONTAINER (overlay), grid);

      entry = gtk_entry_new ();

      for (j = 0; j < 5; j++)
        {
          for (i = 0; i < 5; i++)
            {
              text = g_strdup_printf ("%d", 5*j + i);
              button = gtk_button_new_with_label (text);
              g_free (text);
              gtk_widget_set_hexpand (button, TRUE);
              gtk_widget_set_vexpand (button, TRUE);
              g_signal_connect (button, "clicked", G_CALLBACK (do_number), entry);
              gtk_grid_attach (GTK_GRID (grid), button, i, j, 1, 1);
            }
        }

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), vbox);
      gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (overlay), vbox, TRUE);
      gtk_widget_set_halign (vbox, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);

      label = gtk_label_new ("<span foreground='blue' weight='ultrabold' font='40'>Numbers</span>");
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 8);

      gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "Your Lucky Number");
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 8);

      gtk_container_add (GTK_CONTAINER (window), overlay);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_widget_show_all (overlay);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
