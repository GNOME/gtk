#include <gtk/gtk.h>

static void
new_window (GtkApplication *app,
            GFile          *file)
{
  GtkWidget *window, *scrolled, *view;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_application (GTK_WINDOW (window), app);
  gtk_window_set_title (GTK_WINDOW (window), "Bloatpad");
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  view = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (scrolled), view);
  gtk_container_add (GTK_CONTAINER (window), scrolled);

  if (file != NULL)
    {
      gchar *contents;
      gsize length;

      if (g_file_load_contents (file, NULL, &contents, &length, NULL, NULL))
        {
          GtkTextBuffer *buffer;

          buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
          gtk_text_buffer_set_text (buffer, contents, length);
          g_free (contents);
        }
    }

  gtk_widget_show_all (GTK_WIDGET (window));
}

static void
activate (GtkApplication *application)
{
  new_window (application, NULL);
}

static void
open (GtkApplication  *application,
      GFile          **files,
      gint             n_files,
      const gchar     *hint,
      gpointer         user_data)
{
  gint i;

  for (i = 0; i < n_files; i++)
    new_window (application, files[i]);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.gtk.Test.bloatpad",
                             G_APPLICATION_HANDLES_OPEN);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
