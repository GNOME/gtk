/* Text View/Undo and Redo
 *
 * The GtkTextView supports undo and redo through the use of a
 * GtkTextBuffer. You can enable or disable undo support using
 * gtk_text_buffer_set_enable_undo().
 *
 * Use Primary+Z to undo and Primary+Shift+Z or Primary+Y to
 * redo previously undone operations.
 */

#include <gtk/gtk.h>
#include <stdlib.h> /* for exit() */

GtkWidget *
do_textundo (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *view;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      GtkTextIter iter;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window),
                                   450, 450);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "TextView Undo");

      view = gtk_text_view_new ();
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_text_buffer_set_enable_undo (buffer, TRUE);

      /* this text cannot be undone */
      gtk_text_buffer_begin_irreversible_action (buffer);
      gtk_text_buffer_get_start_iter (buffer, &iter);
      gtk_text_buffer_insert (buffer, &iter,
                              "Type to add more text.\n"
                              "Use Primary+Z to undo and Primary+Shift+Z to redo a previously undone action.\n"
                              "\n",
                              -1);
      gtk_text_buffer_end_irreversible_action (buffer);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (window), sw);
      gtk_container_add (GTK_CONTAINER (sw), view);
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
