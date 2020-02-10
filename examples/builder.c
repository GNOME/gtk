#include <gtk/gtk.h>
#include <glib/gstdio.h>

static void
print_hello (GtkWidget *widget,
             gpointer   data)
{
  g_print ("Hello World\n");
}

static void
quit_cb (GtkWidget *widget, gpointer data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int   argc,
      char *argv[])
{
  GtkBuilder *builder;
  GObject *window;
  GObject *button;
  gboolean done = FALSE;

#ifdef GTK_SRCDIR
  g_chdir (GTK_SRCDIR);
#endif

  gtk_init ();

  /* Construct a GtkBuilder instance and load our UI description */
  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, "builder.ui", NULL);

  /* Connect signal handlers to the constructed widgets. */
  window = gtk_builder_get_object (builder, "window");
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  button = gtk_builder_get_object (builder, "button1");
  g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);

  button = gtk_builder_get_object (builder, "button2");
  g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);

  button = gtk_builder_get_object (builder, "quit");
  g_signal_connect (button, "clicked", G_CALLBACK (quit_cb), &done);

  gtk_widget_show (GTK_WIDGET (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
