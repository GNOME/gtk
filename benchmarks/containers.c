#include <gtk/gtk.h>
#include "benchmark.h"

/* Command line options */
const char *profile_benchmark_name = NULL;

static GOptionEntry options[] = {
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile_benchmark_name, "Benchmark name to profile using callgrind", NULL },
  { NULL }
};

typedef struct
{
  GType type;
} ContainerData;

/*
 *  PROFILING:
 *
 *  valgrind --tool=callgrind --instr-atstart=no benchmarks/name --profile="benchmark name"
 *
 *  ENJOY.
 */

static void
container_create_benchmark (Benchmark *b,
                            gsize      size,
                            gpointer   user_data)
{
  ContainerData *data = user_data;
  guint i;
  GtkWidget **widgets = g_malloc (sizeof (GtkWidget*) * size);

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    widgets[i] = g_object_new (data->type, NULL);
  benchmark_stop (b);

  g_free (widgets);
}

static void
container_destroy_benchmark (Benchmark *b,
                             gsize      size,
                             gpointer   user_data)
{
  ContainerData *data = user_data;
  guint i;
  GtkWidget **widgets = g_malloc (sizeof (GtkWidget*) * size);

  for (i = 0; i < size; i ++)
    {
      widgets[i] = g_object_new (data->type, NULL);
      g_object_ref_sink (widgets[i]);
    }

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    g_object_unref (widgets[i]);
  benchmark_stop (b);

  g_free (widgets);
}

static void
container_add_benchmark (Benchmark *b,
                         gsize      size,
                         gpointer   user_data)
{
  ContainerData *data = user_data;
  guint i;
  GtkWidget *container;
  GtkWidget **buttons = g_malloc (sizeof (GtkWidget*) * size);

  for (i = 0; i < size; i ++)
    buttons[i] = gtk_button_new ();

  container = g_object_new (data->type, NULL);

  benchmark_start (b);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)container, buttons[i]);

  benchmark_stop (b);

  g_free (buttons);
}

static void
container_remove_benchmark (Benchmark *b,
                            gsize      size,
                            gpointer   user_data)
{
  ContainerData *data = user_data;
  guint i;
  GtkWidget *container;
  GtkWidget **buttons = g_malloc (sizeof (GtkWidget*) * size);

  for (i = 0; i < size; i ++)
    {
      buttons[i] = gtk_button_new ();
      /* We add an extra ref here so the later remove() does NOT dispose the buttons. */
      g_object_ref_sink (buttons[i]);
      g_object_ref (buttons[i]);
    }

  container = g_object_new (data->type, NULL);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)container, buttons[i]);

  benchmark_start (b);

  for (i = 0; i < size; i ++)
    gtk_container_remove ((GtkContainer *)container, buttons[i]);

  benchmark_stop (b);

  g_free (buttons);
}

static void
container_measure_benchmark (Benchmark *b,
                             gsize      size,
                             gpointer   user_data)
{
  ContainerData *data = user_data;
  guint i;
  GtkWidget *container;
  GtkWidget **buttons = g_malloc (sizeof (GtkWidget*) * size);

  for (i = 0; i < size; i ++)
    buttons[i] = gtk_button_new ();

  container = g_object_new (data->type, NULL);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)container, buttons[i]);

  benchmark_start (b);

  gtk_widget_measure (container, GTK_ORIENTATION_HORIZONTAL, -1,
                      NULL, NULL, NULL, NULL);

  benchmark_stop (b);

  g_free (buttons);
}

static void
container_allocate_benchmark (Benchmark *b,
                              gsize      size,
                              gpointer   user_data)
{
  ContainerData *data = user_data;
  guint i;
  GtkWidget *container;
  GtkWidget **buttons = g_malloc (sizeof (GtkWidget*) * size);
  int width, height;

  for (i = 0; i < size; i ++)
    buttons[i] = gtk_button_new ();

  container = g_object_new (data->type, NULL);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)container, buttons[i]);


  gtk_widget_measure (container, GTK_ORIENTATION_HORIZONTAL, -1,
                      &width, NULL, NULL, NULL);
  gtk_widget_measure (container, GTK_ORIENTATION_VERTICAL, width,
                      &height, NULL, NULL, NULL);

  benchmark_start (b);
  gtk_widget_size_allocate (container,
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
  const GType types[] = {
    GTK_TYPE_BOX,
    GTK_TYPE_GRID,
    GTK_TYPE_STACK,
/*    GTK_TYPE_NOTEBOOK, XXX too slow! :( */
  };
  int i;
  int N = 10000;

  option_context = g_option_context_new ("");
  g_option_context_add_main_entries (option_context, options, NULL);
  if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  benchmark_suite_init (&suite, profile_benchmark_name);
  gtk_init ();

  for (i = 0; i < G_N_ELEMENTS (types); i ++)
    {
      ContainerData *data = g_malloc (sizeof (ContainerData));
      data->type = types[i];

      benchmark_suite_add (&suite,
                           g_strdup_printf ("%s create", g_type_name (types[i])),
                           N, container_create_benchmark, data);
      benchmark_suite_add (&suite,
                           g_strdup_printf ("%s destroy", g_type_name (types[i])),
                           N, container_destroy_benchmark, data);
      benchmark_suite_add (&suite,
                           g_strdup_printf ("%s add", g_type_name (types[i])),
                           N, container_add_benchmark, data);
      benchmark_suite_add (&suite,
                           g_strdup_printf ("%s remove", g_type_name (types[i])),
                           N, container_remove_benchmark, data);
      benchmark_suite_add (&suite,
                           g_strdup_printf ("%s measure", g_type_name (types[i])),
                           N, container_measure_benchmark, data);
      benchmark_suite_add (&suite,
                           g_strdup_printf ("%s allocate", g_type_name (types[i])),
                           N, container_allocate_benchmark, data);
    }

  return benchmark_suite_run (&suite);
}
