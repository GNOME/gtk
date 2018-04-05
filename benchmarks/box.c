#include <gtk/gtk.h>
#include "benchmark.h"

/* Command line options */
const char *profile_benchmark_name = NULL;

static GOptionEntry options[] = {
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile_benchmark_name, "Benchmark name to profile using callgrind", NULL },
  { NULL }
};

/*
 *  PROFILING:
 *
 *  valgrind --tool=callgrind --instr-atstart=no benchmarks/name --profile="benchmark name"
 *
 *  ENJOY.
 */

static void
box_create_benchmark (Benchmark *b,
                      gsize      size)
{
  guint i;
  GtkWidget **boxes = g_malloc (sizeof (GtkWidget*) * size);

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      boxes[i] = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    }
  benchmark_stop (b);

  g_free (boxes);
}

static void
box_add_benchmark (Benchmark *b,
                   gsize      size)
{
  guint i;
  GtkWidget *box;
  GtkWidget **buttons = g_malloc (sizeof (GtkWidget*) * size);

  for (i = 0; i < size; i ++)
    buttons[i] = gtk_button_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  benchmark_start (b);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)box, buttons[i]);

  benchmark_stop (b);

  g_free (buttons);
}

static void
box_remove_benchmark (Benchmark *b,
                      gsize      size)
{
  guint i;
  GtkWidget *box;
  GtkWidget **buttons = g_malloc (sizeof (GtkWidget*) * size);

  for (i = 0; i < size; i ++)
    {
      buttons[i] = gtk_button_new ();
      /* We add an extra ref here so the later remove() does NOT dispose the buttons. */
      g_object_ref_sink (buttons[i]);
      g_object_ref (buttons[i]);
    }

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)box, buttons[i]);

  benchmark_start (b);

  for (i = 0; i < size; i ++)
    gtk_container_remove ((GtkContainer *)box, buttons[i]);

  benchmark_stop (b);

  g_free (buttons);
}

static void
box_measure_benchmark (Benchmark *b,
                       gsize      size)
{
  guint i;
  GtkWidget *box;
  GtkWidget **buttons = g_malloc (sizeof (GtkWidget*) * size);

  for (i = 0; i < size; i ++)
    buttons[i] = gtk_button_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)box, buttons[i]);

  benchmark_start (b);

  gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, -1,
                      NULL, NULL, NULL, NULL);

  benchmark_stop (b);

  g_free (buttons);
}

static void
box_allocate_benchmark (Benchmark *b,
                        gsize      size)
{
  guint i;
  GtkWidget *box;
  GtkWidget **buttons = g_malloc (sizeof (GtkWidget*) * size);
  int width, height;

  for (i = 0; i < size; i ++)
    buttons[i] = gtk_button_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)box, buttons[i]);


  gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, -1,
                      &width, NULL, NULL, NULL);
  gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, width,
                      &height, NULL, NULL, NULL);


  benchmark_start (b);
  gtk_widget_size_allocate (box,
                            &(GtkAllocation){0, 0, width, height},
                            -1);
  benchmark_stop (b);

  g_free (buttons);
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

  benchmark_suite_add (&suite, "box create",   10000, box_create_benchmark);
  benchmark_suite_add (&suite, "box add",      10000, box_add_benchmark);
  benchmark_suite_add (&suite, "box remove",   10000, box_remove_benchmark);
  benchmark_suite_add (&suite, "box measure",  10000, box_measure_benchmark);
  benchmark_suite_add (&suite, "box allocate", 10000, box_allocate_benchmark);

  return benchmark_suite_run (&suite);
}
