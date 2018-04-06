#include <gtk/gtk.h>
#include "benchmark.h"

/* Command line options */
const char *profile_benchmark_name = NULL;
static GOptionEntry options[] = {
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile_benchmark_name, "Benchmark name to profile using callgrind", NULL },
  { NULL }
};


static void
css_compute_benchmark (Benchmark *b,
                       gsize      size,
                       gpointer   user_data)
{
  GtkWidget **widgets = g_malloc (sizeof (GtkWidget *) * size);
  GtkWidget *box;
  GtkWidget *scroller;
  GtkWidget *window;
  GdkFrameClock *frame_clock;
  guint i;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    {
      widgets[i] = gtk_label_new ("foo");
      /*widgets[i] = gtk_button_new ();*/
      gtk_container_add (GTK_CONTAINER (box), widgets[i]);
    }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  scroller = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroller), box);
  gtk_container_add (GTK_CONTAINER (window), scroller);

  gtk_widget_realize (window);
  frame_clock = gtk_widget_get_frame_clock (window);
  g_assert (frame_clock != NULL);

  gtk_widget_show (window);
  g_signal_connect (frame_clock, "layout", G_CALLBACK (gtk_main_quit), NULL);

  benchmark_start (b);
  gtk_main ();
  benchmark_stop (b);

  gtk_widget_hide (window);
  gtk_widget_destroy (window);

  g_free (widgets);
}


int
main (int argc, char **argv)
{
  BenchmarkSuite suite;
  GOptionContext *option_context;
  GError *error = NULL;

  option_context = g_option_context_new ("");
  g_option_context_add_main_entries (option_context, options, NULL);
  if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  benchmark_suite_init (&suite, profile_benchmark_name);
  gtk_init ();

  benchmark_suite_add (&suite, "css compute", 10000, css_compute_benchmark, NULL);

  return benchmark_suite_run (&suite);
}
