/* Entry/Entry Undo
 *
 * GtkEntry can provide basic Undo/Redo support using standard keyboard
 * accelerators such as Primary+z to undo and Primary+Shift+z to redo.
 * Additionally, Primary+y can be used to redo.
 *
 * Use gtk_entry_set_enable_undo() to enable undo/redo support.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

GtkWidget *
do_entry_undo (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Entry Undo");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      g_object_set (vbox, "margin", 5, NULL);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "Use Primary+z or Primary+Shift+z to undo or redo changes");
      gtk_container_add (GTK_CONTAINER (vbox), label);

      /* Create our entry */
      entry = gtk_entry_new ();
      gtk_editable_set_enable_undo (GTK_EDITABLE (entry), TRUE);
      gtk_container_add (GTK_CONTAINER (vbox), entry);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
