#include <gtk/gtk.h>

#include <valgrind/callgrind.h>

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


typedef struct _Benchmark Benchmark;
typedef void (*BenchmarkFunc)(Benchmark *b, gsize size);

struct _Benchmark
{
  char *name;
  gint64 start_time;
  gint64 end_time;
  gsize size;
  BenchmarkFunc func;
  guint profile : 1;
};

static void
benchmark_destroy (Benchmark *b)
{
  g_free (b->name);
}

static void
benchmark_start (Benchmark *b)
{
  b->start_time = g_get_monotonic_time ();

  if (b->profile)
    CALLGRIND_START_INSTRUMENTATION;
}

static void
benchmark_end (Benchmark *b)
{
  if (b->profile)
    CALLGRIND_STOP_INSTRUMENTATION;

  b->end_time = g_get_monotonic_time ();
}

typedef struct
{
  GArray *benchmarks;
  char *profile_benchmark_name;
} BenchmarkSuite;

static void
benchmark_suite_init (BenchmarkSuite *bs,
                      const char     *profile_benchmark_name)
{
  bs->benchmarks = g_array_new (FALSE, TRUE, sizeof (Benchmark));
  g_array_set_clear_func (bs->benchmarks, (GDestroyNotify)benchmark_destroy);
  bs->profile_benchmark_name = (char *)profile_benchmark_name; // XXX strdup
}

static void
benchmark_suite_add (BenchmarkSuite *bs,
                     const char     *benchmark_name,
                     gsize           size,
                     BenchmarkFunc   benchmark_func)
{
  Benchmark *b;

  g_array_set_size (bs->benchmarks, bs->benchmarks->len + 1);
  b = &g_array_index (bs->benchmarks, Benchmark, bs->benchmarks->len - 1);

  b->name = (char *)benchmark_name; /* XXX strdup? */
  b->size = size;
  b->func = benchmark_func;
}

static int
benchmark_suite_run (BenchmarkSuite *bs)
{
  const guint n_benchmarks = bs->benchmarks->len;
  guint i;

  for (i = 0; i < n_benchmarks; i ++)
    {
      gint64 diff;
      Benchmark *b = &g_array_index (bs->benchmarks, Benchmark, i);


      if (bs->profile_benchmark_name != NULL &&
          strcmp (bs->profile_benchmark_name, b->name) == 0)
        b->profile = TRUE;

      b->func (b, b->size);

      if (b->start_time == 0)
        g_error ("Benchmark '%s' did not call benchmark_start()", b->name);

      if (b->end_time == 0)
        g_error ("Benchmark '%s' did not call benchmark_end()", b->name);

      diff = b->end_time - b->start_time;

      g_print ("%s (%" G_GSIZE_FORMAT "): %.2fms\n", b->name, b->size, diff / 1000.0);
    }

  /* XXX Leak bs->benchmarks */
  return 0;
}

/* ################################################################################################ */

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
  benchmark_end (b);

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

  benchmark_end (b);

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
    buttons[i] = gtk_button_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    gtk_container_add ((GtkContainer *)box, buttons[i]);

  benchmark_start (b);

  for (i = 0; i < size; i ++)
    gtk_container_remove ((GtkContainer *)box, buttons[i]);

  benchmark_end (b);

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

  benchmark_suite_add (&suite, "box create", 10000, box_create_benchmark);
  benchmark_suite_add (&suite, "box add", 10000, box_add_benchmark);
  benchmark_suite_add (&suite, "box remove", 10000, box_remove_benchmark);

  return benchmark_suite_run (&suite);
}
