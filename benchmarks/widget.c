#include <gtk/gtk.h>
#include "benchmark.h"

/* Command line options */
const char *profile_benchmark_name = NULL;
static GOptionEntry options[] = {
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile_benchmark_name, "Benchmark name to profile using callgrind", NULL },
  { NULL }
};


static void
set_parent_benchmark (Benchmark *b,
                      gsize      size,
                      gpointer   user_data)
{
  guint i;
  GtkWidget *w;
  GtkWidget **widgets;

  widgets = g_malloc (sizeof (GtkWidget *) * size);
  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    {
      widgets[i] = gtk_button_new ();
    }

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      gtk_widget_set_parent (widgets[i], w);
    }
  benchmark_stop (b);


  g_free (widgets);
}

static void
reorder_benchmark (Benchmark *b,
                   gsize      size,
                   gpointer   user_data)
{
  guint i;
  GtkWidget *w;
  GtkWidget **widgets;

  widgets = g_malloc (sizeof (GtkWidget *) * size);
  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    {
      widgets[i] = gtk_button_new ();
      gtk_widget_set_parent (widgets[i], w);
    }

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      /* Move this child to the very end */
      gtk_widget_insert_before (widgets[i], w, NULL);
    }
  benchmark_stop (b);

  g_free (widgets);
}

static void
get_size_benchmark (Benchmark *b,
                    gsize      size,
                    gpointer   user_data)
{
  guint i;
  GtkWidget *w;
  int width, height;
  int button_width;
  int button_height;

  w = gtk_button_new ();

  gtk_widget_measure (w, GTK_ORIENTATION_HORIZONTAL, -1, &width, NULL, NULL, NULL);
  gtk_widget_measure (w, GTK_ORIENTATION_VERTICAL, width, &height, NULL, NULL, NULL);

  button_width = 200 + width;
  button_height = 300 + height;

  gtk_widget_size_allocate (w,
                            &(GtkAllocation){0, 0, button_width, button_height}, -1);

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      width = gtk_widget_get_width (w);
      height = gtk_widget_get_height (w);
    }
  benchmark_stop (b);

  g_assert_cmpint (width, <=, button_width);
  g_assert_cmpint (height, <=, button_height);
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

  benchmark_suite_add (&suite, "set_parent", 10000, set_parent_benchmark, NULL);
  benchmark_suite_add (&suite, "reorder", 10000, reorder_benchmark, NULL);
  benchmark_suite_add (&suite, "get_size", 10000, get_size_benchmark, NULL);

  return benchmark_suite_run (&suite);
}
